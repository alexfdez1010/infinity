#include "PoliticalSimActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <esp_random.h>

#include <cstdio>
#include <cstring>

#include "PoliticalSimData.h"
#include "components/UITheme.h"
#include "fontIds.h"

using namespace polsim;

// ---------------------------------------------------------------------------
// Engine (ported from political-simulator js/engine.js)
// ---------------------------------------------------------------------------

static inline int clamp100(int v) { return v < 0 ? 0 : (v > 100 ? 100 : v); }
static inline int randn(int n) { return n <= 0 ? 0 : (int)(esp_random() % (uint32_t)n); }

// Random ±2 wobble on an effect, never flipping its sign, min magnitude 1.
int PoliticalSimActivity::jitter(int v) const {
  if (!v) return 0;
  int out = v + randn(5) - 2;
  if (v > 0) return out < 1 ? 1 : out;
  return out > -1 ? -1 : out;
}

bool PoliticalSimActivity::checkCond(const Card& c) const {
  if (!c.repeat && flags.count(std::string("done_") + c.id)) return false;
  for (const auto& r : recent)
    if (r == c.id) return false;
  if (c.minDay && day < c.minDay) return false;
  if (c.req)
    for (int i = 0; c.req[i]; i++)
      if (!flags.count(c.req[i])) return false;
  if (c.excludes)
    for (int i = 0; c.excludes[i]; i++)
      if (flags.count(c.excludes[i])) return false;
  for (int i = 0; i < c.ifStatCount; i++) {
    const Cond& cn = c.ifStat[i];
    int v = stats[cn.stat];
    if (cn.op == '<' && !(v < cn.val)) return false;
    if (cn.op == '>' && !(v > cn.val)) return false;
  }
  return true;
}

const SecretEnding* PoliticalSimActivity::secretHit() const {
  for (int s = 0; s < kSecretCount; s++) {
    const SecretEnding& se = kSecretEndings[s];
    if (se.minDay && day < se.minDay) continue;
    if (se.maxDay && day > se.maxDay) continue;
    bool ok = true;
    if (se.req)
      for (int i = 0; se.req[i]; i++)
        if (!flags.count(se.req[i])) { ok = false; break; }
    if (!ok) continue;
    if (se.excludes)
      for (int i = 0; se.excludes[i]; i++)
        if (flags.count(se.excludes[i])) { ok = false; break; }
    if (!ok) continue;
    if (se.allStatsLo >= 0) {
      for (int i = 0; i < 4; i++)
        if (stats[i] < se.allStatsLo || stats[i] > se.allStatsHi) { ok = false; break; }
      if (!ok) continue;
    }
    for (int i = 0; i < se.ifStatCount; i++) {
      const Cond& cn = se.ifStat[i];
      int v = stats[cn.stat];
      if (cn.op == '<' && !(v < cn.val)) { ok = false; break; }
      if (cn.op == '>' && !(v > cn.val)) { ok = false; break; }
    }
    if (!ok) continue;
    return &se;
  }
  return nullptr;
}

const Card* PoliticalSimActivity::pickCard() const {
  std::vector<const Card*> pool;
  for (int i = 0; i < kCardCount; i++)
    if (checkCond(kCards[i])) pool.push_back(&kCards[i]);

  // Urgent cards have absolute priority.
  std::vector<const Card*> urgent;
  for (auto* c : pool)
    if (c->urgent) urgent.push_back(c);
  if (!urgent.empty()) pool.swap(urgent);

  // Deck exhausted: forget resolved arcs and rebuild.
  if (pool.empty()) {
    std::vector<std::string> toErase;
    for (const auto& f : flags)
      if (f.rfind("done_", 0) == 0) toErase.push_back(f);
    for (const auto& f : toErase) const_cast<PoliticalSimActivity*>(this)->flags.erase(f);
    for (int i = 0; i < kCardCount; i++)
      if (checkCond(kCards[i])) pool.push_back(&kCards[i]);
  }

  // Safety net: relax minDay + recent window so we never stall.
  if (pool.empty()) {
    for (int i = 0; i < kCardCount; i++) {
      const Card& c = kCards[i];
      bool ok = true;
      if (c.req)
        for (int j = 0; c.req[j]; j++)
          if (!flags.count(c.req[j])) { ok = false; break; }
      if (ok && c.excludes)
        for (int j = 0; c.excludes[j]; j++)
          if (flags.count(c.excludes[j])) { ok = false; break; }
      if (ok)
        for (int j = 0; j < c.ifStatCount; j++) {
          const Cond& cn = c.ifStat[j];
          int v = stats[cn.stat];
          if (cn.op == '<' && !(v < cn.val)) { ok = false; break; }
          if (cn.op == '>' && !(v > cn.val)) { ok = false; break; }
        }
      if (ok) pool.push_back(&c);
    }
  }
  if (pool.empty())
    for (int i = 0; i < kCardCount; i++) pool.push_back(&kCards[i]);

  // Weighted random pick.
  int total = 0;
  for (auto* c : pool) total += c->weight ? c->weight : 1;
  int r = randn(total);
  for (auto* c : pool) {
    r -= c->weight ? c->weight : 1;
    if (r < 0) return c;
  }
  return pool.back();
}

