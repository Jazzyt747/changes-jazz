#pragma once
// ============================================================================
//  CHANGES — Jazz Harmony Instrument
//  Theory engine: chord qualities, tensions per level, voicing styles,
//  voice leading, chord-scales, naming, roman numerals and suggestions.
//  Everything here is real-time safe (no allocation) except the String
//  helpers, which are only called from the message thread.
// ============================================================================
#include <juce_core/juce_core.h>
#include <array>
#include <cstdint>

namespace theory
{
//==============================================================================
enum class Level : int { Beginner = 0, Intermediate, Advanced, Conservatory, Num };

enum class Quality : int
{
    Maj7 = 0, Maj6, Maj7Sharp11, Maj7Sharp5,
    Dom7, Dom7Sus, Dom7Alt, Dom7b9, Dom7Lyd,
    Min7, Min6, MinMaj7, Min7b5, Dim7,
    Num
};

enum class Style : int
{
    Auto = 0, Close, Shell, Rootless, Drop2, Quartal, UpperStructure, Spread, Num
};

enum class MelodyMode : int { Key = 0, ChordScale, ChordTones, Pentatonic, Blues, Bebop, Num };
enum class Harmonize : int { Off = 0, Octaves, Thirds, BlockChords, Num };

constexpr int numQualities = (int) Quality::Num;
constexpr int numStyles    = (int) Style::Num;

//==============================================================================
/** A chord as stored on a pad, relative to the current key. Packs into 32 bits
    so the audio thread can read pads lock-free. */
struct ChordSpec
{
    int rootOffset = 0;     // 0..11 semitones above key tonic
    Quality quality = Quality::Maj7;
    int variation = 0;      // 0..3
    int style = -1;         // -1 = use global style, else Style index
    int bassOffset = -1;    // -1 = none, else semitones above key tonic (slash bass)
    int beats = 4;          // used by the progression only (1..15)
    bool valid = true;

