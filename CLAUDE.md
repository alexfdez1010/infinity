# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

CrossPet is a Spanish (Spain) fork of [CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader) — open-source ESP32-C3 firmware for the Xteink X4 e-paper reader. Built with PlatformIO, C++17 (gnu++2a), Arduino framework. Current version: defined in `platformio.ini` under `[crosspoint] version`.

**Hardware constraints:** ESP32-C3 RISC-V @ 160MHz, ~380KB RAM (no PSRAM), 16MB flash, 800x480 E-Ink display (SSD1677), SD card storage.

## Build Commands

```bash
# Build (default env with serial logging)
pio run

# Build specific environment
pio run -e default        # Development build (serial log enabled)
pio run -e slim           # Release build (no serial, smaller)
pio run -e ble            # Bluetooth-enabled build
pio run -e gh_release     # GitHub release build

# Upload to device via USB
pio run -t upload

# Serial monitor
pio run -t monitor        # or: pio device monitor

# Static analysis
pio check

# Clean build artifacts
pio run -t clean
```

**Pre-build scripts** (run automatically by PlatformIO):
- `scripts/build_html.py` — generates HTML headers from web UI templates
- `scripts/gen_i18n.py` — generates I18n string tables from YAML translations
- `scripts/patch_jpegdec.py` — patches JPEGDEC library for ESP32-C3

## Testing

Tests live in `test/` but are mostly native evaluation scripts, not a unit test suite:
- `test/hyphenation_eval/` — hyphenation quality evaluation
- `test/utf8_nfc/` — UTF-8 NFC normalization tests
- `test/differential_rounding/` — PlatformIO unit test (`DifferentialRoundingTest.cpp`)
- `test/epubs/` — sample EPUB files for manual testing

```bash
# Run hyphenation evaluation
cd test && bash run_hyphenation_eval.sh

# Run the differential rounding unit test
cd test && bash run_differential_rounding_test.sh
```

There is no automated test framework (no `pio test`). Verify changes by building successfully and testing on device.

## Architecture

### Layer Model (top to bottom)

```
UI Activities → ActivityManager → Subsystems (EPUB, Network, Gamification) → Settings/State → HAL → Hardware
```

### Key Architectural Patterns

- **Activity system**: Android-like stack-based navigation. All screens inherit from `Activity` base class. `ActivityManager` is a singleton managing lifecycle, render task, and a shared FreeRTOS mutex. Activities use `startActivityForResult()` / `setResult()` / `finish()` pattern.
- **Singleton pattern**: Settings (`CrossPointSettings`, `CrossPetSettings`), `CrossPointState`, `ActivityManager` all use `getInstance()`.
- **Render pipeline**: Single FreeRTOS task with mutex (`RenderLock` RAII). Activities call `requestUpdate()` to trigger redraws. Display uses partial/full refresh modes for e-ink.
- **Persistence**: JSON via `JsonSettingsIO` for settings and the gamification state, binary serialization for reading stats, all stored on SD card.

### Source Layout

**`src/`** (application code):
- `main.cpp` — entry point, HAL init, activity manager setup, main loop
- `activities/` — UI screens organized by domain: `reader/`, `home/`, `settings/`, `tools/`, `network/`, `boot_sleep/`, `browser/`, `flashcard/`
- `gamification/` — reading gamification layer (`Gamification.h/.cpp`): daily goals, streaks with freeze tokens, achievement badges; consumes `ReadingStats`, persists as JSON on SD
- `components/` — `UITheme`, icon bitmaps, theme assets
- `network/` — `CrossPointWebServer` (HTTP + WebSocket), `OtaUpdater`, `WebDAVHandler`, `OpportunisticTimeSync` (background NTP resync on wake — the X4 has no battery-backed RTC), generated HTML in `html/`
- `util/` — `LunarCalendar`, `ButtonNavigator`, `QrUtils`, `SleepScreenCache`

**`lib/`** (libraries — mostly font glyph data):
- `EpdFont` — font engine with pre-compiled glyph tables (mostly data)
- `GfxRenderer` — e-ink graphics abstraction (orientation, dithering, grayscale)
- `Epub` — EPUB 2/3 parser with caching, CSS, cover generation
- `I18n` — internationalization (English + Spanish only, generated from YAML)
- `hal/` — hardware abstraction: `HalDisplay`, `HalStorage`, `HalPowerManager`, `HalGPIO`, `HalSystem`
- Third-party: `expat` (XML), `picojpeg`, `uzlib` (deflate)

### Flash Partitions

Dual OTA scheme: two 6.25MB app partitions (`app0`/`app1`), 3.375MB SPIFFS, 64KB coredump. Max firmware size: ~6.25MB.

### Memory Constraints

~380KB total RAM, no PSRAM. TLS buffers reduced to 4KB (from 16KB default) to fit WiFi + TLS in ~46KB free heap. PNG buffer capped at 1024px width. Single display buffer mode.

## Key Files for Common Tasks

| Task | Files |
|------|-------|
| Add new activity/screen | `src/activities/`, inherit from `Activity`, register in `ActivityManager` |
| Add new setting | `src/CrossPointSettings.h`, `src/SettingsList.h`, `src/activities/settings/` |
| Add/change a Tools menu app | `src/activities/tools/ToolsActivity.cpp` (`buildMenu()`), toggle in `src/CrossPetSettings.h` |
| Add gamification feature (goals/streaks/achievements) | `src/gamification/Gamification.h/.cpp`, UI in `src/activities/tools/ReadingGoalsActivity.*` |
| Regenerate Political Simulator data | `scripts/gen_polsim.mjs` → `src/activities/tools/PoliticalSimData.h` |
| Add i18n string | `lib/I18n/translations/{english,spanish}.yaml`, then build (auto-generates) |
| Modify web UI | `src/network/html/` templates, `scripts/build_html.py` regenerates headers |
| Add sleep screen mode | `src/activities/boot_sleep/SleepActivity.cpp`, `src/CrossPointSettings.h` |
| Change button behavior | `src/MappedInputManager.cpp`, `src/CrossPointSettings.h` |
