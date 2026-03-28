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
          → ui_init() [components/ui_app/squareline/project/ui.c]  ← SquareLine-generated
```

**Key components:**
- `main/gui.c` — LVGL lifecycle, semaphore-guarded task on Core 1 (Core 0 reserved for WiFi)
- `components/ui_app/` — UI wrapper; calls into SquareLine-generated code
- `components/ui_app/squareline/project/` — All generated UI files (screens, images, animations)
- `components/ui_app/squareline/project/ui_events.c` — **The only file developers edit** after SquareLine export; preserved across re-exports
- `components/lvgl/` — LVGL 8.3.4 (git submodule)
- `components/lvgl_esp32_drivers/` — Display/touch drivers (git submodule, patched via `lvgl_esp32_drivers_8-3.patch`)
- `components/wifi_station/` — Optional WiFi; credentials configured via `idf.py menuconfig`

## SquareLine Workflow

1. Open `components/ui_app/squareline/project/esp32_gui.spj` in SquareLine Studio
2. Design UI visually; test in SquareLine's Play mode
3. Export → overwrites all files in `components/ui_app/squareline/project/` except `ui_events.c`
4. Implement event callbacks in `ui_events.c`
5. Build and flash

The `components/ui_app/CMakeLists.txt` dynamically globbing all `.c` files from the squareline directory — new exported files are picked up automatically.

## Test App

A standalone hardware test UI lives in `components/ui_app/ui_app.c`. It replaces the `ui_init()` call and does **not** use any SquareLine-generated code (those files still compile but are unused).

Three screens, switchable via a bottom nav bar:

| Screen | Purpose |
|--------|---------|
| **Display** | 7 full-height color bars (R, G, B, White, Cyan, Magenta, Yellow) — verifies color depth and full display area |
| **Touch** | Tap anywhere to see live X/Y coordinates; counter button tallies taps and resets on press |
| **Widgets** | Slider, arc, switch, checkbox — verifies interactive LVGL input |

To restore the SquareLine UI, revert `ui_app.c` to call `ui_init()` and re-add `#include "squareline/project/ui.h"`.

## Display Configuration

Resolution, color depth, and driver selection live in `sdkconfig.defaults` (and propagate to `sdkconfig`). Changes to display hardware require updating both `sdkconfig` and potentially the SquareLine project settings (Project Settings → Board).
