#pragma once

#include <cstdint>
#include <string>
#define REPLACEMENT_GLYPH 0xFFFD

uint32_t utf8NextCodepoint(const unsigned char** string);
// Remove the last UTF-8 codepoint from a std::string and return the new size.
size_t utf8RemoveLastChar(std::string& str);
// Truncate string by removing N UTF-8 codepoints from the end.
void utf8TruncateChars(std::string& str, size_t numChars);

// Returns true for Unicode combining diacritical marks that should not advance the cursor.
inline bool utf8IsCombiningMark(const uint32_t cp) {
  return (cp >= 0x0300 && cp <= 0x036F)      // Combining Diacritical Marks
         || (cp >= 0x1DC0 && cp <= 0x1DFF)   // Combining Diacritical Marks Supplement
         || (cp >= 0x20D0 && cp <= 0x20FF)   // Combining Diacritical Marks for Symbols
         || (cp >= 0xFE20 && cp <= 0xFE2F);  // Combining Half Marks
}

// Returns true for any combining mark handled by NFC composition below,
// including U+031B (COMBINING HORN) which sits outside the standard range above.
inline bool utf8IsComposingCombining(const uint32_t cp) {
  return utf8IsCombiningMark(cp) || cp == 0x031B;
}

// Apply lightweight NFC-like normalization for Latin precomposed characters.
// Converts NFD sequences (base letter + combining marks) into NFC precomposed
// codepoints so accented Spanish/French/etc. text (á é í ó ú ñ ü ç …) matches
// the font glyph table. Safe no-op for already-NFC text. Handles both canonical
// NFD ordering and the "natural" order commonly produced by macOS and Word.
std::string utf8NfcNorm(std::string s);
