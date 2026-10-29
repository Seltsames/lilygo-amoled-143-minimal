# LilyGo T-Display S3 AMOLED 1.43" — Minimal Setup

Working, calibrated baseline for drawing on the **LilyGo T-Display S3 AMOLED 1.43"**
(round AMOLED, **CO5300** controller, FT3168 touch) with PlatformIO + Arduino.

Status: verified on hardware (2026-10-27): text, lines, circles all render; the
screen-limit ring is centered on the round glass.

---

## 1. Quick reference

| Item | Value |
|---|---|
| MCU | ESP32-S3R8: 16 MB flash, 8 MB **octal** PSRAM |
| Display controller | CO5300 (panel model `DO0143FMST10`) — **not** SH8601 |
| Driver | `Arduino_CO5300` over `Arduino_ESP32QSPI` |
| Drawing area | 466 × 466 (panel memory is really 473 × 467) |
| Panel column offset | **4** (row offset 0) |
| Visual center of the glass | **CX = 234, CY = 232** |
| Outermost visible ring (glass edge) | radius **232** |
| UI boundary (yellow ring) | everything inside radius **227** |
| USB port on this laptop | COM7 (native USB, VID 303A) |

---

## 2. Pins

| Function | GPIO |
|---|---|
| LCD_CS | 10 |
| LCD_SCLK | 12 |
| LCD_SDIO0 / 1 / 2 / 3 | 11 / 13 / 14 / 15 |
| LCD_RST | 17 |
| LCD_EN (panel power) | 16 — **must be driven HIGH** |
| Touch SDA / SCL / INT | 7 / 6 / 9 |
| Battery voltage ADC | 4 |

Defined in `include/pin_config.h`.

---

## 3. Project layout

```
AMOLED_143_Minimal/
├── platformio.ini          build config (board, PSRAM mode, panel define, COM7)
├── boards/
│   └── esp32s3_flash_16MB.json   custom board definition (from LilyGo repo)
├── include/
│   └── pin_config.h        pins + 466x466 size
├── lib/
│   └── Arduino_GFX/        LilyGo's fork of Arduino_GFX 1.3.7 (patched, see §5)
└── src/
    └── main.cpp            init + calibrated border test
```

---

## 4. platformio.ini (key settings)

```ini
[platformio]
boards_dir = ./boards
default_envs = amoled143

[env:amoled143]
platform = espressif32 @ ^6.13.0
board = esp32s3_flash_16MB
framework = arduino
monitor_speed = 115200
upload_speed = 921600
upload_port = COM7
monitor_port = COM7

board_build.arduino.memory_type = qio_opi
board_build.arduino.partitions = default_16MB.csv

build_flags =
    -D BOARD_HAS_PSRAM
    -D ARDUINO_USB_MODE=1
    -D ARDUINO_USB_CDC_ON_BOOT=1
    -D ARDUINO_RUNNING_CORE=1
    -D ARDUINO_EVENT_RUNNING_CORE=1
    -D DO0143FMST10          ; panel model — REQUIRED, see §5

lib_ldf_mode = deep+
```

Notes:
- **`qio_opi` is required for PSRAM.** With `qio_qspi` the board boots and draws, but logs
  `PSRAM ID read error` and has no PSRAM, so a full-screen `Arduino_Canvas` (434 KB) fails.
- If the COM port changes (different USB port / laptop), update `upload_port` and
  `monitor_port`, or delete both lines and PlatformIO will auto-detect.

---

## 5. The graphics library (important gotchas)

**Use LilyGo's fork, not upstream Arduino_GFX.**
Source: <https://github.com/Xinyuan-LilyGO/T-Display-S3-AMOLED-1.43> →
`libraries/Arduino_GFX-1.3.7`, copied into `lib/Arduino_GFX/`.
Upstream 1.6.2+ fails (`esp32-hal-periman.h` missing) and 1.6.1 fails around
`esp_rgb_panel_t` on this platform version.

Two fixes applied to make the fork work standalone:

