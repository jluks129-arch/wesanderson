/**
 * @file screen_capture.h
 *
 * @brief Shadow framebuffer — mirrors the LVGL display for web streaming.
 */

#ifndef __SCREEN_CAPTURE_H__
#define __SCREEN_CAPTURE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Allocate the shadow buffer. Call once before the GUI task starts.
 */
void screen_capture_init(void);

/**
 * @brief Copy a flushed LVGL region into the shadow buffer.
 *        Call this from the display flush callback.
 */
void screen_capture_flush(int x1, int y1, int x2, int y2, const uint16_t *pixels);

/**
 * @brief Lock the shadow buffer for reading.
 * @param buf  Receives a pointer to the RGB565 pixel data (row-major, top-to-bottom).
 * @param w    Display width in pixels.
 * @param h    Display height in pixels.
 * @return true if the lock was acquired.
 */
bool screen_capture_take(const uint16_t **buf, int *w, int *h);

/**
 * @brief Release the lock acquired by screen_capture_take().
 */
void screen_capture_give(void);

#ifdef __cplusplus
}
#endif

#endif /* __SCREEN_CAPTURE_H__ */
