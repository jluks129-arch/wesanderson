/**
 * @file audio_test.h
 *
 * @brief MAX98357A I2S tone generator for the speaker test screen.
 *
 * Pin assignments:
 *   BCLK  → GPIO13  (BLDK pin19, ACC CS)
 *   LRC   → GPIO2   (BLDK pin9,  BUZZ)
 *   DOUT  → GPIO26  (rewired — original BLDK pin18/IO39 is input-only on ESP32)
 *
 * GAIN pin shorted to GND → 9 dB gain, left-channel output.
 */

#ifndef AUDIO_TEST_H
#define AUDIO_TEST_H

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"
#include <stdint.h>

/* ---- GPIO assignments (plain integers so the #error guard works) -------- */
#define AUDIO_BCLK_GPIO   13   /* GPIO13 → BCLK  */
#define AUDIO_LRC_GPIO     2   /* GPIO2  → LRC/WS */
#define AUDIO_DOUT_GPIO   26   /* GPIO26 → DIN (rewired from IO39 which is input-only) */

#if AUDIO_DOUT_GPIO == 39
#  error "GPIO39 is input-only on ESP32 — rewire MAX98357A DIN to an output GPIO and update AUDIO_DOUT_GPIO."
#endif

/* ---- Public API --------------------------------------------------------- */

/**
 * @brief Allocate and initialise the I2S TX channel and sine lookup table.
 *        Safe to call multiple times; subsequent calls are no-ops.
 */
esp_err_t audio_test_init(void);

/**
 * @brief Start (or switch to) a continuous sine-wave tone.
 * @param freq_hz  Desired frequency in Hz (e.g. 220, 440, 880, 1760).
 */
void audio_test_play_tone(uint32_t freq_hz);

/**
 * @brief Stop audio output and silence the I2S bus.
 */
void audio_test_stop(void);

/**
 * @brief Return the currently playing frequency, or 0 if stopped.
 */
uint32_t audio_test_get_freq(void);

#ifdef __cplusplus
}
#endif
#endif /* AUDIO_TEST_H */
