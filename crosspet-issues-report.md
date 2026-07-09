# CrossPet Firmware Issues & Feature Requests
# Source: Reddit r/xteinkereader (2026-03-01 to 2026-04-22)
# Total posts analyzed: 41 crosspet-related posts

---

## 🐛 BUGS (Confirmed/User-Reported)

### BUG-001: Apostrophe Word-Breaking + Letter Spacing (HIGH PRIORITY)
- **Severity:** High
- **Affected:** Crosspet v1.8.3, Lexend & Bookerly fonts
- **Description:** Words with apostrophes ("I'm", "can't", "don't") are incorrectly split across lines. The spacing between letters also appears abnormally wide, making text look like separate words.
- **Reports:** 
  - BackHefe21 (Apr 21, 2026) - 46 upvotes, 23 comments - "Weird formatting in Project Hail Mary" with screenshot
  - Legitimate-Memory744 (Apr 21, 2026) - "I don't remember apostrophe spacing being this wide before"
- **Expected:** No line breaks mid-word with apostrophe, normal letter spacing
- **Evidence:** https://www.reddit.com/r/xteinkereader/comments/1srclp6/
- **GitHub:** Already filed as Issue #12 by BackHefe21

---

### BUG-002: Reading Progress Skips to End on Wake
- **Severity:** High
- **Affected:** Crosspet v1.8.0
- **Description:** Device keeps skipping to the end of the book every time it wakes from sleep, whether user was reading or not.
- **Report:** miletant (Mar 30, 2026) - 5 upvotes, 13 comments
- **Workaround:** Use "go to specific chapter or percentage" option
- **Evidence:** https://www.reddit.com/r/xteinkereader/comments/1s7xhkq/

---

### BUG-003: Slow Loading/Indexing + Battery Drain
- **Severity:** High
- **Affected:** Crosspet latest versions (reported Mar 2026)
- **Description:** Device very slow in loading books, indexing takes forever, battery drains extremely fast
- **Report:** minomycin (Mar 25, 2026) - "Battery has been draining so fast ever since the latest update"
- **Note:** Dev suggested removing crosspet folder on SD card and re-flashing, but didn't fully fix
- **Evidence:** https://www.reddit.com/r/xteinkereader/comments/1mjf0tz/

---

### BUG-004: Clock/Time Stops Updating (Zombie Clock)
- **Severity:** Medium
- **Affected:** Crosspet (fixed in 1.8.2, but some users still report issues)
- **Description:** Clock gets behind after 5+ minutes. "Keep clock alive" toggle exists but drains battery. Option was removed in 1.8.3 due to battery drain.
- **Reports:**
  - ImAFurniture (Apr 20, 2026) - Clock gets behind, have to update time every time
  - senorblueduck (Apr 14, 2026) - "zombie clock issue" mentioned in 1.8.1→1.8.2 changelog
- **Dev Note (v1.8.3):** "removed the keep clock always on option (reverse engineering experiment that drain battery if you enabled)"
- **Status:** Partially fixed in 1.8.2 (Lexend reverted from 350 to 400 weight)

---

### BUG-005: Lexend Font Readability Regression (1.8.1 → 1.8.2)
- **Severity:** Medium
- **Affected:** Crosspet v1.8.2
- **Description:** Font readability noticeably degraded after update. Users prefer v1.8.1 font rendering.
- **Report:** senorblueduck (Apr 14, 2026) - 5 upvotes
- **Context:** 1.8.2 changed Lexend font weight from 350 to 400 (then 1.8.3 reverted to 400 with Text Darkness setting moved)
- **Evidence:** https://www.reddit.com/r/xteinkereader/comments/1slk78q/

---

### BUG-006: Reading Stats Not Working (Time/Day Count)
- **Severity:** Medium
- **Affected:** Crosspet
- **Description:** Reading time and day count don't work - only sessions finished and percentage are tracked
- **Report:** _echoO (Apr 3, 2026) - 2 upvotes, 3 comments
- **Note:** User asked if it's because "keep clock alive" needs to be activated
- **Evidence:** https://www.reddit.com/r/xteinkereader/comments/1sbax0n/

