#include "BlocksActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <cstdio>
#include <cstring>
#include <esp_random.h>
#include <queue>
#include <unordered_set>
#include <vector>

#include "GameScores.h"
#include "components/UITheme.h"
#include "fontIds.h"

// Difficulty bands, indexed by difficulty (0=easy, 1=medium, 2=hard). The piece
// counts include the target; the length range is the accepted minimum solution
// length (in piece-moves) reported by the BFS solver.
// Difficulty grading: how many pieces to place and the accepted minimum
// solution length (in piece-moves) for a generated board.
struct DiffCfg {
  int pieces;
  int minLen;
  int maxLen;
};
static const DiffCfg CFG[3] = {
    {6, 3, 8},     // easy   — few pieces, short solution
    {8, 9, 16},    // medium — more clutter, moderate solution
    {11, 17, 40},  // hard   — packed board, long solution
};

// --- Board helpers ---

// Rebuild the occupancy grid from the current piece list. Empty cells hold -1.
void BlocksActivity::buildGrid(int8_t grid[BOARD][BOARD]) const {
  memset(grid, -1, sizeof(int8_t) * BOARD * BOARD);
  for (int i = 0; i < pieceCount; i++) {
    const Piece& p = pieces[i];
    for (int k = 0; k < p.len; k++) {
      const int r = p.horizontal ? p.row : p.row + k;
      const int c = p.horizontal ? p.col + k : p.col;
      grid[r][c] = static_cast<int8_t>(i);
    }
  }
}

// Slide piece idx by (dRow,dCol). A move is legal only along the piece's own
// orientation, must stay on the board, and must not overlap another piece.
bool BlocksActivity::tryMove(int idx, int dRow, int dCol) {
  Piece& p = pieces[idx];
  if (p.horizontal && dRow != 0) return false;   // horizontal pieces never change row
  if (!p.horizontal && dCol != 0) return false;  // vertical pieces never change col

  const int nr = p.row + dRow;
  const int nc = p.col + dCol;
  const int endR = nr + (p.horizontal ? 0 : p.len - 1);
  const int endC = nc + (p.horizontal ? p.len - 1 : 0);
  if (nr < 0 || nc < 0 || endR >= BOARD || endC >= BOARD) return false;

  int8_t grid[BOARD][BOARD];
  buildGrid(grid);
  for (int k = 0; k < p.len; k++) {
    const int r = p.horizontal ? nr : nr + k;
    const int c = p.horizontal ? nc + k : nc;
    if (grid[r][c] != -1 && grid[r][c] != idx) return false;  // blocked by another piece
  }

  p.row = static_cast<uint8_t>(nr);
  p.col = static_cast<uint8_t>(nc);
  return true;
}

// The target is a horizontal piece on the exit row; it has escaped once its
// right cell reaches the last column (the exit gap on the right edge).
bool BlocksActivity::targetEscaped() const {
  return pieces[0].col + pieces[0].len >= BOARD;
}

// --- Solver ---

