#include "NQueensActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <esp_random.h>

#include "GameScores.h"
#include "ChessQueenSprite.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
void drawQueen(GfxRenderer& renderer, int x, int y, int cell, bool white) {
  const int side = std::min(48, cell - 6);
  const int left = x + (cell - side) / 2;
  const int top = y + (cell - side) / 2;
  for (int dy = 0; dy < side; dy++) {
    const int sourceY = dy * 48 / side;
    for (int dx = 0; dx < side; dx++) {
      const int sourceX = dx * 48 / side;
      const uint8_t byte = CHESS_QUEEN_48[sourceY * 6 + sourceX / 8];
      if ((byte & (0x80 >> (sourceX % 8))) == 0) renderer.drawPixel(left + dx, top + dy, !white);
    }
  }
}
}  // namespace

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

void NQueensActivity::newPuzzle() {
  int givenCount = 0;
  if (difficulty == 0) {
    size = 5;
  } else if (difficulty == 1) {
    size = 6;
    givenCount = 1;
  } else if (difficulty == 2) {
    size = 7 + static_cast<int>(esp_random() % 2);
    givenCount = 1 + static_cast<int>(esp_random() % 2);
  } else {
    size = 9 + static_cast<int>(esp_random() % 2);
    givenCount = 2 + static_cast<int>(esp_random() % 2);
  }

  for (int attempt = 0; attempt < 50; attempt++) {
    std::fill(solution, solution + MAX_N, -1);
    if (!solveRandom(solution, 0)) continue;
    std::fill(givens, givens + MAX_N, -1);
    int8_t rows[MAX_N];
    for (int i = 0; i < size; i++) rows[i] = i;
    shuffle(rows, size);
    for (int i = 0; i < givenCount; i++) givens[rows[i]] = solution[rows[i]];
    std::copy(givens, givens + MAX_N, queens);
    cursorX = cursorY = 0;
    state = State::PLAY;
    requestUpdate();
    return;
  }
}

void NQueensActivity::generateNextPuzzle() {
  state = State::GENERATING;
  requestUpdateAndWait();
  newPuzzle();
  requestUpdate();
}

void NQueensActivity::completePuzzle() {
  state = State::WON;
  GAME_SCORES.queensSolved++;
  GAME_SCORES.saveToFile();
}

void NQueensActivity::onEnter() {
  Activity::onEnter();
  state = State::SELECT;
  requestUpdate();
}

void NQueensActivity::loop() {
  if (state == State::SELECT) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) finish();
    else if (mappedInput.wasReleased(MappedInputManager::Button::Up)) {
      menuSel = (menuSel + 3) % 4;
      requestUpdate();
    } else if (mappedInput.wasReleased(MappedInputManager::Button::Down)) {
      menuSel = (menuSel + 1) % 4;
      requestUpdate();
    } else if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      difficulty = menuSel;
      generateNextPuzzle();
    }
    return;
  }
  if (state == State::WON) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      state = State::SELECT;
      requestUpdate();
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) generateNextPuzzle();
    return;
  }
  if (state != State::PLAY) return;
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    state = State::SELECT;
    requestUpdate();
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

  if (state == State::SELECT) {
    GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_DIFFICULTY));
    const char* options[4] = {tr(STR_EASY), tr(STR_MEDIUM), tr(STR_HARD), tr(STR_IMPOSSIBLE)};
    const int lineH = renderer.getLineHeight(UI_12_FONT_ID);
    const int rowH = lineH + 16;
    const int startY = pageHeight / 2 - (4 * rowH) / 2;
    for (int i = 0; i < 4; i++) {
      const int y = startY + i * rowH;
      if (i == menuSel) {
        const int boxW = pageWidth / 2;
        renderer.drawRoundedRect((pageWidth - boxW) / 2, y - 4, boxW, rowH - 4, 2, 8, true);
      }
      renderer.drawCenteredText(UI_12_FONT_ID, y + (rowH - lineH) / 2, options[i]);
    }
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_N_QUEENS));
  if (state == State::GENERATING) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, tr(STR_N_QUEENS_GENERATING), true,
                              EpdFontFamily::BOLD);
    renderer.displayBuffer();
    return;
  }

  char status[64];
  const char* levels[4] = {tr(STR_EASY), tr(STR_MEDIUM), tr(STR_HARD), tr(STR_IMPOSSIBLE)};
  snprintf(status, sizeof(status), tr(STR_N_QUEENS_STATUS_FORMAT), levels[difficulty], size, size,
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
      if (dark) renderer.fillRect(x, y, cell, cell, true);
      renderer.drawRect(x, y, cell, cell, true);
      if (queens[r] == c) {
        drawQueen(renderer, x, y, cell, dark);
        if (givens[r] == c) renderer.drawRect(x + 3, y + 3, cell - 6, cell - 6, !dark);
      }
      if (cursorX == c && cursorY == r) {
        renderer.drawRect(x + 2, y + 2, cell - 4, cell - 4, 2, !dark);
        renderer.drawRect(x + 5, y + 5, cell - 10, cell - 10, !dark);
      }
    }
  }
  if (state == State::WON) GUI.drawCenteredCard(renderer, tr(STR_YOU_WIN), pageHeight / 2);
  const char* confirm = state == State::WON ? tr(STR_NEW_GAME) : tr(STR_N_QUEENS_PLACE);
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), confirm, tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
