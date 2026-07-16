#include "NQueensActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <esp_random.h>

#include "GameScores.h"
#include "components/UITheme.h"
#include "fontIds.h"

bool NQueensActivity::isValid(const int8_t* setup) const {
  for (int r1 = 0; r1 < size; r1++) {
    if (setup[r1] < 0) continue;
    for (int r2 = r1 + 1; r2 < size; r2++) {
      if (setup[r2] < 0) continue;
      if (setup[r1] == setup[r2] || std::abs(r1 - r2) == std::abs(setup[r1] - setup[r2])) {
        return false;
      }
    }
  }
  return true;
}

int NQueensActivity::countCompletions(int8_t* setup, int row, int limit) const {
  if (row == size) return 1;
  if (setup[row] >= 0) return countCompletions(setup, row + 1, limit);
  int count = 0;
  for (int c = 0; c < size && count < limit; c++) {
    setup[row] = c;
    if (isValid(setup)) count += countCompletions(setup, row + 1, limit - count);
    setup[row] = -1;
  }
  return count;
}

void NQueensActivity::shuffle(int8_t* values, int count) const {
  for (int i = count - 1; i > 0; i--) {
    const int j = static_cast<int>(esp_random() % (i + 1));
    std::swap(values[i], values[j]);
  }
}

bool NQueensActivity::solveRandom(int8_t* setup, int row) {
  if (row == size) return true;
  int8_t order[MAX_N];
  for (int i = 0; i < size; i++) order[i] = i;
  shuffle(order, size);
  for (int i = 0; i < size; i++) {
    setup[row] = order[i];
    if (isValid(setup) && solveRandom(setup, row + 1)) return true;
  }
  setup[row] = -1;
  return false;
}

void NQueensActivity::chooseDifficulty() {
  const uint32_t target = GAME_SCORES.queensElo;
  int bestDistance = 100000;
  for (int n = 4; n <= MAX_N; n++) {
    const uint32_t predicted = 1380 + (n - 4) * 120 + (n - 1) * (25 + (n - 4) * 7);
    const int distance = std::abs(static_cast<int>(predicted) - static_cast<int>(target));
    if (distance < bestDistance) {
      bestDistance = distance;
      size = n;
    }
  }
}

void NQueensActivity::newPuzzle() {
  chooseDifficulty();
  for (int attempt = 0; attempt < 50; attempt++) {
    std::fill(solution, solution + MAX_N, -1);
    if (!solveRandom(solution, 0)) continue;
    std::copy(solution, solution + MAX_N, givens);
    int8_t rows[MAX_N];
    for (int i = 0; i < size; i++) rows[i] = i;
    shuffle(rows, size);
    for (int i = 0; i < size; i++) {
      const int row = rows[i];
      const int8_t old = givens[row];
      givens[row] = -1;
      int8_t candidate[MAX_N];
      std::copy(givens, givens + MAX_N, candidate);
      if (countCompletions(candidate, 0, 2) != 1) givens[row] = old;
    }
    int freeQueens = 0;
    for (int i = 0; i < size; i++) freeQueens += givens[i] < 0;
    if (freeQueens == 0) continue;
    puzzleElo = 1380 + (size - 4) * 120 + freeQueens * (25 + (size - 4) * 7);
    std::copy(givens, givens + MAX_N, queens);
    cursorX = cursorY = 0;
    solved = false;
    requestUpdate();
    return;
  }
}

void NQueensActivity::generateNextPuzzle() {
  generating = true;
  requestUpdateAndWait();
  newPuzzle();
  generating = false;
  requestUpdate();
}

void NQueensActivity::completePuzzle() {
  solved = true;
  const double expected = 1.0 / (1.0 + pow(10.0, (static_cast<int>(puzzleElo) -
                                                        static_cast<int>(GAME_SCORES.queensElo)) /
                                                       400.0));
  const uint32_t k = GAME_SCORES.queensSolved < 10 ? 64 :
                     GAME_SCORES.queensSolved < 30 ? 32 :
                     GAME_SCORES.queensElo >= 2200 ? 16 : 24;
  GAME_SCORES.queensElo = static_cast<uint32_t>(
      std::max(100, static_cast<int>(lround(GAME_SCORES.queensElo + k * (1.0 - expected)))));
  GAME_SCORES.queensSolved++;
  GAME_SCORES.saveToFile();
}

