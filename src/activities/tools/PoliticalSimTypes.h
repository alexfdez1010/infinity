#pragma once

#include <cstdint>

// Data types for the "Simulador Político" card game (Reigns-style).
// Instances are generated into PoliticalSimData.h and live in flash.
namespace polsim {

// Effect on the four powers, in order: pueblo, jueces, socios, partido.
struct Effect {
  int8_t d[4];
};

// Stat condition: stat index, operator ('<' or '>'), threshold.
struct Cond {
  uint8_t stat;
  char op;
  int8_t val;
};

// One swipe choice (left or right).
struct Option {
  const char* label;
  Effect eff;
  const char* const* set;  // null-terminated flag list, or nullptr
  const char* quip;        // headline blurb, or nullptr
};

struct Card {
  const char* id;
  const char* tag;
  const char* speaker;
  const char* role;
  const char* text;
  uint16_t minDay;
  const char* const* req;  // null-terminated, or nullptr
  const char* const* excludes;  // null-terminated, or nullptr
  const Cond* ifStat;           // array, or nullptr
  uint8_t ifStatCount;
  bool urgent;
  bool repeat;
  uint8_t weight;
  Option left;
  Option right;
};

struct Ending {
  const char* stamp;
  const char* headline;
  const char* body;
  const char* epitaph;
};

struct SecretEnding {
  const char* id;
  const char* const* req;
  const char* const* excludes;
  const Cond* ifStat;
  uint8_t ifStatCount;
  uint16_t minDay;
  uint16_t maxDay;         // 0 = no cap
  int8_t allStatsLo;       // -1 = unused
  int8_t allStatsHi;       // -1 = unused
  Ending end;
};

}  // namespace polsim
