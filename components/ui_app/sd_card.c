/**
 * @file sd_card.c
 *
 * @brief SD card initialisation over the shared VSPI bus.
 *
 * The SD card reader is on the ILI9341 display module (CS = GPIO5).
 * GPIO5 is a boot-strapping pin — it floats or is pulled HIGH at power-on,
 * which can confuse the SD card about whether it should enter SPI or SD mode.
 * sd_card_cs_pre_init() must be called before lvgl_driver_init() to hold CS
 * HIGH (deasserted) throughout display/touch init.
 */

#include "sd_card.h"

#include "driver/gpio.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

static const char *TAG = "sd_card";

void sd_card_cs_pre_init(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << SD_CS_GPIO),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);
    gpio_set_level(SD_CS_GPIO, 1);   /* CS deasserted (HIGH) */
    ESP_LOGI(TAG, "GPIO%d pre-configured HIGH (SD CS deasserted)", SD_CS_GPIO);
}

esp_err_t sd_card_init(void)
{
    esp_vfs_fat_sdmmc_mount_config_t mount_cfg = {
        .format_if_mount_failed = false,
        .max_files              = 8,
        .allocation_unit_size   = 16 * 1024,
    };

    /*
     * sdspi_host_init() is software-only — it does NOT call spi_bus_initialize().
     * The VSPI bus was already brought up by lvgl_driver_init() with MISO=19.
     *
     * Use 4 MHz instead of the 20 MHz default: the shared MISO line (GPIO19)
     * is also driven by the XPT2046 touch IC which can cause signal integrity
     * issues at higher speeds.
     */
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot         = VSPI_HOST;
    host.max_freq_khz = 8000;

    /* Pull-up on MISO so the line is not floating between SPI transactions */
    gpio_set_pull_mode(19, GPIO_PULLUP_ONLY);

    sdspi_device_config_t dev_cfg = SDSPI_DEVICE_CONFIG_DEFAULT();
    dev_cfg.host_id = VSPI_HOST;
    dev_cfg.gpio_cs = SD_CS_GPIO;

    sdmmc_card_t *card;
    esp_err_t ret = esp_vfs_fat_sdspi_mount(SD_MOUNT_POINT, &host,
                                             &dev_cfg, &mount_cfg, &card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Mount failed on GPIO%d: %s", SD_CS_GPIO, esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Mounted at %s — %s %.1f GB",
             SD_MOUNT_POINT, card->cid.name,
             (double)((uint64_t)card->csd.capacity * card->csd.sector_size) / 1e9);
    return ESP_OK;
}
