/**
 * @file babyphone.h
 *
 * @brief UART-driven camera capture for the Babyphone screen.
 *
 * Every 10 seconds while active, the BLDK sends "CAP\n" to an attached
 * ESP32-CAM over UART1.  The ESP32-CAM replies with:
 *
 *   "SIZE:<n>\r\n"   — decimal byte count
 *   <n bytes>        — raw JPEG from OV2640
 *
 * The JPEG is stored in a static 32 KB buffer and served at HTTP GET /baby.
 *
 * Pin assignments:
 *   CAM_UART_TX_GPIO → ESP32-CAM RXD0 (GPIO3)
 *   CAM_UART_RX_GPIO → ESP32-CAM TXD0 (GPIO1)
 *
 * !!! UPDATE CAM_UART_TX_GPIO and CAM_UART_RX_GPIO to match your BLDK wiring.
 */

#ifndef BABYPHONE_H
#define BABYPHONE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ---- UART configuration ------------------------------------------------- */
#define CAM_UART_NUM       1          /* UART_NUM_1; avoids console UART0     */
#define CAM_UART_TX_GPIO   14         /* BLDK IO14 → ESP32-CAM RXD0 */
#define CAM_UART_RX_GPIO   27         /* BLDK IO27 ← ESP32-CAM TXD0 */
#define CAM_UART_BAUD      115200
#define CAM_JPEG_BUF_SIZE  (32 * 1024)

/* ---- UI refresh callback type ------------------------------------------- */
/* Compatible with lv_async_cb_t — no LVGL include needed in this header.    */
typedef void (*babyphone_refresh_cb_t)(void *arg);

/* ---- Public API ---------------------------------------------------------- */

/**
 * @brief Initialise UART driver and create the capture task.
 *        Safe to call multiple times; subsequent calls are no-ops.
 */
esp_err_t babyphone_init(void);

/**
 * @brief Start periodic capture (every 10 s).
 */
void babyphone_start(void);

/**
 * @brief Stop periodic capture.
 *        Ongoing transfers complete normally; the buffer is not cleared.
 */
void babyphone_stop(void);

/**
 * @brief Returns true if capture is currently active.
 */
bool babyphone_is_running(void);

/**
 * @brief Lock the JPEG buffer and return a pointer + length.
 *        Caller MUST call babyphone_release_image() afterwards.
 * @return true if a valid image is available and the lock was taken.
 */
bool babyphone_get_image(const uint8_t **buf, size_t *len);

/**
 * @brief Release the JPEG buffer lock acquired by babyphone_get_image().
 */
void babyphone_release_image(void);

/**
 * @brief Store a received JPEG frame.
 *        Called by the HTTP POST /baby handler in web_server.c.
 *        No-op when babyphone is stopped.
 * @param data  Pointer to JPEG data.
 * @param len   Length in bytes (must be <= CAM_JPEG_BUF_SIZE).
 * @return ESP_OK on success.
 */
esp_err_t babyphone_store_image(const uint8_t *data, size_t len);

/**
 * @brief Return seconds-since-boot at the time of the last successful capture.
 *        Returns 0 if no capture has occurred yet.
 */
uint32_t babyphone_last_capture_uptime(void);

/**
 * @brief Register a callback invoked (via lv_async_call) after each capture.
 *        The callback runs in the LVGL task context (Core 1).
 *        Call this before babyphone_start().
 */
void babyphone_set_ui_refresh_cb(babyphone_refresh_cb_t cb);

#ifdef __cplusplus
}
#endif
#endif /* BABYPHONE_H */
