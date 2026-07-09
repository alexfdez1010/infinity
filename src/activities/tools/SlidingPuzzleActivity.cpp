#include "SlidingPuzzleActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <cstdio>
#include <cstring>
#include <esp_random.h>

#include "GameScores.h"
#include "components/UITheme.h"
#include "fontIds.h"

// --- Core logic ---

// Slide the tile at (blank + dr/dc) into the blank. dr/dc point from the blank
// towards the neighbouring tile that moves. No-op (returns false) when that
// neighbour is off the board.
bool SlidingPuzzleActivity::slide(int dr, int dc) {
  const int tr = blankR + dr;
  const int tc = blankL + dc;
  if (tr < 0 || tr >= SIZE || tc < 0 || tc >= SIZE) return false;
  grid[blankR][blankL] = grid[tr][tc];
  grid[tr][tc] = 0;
  blankR = tr;
  blankL = tc;
  return true;
}

bool SlidingPuzzleActivity::isSolved() const {
  for (int r = 0; r < SIZE; r++) {
    for (int c = 0; c < SIZE; c++) {
      const int k = r * SIZE + c;
      const uint8_t expected = (k == SIZE * SIZE - 1) ? 0 : static_cast<uint8_t>(k + 1);
      if (grid[r][c] != expected) return false;
    }
  }
  return true;
}

// Reset to the solved board, then scramble by walking the blank with random
// legal moves. Scrambling from a solved state guarantees the board stays
// solvable (unlike a raw random permutation, half of which are unsolvable).
void SlidingPuzzleActivity::newPuzzle() {
  do {
    for (int r = 0; r < SIZE; r++)
      for (int c = 0; c < SIZE; c++)
        grid[r][c] = static_cast<uint8_t>((r * SIZE + c + 1) % (SIZE * SIZE));
    blankR = SIZE - 1;
    blankL = SIZE - 1;

    // 200 random legal slides — enough to fully mix a 4x4 board.
    static const int drs[4] = {0, 0, 1, -1};
    static const int dcs[4] = {1, -1, 0, 0};
    for (int i = 0; i < 200; i++) {
      const int d = static_cast<int>(esp_random() % 4);
      slide(drs[d], dcs[d]);
    }
  } while (isSolved());  // never hand out an already-solved board

  moves = 0;
  solved = false;
  requestUpdate();
}

void SlidingPuzzleActivity::recordBestIfSolved() {
  if (!solved) return;
  // Best = fewest moves; 0 means "no record yet".
  if (GAME_SCORES.bestSlide == 0 || moves < GAME_SCORES.bestSlide) {
    GAME_SCORES.bestSlide = moves;
    GAME_SCORES.saveToFile();
  }
}

// --- Lifecycle ---

void SlidingPuzzleActivity::onEnter() {
  Activity::onEnter();
  newPuzzle();
}

void SlidingPuzzleActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    newPuzzle();
    return;
  }

  if (solved) return;

  bool moved = false;
  // The tile in the pressed direction slides into the blank: e.g. pressing
  // Left pulls the tile to the blank's right one step left.
  if (mappedInput.wasReleased(MappedInputManager::Button::Left))       moved = slide(0, 1);
  else if (mappedInput.wasReleased(MappedInputManager::Button::Right)) moved = slide(0, -1);
  else if (mappedInput.wasReleased(MappedInputManager::Button::Up))    moved = slide(1, 0);
  else if (mappedInput.wasReleased(MappedInputManager::Button::Down))  moved = slide(-1, 0);

  if (moved) {
    moves++;
    if (isSolved()) {
      solved = true;
      recordBestIfSolved();
    }
    requestUpdate();
  }
}

// --- Render ---

void SlidingPuzzleActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();

  // Header
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight},
                 tr(STR_SLIDING_PUZZLE));

  // Status line: moves and best (fewest moves ever)
  const int statusY = metrics.topPadding + metrics.headerHeight + 10;
  char bestStr[12];
  if (GAME_SCORES.bestSlide == 0) {
    snprintf(bestStr, sizeof(bestStr), "-");
  } else {
    snprintf(bestStr, sizeof(bestStr), "%lu", (unsigned long)GAME_SCORES.bestSlide);
  }
  char status[64];
  snprintf(status, sizeof(status), tr(STR_SLIDE_STATUS_FORMAT), (unsigned long)moves, bestStr);
  renderer.drawCenteredText(SMALL_FONT_ID, statusY, status);

  // --- Grid geometry (centred, fits the available area) ---
  const int GAP = 8;
  const int sideMargin = 16;
  const int top = statusY + renderer.getLineHeight(SMALL_FONT_ID) + 12;
  const int bottom = pageHeight - metrics.buttonHintsHeight - metrics.verticalSpacing;
  const int availW = pageWidth - 2 * sideMargin;
  const int availH = bottom - top;

  int tile = (availW - (SIZE - 1) * GAP) / SIZE;
  const int tileH = (availH - (SIZE - 1) * GAP) / SIZE;
  if (tileH < tile) tile = tileH;
  if (tile > 108) tile = 108;

  const int gridW = SIZE * tile + (SIZE - 1) * GAP;
  const int gridH = SIZE * tile + (SIZE - 1) * GAP;
  const int gridLeft = (pageWidth - gridW) / 2;
  const int gridTop = top + (availH - gridH) / 2;

  for (int r = 0; r < SIZE; r++) {
    for (int c = 0; c < SIZE; c++) {
      const uint8_t val = grid[r][c];
      if (val == 0) continue;  // blank cell drawn as empty space

      const int x = gridLeft + c * (tile + GAP);
      const int y = gridTop + r * (tile + GAP);
      renderer.drawRoundedRect(x, y, tile, tile, 1, 6, true);

      char buf[4];
      snprintf(buf, sizeof(buf), "%u", val);
      const int textW = renderer.getTextWidth(UI_12_FONT_ID, buf);
      const int lineH = renderer.getLineHeight(UI_12_FONT_ID);
      renderer.drawText(UI_12_FONT_ID, x + (tile - textW) / 2, y + (tile - lineH) / 2, buf);
    }
  }

  // Win overlay (shared black card + white bold text)
  if (solved) {
    GUI.drawCenteredCard(renderer, tr(STR_YOU_WIN), pageHeight / 2);
  }

  // Button hints
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_NEW_GAME), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
