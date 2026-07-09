#pragma once
#include <I18n.h>

#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include "Activity.h"
#include "util/ButtonNavigator.h"

// Base class for the recurring "header + vertical list + button hints" screen.
//
// Owns the selection state, D-pad navigation, and the standard loop()/render().
// Subclasses supply only a header title and, in buildMenu(), the list entries.
// This collapses the ~80 lines of boilerplate that every menu screen used to
// copy (see ToolsActivity, GamesActivity, ...).
//
// Typical subclass:
//   class GamesActivity final : public ListMenuActivity {
//    public:
//     GamesActivity(GfxRenderer& r, MappedInputManager& m)
//         : ListMenuActivity("Games", StrId::STR_GAMES, r, m) {}
//    protected:
//     void buildMenu() override {
//       add(StrId::STR_2048, [this] { push<TwentyFortyEightActivity>(); });
//     }
//   };
class ListMenuActivity : public Activity {
  ButtonNavigator buttonNavigator;
  int selectorIndex = 0;
  StrId titleId;

 protected:
  struct MenuEntry {
    StrId labelId;
    std::function<void()> launch;
  };
  std::vector<MenuEntry> menuEntries;

  // Rebuild menuEntries. Called on every onEnter() so entries can reflect
  // current settings/state. Use add() (and optionally push<>()) to populate.
  virtual void buildMenu() = 0;

  // Append a menu entry: a label plus the action to run when it is selected.
  void add(StrId labelId, std::function<void()> launch) {
    menuEntries.push_back({labelId, std::move(launch)});
  }

  // Convenience for the common "push a child activity" action. The child is
  // constructed with (renderer, mappedInput, extra...), matching every
  // Activity subclass' constructor convention.
  template <typename A, typename... Args>
  void push(Args&&... extra) {
    activityManager.pushActivity(
        std::make_unique<A>(renderer, mappedInput, std::forward<Args>(extra)...));
  }

 public:
  ListMenuActivity(std::string name, StrId titleId, GfxRenderer& renderer,
                   MappedInputManager& mappedInput)
      : Activity(std::move(name), renderer, mappedInput), titleId(titleId) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
