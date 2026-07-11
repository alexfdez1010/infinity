# ∞ Infinity

**Firmware for the [Xteink X4 and X3](https://www.xteink.com/es/products/xteink-%28x3/x4%29) e-ink readers.**

> 🛒 **Get the hardware:** Infinity runs on the **[Xteink X3 / X4](https://www.xteink.com/es/products/xteink-%28x3/x4%29)** — buy the reader that runs this firmware.

Infinity stands on the shoulders of two open-source projects: the [CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader) firmware, which provides the EPUB reading engine and the foundation of the system, and [CrossPet](https://github.com/trilwu/crosspet), the adaptation that Infinity takes over from. From here on, everything you read is Infinity.

---

## 🧠 Philosophy

Most e-reader firmwares are content to just *let* you read. Infinity wants you to **not be able to stop**.

Reading is a habit, and habits are built through reinforcement. That's why Infinity treats reading as what it is —the best entertainment in the world— and adds a **gamification** layer designed to hook you: daily goals, streaks you'll hate to break, rewards that show up when you least expect them, and achievements to unlock. The idea is simple: turn "I should read for a bit" into "I need to keep my streak alive".

And when you close the book, the device doesn't go quiet: a collection of carefully chosen **mini-games** awaits for those spare moments, all playable with the device's few buttons.

---

## 📖 Reading experience

Everything you expect from a good reader, polished:

- 📚 **EPUB 2/3** with images, CSS styles, and multilingual hyphenation
- ⚡ **XTC** (pre-rendered format, supports books larger than 2 GB) and **TXT / Markdown**
- 🔤 **3 built-in fonts** (Bookerly, Lexend, Bokerlam) + **custom fonts from the SD card**
- 🔠 **4 font sizes** with smoothed grayscale rendering (anti-aliasing)
- 🚀 **Font cache** for instant page turns + silent pre-indexing of the next chapter
- 🔖 **Bookmarks** via long-press, reading stats, and streaks
- 🌙 **Dark mode**, 5 interface themes, and a fully configurable status bar
- 🎯 **Focus mode** — hides every extra: just you and your books
- 🔄 **KOReader sync** to continue your progress across devices
- 🔀 **4 screen orientations** with remappable buttons
- 🎛️ **9 power-button actions** via single/double/triple press

---

## 🏆 Gamification

Infinity's addictive core. All optional and can be turned off, but hard to give up once you start:

- 🎯 **Daily reading goal** — you set the minutes; Infinity nudges you to hit them
- 🔥 **Reading-day streak** — with **freeze tokens** that forgive an off day without breaking the streak
- 🎁 **Surprise rewards** — variable-ratio reinforcement: sometimes hitting your goal gifts you a token, sometimes not. Exactly what hooks you
- 📜 **Daily quests** — rotating challenges that change every day
- 🏅 **Achievements** — badges unlocked by reading milestones
- 📊 **Records and weekly history** — to see how far you've come

---

## 🎮 Games

For those spare moments. This isn't a junk drawer: each game was **chosen because it controls well with the D-pad and the device's few buttons**, no pointer or touchscreen needed. Each one keeps its own high score.

- 🔢 **2048** — slide and merge tiles with the same number until you reach 2048. An addictive logic classic, perfect for directional controls.
- 🧩 **Sliding puzzle** — the numbered-tile puzzle: slide them through the empty gap until they're in order.
- 💡 **Lights Out** — press a cell and you flip its state and its neighbors'. Goal: turn off all the lights in the fewest moves.
- ♟️ **Peg solitaire (Senku)** — jump pegs over one another to remove them. A perfect game leaves a single peg on the board.
- 🌀 **Maze** — find the exit through level-generated mazes, counting your steps.
- 🚗 **Rush Hour (Desatasco)** — slide the blocking pieces out of the way to free the stuck car, in as few moves as possible.

---

## ⏰ Reliable NTP clock

One of the technical battles Infinity is proudest of, and a good example of how carefully crafted the firmware is under the hood.

**The problem:** the Xteink X4 **has no battery-backed real-time clock (RTC)**. When it wakes from deep sleep, the time is restored from an imprecise internal oscillator that keeps accumulating drift. The result: the device could show 23:55 when it was really 09:00.

**The solution:** Infinity resyncs the time opportunistically. On wake, if it detects the clock is "approximate", it connects in the background to one of your saved WiFi networks, sets the time via **NTP**, and switches the radio back off. Meanwhile, a discreet **`~` marker** in the status bar warns you the time is still approximate, until the sync finishes. It also includes a **timezone setting** (Madrid by default, with summer/winter DST).

Along the way we had to tame a serious hang: bringing up WiFi from a low-priority task with little free memory **froze the whole device**. The fix was to always start the radio from the main task, via a state machine that advances one small step per cycle, so the interface never blocks.

---

## 🛒 Get the hardware

Infinity is firmware — you need the reader it runs on. Both models are supported:

- **[Xteink X3 / X4 — buy here](https://www.xteink.com/es/products/xteink-%28x3/x4%29)**

---

## 💾 Installation

### Web flasher (recommended)

1. Connect your [X3 / X4](https://www.xteink.com/es/products/xteink-%28x3/x4%29) via USB-C and turn it on
2. Go to https://xteink.dve.al/ and flash from your browser

### Manual

```sh
git clone --recursive https://github.com/alexfdez1010/Infinity.git
cd Infinity
pio run --target upload
```

---

## 🗂️ SD card

```
/fonts/          # Custom fonts (Xteink .bin format: Name_size_AxB.bin)
```

### 🔤 Custom fonts

1. Go to [xteink.lakafior.com](https://xteink.lakafior.com/) — the Xteink web font generator
2. Upload a `.ttf` / `.otf` and adjust weight and smoothing in the preview
3. Click **Convert to .BIN**
4. Rename it as `Name_size_AxB.bin` — e.g. `Lexend_38_33x39.bin` (the firmware reads the size and glyph box from the file name)
5. Copy it into `/fonts/` on the SD card and restart. It will show up under **Settings → Font**

Dual-font model: pick a **primary** font (e.g. Latin) and a **supplementary** one (e.g. CJK) so mixed-script text renders without missing-glyph gaps.

---

## 🛠️ Development

Infinity is built with **PlatformIO** and the Arduino framework, in **C++17**, for the **ESP32-C3** (RISC-V at 160 MHz, ~380 KB of RAM without PSRAM, 16 MB of flash, 800×480 SSD1677 e-ink display). Every byte of RAM counts: the TLS buffers are trimmed to 4 KB so WiFi and encryption fit in the little free heap.

The architecture follows an Android-style **activity** model: each screen inherits from an `Activity` class and is pushed onto an `ActivityManager` that handles the lifecycle and a single render thread. Settings and gamification state are saved as JSON on the SD card; reading stats, as binary.

```bash
pio run -e default      # Standard build (with serial log)
pio run -e slim         # Release build (no serial, smaller)
pio run -e ble          # Bluetooth build (beta)
pio run -t upload       # Flash via USB
pio run -t monitor      # Serial monitor
```

The pre-build scripts (`build_html.py`, `gen_i18n.py`, `patch_jpegdec.py`) run automatically. See [`CLAUDE.md`](./CLAUDE.md) for more detail on the code structure.

**Contributing:** *fork* → branch → changes → *pull request*. Every visible text string is translated in `lib/I18n/translations/` (English and Castilian Spanish).

---

<sub>Infinity is **not affiliated with Xteink**. Released under the MIT license. Based on [CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader) and [CrossPet](https://github.com/trilwu/crosspet).</sub>
