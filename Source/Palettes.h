#pragma once
#include "Theory.h"

namespace palettes
{
constexpr int numPads = 24;
using Palette = std::array<theory::ChordSpec, numPads>;

/** Default chord palette for a level, major or minor key. */
Palette levelDefault (theory::Level level, bool minorKey);

/** Index 0 is "Level default"; others are fixed teaching palettes. */
juce::StringArray presetNames();
Palette preset (int index, theory::Level level, bool minorKey);

/** Short description shown in the help bar for each preset. */
juce::String presetDescription (int index);

/** Pads laid out like a piano: true if pad index sits on a black key. */
bool isBlackPad (int pad) noexcept;

/** Computer-keyboard hint for a pad (bank-relative). */
juce::String qwertyHint (int pad);
}
