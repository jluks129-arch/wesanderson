/**
 * @file story_player.c
 *
 * @brief MP3 story player for BLDK.
 *
 * The player task is always created at init. SD card mount and file open
 * happen inside the task so failures are logged rather than silently blocking
 * all playback.
 */

//--------------------------------- INCLUDES ----------------------------------
#include "story_player.h"

#define MINIMP3_ONLY_MP3
#include "minimp3.h"

#include "driver/i2s_std.h"
#include "driver/spi_common.h"
#include "driver/sdspi_host.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

//---------------------------------- MACROS -----------------------------------
#define TAG  "story_player"

/* I2S pins */
#define BCLK_GPIO   13
#define LRC_GPIO     2
#define DOUT_GPIO   26

/* SD card — shared VSPI bus (already initialised by LVGL) */
#define SD_CS_GPIO   5
#define SD_SPI_HOST  SPI3_HOST
#define SD_MOUNT     "/sdcard"

/* Buffers */
#define MP3_FILE_BUF  (8 * 1024)
#define PCM_BUF_SIZE  (MINIMP3_MAX_SAMPLES_PER_FRAME * 2)

//------------------------------- STATIC STATE --------------------------------
static i2s_chan_handle_t  s_tx_chan   = NULL;
static TaskHandle_t       s_task     = NULL;
static SemaphoreHandle_t  s_play_sem = NULL;
static sdmmc_card_t      *s_card     = NULL;
static bool               s_sd_tried = false;

static char             s_path[128];
static volatile bool    s_stop_req  = false;
static volatile bool    s_playing   = false;

static const char * const s_hero_names[]   = { "vitez", "carobnjak", "robot", "vila"   };
static const char * const s_world_names[]  = { "suma",  "svemir",    "more",  "dvorac" };
static const char * const s_animal_names[] = { "zmaj",  "pas",       "sova",  "macka"  };

//------------------------ PRIVATE: I2S -----------------------------------------

static esp_err_t _i2s_init(void)
{
    if (s_tx_chan != NULL) return ESP_OK;

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;

    esp_err_t ret = i2s_new_channel(&chan_cfg, &s_tx_chan, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2s_new_channel failed: %s", esp_err_to_name(ret));
        return ret;
    }

    i2s_std_config_t std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(44100),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
                        I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = (gpio_num_t)BCLK_GPIO,
            .ws   = (gpio_num_t)LRC_GPIO,
            .dout = (gpio_num_t)DOUT_GPIO,
            .din  = I2S_GPIO_UNUSED,
        },
    };
    ret = i2s_channel_init_std_mode(s_tx_chan, &std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2s_channel_init_std_mode failed: %s", esp_err_to_name(ret));
        i2s_del_channel(s_tx_chan);
        s_tx_chan = NULL;
        return ret;
    }

    ret = i2s_channel_enable(s_tx_chan);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2s_channel_enable failed: %s", esp_err_to_name(ret));
        i2s_del_channel(s_tx_chan);
        s_tx_chan = NULL;
        return ret;
    }

    ESP_LOGI(TAG, "I2S ready  BCLK=%d  WS=%d  DOUT=%d", BCLK_GPIO, LRC_GPIO, DOUT_GPIO);
    return ESP_OK;
}

static void _i2s_reconfigure(uint32_t hz, int channels)
{
    i2s_channel_disable(s_tx_chan);

    i2s_std_clk_config_t clk = I2S_STD_CLK_DEFAULT_CONFIG(hz);
    i2s_channel_reconfig_std_clock(s_tx_chan, &clk);

    i2s_slot_mode_t sm = (channels == 2) ? I2S_SLOT_MODE_STEREO : I2S_SLOT_MODE_MONO;
    i2s_std_slot_config_t slot = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
                                     I2S_DATA_BIT_WIDTH_16BIT, sm);
    i2s_channel_reconfig_std_slot(s_tx_chan, &slot);

    i2s_channel_enable(s_tx_chan);
    ESP_LOGI(TAG, "I2S reconfigured: %lu Hz  %s",
             (unsigned long)hz, (channels == 2) ? "stereo" : "mono");
}

//------------------------ PRIVATE: SD card ------------------------------------

static esp_err_t _sd_mount(void)
{
    if (s_card != NULL) return ESP_OK;

    ESP_LOGI(TAG, "Mounting SD card on SPI3_HOST, CS=GPIO%d ...", SD_CS_GPIO);

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SD_SPI_HOST;

    sdspi_device_config_t slot_cfg = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_cfg.gpio_cs  = SD_CS_GPIO;
    slot_cfg.host_id  = SD_SPI_HOST;

    esp_vfs_fat_mount_config_t mount_cfg = {
        .format_if_mount_failed = false,
        .max_files              = 4,
        .allocation_unit_size   = 16 * 1024,
    };

    esp_err_t ret = esp_vfs_fat_sdspi_mount(SD_MOUNT, &host, &slot_cfg,
                                            &mount_cfg, &s_card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SD mount FAILED: %s  (check: card inserted? DIP switch S1-4 ON? FAT32?)",
                 esp_err_to_name(ret));
        s_card = NULL;
        return ret;
    }

    sdmmc_card_print_info(stdout, s_card);
    ESP_LOGI(TAG, "SD mounted at %s", SD_MOUNT);
    return ESP_OK;
}

