/**
 * @file babyphone.c
 *
 * @brief JPEG frame buffer for the Babyphone screen.
 *
 * The ESP32-CAM sends frames via HTTP POST to /baby on this device.
 * _web_server_ calls babyphone_store_image() on each POST.
 * The HTTP GET /baby handler serves the latest stored frame.
 * The UI is notified via lv_async_call after each store.
 */

//--------------------------------- INCLUDES ----------------------------------
#include "babyphone.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "lvgl.h"   /* lv_async_call */
#include <string.h>

//---------------------------------- MACROS -----------------------------------
#define TAG  "babyphone"

//----------------------------- STATIC STORAGE --------------------------------
static uint8_t            s_jpeg_buf[CAM_JPEG_BUF_SIZE];
static size_t             s_jpeg_len       = 0;
static SemaphoreHandle_t  s_buf_mutex      = NULL;
static volatile bool      s_running        = true;  /* accept frames immediately */
static volatile uint32_t  s_last_uptime_s  = 0;
static babyphone_refresh_cb_t s_refresh_cb = NULL;

//------------------------------ PUBLIC FUNCTIONS -----------------------------

esp_err_t babyphone_init(void)
{
    if (s_buf_mutex != NULL) {
        return ESP_OK;   /* idempotent */
    }

    s_buf_mutex = xSemaphoreCreateMutex();
    if (!s_buf_mutex) {
        ESP_LOGE(TAG, "failed to create mutex");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "init OK");
    return ESP_OK;
}

void babyphone_start(void)
{
    s_running = true;
    ESP_LOGI(TAG, "started — accepting incoming frames");
}

void babyphone_stop(void)
{
    s_running = false;
    ESP_LOGI(TAG, "stopped");
}

bool babyphone_is_running(void)
{
    return s_running;
}

esp_err_t babyphone_store_image(const uint8_t *data, size_t len)
{
    if (!s_running) {
        return ESP_OK;   /* discard while stopped */
    }
    if (!data || len == 0 || len > CAM_JPEG_BUF_SIZE) {
        ESP_LOGW(TAG, "store_image: invalid args (len=%zu)", len);
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_buf_mutex) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(s_buf_mutex, pdMS_TO_TICKS(300)) != pdTRUE) {
        ESP_LOGW(TAG, "store_image: mutex timeout");
        return ESP_ERR_TIMEOUT;
    }

    memcpy(s_jpeg_buf, data, len);
    s_jpeg_len      = len;
    s_last_uptime_s = (uint32_t)(pdTICKS_TO_MS(xTaskGetTickCount()) / 1000UL);

    xSemaphoreGive(s_buf_mutex);

    ESP_LOGI(TAG, "stored %zu bytes", len);

    if (s_refresh_cb) {
        lv_async_call((lv_async_cb_t)s_refresh_cb, NULL);
    }

    return ESP_OK;
}

bool babyphone_get_image(const uint8_t **buf, size_t *len)
{
    if (!s_buf_mutex || s_jpeg_len == 0) return false;
    if (xSemaphoreTake(s_buf_mutex, pdMS_TO_TICKS(200)) != pdTRUE) return false;
    *buf = s_jpeg_buf;
    *len = s_jpeg_len;
    return true;
}

void babyphone_release_image(void)
{
    if (s_buf_mutex) {
        xSemaphoreGive(s_buf_mutex);
    }
}

uint32_t babyphone_last_capture_uptime(void)
{
    return s_last_uptime_s;
}

void babyphone_set_ui_refresh_cb(babyphone_refresh_cb_t cb)
{
    s_refresh_cb = cb;
}