void PoliticalSimActivity::dealCard(const Card* c) { current = c; }

void PoliticalSimActivity::choose(bool right) {
  if (over || !current) return;
  const Card* card = current;
  const Option& opt = right ? card->right : card->left;
  current = nullptr;

  for (int k = 0; k < 4; k++) {
    int d = jitter(opt.eff.d[k]);
    stats[k] = clamp100(stats[k] + d);
  }
  if (opt.set)
    for (int i = 0; opt.set[i]; i++) flags.insert(opt.set[i]);
  if (!card->repeat) flags.insert(std::string("done_") + card->id);

  recent.push_back(card->id);
  if ((int)recent.size() > RECENT_WINDOW) recent.pop_front();

  if (strcmp(card->id, "intro") != 0) day += 3 + randn(6);

  if (opt.quip) {
    legacy.push_back(opt.quip);
    if (legacy.size() > 3) legacy.erase(legacy.begin());
  }

  // Secret ending takes priority over death by stat.
  if (const SecretEnding* se = secretHit()) {
    over = true;
    overScroll = 0;
    ending = &se->end;
    endingIsSecret = true;
    return;
  }
  for (int i = 0; i < 4; i++) {
    if (stats[i] <= 0 || stats[i] >= 100) {
      over = true;
      overScroll = 0;
      ending = &kStatEndings[i][stats[i] <= 0 ? 0 : 1];
      endingIsSecret = false;
      return;
    }
  }
  dealCard(pickCard());
}

void PoliticalSimActivity::resetGame() {
  for (int i = 0; i < 4; i++) stats[i] = 50;
  day = 1;
  over = false;
  overScroll = 0;
  overMore = false;
  ending = nullptr;
  endingIsSecret = false;
  flags.clear();
  recent.clear();
  legacy.clear();
  dealCard(&kIntro);
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void PoliticalSimActivity::onEnter() {
  Activity::onEnter();
  resetGame();
  requestUpdate();
}

void PoliticalSimActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  if (over) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      resetGame();
      requestUpdate();
    } else if (mappedInput.wasReleased(MappedInputManager::Button::Up) && overScroll > 0) {
      overScroll--;
      requestUpdate();
    } else if (mappedInput.wasReleased(MappedInputManager::Button::Down) && overMore) {
      overScroll++;
      requestUpdate();
    }
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
    choose(false);
    requestUpdate();
  } else if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
    choose(true);
    requestUpdate();
  }
}

// ---------------------------------------------------------------------------
// Render
// ---------------------------------------------------------------------------

void PoliticalSimActivity::render(RenderLock&&) {
  renderer.clearScreen();
  if (over)
    renderOver();
  else
    renderPlaying();
  renderer.displayBuffer();
}

