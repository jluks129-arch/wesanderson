/**
 * @file story_player.h
 *
 * @brief MP3 story player — reads from SD card, outputs via MAX98357A I2S.
 *
 * Pin assignments (shared with audio_test):
 *   BCLK  → GPIO13
 *   LRC   → GPIO2
 *   DOUT  → GPIO26
 *
 * SD card (shared VSPI bus with display/touch):
 *   CS    → GPIO5
 *   MOSI  → GPIO23
 *   MISO  → GPIO19
 *   SCK   → GPIO18
 *
 * Audio files must be placed on SD card at:
 *   /audio/<hero>-<world>-<animal>.mp3
 *
 * Example:  /audio/robot-svemir-sova.mp3
 */

#pragma once
#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialise I2S TX channel and mount the SD card.
 *        Safe to call multiple times; subsequent calls are no-ops.
 */
esp_err_t story_player_init(void);

/**
 * @brief Start playing /audio/<hero>-<world>-<animal>.mp3 from the SD card.
 *        Any currently-playing track is stopped first.
 *
 * Index mapping matches ui_app.c:
 *   hero:   0=vitez  1=carobnjak  2=robot  3=vila
 *   world:  0=suma   1=svemir     2=more   3=dvorac
 *   animal: 0=zmaj   1=pas        2=sova   3=macka
 */
void story_player_play(int hero, int world, int animal);

/** @brief Stop current playback immediately. */
void story_player_stop(void);

/** @brief Returns true while an audio track is being decoded/output. */
bool story_player_is_playing(void);

#ifdef __cplusplus
}
#endif