//------------------------ PRIVATE: player task --------------------------------

static void _player_task(void *arg)
{
    static uint8_t         file_buf[MP3_FILE_BUF];
    static mp3d_sample_t   pcm_buf[PCM_BUF_SIZE];
    static mp3dec_t        dec;

    ESP_LOGI(TAG, "Player task started on core %d", xPortGetCoreID());

    for (;;) {
        xSemaphoreTake(s_play_sem, portMAX_DELAY);
        ESP_LOGI(TAG, "Play request: %s", s_path);

        s_stop_req = false;
        s_playing  = true;

        /* Mount SD on first use (or retry after failure) */
        if (s_card == NULL) {
            if (_sd_mount() != ESP_OK) {
                ESP_LOGE(TAG, "Cannot play — SD card not available");
                s_playing = false;
                continue;
            }
        }

        FILE *f = fopen(s_path, "rb");
        if (!f) {
            ESP_LOGE(TAG, "Cannot open: %s  (check filename and path)", s_path);
            s_playing = false;
            continue;
        }
        ESP_LOGI(TAG, "Opened: %s", s_path);

        if (s_tx_chan == NULL) {
            ESP_LOGE(TAG, "I2S not ready");
            fclose(f);
            s_playing = false;
            continue;
        }

        mp3dec_init(&dec);

        size_t buf_filled = 0;
        bool   configured = false;
        bool   eof        = false;

        while (!s_stop_req) {
            /* Refill the read buffer */
            if (!eof && buf_filled < MP3_FILE_BUF) {
                size_t space = MP3_FILE_BUF - buf_filled;
                size_t got   = fread(file_buf + buf_filled, 1, space, f);
                buf_filled  += got;
                if (got == 0) eof = true;
            }

            if (buf_filled == 0) break;

            mp3dec_frame_info_t info;
            int samples = mp3dec_decode_frame(&dec, file_buf, (int)buf_filled,
                                              pcm_buf, &info);

            if (info.frame_bytes == 0) {
                /* Sync lost — skip 1 byte */
                if (buf_filled > 1) memmove(file_buf, file_buf + 1, buf_filled - 1);
                if (buf_filled > 0) buf_filled--;
                continue;
            }

            /* Consume decoded bytes */
            size_t remaining = buf_filled - (size_t)info.frame_bytes;
            if (remaining > 0) memmove(file_buf, file_buf + info.frame_bytes, remaining);
            buf_filled = remaining;

            if (samples <= 0) continue;

            /* Reconfigure I2S on first valid frame */
            if (!configured) {
                _i2s_reconfigure((uint32_t)info.hz, info.channels);
                configured = true;
            }

            size_t byte_count = (size_t)samples * (size_t)info.channels
                                * sizeof(mp3d_sample_t);
            size_t written;
            i2s_channel_write(s_tx_chan, pcm_buf, byte_count,
                              &written, pdMS_TO_TICKS(2000));
        }

        fclose(f);
        s_playing = false;
        ESP_LOGI(TAG, "Playback done: %s", s_path);
    }
}

//---------------------------- PUBLIC FUNCTIONS --------------------------------

esp_err_t story_player_init(void)
{
    if (s_task != NULL) return ESP_OK;

    esp_err_t ret = _i2s_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S init failed — audio will not work");
        /* Continue anyway so the task exists and logs the error on play */
    }

    s_play_sem = xSemaphoreCreateBinary();
    if (!s_play_sem) {
        ESP_LOGE(TAG, "Failed to create semaphore");
        return ESP_ERR_NO_MEM;
    }

    xTaskCreatePinnedToCore(_player_task, "story_player", 8192, NULL,
                            5, &s_task, 0);
    if (!s_task) {
        ESP_LOGE(TAG, "Failed to create player task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "story_player_init OK (SD mount deferred to first play)");
    return ESP_OK;
}

void story_player_play(int hero, int world, int animal)
{
    if (s_task == NULL) {
        ESP_LOGE(TAG, "story_player_play: not initialised");
        return;
    }

    snprintf(s_path, sizeof(s_path), SD_MOUNT "/audio/%s-%s-%s.mp3",
             s_hero_names[hero], s_world_names[world], s_animal_names[animal]);

    s_stop_req = true;            /* stop current playback if any */
    xSemaphoreGive(s_play_sem);   /* wake the player task */
}

void story_player_stop(void)
{
    s_stop_req = true;
}

bool story_player_is_playing(void)
{
    return s_playing;
}
