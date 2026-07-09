#include "PegSolitaireActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <cstdio>
#include <cstring>

#include "GameScores.h"
#include "components/UITheme.h"
#include "fontIds.h"

// --- Core logic ---

// A jump moves the peg at (x,y) two cells in (dx,dy), removing the peg it
// hops over. Legal only when the middle cell holds a peg and the landing cell
// is an empty board hole.
bool PegSolitaireActivity::tryJump(int x, int y, int dx, int dy) {
  const int mx = x + dx, my = y + dy;        // jumped-over cell
  const int tx = x + 2 * dx, ty = y + 2 * dy;  // landing cell
  if (!inBoard(x, y) || !peg[y][x]) return false;
  if (!inBoard(mx, my) || !peg[my][mx]) return false;
  if (!inBoard(tx, ty) || peg[ty][tx]) return false;
  peg[y][x] = false;
  peg[my][mx] = false;
  peg[ty][tx] = true;
  return true;
}

bool PegSolitaireActivity::hasAnyMove() const {
  static const int dx[4] = {0, 0, -1, 1};
  static const int dy[4] = {-1, 1, 0, 0};
  for (int y = 0; y < GRID; y++) {
    for (int x = 0; x < GRID; x++) {
      if (!inBoard(x, y) || !peg[y][x]) continue;
      for (int d = 0; d < 4; d++) {
        const int mx = x + dx[d], my = y + dy[d];
        const int tx = x + 2 * dx[d], ty = y + 2 * dy[d];
        if (inBoard(mx, my) && peg[my][mx] && inBoard(tx, ty) && !peg[ty][tx]) return true;
      }
    }
  }
  return false;
}

int PegSolitaireActivity::pegsLeft() const {
  int n = 0;
  for (int y = 0; y < GRID; y++)
    for (int x = 0; x < GRID; x++)
      if (valid[y][x] && peg[y][x]) n++;
  return n;
}

void PegSolitaireActivity::newGame() {
  for (int y = 0; y < GRID; y++) {
    for (int x = 0; x < GRID; x++) {
      // Cross board: everything except the four 2x2 corners.
      const bool corner = (x < 2 || x > 4) && (y < 2 || y > 4);
      valid[y][x] = !corner;
      peg[y][x] = valid[y][x];
    }
  }
  peg[3][3] = false;  // centre starts empty
  cursorX = 3;
  cursorY = 3;
  selX = -1;
  selY = -1;
  moves = 0;
  gameOver = false;
  requestUpdate();
}

void PegSolitaireActivity::recordBestIfOver() {
  if (!gameOver) return;
  // Best = fewest pegs left (1 is a perfect game); 0 means "no record yet".
  const int left = pegsLeft();
  if (GAME_SCORES.bestPeg == 0 || (uint32_t)left < GAME_SCORES.bestPeg) {
    GAME_SCORES.bestPeg = (uint32_t)left;
    GAME_SCORES.saveToFile();
  }
}

// --- Lifecycle ---

void PegSolitaireActivity::onEnter() {
  Activity::onEnter();
  newGame();
}

void PegSolitaireActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  if (gameOver) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) newGame();
    return;
  }

  bool dirty = false;

  if (selX < 0) {
    // No peg picked up: D-pad moves the cursor over board cells.
    if (mappedInput.wasReleased(MappedInputManager::Button::Left) && inBoard(cursorX - 1, cursorY)) {
      cursorX--;
      dirty = true;
    } else if (mappedInput.wasReleased(MappedInputManager::Button::Right) && inBoard(cursorX + 1, cursorY)) {
      cursorX++;
      dirty = true;
    } else if (mappedInput.wasReleased(MappedInputManager::Button::Up) && inBoard(cursorX, cursorY - 1)) {
      cursorY--;
      dirty = true;
    } else if (mappedInput.wasReleased(MappedInputManager::Button::Down) && inBoard(cursorX, cursorY + 1)) {
      cursorY++;
      dirty = true;
    }

    // Confirm picks up the peg under the cursor.
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm) && peg[cursorY][cursorX]) {
      selX = cursorX;
      selY = cursorY;
      dirty = true;
    }
  } else {
    // A peg is picked up: Confirm drops it, a direction attempts a jump.
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      selX = selY = -1;
      dirty = true;
    } else {
      int dx = 0, dy = 0;
      if (mappedInput.wasReleased(MappedInputManager::Button::Left)) dx = -1;
      else if (mappedInput.wasReleased(MappedInputManager::Button::Right)) dx = 1;
      else if (mappedInput.wasReleased(MappedInputManager::Button::Up)) dy = -1;
      else if (mappedInput.wasReleased(MappedInputManager::Button::Down)) dy = 1;

      if (dx != 0 || dy != 0) {
        if (tryJump(selX, selY, dx, dy)) {
          moves++;
          cursorX = selX + 2 * dx;
          cursorY = selY + 2 * dy;
          selX = selY = -1;
          if (!hasAnyMove()) {
            gameOver = true;
            recordBestIfOver();
          }
        }
        dirty = true;  // redraw even on an illegal jump (clears any stale state)
      }
    }
  }

  if (dirty) requestUpdate();
}

