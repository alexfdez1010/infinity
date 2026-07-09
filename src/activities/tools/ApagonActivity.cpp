#include "ApagonActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <cstdio>
#include <cstring>
#include <esp_random.h>

#include "GameScores.h"
#include "components/UITheme.h"
#include "fontIds.h"

// --- Core logic ---

// Toggle the pressed cell and its orthogonal neighbours (clamped to the grid).
void ApagonActivity::press(int x, int y) {
  const int dx[5] = {0, 0, 0, -1, 1};
  const int dy[5] = {0, -1, 1, 0, 0};
  for (int i = 0; i < 5; i++) {
    const int nx = x + dx[i];
    const int ny = y + dy[i];
    if (nx >= 0 && nx < GRID && ny >= 0 && ny < GRID) {
      lights[ny][nx] = !lights[ny][nx];
    }
  }
}

bool ApagonActivity::isAllOff() const {
  for (int r = 0; r < GRID; r++)
    for (int c = 0; c < GRID; c++)
      if (lights[r][c]) return false;
  return true;
}

// Scramble an all-off board with random presses. Because every press is
// reversible by pressing the same cell again, the resulting board is always
// solvable. More presses at higher levels → harder puzzles.
void ApagonActivity::newPuzzle() {
  const int scramble = 3 + static_cast<int>(level);  // grows with level
  do {
    memset(lights, 0, sizeof(lights));
    for (int i = 0; i < scramble; i++) {
      const int rx = static_cast<int>(esp_random() % GRID);
      const int ry = static_cast<int>(esp_random() % GRID);
      press(rx, ry);
    }
  } while (isAllOff());  // never hand out an already-solved board

  cursorX = GRID / 2;
  cursorY = GRID / 2;
  moves = 0;
  solved = false;
  requestUpdate();
}

void ApagonActivity::recordBestIfSolved() {
  if (!solved) return;
  // Best = fewest moves; 0 means "no record yet".
  if (GAME_SCORES.bestApagon == 0 || moves < GAME_SCORES.bestApagon) {
    GAME_SCORES.bestApagon = moves;
    GAME_SCORES.saveToFile();
  }
}

// --- Lifecycle ---

void ApagonActivity::onEnter() {
  Activity::onEnter();
  level = 1;
  newPuzzle();
}

void ApagonActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  if (solved) {
    // Confirm starts the next, harder level.
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      level++;
      newPuzzle();
    }
    return;
  }

  bool dirty = false;
  if (mappedInput.wasReleased(MappedInputManager::Button::Left) && cursorX > 0) {
    cursorX--;
    dirty = true;
  } else if (mappedInput.wasReleased(MappedInputManager::Button::Right) && cursorX < GRID - 1) {
    cursorX++;
    dirty = true;
  } else if (mappedInput.wasReleased(MappedInputManager::Button::Up) && cursorY > 0) {
    cursorY--;
    dirty = true;
  } else if (mappedInput.wasReleased(MappedInputManager::Button::Down) && cursorY < GRID - 1) {
    cursorY++;
    dirty = true;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    press(cursorX, cursorY);
    moves++;
    if (isAllOff()) {
      solved = true;
      recordBestIfSolved();
    }
    dirty = true;
  }

  if (dirty) requestUpdate();
}

// --- Render ---

void ApagonActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();

  // Header
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight},
                 tr(STR_APAGON));

  // Status line: level, moves, best (fewest moves ever)
  const int statusY = metrics.topPadding + metrics.headerHeight + 10;
  char bestStr[12];
  if (GAME_SCORES.bestApagon == 0) {
    snprintf(bestStr, sizeof(bestStr), "-");
  } else {
    snprintf(bestStr, sizeof(bestStr), "%lu", (unsigned long)GAME_SCORES.bestApagon);
  }
  char status[64];
  snprintf(status, sizeof(status), tr(STR_APAGON_STATUS_FORMAT),
           (unsigned long)level, (unsigned long)moves, bestStr);
  renderer.drawCenteredText(SMALL_FONT_ID, statusY, status);

  // --- Grid geometry (fit to the available area, centred) ---
  const int GAP = 8;
  const int sideMargin = 16;
  const int top = statusY + renderer.getLineHeight(SMALL_FONT_ID) + 12;
  const int bottom = pageHeight - metrics.buttonHintsHeight - metrics.verticalSpacing;
  const int availW = pageWidth - 2 * sideMargin;
  const int availH = bottom - top;

  int tile = (availW - (GRID - 1) * GAP) / GRID;
  const int tileH = (availH - (GRID - 1) * GAP) / GRID;
  if (tileH < tile) tile = tileH;
  if (tile > 120) tile = 120;  // keep cells from looking cartoonishly large

  const int gridW = GRID * tile + (GRID - 1) * GAP;
  const int gridH = GRID * tile + (GRID - 1) * GAP;
  const int gridLeft = (pageWidth - gridW) / 2;
  const int gridTop = top + (availH - gridH) / 2;

  for (int r = 0; r < GRID; r++) {
    for (int c = 0; c < GRID; c++) {
      const int x = gridLeft + c * (tile + GAP);
      const int y = gridTop + r * (tile + GAP);
      const bool on = lights[r][c];

      // Cell body: solid black when lit, outlined when off
      renderer.drawRoundedRect(x, y, tile, tile, 1, 8, true);
      if (on) {
        renderer.fillRoundedRect(x + 1, y + 1, tile - 2, tile - 2, 7, Color::Black);
      }

      // Cursor: a contrasting inner square, readable on lit and unlit cells
      if (r == cursorY && c == cursorX) {
        const int inset = tile / 4;
        renderer.fillRect(x + inset, y + inset, tile - 2 * inset, tile - 2 * inset, !on);
      }
    }
  }

  // Win overlay (shared black card + white bold text)
  if (solved) {
    GUI.drawCenteredCard(renderer, tr(STR_YOU_WIN), pageHeight / 2);
  }

  // Button hints
  const char* confirmLabel = solved ? tr(STR_NEW_GAME) : tr(STR_APAGON_PRESS);
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), confirmLabel, tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
