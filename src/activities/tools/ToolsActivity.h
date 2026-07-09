#pragma once
#include <I18n.h>

#include "../ListMenuActivity.h"

class ToolsActivity final : public ListMenuActivity {
 protected:
  // Dynamic menu built from enabled apps (see CrossPetSettings toggles).
  void buildMenu() override;

 public:
  ToolsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : ListMenuActivity("Tools", StrId::STR_TOOLS, renderer, mappedInput) {}
};