// --- Render ---

void PegSolitaireActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();

  // Header
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight},
                 tr(STR_SENKU));

  // Status line: pegs left and best (fewest ever)
  const int statusY = metrics.topPadding + metrics.headerHeight + 10;
  char bestStr[12];
  if (GAME_SCORES.bestPeg == 0) {
    snprintf(bestStr, sizeof(bestStr), "-");
  } else {
    snprintf(bestStr, sizeof(bestStr), "%lu", (unsigned long)GAME_SCORES.bestPeg);
  }
  char status[64];
  snprintf(status, sizeof(status), tr(STR_SENKU_STATUS_FORMAT), (unsigned long)pegsLeft(), bestStr);
  renderer.drawCenteredText(SMALL_FONT_ID, statusY, status);

  // --- Grid geometry (centred, fits the available area) ---
  const int GAP = 6;
  const int sideMargin = 16;
  const int top = statusY + renderer.getLineHeight(SMALL_FONT_ID) + 12;
  const int bottom = pageHeight - metrics.buttonHintsHeight - metrics.verticalSpacing;
  const int availW = pageWidth - 2 * sideMargin;
  const int availH = bottom - top;

  int tile = (availW - (GRID - 1) * GAP) / GRID;
  const int tileH = (availH - (GRID - 1) * GAP) / GRID;
  if (tileH < tile) tile = tileH;
  if (tile > 90) tile = 90;

  const int gridW = GRID * tile + (GRID - 1) * GAP;
  const int gridH = GRID * tile + (GRID - 1) * GAP;
  const int gridLeft = (pageWidth - gridW) / 2;
  const int gridTop = top + (availH - gridH) / 2;

  for (int r = 0; r < GRID; r++) {
    for (int c = 0; c < GRID; c++) {
      if (!valid[r][c]) continue;  // outside the cross — draw nothing

      const int x = gridLeft + c * (tile + GAP);
      const int y = gridTop + r * (tile + GAP);
      const bool on = peg[r][c];

      if (on) {
        renderer.fillRoundedRect(x, y, tile, tile, tile / 2, Color::Black);  // peg
      } else {
        renderer.drawRoundedRect(x, y, tile, tile, 1, tile / 2, true);  // empty hole
      }

      // Picked-up peg: a white ring inside the black peg.
      if (r == selY && c == selX) {
        renderer.drawRoundedRect(x + 4, y + 4, tile - 8, tile - 8, 3, (tile - 8) / 2, false);
      }

      // Cursor: a contrasting inner square, readable on peg and empty cells.
      if (r == cursorY && c == cursorX) {
        const int inset = tile / 3;
        renderer.fillRect(x + inset, y + inset, tile - 2 * inset, tile - 2 * inset, !on);
      }
    }
  }

  // End overlay: win (one peg) vs no-moves-left.
  if (gameOver) {
    const bool win = pegsLeft() == 1;
    GUI.drawCenteredCard(renderer, win ? tr(STR_YOU_WIN) : tr(STR_GAME_OVER), pageHeight / 2);
  }

  // Button hints
  const char* confirmLabel =
      gameOver ? tr(STR_NEW_GAME) : (selX < 0 ? tr(STR_SENKU_PICK) : tr(STR_SENKU_DROP));
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), confirmLabel, tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