---

### BUG-007: Images in EPUB Super Dark
- **Severity:** Medium
- **Affected:** Crosspet (compared to other firmwares)
- **Description:** Images loaded from EPUB appear significantly darker than in other firmwares
- **Report:** Informal_Hurry_8340 (Apr 19, 2026) - 3 upvotes, 16 comments
- **Context:** User chose Crosspet for Chinese font support but images are darkest compared to Crosspoint/Paprix
- **Evidence:** https://www.reddit.com/r/xteinkereader/comments/1sq8n9v/

---

### BUG-008: Sleep Screen Random Mode Reset
- **Severity:** Low
- **Affected:** Crosspet
- **Description:** After selecting a specific sleep screen image, can't switch back to random mode
- **Report:** Rakshasa-aila (Apr 4, 2026) - 1 upvote, 5 comments
- **Additional:** th3v3N1N (Mar 23, 2026) - "*image.bmp" marked with asterisk, "Reload Sleep Image" doesn't fix
- **Evidence:** https://www.reddit.com/r/xteinkereader/comments/1sc8xyq/

---

### BUG-009: Book Covers Not Showing
- **Severity:** Low-Medium
- **Affected:** Crosspet (and Crosspoint)
- **Description:** Book covers don't display - possibly related to XTC file conversion
- **Report:** Sweaty-Literature837 (Apr 1, 2026) - 6 upvotes, 4 comments
- **Note:** "They also don't show up in crosspoint" - may be XTC conversion issue
- **Evidence:** https://www.reddit.com/r/xteinkereader/comments/1s9zbhz/

---

### BUG-010: Battery Drain Faster Than Other Firmwares
- **Severity:** Medium
- **Affected:** Crosspet
- **Description:** Multiple users report faster battery drain compared to Crosspoint/cpr-vcodex
- **Reports:**
  - minomycin (Apr 5, 2026) - "uses up battery so fast", switched to cpr-vcodex but eyes prefer Crosspet's darker text
  - TheGrtKing (Mar 21, 2026) - "Xteink X4 reduced to 58% in less than 2 days"
- **Dev Note:** 1.8.1 claimed "battery consumption fixes"
- **Evidence:** https://www.reddit.com/r/xteinkereader/comments/1mbm3tz/

---

## ❓ POTENTIAL BUGS (Needs Investigation)

### POTENTIAL-001: Word Splitting (Hyphenation) Inconsistent
- **Report:** Ahura_Maxda (Apr 21, 2026) - "word spitting works in some epubs and not in others"
- **Note:** Same behavior in Crosspoint 1.2.0 and Crosspet 1.8.3
- **Evidence:** https://www.reddit.com/r/xteinkereader/comments/1srfm04/

---

### POTENTIAL-002: Weather Symbols Not Documented
- **Report:** Stunning_Walrus6276 (Apr 2, 2026) - "What do these symbols mean on CrossPet Weather?"
- **Evidence:** https://www.reddit.com/r/xteinkereader/comments/1samp62/

---

## 💡 FEATURE REQUESTS

### FR-001: Add Extra Small Font
- **Requested by:** FreddySourial (Apr 16, 2026) - GitHub Issue #9
- **GitHub:** https://github.com/trilwu/crosspet/issues/9

---

### FR-002: Add Atkinson Hyperlegible Font
- **Requested by:** SnehilJain (Apr 14, 2026) - GitHub Issue #8
- **Additional context:** decompgal (Apr 2, 2026) asked same thing on Reddit
- **GitHub:** https://github.com/trilwu/crosspet/issues/8

---

### FR-003: Add Literata Font
- **Requested by:** SnehilJain (Apr 14, 2026) - GitHub Issue #8
- **GitHub:** https://github.com/trilwu/crosspet/issues/8

---

