#pragma once
#include <I18n.h>

#include "../ListMenuActivity.h"

// Submenu listing the available games (opened from the Apps menu).
class GamesActivity final : public ListMenuActivity {
 protected:
  void buildMenu() override;
  bool showDirectionHints() const override { return false; }

 public:
  GamesActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : ListMenuActivity("Games", StrId::STR_GAMES, renderer, mappedInput) {}
};
