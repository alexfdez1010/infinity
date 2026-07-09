#pragma once

#include <cstdint>
#include <deque>
#include <string>
#include <unordered_set>
#include <vector>

#include "../Activity.h"
#include "PoliticalSimTypes.h"

// "Simulador Político" — a Reigns-style card game ported from the
// political-simulator web project (Spain edition). Four powers start at 50;
// each card offers a left/right choice that nudges them. Any power hitting
// 0 or 100 ends your term. Secret endings trigger on hidden conditions.
//
// Controls: Left button = left choice, Right button = right choice,
// Back = exit, Confirm = restart (on the game-over screen).
class PoliticalSimActivity final : public Activity {
  static constexpr int RECENT_WINDOW = 14;

  int stats[4];  // pueblo, jueces, socios, partido
  int day;
  bool over;
  std::unordered_set<std::string> flags;
  std::deque<std::string> recent;
  std::vector<std::string> legacy;  // last few satirical headlines, for the ending

  const polsim::Card* current;
  const polsim::Ending* ending;  // set when over
  bool endingIsSecret;
  int overScroll;   // first visible line index on the game-over screen
  bool overMore;    // true when more ending lines exist below the viewport

  // engine
  void resetGame();
  bool checkCond(const polsim::Card& c) const;
  const polsim::SecretEnding* secretHit() const;
  const polsim::Card* pickCard() const;
  void dealCard(const polsim::Card* c);
  void choose(bool right);
  int jitter(int v) const;

  // render helpers
  void renderPlaying();
  void renderOver();

 public:
  explicit PoliticalSimActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("PoliticalSim", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