    uint32_t pack() const noexcept;
    static ChordSpec unpack (uint32_t) noexcept;
    bool operator== (const ChordSpec& o) const noexcept { return pack() == o.pack(); }
};

//==============================================================================
struct Options
{
    int keyTonic = 0;           // 0 = C
    bool minorKey = false;
    Level level = Level::Intermediate;
    Style style = Style::Auto;
    bool bassNote = true;
    bool extensions = true;
    bool alterations = false;
    bool voiceLeading = true;
    bool autoReharm = false;    // tritone-substitute dominant chords
    int registerShift = 0;      // octaves
};

/** Fully resolved chord: absolute root, effective quality, tensions chosen. */
struct ResolvedChord
{
    int rootPc = 0;
    Quality quality = Quality::Maj7;
    int bassPc = -1;             // slash bass pitch class, -1 none
    int tensions[5] {};          // intervals above root (13..23 range)
    int numTensions = 0;
    bool omitFifth = false;
    bool substituted = false;    // auto reharm applied
    int effectiveRootOffset = 0; // relative to key after reharm
    uint16_t scaleMask = 0;      // chord-scale, absolute pitch classes
    int scaleId = 0;             // see scaleName()
    bool hasTension (int interval) const noexcept
    {
        for (int i = 0; i < numTensions; ++i) if (tensions[i] == interval) return true;
        return false;
    }
};

struct Voicing
{
    int notes[12] {};
    int count = 0;
    int bass = -1;               // separate bass note (not part of voice leading)
    void add (int n) noexcept { if (count < 12) notes[count++] = n; }
    void sort() noexcept;
    float centre() const noexcept;
};

//==============================================================================
// Basic tables
int  qualityThird   (Quality q) noexcept;
int  qualityFifth   (Quality q) noexcept;
int  qualitySeventh (Quality q) noexcept;
bool isDominant     (Quality q) noexcept;
bool isMinorish     (Quality q) noexcept;   // minor 3rd chords incl. half-dim & dim

uint16_t keyScaleMask (int tonic, bool minor) noexcept;
uint16_t pcMask (int pc) noexcept;
bool     maskHas (uint16_t mask, int pc) noexcept;

//==============================================================================
/** Applies level/toggle rules to a spec. */
ResolvedChord resolve (const ChordSpec& spec, const Options& opt) noexcept;

/** Mask of chord tones (incl. chosen tensions), absolute pitch classes. */
uint16_t chordToneMask (const ResolvedChord& rc) noexcept;

/** Chord-scale for the right hand (absolute pitch classes). */
uint16_t chordScaleMask (const ResolvedChord& rc, const Options& opt) noexcept;

/** Mask used by the melody engine for a given melody mode. */
uint16_t melodyMask (MelodyMode mode, const ResolvedChord& rc, const Options& opt) noexcept;

//==============================================================================
/** Voice-leading aware voicing generator. `previous` may be empty. */
Voicing buildVoicing (const ChordSpec& spec, const Options& opt, const Voicing& previous) noexcept;

/** Same, from an already resolved chord. */
Voicing buildVoicing (const ResolvedChord& rc, const ChordSpec& spec, const Options& opt, const Voicing& previous) noexcept;

Style effectiveStyle (const ChordSpec& spec, const Options& opt, const ResolvedChord& rc) noexcept;

/** Voicing styles (quartal, upper-structure...) can add colour tones. This adds
    every voiced non-chord tone to the chord's tensions so naming, melody snapping
    and the display all describe what is actually sounding. */
void addVoicedTensions (ResolvedChord& rc, const Voicing& v) noexcept;

//==============================================================================
/** Melody: map an incoming note to an output note. */
int snapToMask (int note, uint16_t mask) noexcept;
int mapWhiteKey (int note, uint16_t mask, int tonic) noexcept;

/** Harmonize a melody note. Returns number of extra notes written to out. */
int harmonize (Harmonize h, int melodyNote, const ResolvedChord& rc, const Options& opt, int* out, int maxOut) noexcept;

//==============================================================================
/** Suggestions: score each pad as a "next chord" given the current one. */
void suggestNext (const ChordSpec& current, const ChordSpec* pads, int numPads,
                  const Options& opt, int currentPad, float* scoresOut) noexcept;

//==============================================================================
// Strings (message thread only)
juce::String noteName (int pc, int keyTonic, bool minorKey);
juce::String noteNameWithOctave (int midiNote, int keyTonic, bool minorKey);
juce::String chordName (const ResolvedChord& rc, const Options& opt);
juce::String chordName (const ChordSpec& spec, const Options& opt);
juce::String romanNumeral (const ResolvedChord& rc, const Options& opt);
juce::String functionDescription (const ResolvedChord& rc, const Options& opt);
juce::String intervalName (int semitonesAboveRoot);
juce::String qualityName (Quality q);
juce::String styleName (Style s);
juce::String levelName (Level l);
juce::String scaleName (const ResolvedChord& rc, const Options& opt, MelodyMode mode);
juce::String degreeName (int rootOffset, bool minorKey);
/** Correctly spelled chord tone (F# not Gb as the #11 of C7) relative to the chord root. */
juce::String chordToneName (const ResolvedChord& rc, int notePc, const Options& opt);
/** Scale notes spelled from the chord root (one letter per degree for 7-note scales). */
juce::String scaleNoteNames (const ResolvedChord& rc, uint16_t mask, const Options& opt);

juce::StringArray qualityNames();
juce::StringArray styleNames();
juce::StringArray levelNames();
juce::StringArray keyNames();
juce::StringArray melodyModeNames();
juce::StringArray harmonizeNames();

struct Modulation { int newTonic; bool newMinor; juce::String label; juce::String pivot; };
juce::Array<Modulation> modulationSuggestions (const Options& opt);

} // namespace theory
