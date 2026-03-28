/**
 * @file screen_capture.c
 *
 * @brief Shadow framebuffer updated from the LVGL flush callback.
 */

#include "screen_capture.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include <string.h>

#define SCREEN_W 320
#define SCREEN_H 240
#define BUF_BYTES (SCREEN_W * SCREEN_H * sizeof(uint16_t))

static const char *TAG = "screen_capture";

static uint16_t         *s_shadow = NULL;
static SemaphoreHandle_t s_mutex  = NULL;

void screen_capture_init(void)
{
    /* Prefer PSRAM; fall back to internal RAM only if enough contiguous space exists */
    s_shadow = heap_caps_malloc(BUF_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_shadow && heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) >= BUF_BYTES) {
        s_shadow = malloc(BUF_BYTES);
    }
    if (!s_shadow) {
        ESP_LOGW(TAG, "Screen capture disabled — no contiguous RAM for shadow buffer (%u bytes). Enable PSRAM to use this feature.", (unsigned)BUF_BYTES);
    } else {
        memset(s_shadow, 0, BUF_BYTES);
        ESP_LOGI(TAG, "Shadow buffer allocated (%u bytes)", (unsigned)BUF_BYTES);
    }
    s_mutex = xSemaphoreCreateMutex();
}

void screen_capture_flush(int x1, int y1, int x2, int y2, const uint16_t *pixels)
{
    if (!s_mutex || !s_shadow) return;
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(10)) != pdTRUE) return;

    int w = x2 - x1 + 1;
    for (int y = y1; y <= y2; y++) {
        memcpy(&s_shadow[y * SCREEN_W + x1],
               &pixels[(y - y1) * w],
               w * sizeof(uint16_t));
    }

    xSemaphoreGive(s_mutex);
}

bool screen_capture_take(const uint16_t **buf, int *w, int *h)
{
    if (!s_mutex || !s_shadow) return false;
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(200)) != pdTRUE) return false;
    *buf = s_shadow;
    *w   = SCREEN_W;
    *h   = SCREEN_H;
    return true;
}

void screen_capture_give(void)
{
    xSemaphoreGive(s_mutex);
}
