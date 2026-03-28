# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Embedded GUI application for the **Byte Lab Development Kit (BLDK)** — an ESP32-based teaching platform with integrated display and touch input. The UI is designed in SquareLine Studio and rendered via LVGL.

## Build & Flash

```bash
# Source ESP-IDF environment (required each session)
. $HOME/.espressif/v5.5.3/esp-idf/export.sh

# First-time setup: apply driver compatibility patch
bash scripts/apply_patch.sh

# Build
idf.py build

# Flash and monitor
idf.py -p /dev/ttyUSB1 flash monitor
```

ESP-IDF 5.5.3 is required (see `dependencies.lock`). On first use, copy `sdkconfig.defaults` to `sdkconfig`.

There is no automated test suite — validation is done by flashing to hardware and visual inspection.

## Architecture

**Startup flow:**
```
app_main() [main/app_main.c]
  → gui_init() [main/gui.c]
    → FreeRTOS task on Core 1
      → LVGL init + display/touch driver init
      → lv_task_handler() every 10ms
        → ui_app_init() [components/ui_app/ui_app.c]
          → audio_test_init()            ← I2S init, audio task on Core 0
          → ui_init() [components/ui_app/squareline/project/ui.c]  ← SquareLine-generated
```

**Key components:**
- `main/gui.c` — LVGL lifecycle, semaphore-guarded task on Core 1 (Core 0 reserved for WiFi/audio)
- `components/ui_app/` — UI wrapper; calls into SquareLine-generated code
- `components/ui_app/ui_app.c` — Story generator app + audio test screen (7 screens total)
- `components/ui_app/audio_test.c/.h` — MAX98357A I2S tone generator; FreeRTOS task on Core 0
- `components/ui_app/squareline/project/` — All generated UI files (screens, images, animations)
- `components/ui_app/squareline/project/ui_events.c` — **The only file developers edit** after SquareLine export; preserved across re-exports
- `components/lvgl/` — LVGL 8.3.4 (git submodule)
- `components/lvgl_esp32_drivers/` — Display/touch drivers (git submodule, patched via `lvgl_esp32_drivers_8-3.patch`)
- `components/wifi_station/` — Optional WiFi; credentials configured via `idf.py menuconfig`

## Current App — Story Generator + Audio Test

`components/ui_app/ui_app.c` implements a Croatian children's story generator across 7 screens. The SquareLine-generated files still compile but are unused.

### Screens

| # | Name | Purpose |
|---|------|---------|
| 0 | Welcome | "Stvori svoju pričicu!" start button + speaker test button |
| 1 | Hero | Pick hero: Vitez, Čarobnjak, Robot, Vila |
| 2 | World | Pick world: Šuma, Svemir, More, Dvorac |
| 3 | Animal | Pick animal: Zmaj, Pas, Sova, Mačka |
| 4 | Mood | Pick mood: Smješno, Strašno, Uzbudljivo, Dirljivo |
| 5 | Story | Display generated story; "Još jednom!" to restart |
| 6 | Audio Test | Tone generator for MAX98357A speaker verification |

### Audio Test Screen (screen 6)

Reached via "Test zvucnika" on the welcome screen. Plays sine-wave tones at 220 / 440 / 880 / 1760 Hz through the MAX98357A. Stop and Nazad (back) buttons silence audio.

## MAX98357A Hardware

I2S amplifier wired to the BLDK board:

| MAX98357A pin | BLDK label | ESP32 GPIO |
|---------------|------------|------------|
| BCLK | ACC CS (pin19) | GPIO13 |
| LRC | BUZZ (pin9) | GPIO2 |
| DIN | *(rewired)* | **GPIO26** |
| GND | GND | GND |
| GAIN | shorted to GND | — |

> **Note:** The BLDK silkscreen routes DIN to pin18 (IO39), but GPIO39 is input-only on ESP32.
> DIN must be wired to GPIO26 (or any other output-capable GPIO) and `AUDIO_DOUT_GPIO` in
> `audio_test.h` updated to match. A compile-time `#error` fires if it is set to 39.

GAIN shorted to GND → 9 dB gain, left-channel output.

## SquareLine Workflow

1. Open `components/ui_app/squareline/project/esp32_gui.spj` in SquareLine Studio
2. Design UI visually; test in SquareLine's Play mode
3. Export → overwrites all files in `components/ui_app/squareline/project/` except `ui_events.c`
4. Implement event callbacks in `ui_events.c`
5. Build and flash

The `components/ui_app/CMakeLists.txt` dynamically globs all `.c` files from the squareline directory — new exported files are picked up automatically.

To restore the SquareLine UI (instead of the story generator), revert `ui_app.c` to call `ui_init()` and re-add `#include "squareline/project/ui.h"`.

## Display Configuration

Resolution, color depth, and driver selection live in `sdkconfig.defaults` (and propagate to `sdkconfig`). Changes to display hardware require updating both `sdkconfig` and potentially the SquareLine project settings (Project Settings → Board).