// Breadth-first search over reachable board states. Each state is the tuple of
// every piece's variable coordinate (col for horizontal pieces, row for
// vertical ones); the fixed coordinate, length and orientation never change.
// Coordinates are 0..5, so each piece packs into 3 bits and up to 14 pieces fit
// in a uint64 key. Returns the minimum number of piece-moves to free the
// target, or -1 if unsolvable within the bounded state cap.
int BlocksActivity::solve() const {
  static constexpr int MAX_STATES = 20000;  // bound RAM/time on the C3

  // Fixed descriptors + the starting variable coordinate of each piece.
  uint8_t fixedLine[MAX_PIECES];
  uint8_t len[MAX_PIECES];
  bool horiz[MAX_PIECES];
  uint8_t startVar[MAX_PIECES];
  for (int i = 0; i < pieceCount; i++) {
    horiz[i] = pieces[i].horizontal;
    len[i] = pieces[i].len;
    fixedLine[i] = horiz[i] ? pieces[i].row : pieces[i].col;
    startVar[i] = horiz[i] ? pieces[i].col : pieces[i].row;
  }

  const int n = pieceCount;
  auto encode = [&](const uint8_t* var) -> uint64_t {
    uint64_t key = 0;
    for (int i = 0; i < n; i++) key |= static_cast<uint64_t>(var[i] & 7u) << (3 * i);
    return key;
  };

  std::unordered_set<uint64_t> visited;
  std::queue<std::pair<uint64_t, int>> frontier;
  const uint64_t start = encode(startVar);
  visited.insert(start);
  frontier.push({start, 0});

  int expanded = 0;
  while (!frontier.empty()) {
    if (++expanded > MAX_STATES) return -1;  // too big — treat as unsolvable
    const uint64_t key = frontier.front().first;
    const int dist = frontier.front().second;
    frontier.pop();

    // Unpack the state and check the goal (target's right cell at the exit).
    uint8_t var[MAX_PIECES];
    for (int i = 0; i < n; i++) var[i] = static_cast<uint8_t>((key >> (3 * i)) & 7u);
    if (var[0] + len[0] >= BOARD) return dist;

    // Occupancy for this state.
    int8_t grid[BOARD][BOARD];
    memset(grid, -1, sizeof(grid));
    for (int i = 0; i < n; i++) {
      for (int k = 0; k < len[i]; k++) {
        const int r = horiz[i] ? fixedLine[i] : var[i] + k;
        const int c = horiz[i] ? var[i] + k : fixedLine[i];
        grid[r][c] = static_cast<int8_t>(i);
      }
    }

    // Each piece can try to step -1 or +1 along its axis; only the single cell
    // it newly enters needs to be empty.
    for (int i = 0; i < n; i++) {
      for (int delta = -1; delta <= 1; delta += 2) {
        const int nv = var[i] + delta;
        if (nv < 0 || nv + len[i] > BOARD) continue;  // off the board
        int r, c;
        if (delta > 0) {  // stepping forward: the cell just past the old end
          r = horiz[i] ? fixedLine[i] : var[i] + len[i];
          c = horiz[i] ? var[i] + len[i] : fixedLine[i];
        } else {  // stepping back: the cell just before the new start
          r = horiz[i] ? fixedLine[i] : nv;
          c = horiz[i] ? nv : fixedLine[i];
        }
        if (grid[r][c] != -1) continue;  // blocked

        uint8_t nvar[MAX_PIECES];
        memcpy(nvar, var, sizeof(uint8_t) * n);
        nvar[i] = static_cast<uint8_t>(nv);
        const uint64_t nkey = encode(nvar);
        if (visited.count(nkey)) continue;
        if (visited.size() >= static_cast<size_t>(MAX_STATES)) return -1;  // cap the visited set
        visited.insert(nkey);
        frontier.push({nkey, dist + 1});
      }
    }
  }
  return -1;  // exhausted without reaching the goal — unsolvable
}

// --- Generation ---

// Lay out a fresh random board for the given difficulty. The target is placed
// first on the exit row (never already at the exit), then random pieces are
// added onto empty cells until the difficulty's piece count is reached (or the
// placement attempts run out). Horizontal pieces are barred from the exit row
// (other than the target) since such a blocker could never clear the exit.
void BlocksActivity::generate(int diff) {
  const DiffCfg& cfg = CFG[diff];

  int8_t grid[BOARD][BOARD];
  memset(grid, -1, sizeof(grid));

  // Target: horizontal length-2 block on the exit row, at columns 0..3 so it is
  // not already sitting at the exit.
  const uint8_t tcol = static_cast<uint8_t>(esp_random() % 4);
  pieces[0] = Piece{static_cast<uint8_t>(EXIT_ROW), tcol, 2, true};
  grid[EXIT_ROW][tcol] = 0;
  grid[EXIT_ROW][tcol + 1] = 0;
  pieceCount = 1;

  int attempts = 0;
  while (pieceCount < cfg.pieces && pieceCount < MAX_PIECES && attempts < 300) {
    attempts++;
    const bool horizontal = (esp_random() & 1u) != 0;
    const int len = 2 + static_cast<int>(esp_random() % 2);  // 2 or 3

    int row, col;
    if (horizontal) {
      row = static_cast<int>(esp_random() % BOARD);
      if (row == EXIT_ROW) continue;  // no horizontal blockers on the exit row
      col = static_cast<int>(esp_random() % (BOARD - len + 1));
    } else {
      col = static_cast<int>(esp_random() % BOARD);
      row = static_cast<int>(esp_random() % (BOARD - len + 1));
    }

    // Reject if any target cell is already occupied.
    bool fits = true;
    for (int k = 0; k < len && fits; k++) {
      const int r = horizontal ? row : row + k;
      const int c = horizontal ? col + k : col;
      if (grid[r][c] != -1) fits = false;
    }
    if (!fits) continue;

    pieces[pieceCount] = Piece{static_cast<uint8_t>(row), static_cast<uint8_t>(col),
                               static_cast<uint8_t>(len), horizontal};
    for (int k = 0; k < len; k++) {
      const int r = horizontal ? row : row + k;
      const int c = horizontal ? col + k : col;
      grid[r][c] = static_cast<int8_t>(pieceCount);
    }
    pieceCount++;
  }
}