### FR-004: Add Bitter Font
- **Requested by:** SnehilJain (Apr 14, 2026) - GitHub Issue #8
- **GitHub:** https://github.com/trilwu/crosspet/issues/8

---

### FR-005: Highlight/Annotation Support
- **Requested by:** the_fugitive_biker (Apr 21, 2026) - 1 upvote
- **Use case:** Kindle-style highlighting to sync highlights via KOReader
- **Evidence:** https://www.reddit.com/r/xteinkereader/comments/1src4m2/

---

### FR-006: Standalone UI Mode (No Pet/Games)
- **Requested by:** antman0623 (Apr 8, 2026) - 5 upvotes
- **Description:** "just the UI of crosspet without everything else" - home screen with stats but no pet/games
- **Note:** Dev mentioned "you can now disable all apps yourself" in v1.8.3
- **Evidence:** https://www.reddit.com/r/xteinkereader/comments/1sg4lb1/

---

### FR-007: CrossPoint Sync App Compatibility
- **Requested by:** ElrondTheHater (Apr 14, 2026) - 2 upvotes
- **Description:** iOS app/web clipper used to push articles - works with Crosspoint but compatibility unclear with Crosspet
- **Evidence:** https://www.reddit.com/r/xteinkereader/comments/1slfekp/

---

### FR-008: iOS App for File Loading
- **Requested by:** BazaslavKringeborets (Apr 4, 2026) - 6 upvotes
- **Description:** "Does crosspet have an app on ios like crosspoint?"
- **Evidence:** https://www.reddit.com/r/xteinkereader/comments/1sc4k2g/

---

### FR-009: Reading Progress Sync
- **Requested by:** KelpEG (Apr 13, 2026) - 2 upvotes, 4 comments
- **Use case:** After screen broke, wanted to recover book progress
- **Evidence:** https://www.reddit.com/r/xteinkereader/comments/1skrlw0/

---

### FR-010: Comprehensive Reading Stats (Like cpr-vcodex)
- **Requested by:** minomycin (Apr 5, 2026) - 15 upvotes, 31 comments
- **Comparison:** cpr-vcodex has better stats but worse text rendering
- **Evidence:** https://www.reddit.com/r/xteinkereader/comments/1sctrit/

---

### FR-011: Theme Browser/Repository
- **Requested by:** decompgal (Apr 2, 2026) - 5 upvotes, 14 comments
- **Description:** "what themes are available and is there a source where i can browse them?"
- **Evidence:** https://www.reddit.com/r/xteinkereader/comments/1samp62/

---

### FR-012: Custom Sleep Screen Transition Image
- **Requested by:** Deobulakenyo (Mar 21, 2026) - 6 upvotes, 9 comments
- **Description:** Change the booting/sleeping transition image to custom image
- **Evidence:** https://www.reddit.com/r/xteinkereader/comments/1mbm3tz/

---

### FR-013: Russian Fonts Support
- **Requested by:** BazaslavKringeborets (Apr 4, 2026) - 6 upvotes
- **Evidence:** https://www.reddit.com/r/xteinkereader/comments/1sc4k2g/

---

## 📝 NOTES

### Battery Optimization
- Multiple users report Crosspet drains battery faster than other firmwares
- Dev is aware: v1.8.1 claimed "battery consumption fixes"
- v1.8.3 removed "keep clock always on" option due to battery drain

### Memory Constraints
- Dev mentioned: "380KB RAM" - very limited
- BLE build requires disabling images and embedded CSS to fit
- Custom font support needs TTF→.bin conversion

### Development Notes (from Dev trilwu)
- Started as "playground experiment with Claude Code"
- "Spent weeks memory juggling, adding & disabling features per build"
- 1.8.3: Two new features that required "fun hacks with this low memory device"

---

## 🔗 LINKS

- Reddit: https://www.reddit.com/r/xteinkereader/
- GitHub: https://github.com/trilwu/crosspet
- Website: https://trilwu.github.io/crosspet/
- Latest Release: https://github.com/trilwu/crosspet/releases/latest
