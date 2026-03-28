/**
 * @file audio_test.c
 *
 * @brief MAX98357A I2S tone generator.
 *
 * A FreeRTOS task on Core 0 continuously writes sine-wave samples to the
 * I2S TX DMA.  Frequency changes and stop commands are delivered via
 * xTaskNotify() from the LVGL event callbacks on Core 1.
 *
 * Tone synthesis uses a 256-entry sine lookup table computed once at init
 * and a 16-bit phase accumulator with 8-bit sub-sample precision.
 */

//--------------------------------- INCLUDES ----------------------------------
#include "audio_test.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"  /* for gpio_num_t */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <string.h>
#include <math.h>
#include <stdint.h>

//---------------------------------- MACROS -----------------------------------
#define TAG              "audio_test"
#define SINE_TABLE_SIZE  256U
#define AUDIO_AMPLITUDE  24576   /* 75 % of INT16_MAX — headroom through 9 dB gain stage */
#define SAMPLE_RATE      44100U
#define BUF_FRAMES       256U    /* stereo int16 frames per i2s_channel_write() call */

//----------------------------- STATIC STORAGE --------------------------------
static int16_t           s_sine_table[SINE_TABLE_SIZE];
static i2s_chan_handle_t  s_tx_chan      = NULL;
static TaskHandle_t       s_task_handle = NULL;
static volatile uint32_t  s_current_freq = 0;   /* 0 = stopped */

/* DMA write buffer: BUF_FRAMES × 2 channels (L+R) × int16_t */
static int16_t s_write_buf[BUF_FRAMES * 2];

//---------------------------- AUDIO TASK -------------------------------------

static void _audio_task(void *arg)
{
    ESP_ERROR_CHECK(i2s_channel_enable(s_tx_chan));

    uint32_t freq      = 0;
    uint32_t phase_acc = 0;   /* 16-bit accumulator; upper 8 bits = table index */
    uint32_t notif_val;

    for (;;) {
        /* Non-blocking check for a new frequency command */
        if (xTaskNotifyWait(0, UINT32_MAX, &notif_val, 0) == pdTRUE) {
            freq           = notif_val;
            s_current_freq = freq;
            phase_acc      = 0;
        }

        if (freq == 0) {
            memset(s_write_buf, 0, sizeof(s_write_buf));
        } else {
            /*
             * Phase step (16-bit space, range 0-65535 = one full cycle):
             *   step = freq * 65536 / sample_rate
             * Table index = upper 8 bits of the 16-bit phase = (phase_acc >> 8) & 0xFF
             */
            uint32_t step = (uint32_t)((uint64_t)freq * 65536ULL / SAMPLE_RATE);
            for (uint32_t i = 0; i < BUF_FRAMES; i++) {
                int16_t sample = s_sine_table[(phase_acc >> 8) & 0xFFU];
                s_write_buf[i * 2]     = sample;  /* LEFT channel (MAX98357A GAIN=GND → left) */
                s_write_buf[i * 2 + 1] = 0;       /* RIGHT channel unused */
                phase_acc = (phase_acc + step) & 0xFFFFU;
            }
        }

        size_t bytes_written;
        i2s_channel_write(s_tx_chan, s_write_buf, sizeof(s_write_buf),
                          &bytes_written, portMAX_DELAY);
    }
    /* unreachable */
    vTaskDelete(NULL);
}

//------------------------------ PUBLIC FUNCTIONS -----------------------------

esp_err_t audio_test_init(void)
{
    if (s_tx_chan != NULL) {
        return ESP_OK;  /* already initialised */
    }

    /* Build sine lookup table (computed once at startup) */
    for (uint32_t i = 0; i < SINE_TABLE_SIZE; i++) {
        s_sine_table[i] = (int16_t)(AUDIO_AMPLITUDE *
                          sinf(2.0f * (float)M_PI * (float)i / (float)SINE_TABLE_SIZE));
    }

    /* Allocate TX-only I2S channel */
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;   /* emit zeros automatically if DMA starves */
    esp_err_t ret = i2s_new_channel(&chan_cfg, &s_tx_chan, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2s_new_channel: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Configure Philips I2S, 44100 Hz, 16-bit stereo */
    i2s_std_config_t std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
                                                         I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = (gpio_num_t)AUDIO_BCLK_GPIO,
            .ws   = (gpio_num_t)AUDIO_LRC_GPIO,
            .dout = (gpio_num_t)AUDIO_DOUT_GPIO,
            .din  = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv   = false,
            },
        },
    };
    ret = i2s_channel_init_std_mode(s_tx_chan, &std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2s_channel_init_std_mode: %s", esp_err_to_name(ret));
        i2s_del_channel(s_tx_chan);
        s_tx_chan = NULL;
        return ret;
    }

    /* Audio task on Core 0 (LVGL runs on Core 1) at priority 5 */
    xTaskCreatePinnedToCore(_audio_task, "audio_test", 4096, NULL,
                            5, &s_task_handle, 0);

    ESP_LOGI(TAG, "init OK — BCLK=%d  WS=%d  DOUT=%d  SR=%lu Hz",
             AUDIO_BCLK_GPIO, AUDIO_LRC_GPIO, AUDIO_DOUT_GPIO, (unsigned long)SAMPLE_RATE);
    return ESP_OK;
}

void audio_test_play_tone(uint32_t freq_hz)
{
    if (s_task_handle) {
        xTaskNotify(s_task_handle, freq_hz, eSetValueWithOverwrite);
    }
}

void audio_test_stop(void)
{
    if (s_task_handle) {
        xTaskNotify(s_task_handle, 0U, eSetValueWithOverwrite);
    }
}

uint32_t audio_test_get_freq(void)
{
    return s_current_freq;
}