// Tiny hardcoded boards, one per difficulty, all solvable by construction (the
// vertical blockers on the exit row always have empty cells below to slide
// into). Used only if random generation never lands a graded, solvable board.
void BlocksActivity::loadFallback(int diff) {
  pieceCount = 0;
  auto add = [&](int r, int c, int len, bool h) {
    pieces[pieceCount++] = Piece{static_cast<uint8_t>(r), static_cast<uint8_t>(c),
                                 static_cast<uint8_t>(len), h};
  };
  add(EXIT_ROW, 0, 2, true);  // target, always index 0
  if (diff == 0) {
    add(0, 4, 2, false);
    add(3, 1, 2, false);
  } else if (diff == 1) {
    add(0, 3, 3, false);  // crosses the exit row; slides down to clear
    add(0, 5, 2, false);
  } else {
    add(0, 5, 3, false);  // blocks the exit cell itself; slides down to clear
    add(0, 3, 3, false);  // blocks the exit row; slides down to clear
    add(3, 1, 2, false);
  }
}

// Deal a new solvable, difficulty-graded board. Retries random generation until
// the BFS solver reports a solution length inside the difficulty band; keeps the
// most recent solvable board as a fallback, and drops to a hardcoded board if no
// solvable one turns up at all. Bounded attempts guarantee it always returns.
void BlocksActivity::newPuzzle() {
  const DiffCfg& cfg = CFG[difficulty];

  Piece fallback[MAX_PIECES];
  int fallbackCount = 0;
  bool haveFallback = false;
  bool found = false;

  for (int attempt = 0; attempt < 300 && !found; attempt++) {
    generate(difficulty);
    const int sol = solve();
    if (sol < 0) continue;  // unsolvable within the state cap — discard

    // Remember this solvable board in case nothing lands in the band.
    memcpy(fallback, pieces, sizeof(Piece) * pieceCount);
    fallbackCount = pieceCount;
    haveFallback = true;

    if (sol >= cfg.minLen && sol <= cfg.maxLen) found = true;  // pieces[] already holds it
  }

  if (!found) {
    if (haveFallback) {
      memcpy(pieces, fallback, sizeof(Piece) * fallbackCount);
      pieceCount = fallbackCount;
    } else {
      loadFallback(difficulty);
    }
  }

  selectedPiece = 0;
  moves = 0;
  state = State::PLAY;
  requestUpdate();
}

void BlocksActivity::recordBest() {
  // Best = fewest moves; 0 means "no record yet".
  if (GAME_SCORES.bestBlocks[difficulty] == 0 || moves < GAME_SCORES.bestBlocks[difficulty]) {
    GAME_SCORES.bestBlocks[difficulty] = moves;
    GAME_SCORES.saveToFile();
  }
}

// --- Lifecycle ---

void BlocksActivity::onEnter() {
  Activity::onEnter();
  state = State::SELECT;
  menuSel = 0;
  requestUpdate();
}

void BlocksActivity::loop() {
  if (state == State::SELECT) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      finish();
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Up)) {
      menuSel = (menuSel + 2) % 3;
      requestUpdate();
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Down)) {
      menuSel = (menuSel + 1) % 3;
      requestUpdate();
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      difficulty = menuSel;
      newPuzzle();  // generates the board and switches to PLAY
      return;
    }
    return;
  }

  if (state == State::WON) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      state = State::SELECT;
      requestUpdate();
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      newPuzzle();  // fresh board, same difficulty
      return;
    }
    return;
  }

  // state == PLAY
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    state = State::SELECT;
    requestUpdate();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    selectedPiece = (selectedPiece + 1) % pieceCount;  // cycle the driven piece
    requestUpdate();
    return;
  }

  bool moved = false;
  if (mappedInput.wasReleased(MappedInputManager::Button::Left))        moved = tryMove(selectedPiece, 0, -1);
  else if (mappedInput.wasReleased(MappedInputManager::Button::Right))  moved = tryMove(selectedPiece, 0, 1);
  else if (mappedInput.wasReleased(MappedInputManager::Button::Up))     moved = tryMove(selectedPiece, -1, 0);
  else if (mappedInput.wasReleased(MappedInputManager::Button::Down))   moved = tryMove(selectedPiece, 1, 0);

  if (moved) {
    moves++;
    if (targetEscaped()) {
      state = State::WON;
      recordBest();
    }
    requestUpdate();
  }
}

// --- Render ---

void BlocksActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();

  // SELECT screen: difficulty picker.
  if (state == State::SELECT) {
    GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_DIFFICULTY));

    const char* options[3] = {tr(STR_EASY), tr(STR_MEDIUM), tr(STR_HARD)};
    const int lineH = renderer.getLineHeight(UI_12_FONT_ID);
    const int rowH = lineH + 22;
    const int startY = pageHeight / 2 - (3 * rowH) / 2;
    for (int i = 0; i < 3; i++) {
      const int y = startY + i * rowH;
      if (i == menuSel) {
        const int boxW = pageWidth / 2;
        renderer.drawRoundedRect((pageWidth - boxW) / 2, y - 4, boxW, rowH - 6, 2, 8, true);
      }
      renderer.drawCenteredText(UI_12_FONT_ID, y + (rowH - 6 - lineH) / 2, options[i]);
    }

    const auto labels =
        mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }

  // PLAY / WON: header + status line + board.
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_BLOCKS));

  const int statusY = metrics.topPadding + metrics.headerHeight + 10;
  char bestStr[12];
  if (GAME_SCORES.bestBlocks[difficulty] == 0) {
    snprintf(bestStr, sizeof(bestStr), "-");
  } else {
    snprintf(bestStr, sizeof(bestStr), "%lu", (unsigned long)GAME_SCORES.bestBlocks[difficulty]);
  }
  char status[80];
  snprintf(status, sizeof(status), tr(STR_BLOCKS_STATUS_FORMAT), (unsigned long)(difficulty + 1),
           (unsigned long)moves, bestStr);
  renderer.drawCenteredText(SMALL_FONT_ID, statusY, status);

  // --- Board geometry (centred, fits between the status line and hints) ---
  const int GAP = 4;
  const int sideMargin = 16;
  const int top = statusY + renderer.getLineHeight(SMALL_FONT_ID) + 12;
  const int bottom = pageHeight - metrics.buttonHintsHeight - metrics.verticalSpacing;
  const int availW = pageWidth - 2 * sideMargin;
  const int availH = bottom - top;

  int cell = (availW - (BOARD - 1) * GAP) / BOARD;
  const int cellH = (availH - (BOARD - 1) * GAP) / BOARD;
  if (cellH < cell) cell = cellH;
  if (cell > 64) cell = 64;

  const int gridW = BOARD * cell + (BOARD - 1) * GAP;
  const int gridH = BOARD * cell + (BOARD - 1) * GAP;
  const int gridLeft = (pageWidth - gridW) / 2;
  const int gridTop = top + (availH - gridH) / 2;

  // Board frame.
  renderer.drawRect(gridLeft - 3, gridTop - 3, gridW + 6, gridH + 6, 1, true);

  // Exit marker: a right-pointing arrow in the margin past the exit row, sized
  // to whatever space is available so it never spills off the screen.
  const int exitY = gridTop + EXIT_ROW * (cell + GAP) + cell / 2;
  const int ax = gridLeft + gridW + 4;
  int arrowLen = pageWidth - ax - 4;
  if (arrowLen > cell / 2) arrowLen = cell / 2;
  if (arrowLen >= 8) {
    const int tip = ax + arrowLen;
    renderer.drawLine(ax, exitY, tip, exitY, 2, true);
    renderer.drawLine(tip, exitY, tip - 6, exitY - 6, 2, true);
    renderer.drawLine(tip, exitY, tip - 6, exitY + 6, 2, true);
  }

  // Pieces. The target (index 0) is a solid black block; other pieces are
  // outlined; the selected piece gets an extra ring around it.
  const int inset = 2;
  for (int i = 0; i < pieceCount; i++) {
    const Piece& p = pieces[i];
    const int x = gridLeft + p.col * (cell + GAP) + inset;
    const int y = gridTop + p.row * (cell + GAP) + inset;
    const int w = (p.horizontal ? p.len * cell + (p.len - 1) * GAP : cell) - 2 * inset;
    const int h = (p.horizontal ? cell : p.len * cell + (p.len - 1) * GAP) - 2 * inset;

    if (i == 0) {
      renderer.fillRect(x, y, w, h, true);  // target: solid, unmistakable
    } else {
      renderer.drawRoundedRect(x, y, w, h, 2, 6, true);  // blocker: outline
    }
    if (i == selectedPiece) {
      renderer.drawRoundedRect(x - inset - 1, y - inset - 1, w + 2 * inset + 2, h + 2 * inset + 2, 2, 8, true);
    }
  }

  // Win overlay (shared black card + white bold text).
  if (state == State::WON) {
    GUI.drawCenteredCard(renderer, tr(STR_YOU_WIN), pageHeight / 2);
  }

  // Button hints: Back / cycle-piece (or new game when won) / Up / Down.
  const char* btn2 = (state == State::WON) ? tr(STR_NEW_GAME) : tr(STR_SELECT);
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), btn2, tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
