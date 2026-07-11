#include "MazeActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <cstdio>
#include <cstring>
#include <esp_random.h>

#include "GameScores.h"
#include "components/UITheme.h"
#include "fontIds.h"

// --- Core logic ---

// Map a difficulty index onto a grid size (columns x rows). Sizes are chosen to
// fit the ~800x480 landscape screen with clearly visible walls, and never
// exceed the fixed MAXC/MAXR storage.
void MazeActivity::applyDifficulty(int d) {
  switch (d) {
    case 0:  cols = 8;  rows = 6;  break;   // Easy
    case 1:  cols = 12; rows = 9;  break;   // Medium
    default: cols = 16; rows = 12; break;   // Hard
  }
}

// Clear the shared wall between cell (r,c) and its neighbour (nr,nc). Both cells
// must be orthogonally adjacent; the matching wall bit is cleared on each side.
void MazeActivity::carve(int r, int c, int nr, int nc) {
  if (nr == r - 1) {         // neighbour is North
    cellWalls[r][c]  &= ~WALL_N;
    cellWalls[nr][nc] &= ~WALL_S;
  } else if (nr == r + 1) {  // neighbour is South
    cellWalls[r][c]  &= ~WALL_S;
    cellWalls[nr][nc] &= ~WALL_N;
  } else if (nc == c - 1) {  // neighbour is West
    cellWalls[r][c]  &= ~WALL_W;
    cellWalls[nr][nc] &= ~WALL_E;
  } else if (nc == c + 1) {  // neighbour is East
    cellWalls[r][c]  &= ~WALL_E;
    cellWalls[nr][nc] &= ~WALL_W;
  }
}

// Build a perfect maze with a randomized depth-first backtracker. The DFS is
// iterative with an explicit stack of cell indices (r*cols+c) so it never
// recurses — deep recursion would blow the small C3 stack. A backtracker always
// produces a fully-connected maze, so start->end is guaranteed solvable.
void MazeActivity::generateMaze() {
  applyDifficulty(diff);

  // Start every cell fully walled and unvisited.
  bool visited[MAXR][MAXC];
  for (int r = 0; r < rows; r++) {
    for (int c = 0; c < cols; c++) {
      cellWalls[r][c] = WALL_N | WALL_E | WALL_S | WALL_W;
      visited[r][c] = false;
    }
  }

  // Explicit DFS stack of visited cells awaiting exploration.
  uint8_t stackR[MAXCELLS];
  uint8_t stackC[MAXCELLS];
  int sp = 0;

  stackR[sp] = 0;
  stackC[sp] = 0;
  sp++;
  visited[0][0] = true;

  static const int drs[4] = {-1, 1, 0, 0};  // N, S, W, E
  static const int dcs[4] = {0, 0, -1, 1};

  while (sp > 0) {
    const int r = stackR[sp - 1];
    const int c = stackC[sp - 1];

    // Collect unvisited orthogonal neighbours.
    int candR[4];
    int candC[4];
    int n = 0;
    for (int d = 0; d < 4; d++) {
      const int nr = r + drs[d];
      const int nc = c + dcs[d];
      if (nr < 0 || nr >= rows || nc < 0 || nc >= cols) continue;
      if (visited[nr][nc]) continue;
      candR[n] = nr;
      candC[n] = nc;
      n++;
    }

    if (n == 0) {
      sp--;  // dead end — backtrack
      continue;
    }

    // Pick a random neighbour, open the wall to it, and descend.
    const int pick = static_cast<int>(esp_random() % static_cast<uint32_t>(n));
    const int nr = candR[pick];
    const int nc = candC[pick];
    carve(r, c, nr, nc);
    visited[nr][nc] = true;
    stackR[sp] = static_cast<uint8_t>(nr);
    stackC[sp] = static_cast<uint8_t>(nc);
    sp++;
  }

  // Reset avatar to the start (top-left) cell.
  playerR = 0;
  playerC = 0;
  steps = 0;
  requestUpdate();
}

