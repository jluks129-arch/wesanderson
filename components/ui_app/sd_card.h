/**
 * @file sd_card.h
 *
 * @brief SD card initialisation over SPI (shared VSPI bus with display).
 *
 * The card is mounted at SD_MOUNT_POINT via ESP-IDF VFS/FATFS so that normal
 * POSIX file calls (and LVGL's FS-POSIX driver) can reach it.
 *
 * Pin wiring – change SD_CS_GPIO to match your board:
 *
 *   MOSI  GPIO23  (shared with display)
 *   MISO  GPIO19  (shared with touch)
 *   CLK   GPIO18  (shared with display/touch)
 *   CS    GPIO22  ← only SD-card-specific pin
 */

#ifndef SD_CARD_H
#define SD_CARD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"

/* Filesystem mount point exposed to VFS */
#define SD_MOUNT_POINT   "/sdcard"

/* CS pin – SD reader on the ILI9341 display module */
#define SD_CS_GPIO       5

/**
 * @brief  Configure the SD CS pin as output HIGH as early as possible.
 *
 * Call this BEFORE lvgl_driver_init() so the SD card ignores all SPI clock
 * pulses during display/touch initialisation and stays in a known state.
 */
void sd_card_cs_pre_init(void);

/**
 * @brief  Initialise and mount the SD card.
 *
 * Call AFTER lvgl_driver_init() (i.e. from ui_app_init()), once the VSPI
 * bus is fully up.
 *
 * @return ESP_OK on success, or an esp_err_t code on failure.
 */
esp_err_t sd_card_init(void);

#ifdef __cplusplus
}
#endif

#endif /* SD_CARD_H */
