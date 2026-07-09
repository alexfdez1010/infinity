#include "GamesActivity.h"

#include "ApagonActivity.h"
#include "PoliticalSimActivity.h"
#include "TwentyFortyEightActivity.h"

void GamesActivity::buildMenu() {
  menuEntries.clear();

  add(StrId::STR_2048, [this] { push<TwentyFortyEightActivity>(); });
  add(StrId::STR_POLITICAL_SIM, [this] { push<PoliticalSimActivity>(); });
  add(StrId::STR_APAGON, [this] { push<ApagonActivity>(); });
}