// Attempt to move the avatar one cell by (dr,dc). Allowed only when the target
// is on the board and no wall stands between the current and target cell.
bool MazeActivity::tryMove(int dr, int dc) {
  const int nr = playerR + dr;
  const int nc = playerC + dc;
  if (nr < 0 || nr >= rows || nc < 0 || nc >= cols) return false;

  const uint8_t w = cellWalls[playerR][playerC];
  if (dr == -1 && (w & WALL_N)) return false;
  if (dr == 1  && (w & WALL_S)) return false;
  if (dc == -1 && (w & WALL_W)) return false;
  if (dc == 1  && (w & WALL_E)) return false;

  playerR = nr;
  playerC = nc;
  return true;
}

void MazeActivity::recordBestIfWon() {
  if (mode != Mode::WON) return;
  // Best = fewest steps; 0 means "no record yet".
  if (GAME_SCORES.bestMaze[diff] == 0 || steps < GAME_SCORES.bestMaze[diff]) {
    GAME_SCORES.bestMaze[diff] = steps;
    GAME_SCORES.saveToFile();
  }
}

// --- Lifecycle ---

void MazeActivity::onEnter() {
  Activity::onEnter();
  mode = Mode::SELECT;
  selDiff = 0;
  requestUpdate();
}

void MazeActivity::loop() {
  switch (mode) {
    case Mode::SELECT: {
      if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
        finish();
        return;
      }
      if (mappedInput.wasReleased(MappedInputManager::Button::Up)) {
        selDiff = (selDiff + 3 - 1) % 3;
        requestUpdate();
        return;
      }
      if (mappedInput.wasReleased(MappedInputManager::Button::Down)) {
        selDiff = (selDiff + 1) % 3;
        requestUpdate();
        return;
      }
      if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
        diff = selDiff;
        mode = Mode::PLAY;
        generateMaze();
        return;
      }
      break;
    }

    case Mode::PLAY: {
      if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
        mode = Mode::SELECT;  // back to difficulty picker, not out of the game
        requestUpdate();
        return;
      }
      if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
        generateMaze();  // fresh maze at the same difficulty
        return;
      }

      bool moved = false;
      if (mappedInput.wasReleased(MappedInputManager::Button::Up))         moved = tryMove(-1, 0);
      else if (mappedInput.wasReleased(MappedInputManager::Button::Down))  moved = tryMove(1, 0);
      else if (mappedInput.wasReleased(MappedInputManager::Button::Left))  moved = tryMove(0, -1);
      else if (mappedInput.wasReleased(MappedInputManager::Button::Right)) moved = tryMove(0, 1);

      if (moved) {
        steps++;
        if (playerR == rows - 1 && playerC == cols - 1) {  // reached the exit
          mode = Mode::WON;
          recordBestIfWon();
        }
        requestUpdate();
      }
      break;
    }

    case Mode::WON: {
      if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
        mode = Mode::SELECT;
        requestUpdate();
        return;
      }
      if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
        mode = Mode::PLAY;
        generateMaze();
        return;
      }
      break;
    }
  }
}

// --- Render ---

void MazeActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();

  // Header
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight},
                 tr(STR_MAZE));

  // --- Difficulty picker ---
  if (mode == Mode::SELECT) {
    const int titleY = metrics.topPadding + metrics.headerHeight + 30;
    renderer.drawCenteredText(UI_12_FONT_ID, titleY, tr(STR_DIFFICULTY));

    const char* options[3] = {tr(STR_EASY), tr(STR_MEDIUM), tr(STR_HARD)};
    const int lineH = renderer.getLineHeight(UI_12_FONT_ID);
    const int rowH = lineH + 22;
    const int boxW = 260;
    const int firstY = titleY + lineH + 24;

    for (int i = 0; i < 3; i++) {
      const int y = firstY + i * rowH;
      const int x = (pageWidth - boxW) / 2;
      if (i == selDiff) {
        // Highlight the current selection with a rounded outline box.
        renderer.drawRoundedRect(x, y - 6, boxW, rowH - 8, 2, 8, true);
      }
      const int textW = renderer.getTextWidth(UI_12_FONT_ID, options[i]);
      renderer.drawText(UI_12_FONT_ID, (pageWidth - textW) / 2, y, options[i]);
    }

    const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }

  // --- Status line: level, steps, best (fewest steps ever at this difficulty) ---
  const int statusY = metrics.topPadding + metrics.headerHeight + 10;
  char bestStr[12];
  if (GAME_SCORES.bestMaze[diff] == 0) {
    snprintf(bestStr, sizeof(bestStr), "-");
  } else {
    snprintf(bestStr, sizeof(bestStr), "%lu", (unsigned long)GAME_SCORES.bestMaze[diff]);
  }
  char status[80];
  snprintf(status, sizeof(status), tr(STR_MAZE_STATUS_FORMAT),
           (unsigned long)(diff + 1), (unsigned long)steps, bestStr);
  renderer.drawCenteredText(SMALL_FONT_ID, statusY, status);

  // --- Maze geometry (square cells, centred in the available area) ---
  const int sideMargin = 16;
  const int top = statusY + renderer.getLineHeight(SMALL_FONT_ID) + 12;
  const int bottom = pageHeight - metrics.buttonHintsHeight - metrics.verticalSpacing;
  const int availW = pageWidth - 2 * sideMargin;
  const int availH = bottom - top;

  int cell = availW / cols;
  const int cellH = availH / rows;
  if (cellH < cell) cell = cellH;

  const int gridW = cols * cell;
  const int gridH = rows * cell;
  const int gridLeft = (pageWidth - gridW) / 2;
  const int gridTop = top + (availH - gridH) / 2;

  // Draw walls. To avoid drawing shared walls twice we render only the North and
  // West edge of each cell, then add the outer South (bottom) and East (right)
  // borders — the standard technique for a grid of walled cells.
  for (int r = 0; r < rows; r++) {
    for (int c = 0; c < cols; c++) {
      const int x0 = gridLeft + c * cell;
      const int y0 = gridTop + r * cell;
      const uint8_t w = cellWalls[r][c];
      if (w & WALL_N) renderer.drawLine(x0, y0, x0 + cell, y0, 1, true);          // top edge
      if (w & WALL_W) renderer.drawLine(x0, y0, x0, y0 + cell, 1, true);          // left edge
    }
  }
  // Outer bottom and right borders (always walls on a maze boundary).
  renderer.drawLine(gridLeft, gridTop + gridH, gridLeft + gridW, gridTop + gridH, 1, true);
  renderer.drawLine(gridLeft + gridW, gridTop, gridLeft + gridW, gridTop + gridH, 1, true);

  // Exit marker (bottom-right cell): a hollow square target so the avatar shows
  // through when it arrives.
  {
    const int ex = gridLeft + (cols - 1) * cell;
    const int ey = gridTop + (rows - 1) * cell;
    const int inset = cell / 4;
    renderer.drawRect(ex + inset, ey + inset, cell - 2 * inset, cell - 2 * inset, true);
  }

  // Avatar: a filled rounded square inset within its cell.
  {
    const int px = gridLeft + playerC * cell;
    const int py = gridTop + playerR * cell;
    const int inset = cell / 5 + 1;
    const int size = cell - 2 * inset;
    if (size > 0) {
      renderer.fillRect(px + inset, py + inset, size, size, true);
    }
  }

  // Win overlay (shared black card + white bold text)
  if (mode == Mode::WON) {
    GUI.drawCenteredCard(renderer, tr(STR_YOU_WIN), pageHeight / 2);
  }

  // Button hints
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_NEW_GAME), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
