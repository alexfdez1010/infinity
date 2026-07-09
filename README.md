# Infinity

**Custom firmware for Xteink X4 e-reader — custom fonts, games, Bluetooth keyboard, and more.**

Infinity is a Spanish (Spain) firmware (formerly CrossPet), based on [CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader) — open-source firmware for the **Xteink X3 / X4** e-paper readers.

![](./docs/images/crosspet.png)

---

## About this fork

**Infinity** is the Spanish (Spain) firmware previously released as **CrossPet**, itself built on top of upstream [CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader). This fork:

- **Adds** apps and games that upstream leaves out of scope (2048, Puzzle deslizante, Apagón, Senku), custom SD-card fonts, a background weather widget, extra sleep-screen modes, and a beta BLE keyboard.
- **Removes / disables** what didn't fit: **Political Simulator** was dropped, and every optional app (Clock, Reading Stats, Reading Goals, Games) can be **turned off** individually in **Settings → CrossPet** — hiding it from both the Tools menu and the home screen. The BLE build additionally disables images and CSS to fit in RAM.

Everything is fully translated to Castilian Spanish.

---

## What's New in v1.8.4

- **Games submenu** — Tools → Juegos groups all games: **2048**, **Puzzle deslizante** (sliding puzzle), **Apagón** (Lights Out), and **Senku** (peg solitaire). Each keeps its own best-record. _(Political Simulator was removed in this fork.)_
- **Apagón** — a Lights Out logic puzzle. Toggle a cell and its neighbours until every light is off; fewest-moves records are saved
- **Senku** — classic peg solitaire; the record tracks the fewest pegs left (1 = a perfect solve)
- **Libros** — the home screen's "Recientes" list is now a full-library cover grid, relabelled **Libros**
- **Custom fonts from SD card** — Drop `.bin` fonts into `/fonts/`, select in settings. Dual font model: primary + supplement for mixed scripts (e.g. Latin + CJK). Generate `.bin` files at [xteink.lakafior.com](https://xteink.lakafior.com/)
- **Bluetooth keyboard (beta)** — Pair a BLE page turner or keyboard. Separate firmware build due to memory constraints

---

## Features

### Reading Experience
- **EPUB 2/3** with images, CSS styling, and multi-language hyphenation
- **XTC** pre-rendered format (>2GB support) and **TXT/Markdown**
- **3 built-in fonts** (Bookerly, Lexend, Bokerlam) + **custom fonts from SD card**
- **4 font sizes** with anti-aliased grayscale rendering
- **Font cache** for faster page turns + silent next-chapter pre-indexing
- **Auto page turn** — 1-20 pages/min, configurable as power button action
- **Bookmarks** via long-press, reading stats & streaks
- **Dark mode**, 5 UI themes, fully customizable status bar
- **9 sleep screen modes** — Clock, Reading Stats, Page Overlay, custom images, and more
- **Focus mode** — hides all extras, just you and your books
- **KOReader Sync** for cross-device progress
- **4 screen orientations** with remappable buttons
- **9 power button actions** per single/double/triple click

### Apps
| App | Description |
|-----|-------------|
| **Clock** | Digital clock + lunar calendar |
| **Reading Stats** | Session time, streaks, and per-book progress |
| **Reading Goals** | Daily reading goal, streak with freeze tokens, achievement badges |
| **OPDS Browser** | Browse & download from Calibre/OPDS |
| **Games** | 2048, Puzzle deslizante (sliding puzzle), Apagón (Lights Out), Senku (peg solitaire) |

Weather (Open-Meteo, no account needed) runs in the background as a home-screen and sleep-screen widget.

### Connectivity
- WiFi file transfer from browser
- OPDS / Calibre library browsing
- KOReader Sync
- OTA firmware updates
- BLE keyboard (beta, separate build)

### Bluetooth Keyboard (Beta)

Pair a BLE page turner or keyboard. Available as a **separate firmware download**.

| Key | Reader | Menus |
|-----|--------|-------|
| Arrow keys | Page turn | Navigate |
| Enter | Select | Select |
| Escape | Back | Back |

Supports: GameBrick V1/V2, Free2/Free3, Kobo Remote, generic HID.

> BLE uses ~50KB RAM. The BLE build disables images and CSS to fit. WiFi and BLE share one radio — they can't run simultaneously.

---

## Installing

### Web Flasher

1. Connect X3/X4 via USB-C, wake the device
2. Go to https://xteink.dve.al/ and flash

### Manual

```sh
git clone --recursive https://github.com/trilwu/crosspet
pio run --target upload
```

### Build Environments

```bash
pio run -e default      # Standard build
pio run -e ble          # Bluetooth build (beta)
pio run -e gh_release   # Release build
```

---

## SD Card Setup

```
/fonts/          # Custom fonts (Xteink .bin format: FontName_size_WxH.bin)
/sleep/          # Sleep screen images (PNG/BMP)
```

### Adding Custom Fonts

1. Go to [xteink.lakafior.com](https://xteink.lakafior.com/) — the Xteink web font maker
2. Upload a `.ttf` / `.otf`, tune weight + anti-aliasing in the live preview
3. Click **Convert to .BIN**
4. Rename to `FontName_size_WxH.bin` — e.g. `Lexend_38_33x39.bin` (the firmware parses size + glyph box from the filename)
5. Copy into SD `/fonts/` and reboot. The font appears in **Settings → Font**

Dual-font: pick a **primary** (e.g. Latin) and **supplement** (e.g. CJK) — mixed-script text renders correctly without glyph fallback gaps.

---

## Contributing

1. Fork → branch → changes → PR

See [contributing docs](./docs/contributing/README.md).

---

Infinity is **not affiliated with Xteink**. Formerly released as CrossPet; based on [CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader).
