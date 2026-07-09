#include "ToolsActivity.h"

#include "ClockActivity.h"
#include "CrossPetSettings.h"
#include "GamesActivity.h"
#include "ReadingGoalsActivity.h"
#include "ReadingStatsActivity.h"
#include "activities/browser/OpdsBookBrowserActivity.h"
#include "activities/settings/OpdsServerListActivity.h"
#include "OpdsServerStore.h"

void ToolsActivity::buildMenu() {
  menuEntries.clear();

  // File Browser — swapped with File Transfer, which now lives on the home bar
  add(StrId::STR_BROWSE_FILES, [this] { activityManager.goToFileBrowser(); });

  // Apps — filtered by CrossPetSettings toggles
  if (CROSSPET_SETTINGS.appClock)
    add(StrId::STR_CLOCK, [this] { push<ClockActivity>(); });

  // Reading stats & goals are always available
  add(StrId::STR_READING_STATS_APP, [this] { push<ReadingStatsActivity>(); });
  add(StrId::STR_READING_GOALS_APP, [this] { push<ReadingGoalsActivity>(); });

  // OPDS browser (if any servers configured)
  if (OPDS_STORE.hasServers())
    add(StrId::STR_OPDS_BROWSER, [this] {
      const auto& servers = OPDS_STORE.getServers();
      if (servers.size() == 1) {
        push<OpdsBookBrowserActivity>(servers[0]);
      } else {
        push<OpdsServerListActivity>(true);
      }
    });

  // Games — opens a submenu listing the available games
  if (CROSSPET_SETTINGS.appGames)
    add(StrId::STR_GAMES, [this] { push<GamesActivity>(); });
}
