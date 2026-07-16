#include "GamesActivity.h"

#include "ApagonActivity.h"
#include "BlocksActivity.h"
#include "MazeActivity.h"
#include "NQueensActivity.h"
#include "PegSolitaireActivity.h"
#include "SlidingPuzzleActivity.h"
#include "TwentyFortyEightActivity.h"

void GamesActivity::buildMenu() {
  menuEntries.clear();

  add(StrId::STR_2048, [this] { push<TwentyFortyEightActivity>(); });
  add(StrId::STR_SLIDING_PUZZLE, [this] { push<SlidingPuzzleActivity>(); });
  add(StrId::STR_APAGON, [this] { push<ApagonActivity>(); });
  add(StrId::STR_SENKU, [this] { push<PegSolitaireActivity>(); });
  add(StrId::STR_MAZE, [this] { push<MazeActivity>(); });
  add(StrId::STR_BLOCKS, [this] { push<BlocksActivity>(); });
  add(StrId::STR_N_QUEENS, [this] { push<NQueensActivity>(); });
}