void NQueensActivity::onEnter() {
  Activity::onEnter();
  generateNextPuzzle();
}

void NQueensActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  if (solved) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) generateNextPuzzle();
    return;
  }
  bool dirty = false;
  if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
    cursorX = (cursorX + size - 1) % size;
    dirty = true;
  } else if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
    cursorX = (cursorX + 1) % size;
    dirty = true;
  } else if (mappedInput.wasReleased(MappedInputManager::Button::Up)) {
    cursorY = (cursorY + size - 1) % size;
    dirty = true;
  } else if (mappedInput.wasReleased(MappedInputManager::Button::Down)) {
    cursorY = (cursorY + 1) % size;
    dirty = true;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm) && givens[cursorY] < 0) {
    queens[cursorY] = queens[cursorY] == cursorX ? -1 : cursorX;
    int placed = 0;
    for (int r = 0; r < size; r++) placed += queens[r] >= 0;
    if (placed == size && isValid(queens)) completePuzzle();
    dirty = true;
  }
  if (dirty) requestUpdate();
}

void NQueensActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_N_QUEENS));

  if (generating) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, tr(STR_N_QUEENS_GENERATING), true,
                              EpdFontFamily::BOLD);
    renderer.displayBuffer();
    return;
  }

  char status[64];
  snprintf(status, sizeof(status), tr(STR_N_QUEENS_STATUS_FORMAT), size, size,
           static_cast<unsigned long>(GAME_SCORES.queensElo),
           static_cast<unsigned long>(GAME_SCORES.queensSolved + 1));
  const int statusY = metrics.topPadding + metrics.headerHeight + 8;
  renderer.drawCenteredText(SMALL_FONT_ID, statusY, status);

  const int top = statusY + renderer.getLineHeight(SMALL_FONT_ID) + 8;
  const int bottom = pageHeight - metrics.buttonHintsHeight - 8;
  const int cell = std::min((pageWidth - 32) / size, (bottom - top) / size);
  const int boardSize = cell * size;
  const int left = (pageWidth - boardSize) / 2;
  const int boardTop = top + (bottom - top - boardSize) / 2;
  for (int r = 0; r < size; r++) {
    for (int c = 0; c < size; c++) {
      const int x = left + c * cell;
      const int y = boardTop + r * cell;
      const bool dark = (r + c) & 1;
      if (dark) renderer.fillRect(x, y, cell, cell, Color::LightGray);
      renderer.drawRect(x, y, cell, cell, Color::Black);
      if (queens[r] == c) {
        const int inset = std::max(3, cell / 6);
        renderer.fillRoundedRect(x + inset, y + inset, cell - 2 * inset, cell - 2 * inset,
                                 std::max(2, cell / 8), Color::Black);
        const int textWidth = renderer.getTextWidth(UI_12_FONT_ID, "Q", EpdFontFamily::BOLD);
        renderer.drawText(UI_12_FONT_ID, x + (cell - textWidth) / 2, y + inset, "Q", false,
                          EpdFontFamily::BOLD);
        if (givens[r] == c) renderer.drawRect(x + 3, y + 3, cell - 6, cell - 6, Color::Black);
      }
      if (cursorX == c && cursorY == r) {
        renderer.drawRect(x + 2, y + 2, cell - 4, cell - 4, Color::Black);
        renderer.drawRect(x + 4, y + 4, cell - 8, cell - 8, Color::Black);
      }
    }
  }
  if (solved) GUI.drawCenteredCard(renderer, tr(STR_YOU_WIN), pageHeight / 2);
  const char* confirm = solved ? tr(STR_NEW_GAME) : tr(STR_N_QUEENS_PLACE);
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), confirm, tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