1. **Patch:** removed the stray `#include "pin_config.h"` line near the top of
   `lib/Arduino_GFX/src/Arduino_TFT.cpp` (it expected LilyGo's example folder).
2. **Panel define `-D DO0143FMST10`** in `build_flags`.
   That stray include was also where the fork got its panel-model macro. Without it,
   `writePixelPreclipped()` in `Arduino_TFT.cpp` compiles to **nothing**, so every
   pixel-based primitive silently fails: `drawPixel`, `drawCircle`, `drawLine`
   (diagonal), `drawFastVLine`, etc. Filled rectangles and text still work, which makes
   the bug easy to miss.

Hardware quirk: the CO5300's minimum write window is **2×2**, so a single "pixel"
is drawn as a 2×2 block. Thin lines and circles look ~2 px thick. This is normal.

---

## 6. Initialization sequence (all steps required)

```cpp
#include <Arduino.h>
#include "Arduino_GFX_Library.h"
#include "pin_config.h"

Arduino_DataBus *bus = new Arduino_ESP32QSPI(
    LCD_CS, LCD_SCLK, LCD_SDIO0, LCD_SDIO1, LCD_SDIO2, LCD_SDIO3);

Arduino_GFX *gfx = new Arduino_CO5300(bus, LCD_RST, 0 /* rotation */, false /* IPS */,
                                      LCD_WIDTH, LCD_HEIGHT,
                                      4 /* col offset */, 0, 0, 0);

void setup()
{
    pinMode(LCD_EN, OUTPUT);
    digitalWrite(LCD_EN, HIGH);          // 1. power the panel

    gfx->begin();                        // 2. init controller
    gfx->fillScreen(BLACK);              // 3. draw first frame

    for (int i = 0; i <= 255; i++)       // 4. brightness ramp: CO5300 boots at 0,
    {                                    //    screen stays black without this
        gfx->Display_Brightness(i);
        delay(3);
    }
}
```

---

## 7. Screen geometry / calibration

The glass is round, so only a circle of the 466×466 buffer is visible. The circle is
not exactly in the middle of the buffer, so it was calibrated on the real unit:

- Hardware column offset = **4** (kept even because of the 2×2 write window; odd
  offsets risk misaligned writes).
- Remaining sub-offset is done in software through the center constants:

```cpp
const int16_t CX = W / 2 + 1; // 234
const int16_t CY = H / 2 - 1; // 232
const int16_t R  = W / 2 - 1; // 232  outermost visible ring
```

How it was found: draw `drawCircle(CX, CY, 232, RED)` and nudge CX/CY by ±1 until the
ring is complete and evenly spaced from the bezel all the way around.
(Offset 6, used by LilyGo's 1.75" examples, clipped the ring at the bottom-right.)

**Design rules for the interface**
- Always position things relative to `(CX, CY)`, never `(233, 233)`.
- **The whole UI must stay inside the yellow ring: radius 227 around (CX, CY).**
  A point (x, y) is inside if `(x-CX)² + (y-CY)² <= 227²`.
- Corners of the 466×466 buffer are not visible. The largest square that fits inside
  the UI circle is `227·√2 ≈ 321 × 321`, i.e. x ∈ [74, 394], y ∈ [72, 392].
- At a vertical offset `dy` from center, a horizontal row of UI can be
  `2·√(227² − dy²)` wide (e.g. 454 px at the center, ~393 px at dy = ±113).

The border test in `src/main.cpp` draws: red ring r=232 (glass edge), yellow r=227
(**UI boundary**), green r=212 (reference only), crosshair, N/S/W/E markers and a live counter.

---

## 8. Drawing tips

- API is Adafruit-GFX style: `fillScreen`, `fillRect`, `drawCircle`, `fillCircle`,
  `drawLine`, `setCursor`, `setTextColor`, `setTextSize`, `print`, `printf`.
- Colors are RGB565 (`RED`, `GREEN`, `0x07E0`, ...).
- **Avoid flicker:** don't `fillScreen()` every frame. Either
  - erase/redraw only the small rect that changed (as the counter in `loop()` does), or
  - for full-screen animation, render into an `Arduino_Canvas` (PSRAM framebuffer) and
    push the whole frame at once.

---

## 9. Build / upload

From VS Code: open this folder, then use the PlatformIO toolbar
(✓ Build, → Upload, 🔌 Serial Monitor).

From a terminal:

```powershell
# from this project's folder
$pio = "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe"
& $pio run                                  # build
& $pio run -t upload                        # upload
& $pio device monitor -p COM7 -b 115200     # serial
```

First build downloads the ESP32 toolchain (~2–3 min); later builds take ~25 s.
On boot the board prints `Display ready` over serial.

---

## 10. Recreating from scratch

1. Install VS Code + PlatformIO IDE.
2. `git clone --depth 1 https://github.com/Xinyuan-LilyGO/T-Display-S3-AMOLED-1.43.git`
3. Create the folders in §3; copy
   - `boards/esp32s3_flash_16MB.json` → `boards/`
   - `libraries/Arduino_GFX-1.3.7` → `lib/Arduino_GFX`
4. Delete `#include "pin_config.h"` from `lib/Arduino_GFX/src/Arduino_TFT.cpp`.
5. Write `platformio.ini` (§4, including `-D DO0143FMST10`), `include/pin_config.h` (§2),
   `src/main.cpp` (§6 + §7).
6. Find the port: Device Manager → Ports → "USB Serial Device (COMx)" (VID 303A).
7. Build, upload, check the red ring; re-tune CX/CY if using a different unit.

---

## 11. Troubleshooting

| Symptom | Cause / fix |
|---|---|
| Screen stays black, serial says ready | Missing brightness ramp, or `LCD_EN` not HIGH |
| Text/rects show but circles, diagonal or vertical lines don't | `-D DO0143FMST10` missing (§5) |
| Compile error about `pin_config.h` inside Arduino_GFX | Stray include not removed (§5) |
| `esp32-hal-periman.h` / `esp_rgb_panel_t` errors | Upstream Arduino_GFX got pulled in; use the vendored fork only |
| Edge ring clipped on one side | Re-calibrate CX/CY (§7) |
| Upload fails / no port | Use a data USB cable; hold BOOT, tap RST, release BOOT to force download mode |
| Flicker | Don't clear the full screen per frame (§8) |
| Colored line / noise on the east border | Panel RAM is 473×467; columns past our 466-wide window are never written and keep power-on garbage. Fill the whole RAM once at boot (see `fillPanelRam()` in `src/display.cpp` of [oled-round-dashboard](https://github.com/Seltsames/oled-round-dashboard)) |
| `PSRAM ID read error` / `canvas->begin()` fails | `memory_type` must be `qio_opi`, not `qio_qspi` (§4) |
| Touch (FT3168, I2C 0x38) not found | It sleeps after a few idle seconds; init it right after the panel reset and set reg 0xA5 = 0 (active) |