void PoliticalSimActivity::renderPlaying() {
  const auto& m = UITheme::getInstance().getMetrics();
  const int W = renderer.getScreenWidth();
  const int H = renderer.getScreenHeight();
  const int M = 16;

  GUI.drawHeader(renderer, Rect{0, m.topPadding, W, m.headerHeight}, tr(STR_POLITICAL_SIM));

  int y = m.topPadding + m.headerHeight + m.verticalSpacing;

  // Subheader: leader · day
  char dayStr[32];
  snprintf(dayStr, sizeof(dayStr), tr(STR_POLSIM_DAY_FMT), day);
  const int subH = renderer.getLineHeight(SMALL_FONT_ID) + 6;
  GUI.drawSubHeader(renderer, Rect{0, y, W, subH}, kLeader, dayStr);
  y += subH + m.verticalSpacing;

  // Four power meters.
  const int rowH = 26;
  const int lineS = renderer.getLineHeight(SMALL_FONT_ID);
  const int meterX = M + 96;
  const int valW = 34;
  const int meterW = W - M - valW - 6 - meterX;
  for (int i = 0; i < 4; i++) {
    const int ry = y + i * rowH;
    renderer.drawText(SMALL_FONT_ID, M, ry + (rowH - lineS) / 2, kStatLabels[i]);
    const int my = ry + (rowH - 12) / 2;
    renderer.drawRect(meterX, my, meterW, 12, true);
    const int fw = (meterW - 2) * stats[i] / 100;
    if (fw > 0) renderer.fillRect(meterX + 1, my + 1, fw, 10, true);
    char v[8];
    snprintf(v, sizeof(v), "%d", stats[i]);
    const int vw = renderer.getTextWidth(SMALL_FONT_ID, v);
    renderer.drawText(SMALL_FONT_ID, W - M - vw, ry + (rowH - lineS) / 2, v);
  }
  y += 4 * rowH + m.verticalSpacing;

  const Card* c = current;
  if (!c) return;

  // Reserve space at the bottom for the two choice boxes + hints.
  const int lineH = renderer.getLineHeight(UI_10_FONT_ID);
  const int hintsTop = H - m.buttonHintsHeight;
  const int choiceBoxH = lineS + 2 * lineH + 18;
  const int choiceGap = 8;
  const int choicesTop = hintsTop - m.verticalSpacing - (2 * choiceBoxH + choiceGap);

  // Card panel.
  const int cardTop = y;
  const int cardBottom = choicesTop - m.verticalSpacing;
  const int cardH = cardBottom - cardTop;
  renderer.drawRoundedRect(M, cardTop, W - 2 * M, cardH, 1, 8, true);
  const int pad = 12;
  const int innerX = M + pad;
  const int innerW = W - 2 * M - 2 * pad;
  int cy = cardTop + pad;

  // Tag + "Nº day"
  renderer.drawText(SMALL_FONT_ID, innerX, cy, c->tag);
  char no[16];
  snprintf(no, sizeof(no), tr(STR_POLSIM_CARD_NO_FMT), day);
  const int noW = renderer.getTextWidth(SMALL_FONT_ID, no);
  renderer.drawText(SMALL_FONT_ID, M + (W - 2 * M) - pad - noW, cy, no);
  cy += lineS + 4;

  // Speaker + role
  renderer.drawText(UI_12_FONT_ID, innerX, cy, c->speaker, true, EpdFontFamily::BOLD);
  cy += renderer.getLineHeight(UI_12_FONT_ID);
  renderer.drawText(SMALL_FONT_ID, innerX, cy, c->role);
  cy += lineS + 6;

  // Body (word-wrapped, fills remaining card height).
  const int bodyBottom = cardBottom - pad;
  int maxLines = (bodyBottom - cy) / lineH;
  if (maxLines < 1) maxLines = 1;
  auto lines = renderer.wrappedText(UI_10_FONT_ID, c->text, innerW, maxLines);
  for (const auto& ln : lines) {
    renderer.drawText(UI_10_FONT_ID, innerX, cy, ln.c_str());
    cy += lineH;
  }

  // Two choice boxes.
  auto drawChoice = [&](int top, const char* tag, const Option& opt) {
    renderer.drawRoundedRect(M, top, W - 2 * M, choiceBoxH, 1, 8, true);
    int ty = top + 8;
    renderer.drawText(SMALL_FONT_ID, M + pad, ty, tag, true, EpdFontFamily::BOLD);
    ty += lineS + 2;
    auto ll = renderer.wrappedText(UI_10_FONT_ID, opt.label, W - 2 * M - 2 * pad, 2);
    for (const auto& l : ll) {
      renderer.drawText(UI_10_FONT_ID, M + pad, ty, l.c_str());
      ty += lineH;
    }
  };
  drawChoice(choicesTop, tr(STR_POLSIM_CHOICE_LEFT), c->left);
  drawChoice(choicesTop + choiceBoxH + choiceGap, tr(STR_POLSIM_CHOICE_RIGHT), c->right);

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", tr(STR_DIR_LEFT), tr(STR_DIR_RIGHT));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void PoliticalSimActivity::renderOver() {
  const auto& m = UITheme::getInstance().getMetrics();
  const int W = renderer.getScreenWidth();
  const int H = renderer.getScreenHeight();
  const int M = 16;

  GUI.drawHeader(renderer, Rect{0, m.topPadding, W, m.headerHeight},
                 endingIsSecret ? tr(STR_POLSIM_END_SECRET) : tr(STR_POLSIM_END_TERM));

  const Ending* e = ending;
  if (!e) return;

  // Build the full ending as a flat list of lines, then render only the
  // window starting at overScroll. Nothing is truncated — long endings
  // scroll with the Up/Down buttons.
  struct OLine {
    std::string text;
    int font;
    EpdFontFamily::Style style;
    bool centered;
    bool stamp;  // drawn as an inverted boxed banner
    int h;
    int gap;  // extra space after this line
  };
  std::vector<OLine> lines;
  auto add = [&](int font, EpdFontFamily::Style style, bool centered, bool stamp,
                 const std::string& t, int gap) {
    const int h = renderer.getLineHeight(font) + (stamp ? 12 : 0);
    lines.push_back({t, font, style, centered, stamp, h, gap});
  };
  auto addWrapped = [&](int font, EpdFontFamily::Style style, bool centered, const char* text,
                        int maxLines, int gapAfter) {
    auto ls = renderer.wrappedText(font, text, W - 2 * M, maxLines, style);
    for (size_t k = 0; k < ls.size(); k++)
      add(font, style, centered, false, ls[k], k + 1 == ls.size() ? gapAfter : 0);
  };

  add(UI_12_FONT_ID, EpdFontFamily::BOLD, true, true, e->stamp, 20);
  addWrapped(UI_12_FONT_ID, EpdFontFamily::BOLD, true, e->headline, 6, 10);
  addWrapped(UI_10_FONT_ID, EpdFontFamily::REGULAR, false, e->body, 40, 12);

  char days[48];
  snprintf(days, sizeof(days), tr(STR_POLSIM_SURVIVED_FMT), day, kPalace);
  add(SMALL_FONT_ID, EpdFontFamily::REGULAR, true, false, days, 6);
  addWrapped(SMALL_FONT_ID, EpdFontFamily::REGULAR, true, e->epitaph, 6, legacy.empty() ? 0 : 12);

  if (!legacy.empty()) {
    add(SMALL_FONT_ID, EpdFontFamily::BOLD, true, false, tr(STR_POLSIM_LEGACY_TITLE), 4);
    for (const auto& q : legacy) addWrapped(SMALL_FONT_ID, EpdFontFamily::REGULAR, true, ("- " + q).c_str(), 3, 2);
  }

  // Render the visible window.
  const int contentTop = m.topPadding + m.headerHeight + m.verticalSpacing + 8;
  const int contentBottom = H - m.buttonHintsHeight - m.verticalSpacing;
  int y = contentTop;
  size_t i = overScroll < 0 ? 0 : (size_t)overScroll;
  if (i > lines.size()) i = lines.size();
  for (; i < lines.size(); i++) {
    const OLine& L = lines[i];
    if (y + L.h > contentBottom) break;
    if (L.stamp) {
      const int stW = renderer.getTextWidth(L.font, L.text.c_str(), L.style);
      const int stH = renderer.getLineHeight(L.font);
      renderer.fillRoundedRect((W - stW - 24) / 2, y, stW + 24, stH + 12, 6, Color::Black);
      renderer.drawCenteredText(L.font, y + 6, L.text.c_str(), false, L.style);
    } else if (L.centered) {
      renderer.drawCenteredText(L.font, y, L.text.c_str(), true, L.style);
    } else {
      renderer.drawText(L.font, M, y, L.text.c_str(), true, L.style);
    }
    y += L.h + L.gap;
  }
  overMore = (i < lines.size());

  // Side hints for the Up/Down scroll buttons (only while scrollable).
  if (overScroll > 0 || overMore)
    GUI.drawSideButtonHints(renderer, overScroll > 0 ? tr(STR_DIR_UP) : "", overMore ? tr(STR_DIR_DOWN) : "");

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_POLSIM_RESTART), "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}
