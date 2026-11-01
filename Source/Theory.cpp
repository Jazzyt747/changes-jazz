#include "Theory.h"
#include <algorithm>
#include <cmath>

namespace theory
{
//==============================================================================
// Packing
uint32_t ChordSpec::pack() const noexcept
{
    uint32_t v = 0;
    v |= (uint32_t) (juce::jlimit (0, 11, rootOffset));
    v |= (uint32_t) (juce::jlimit (0, numQualities - 1, (int) quality)) << 4;
    v |= (uint32_t) (juce::jlimit (0, 7, variation)) << 9;
    v |= (uint32_t) (juce::jlimit (0, 15, style + 1)) << 12;
    v |= (uint32_t) (bassOffset < 0 ? 0 : juce::jlimit (1, 12, bassOffset + 1)) << 16;
    v |= (uint32_t) (juce::jlimit (1, 15, beats)) << 20;
    v |= (uint32_t) (valid ? 1 : 0) << 24;
    return v;
}

ChordSpec ChordSpec::unpack (uint32_t v) noexcept
{
    ChordSpec s;
    s.rootOffset = (int) (v & 0xF) % 12;
    s.quality    = (Quality) juce::jlimit (0, numQualities - 1, (int) ((v >> 4) & 0x1F));
    s.variation  = (int) ((v >> 9) & 0x7);
    s.style      = juce::jlimit (-1, numStyles - 1, (int) ((v >> 12) & 0xF) - 1);
    int b        = (int) ((v >> 16) & 0xF);
    s.bassOffset = b == 0 ? -1 : (b - 1) % 12;
    s.beats      = juce::jmax (1, (int) ((v >> 20) & 0xF));
    s.valid      = ((v >> 24) & 1) != 0;
    return s;
}

//==============================================================================
void Voicing::sort() noexcept { std::sort (notes, notes + count); }

float Voicing::centre() const noexcept
{
    if (count == 0) return 0.0f;
    float s = 0; for (int i = 0; i < count; ++i) s += (float) notes[i];
    return s / (float) count;
}

//==============================================================================
static inline int pcOf (int n) noexcept { return ((n % 12) + 12) % 12; }

uint16_t pcMask (int pc) noexcept { return (uint16_t) (1u << pcOf (pc)); }
bool maskHas (uint16_t mask, int pc) noexcept { return (mask & pcMask (pc)) != 0; }

static uint16_t maskFromIntervals (int root, std::initializer_list<int> iv) noexcept
{
    uint16_t m = 0; for (int i : iv) m |= pcMask (root + i); return m;
}

//==============================================================================
// Scales
enum ScaleId
{
    scIonian = 0, scDorian, scPhrygian, scLydian, scMixolydian, scAeolian, scLocrian,
    scLocrianNat2, scMelodicMinor, scLydianDominant, scAltered, scPhrygianDominant,
    scHalfWholeDim, scWholeHalfDim, scLydianAugmented, scNum
};

static const char* scaleNames[scNum] = {
    "Ionian (Major)", "Dorian", "Phrygian", "Lydian", "Mixolydian", "Aeolian", "Locrian",
    "Locrian #2", "Melodic Minor", "Lydian Dominant", "Altered", "Phrygian Dominant",
    "Half-Whole Diminished", "Whole-Half Diminished", "Lydian Augmented"
};

static uint16_t scaleMaskFor (int id, int root) noexcept
{
    switch (id)
    {
        case scIonian:           return maskFromIntervals (root, { 0, 2, 4, 5, 7, 9, 11 });
        case scDorian:           return maskFromIntervals (root, { 0, 2, 3, 5, 7, 9, 10 });
        case scPhrygian:         return maskFromIntervals (root, { 0, 1, 3, 5, 7, 8, 10 });
        case scLydian:           return maskFromIntervals (root, { 0, 2, 4, 6, 7, 9, 11 });
        case scMixolydian:       return maskFromIntervals (root, { 0, 2, 4, 5, 7, 9, 10 });
        case scAeolian:          return maskFromIntervals (root, { 0, 2, 3, 5, 7, 8, 10 });
        case scLocrian:          return maskFromIntervals (root, { 0, 1, 3, 5, 6, 8, 10 });
        case scLocrianNat2:      return maskFromIntervals (root, { 0, 2, 3, 5, 6, 8, 10 });
        case scMelodicMinor:     return maskFromIntervals (root, { 0, 2, 3, 5, 7, 9, 11 });
        case scLydianDominant:   return maskFromIntervals (root, { 0, 2, 4, 6, 7, 9, 10 });
        case scAltered:          return maskFromIntervals (root, { 0, 1, 3, 4, 6, 8, 10 });
        case scPhrygianDominant: return maskFromIntervals (root, { 0, 1, 4, 5, 7, 8, 10 });
        case scHalfWholeDim:     return maskFromIntervals (root, { 0, 1, 3, 4, 6, 7, 9, 10 });
        case scWholeHalfDim:     return maskFromIntervals (root, { 0, 2, 3, 5, 6, 8, 9, 11 });
        case scLydianAugmented:  return maskFromIntervals (root, { 0, 2, 4, 6, 8, 9, 11 });
        default:                 return maskFromIntervals (root, { 0, 2, 4, 5, 7, 9, 11 });
    }
}

uint16_t keyScaleMask (int tonic, bool minor) noexcept
{
    return scaleMaskFor (minor ? scAeolian : scIonian, tonic);
}

//==============================================================================
// Quality tables
int qualityThird (Quality q) noexcept
{
    switch (q)
    {
        case Quality::Dom7Sus: return 5;
        case Quality::Min7: case Quality::Min6: case Quality::MinMaj7:
        case Quality::Min7b5: case Quality::Dim7: return 3;
        default: return 4;
    }
}

int qualityFifth (Quality q) noexcept
{
    switch (q)
    {
        case Quality::Min7b5: case Quality::Dim7: return 6;
        case Quality::Maj7Sharp5: return 8;
        default: return 7;
    }
}

int qualitySeventh (Quality q) noexcept
{
    switch (q)
    {
        case Quality::Maj7: case Quality::Maj7Sharp11: case Quality::Maj7Sharp5: case Quality::MinMaj7: return 11;
        case Quality::Maj6: case Quality::Min6: case Quality::Dim7: return 9;
        default: return 10;
    }
}

bool isDominant (Quality q) noexcept
{
    return q == Quality::Dom7 || q == Quality::Dom7Sus || q == Quality::Dom7Alt
        || q == Quality::Dom7b9 || q == Quality::Dom7Lyd;
}

bool isMinorish (Quality q) noexcept
{
    return q == Quality::Min7 || q == Quality::Min6 || q == Quality::MinMaj7
        || q == Quality::Min7b5 || q == Quality::Dim7;
}

static uint16_t coreMask (int root, Quality q) noexcept
{
    return (uint16_t) (pcMask (root) | pcMask (root + qualityThird (q))
                     | pcMask (root + qualityFifth (q)) | pcMask (root + qualitySeventh (q)));
}

static bool targetIsMinor (int rootOffset, bool minorKey) noexcept
{
    const int t = pcOf (rootOffset + 5);
    if (minorKey) return t == 0 || t == 2 || t == 5 || t == 7;
    return t == 2 || t == 4 || t == 9 || t == 11;
}

/** Tension candidates (intervals above root, in priority order) per quality & level. */
static int tensionCandidates (Quality q, Level lv, bool targetMinor, int* out) noexcept
{
    int n = 0;
    auto add = [&] (std::initializer_list<int> l) { for (int t : l) out[n++] = t; };
    const int L = (int) lv;

    switch (q)
    {
        case Quality::Maj7:        if (L == 1) add ({ 14 }); else if (L == 2) add ({ 14, 21 }); else if (L == 3) add ({ 14, 21, 18 }); break;
        case Quality::Maj6:        if (L >= 1) add ({ 14 }); break;
        case Quality::Maj7Sharp11: if (L == 0) add ({ 18 }); else if (L == 1) add ({ 18, 14 }); else add ({ 18, 14, 21 }); break;
        case Quality::Maj7Sharp5:  if (L == 1) add ({ 14 }); else if (L >= 2) add ({ 14, 18 }); break;
        case Quality::Dom7:        if (L == 1) add ({ 14 }); else if (L >= 2) add ({ 14, 21 }); break;
        case Quality::Dom7Sus:     if (L == 1) add ({ 14 }); else if (L >= 2) add ({ 14, 21 }); break;
        case Quality::Dom7Alt:     if (L == 0) add ({ 15 }); else if (L == 1) add ({ 15, 20 }); else if (L == 2) add ({ 15, 20, 13 }); else add ({ 15, 20, 13, 18 }); break;
        case Quality::Dom7b9:
            if (L == 0 || L == 1) add ({ 13 });
            else if (targetMinor) add ({ 13, 20 });
            else add ({ 13, 21 });
            break;
        case Quality::Dom7Lyd:     if (L == 1) add ({ 18, 14 }); else if (L >= 2) add ({ 18, 21, 14 }); break;
        case Quality::Min7:        if (L == 1) add ({ 14, 17 }); else if (L == 2) add ({ 14, 17 }); else if (L == 3) add ({ 14, 17, 21 }); break;
        case Quality::Min6:        if (L == 1) add ({ 14 }); else if (L >= 2) add ({ 14, 17 }); break;
        case Quality::MinMaj7:     if (L == 1) add ({ 14 }); else if (L >= 2) add ({ 14, 21 }); break;
        case Quality::Min7b5:      if (L == 1) add ({ 17 }); else if (L == 2) add ({ 17, 20 }); else if (L == 3) add ({ 17, 14, 20 }); break;
        case Quality::Dim7:        if (L >= 2) add ({ 14 }); if (L == 3) add ({ 23 }); break;
        default: break;
    }
    return n;
}

/** Maximum number of tensions actually used per level (after filtering). */
static int maxTensionsFor (Level lv, Quality q) noexcept
{
    switch (lv)
    {
        case Level::Beginner:     return (q == Quality::Dom7Alt || q == Quality::Dom7b9 || q == Quality::Maj7Sharp11) ? 1 : 0;
        case Level::Intermediate: return (q == Quality::Min7) ? 1 : 2;
        case Level::Advanced:     return q == Quality::Dom7Alt ? 3 : 2;
        default:                  return 3;
    }
}

static bool alterationIsDefining (Quality q, int t) noexcept
{
    return (q == Quality::Dom7Alt && t == 15) || (q == Quality::Dom7b9 && t == 13) || (q == Quality::Maj7Sharp11 && t == 18);
}

//==============================================================================
ResolvedChord resolve (const ChordSpec& spec, const Options& opt) noexcept
{
    ResolvedChord rc;
    int rootOff = pcOf (spec.rootOffset);
    Quality q = spec.quality;

    // Auto reharm: tritone substitution of dominant chords
    if (opt.autoReharm && isDominant (q) && q != Quality::Dom7Sus)
    {
        rootOff = pcOf (rootOff + 6);
        q = Quality::Dom7Lyd;
        rc.substituted = true;
    }

    const bool tMinor = targetIsMinor (rc.substituted ? pcOf (rootOff - 6) : rootOff, opt.minorKey);

    // Alterations toggle: colour plain dominants
    if (opt.alterations && q == Quality::Dom7)
    {
        if (opt.level == Level::Advanced)          q = Quality::Dom7b9;
        else if (opt.level == Level::Conservatory) q = tMinor ? Quality::Dom7b9 : Quality::Dom7Alt;
        else if (opt.level == Level::Intermediate) q = tMinor ? Quality::Dom7b9 : Quality::Dom7;
    }

    // Beginner simplification
    if (opt.level == Level::Beginner && q == Quality::Dom7Lyd) q = Quality::Dom7;

    rc.rootPc = pcOf (opt.keyTonic + rootOff);
    rc.quality = q;
    rc.effectiveRootOffset = rootOff;
    rc.bassPc = spec.bassOffset >= 0 ? pcOf (opt.keyTonic + spec.bassOffset) : -1;
    if (rc.bassPc == rc.rootPc) rc.bassPc = -1;

    // ---- chord scale
    const uint16_t keyMask = keyScaleMask (opt.keyTonic, opt.minorKey);
    const uint16_t core = coreMask (rc.rootPc, q);
    int sc = scIonian;
    const bool diatonic = (core & keyMask) == core;

    if (diatonic && q != Quality::Dom7b9 && q != Quality::Dom7Alt && q != Quality::Dim7)
    {
        // mode of the key scale starting on the chord root
        static const int majModes[12] = { scIonian, -1, scDorian, -1, scPhrygian, scLydian, -1, scMixolydian, -1, scAeolian, -1, scLocrian };
        static const int minModes[12] = { scAeolian, -1, scLocrian, scIonian, -1, scDorian, -1, scPhrygian, scLydian, -1, scMixolydian, -1 };
        const int deg = pcOf (rc.rootPc - opt.keyTonic);
        sc = opt.minorKey ? minModes[deg] : majModes[deg];
        if (sc < 0) sc = scIonian;
    }
    else
    {
        switch (q)
        {
            case Quality::Maj7: case Quality::Maj6: sc = scIonian; break;
            case Quality::Maj7Sharp11: sc = scLydian; break;
            case Quality::Maj7Sharp5:  sc = scLydianAugmented; break;
            case Quality::Dom7: case Quality::Dom7Sus: sc = scMixolydian; break;
            case Quality::Dom7Alt: sc = scAltered; break;
            case Quality::Dom7b9: sc = (opt.level >= Level::Advanced && ! tMinor) ? scHalfWholeDim : scPhrygianDominant; break;
            case Quality::Dom7Lyd: sc = scLydianDominant; break;
            case Quality::Min7: sc = scDorian; break;
            case Quality::Min6: case Quality::MinMaj7: sc = scMelodicMinor; break;
            case Quality::Min7b5: sc = scLocrian; break;
            case Quality::Dim7: sc = scWholeHalfDim; break;
            default: break;
        }
    }

    if (q == Quality::Min7b5 && opt.level == Level::Conservatory) sc = scLocrianNat2;
    if (q == Quality::MinMaj7 || q == Quality::Min6) sc = scMelodicMinor;
    if (q == Quality::Maj7Sharp11) sc = scLydian;

    rc.scaleId = sc;
    rc.scaleMask = scaleMaskFor (sc, rc.rootPc);

    // ---- tensions
    int cands[8]; const int nc = tensionCandidates (q, opt.level, tMinor, cands);
    const int maxT = maxTensionsFor (opt.level, q);
    for (int i = 0; i < nc && rc.numTensions < juce::jmin (5, maxT); ++i)
    {
        const int t = cands[i];
        const bool defining = alterationIsDefining (q, t);
        if (! opt.extensions && ! defining) continue;
        if (! maskHas (rc.scaleMask, rc.rootPc + t)) continue;   // avoid notes are filtered out
        if (rc.hasTension (t)) continue;
        rc.tensions[rc.numTensions++] = t;
    }

    rc.omitFifth = (q == Quality::Dom7Alt) || rc.hasTension (20) || (q == Quality::Dom7Lyd && rc.hasTension (18) && opt.level >= Level::Advanced);
    return rc;
}

uint16_t chordToneMask (const ResolvedChord& rc) noexcept
{
    uint16_t m = (uint16_t) (pcMask (rc.rootPc) | pcMask (rc.rootPc + qualityThird (rc.quality)) | pcMask (rc.rootPc + qualitySeventh (rc.quality)));
    if (! rc.omitFifth) m |= pcMask (rc.rootPc + qualityFifth (rc.quality));
    for (int i = 0; i < rc.numTensions; ++i) m |= pcMask (rc.rootPc + rc.tensions[i]);
    return m;
}

uint16_t chordScaleMask (const ResolvedChord& rc, const Options&) noexcept { return rc.scaleMask; }

uint16_t melodyMask (MelodyMode mode, const ResolvedChord& rc, const Options& opt) noexcept
{
    const int r = rc.rootPc;
    switch (mode)
    {
        case MelodyMode::Key:        return keyScaleMask (opt.keyTonic, opt.minorKey);
        case MelodyMode::ChordScale: return rc.scaleMask;
        case MelodyMode::ChordTones: return chordToneMask (rc);
        case MelodyMode::Pentatonic:
            switch (rc.quality)
            {
                case Quality::Min7: case Quality::Min6: case Quality::MinMaj7: return maskFromIntervals (r, { 0, 3, 5, 7, 10 });
                case Quality::Min7b5: return maskFromIntervals (r, { 0, 3, 5, 6, 10 });
                case Quality::Dim7:   return maskFromIntervals (r, { 0, 2, 3, 6, 9 });
                case Quality::Dom7Alt: return maskFromIntervals (r, { 1, 3, 6, 8, 10 });
                case Quality::Dom7b9: return maskFromIntervals (r, { 0, 1, 4, 7, 10 });
                case Quality::Dom7Sus: return maskFromIntervals (r, { 0, 2, 5, 7, 10 });
                case Quality::Maj7Sharp11: case Quality::Dom7Lyd: return maskFromIntervals (r, { 2, 4, 6, 9, 11 }) | (rc.quality == Quality::Dom7Lyd ? 0 : 0);
                default: return maskFromIntervals (r, { 0, 2, 4, 7, 9 });
            }
        case MelodyMode::Blues: return maskFromIntervals (opt.keyTonic, { 0, 3, 5, 6, 7, 10 });
        case MelodyMode::Bebop:
        {
            uint16_t m = rc.scaleMask;
            if (rc.quality == Quality::Dom7 || rc.quality == Quality::Dom7Sus) m |= pcMask (r + 11);
            else if (rc.quality == Quality::Maj7 || rc.quality == Quality::Maj6) m |= pcMask (r + 8);
            else if (rc.quality == Quality::Min7) m |= pcMask (r + 4);
            return m;
        }
        default: return rc.scaleMask;
    }
}

//==============================================================================
// Voicing generation
struct Shape { int iv[10]; int n = 0; void add (int x) { if (n < 10) iv[n++] = x; } };

static void sortShape (Shape& s) { std::sort (s.iv, s.iv + s.n); }

/** Arrange a set of pitch-class intervals in close position starting from `startIv`. */
static Shape closeArrangement (const int* ivs, int n, int startIndex)
{
    Shape s;
    int prev = -100;
    for (int k = 0; k < n; ++k)
    {
        int x = ivs[(startIndex + k) % n] % 12;
        if (k == 0) x = ivs[startIndex] % 12;
        while (x <= prev) x += 12;
        s.add (x); prev = x;
    }
    return s;
}

Style effectiveStyle (const ChordSpec& spec, const Options& opt, const ResolvedChord& rc) noexcept
{
    Style st = spec.style >= 0 ? (Style) spec.style : opt.style;
    if (st != Style::Auto) return st;

    switch (opt.level)
    {
        case Level::Beginner:     return Style::Close;
        case Level::Intermediate: return Style::Rootless;
        case Level::Advanced:
            if (rc.quality == Quality::Min7 && (rc.scaleId == scDorian)) return Style::Quartal;
            return Style::Rootless;
        case Level::Conservatory:
            if (isDominant (rc.quality) && rc.quality != Quality::Dom7Sus) return Style::UpperStructure;
            if (rc.quality == Quality::Dom7Sus) return Style::UpperStructure;
            if (rc.quality == Quality::Min7 && rc.scaleId == scDorian) return Style::Quartal;
            if (rc.quality == Quality::Maj7 || rc.quality == Quality::Maj7Sharp11) return Style::Drop2;
            return Style::Rootless;
        default: return Style::Rootless;
    }
}

/** The 4 tones used by rootless / drop-2 voicings, as intervals 0..11. */
static int rootlessSet (const ResolvedChord& rc, bool includeRootIfShort, int* out)
{
    const Quality q = rc.quality;
    const int t3 = qualityThird (q), t5 = qualityFifth (q), t7 = qualitySeventh (q);
    int n = 0;
    out[n++] = t3; out[n++] = t7;

    const bool keepFifth = (q == Quality::Min7b5 || q == Quality::Dim7 || q == Quality::Maj7Sharp5);
    if (keepFifth) out[n++] = t5;

    for (int i = 0; i < rc.numTensions && n < 4; ++i)
    {
        const int t = rc.tensions[i] % 12;
        bool dup = false; for (int k = 0; k < n; ++k) if (out[k] == t) dup = true;
        if (! dup) out[n++] = t;
    }
    if (n < 4 && ! keepFifth && ! rc.omitFifth) out[n++] = t5;
    if (n < 4 && includeRootIfShort) out[n++] = 0;
    if (n < 4 && rc.omitFifth) out[n++] = t5;  // last resort
    return n;
}

static int makeShapes (Style st, const ResolvedChord& rc, const Options& opt, Shape* shapes, bool& includesRootLow)
{
    const Quality q = rc.quality;
    const int t3 = qualityThird (q), t5 = qualityFifth (q), t7 = qualitySeventh (q);
    int ns = 0;
    includesRootLow = false;

    auto fallbackRootless = [&]() -> int
    {
        int set[6]; const int n = rootlessSet (rc, true, set);
        // A form starts on the 3rd (index 0), B form on the 7th (index 1)
        int sorted[6]; std::copy (set, set + n, sorted); std::sort (sorted, sorted + n);
        int count = 0;
        for (int start = 0; start < n; ++start)
        {
            const int pcStart = sorted[start];
            if (pcStart == t3 || pcStart == t7)
                shapes[count++] = closeArrangement (sorted, n, start);
        }
        if (count == 0) shapes[count++] = closeArrangement (sorted, n, 0);
        return count;
    };

    switch (st)
    {
        case Style::Close:
        {
            int set[8]; int n = 0;
            const bool beginner = opt.level == Level::Beginner;
            if (beginner || ! opt.bassNote) set[n++] = 0;
            set[n++] = t3;
            if (! rc.omitFifth) set[n++] = t5;
            set[n++] = t7;
            for (int i = 0; i < rc.numTensions && n < (beginner ? 5 : 5); ++i)
            {
                const int t = rc.tensions[i] % 12; bool dup = false;
                for (int k = 0; k < n; ++k) if (set[k] == t) dup = true;
                if (! dup) set[n++] = t;
            }
            std::sort (set, set + n);
            for (int start = 0; start < n; ++start) shapes[ns++] = closeArrangement (set, n, start);
            includesRootLow = false;
            break;
        }
        case Style::Shell:
        {
            Shape a; a.add (0); a.add (t3); a.add (t7);
            Shape b; b.add (0); b.add (t7); b.add (t3 + 12);
            if (rc.numTensions > 0 && opt.level >= Level::Advanced)
            {
                const int top = rc.tensions[0];
                int x = top; while (x <= a.iv[a.n - 1]) x += 12; a.add (x);
                x = top; while (x <= b.iv[b.n - 1]) x += 12; b.add (x);
            }
            shapes[ns++] = a; shapes[ns++] = b;
            includesRootLow = true;
            break;
        }
        case Style::Rootless:
            ns = fallbackRootless();
            break;
        case Style::Drop2:
        {
            int set[6]; const int n = rootlessSet (rc, true, set);
            int sorted[6]; std::copy (set, set + n, sorted); std::sort (sorted, sorted + n);
            for (int start = 0; start < n; ++start)
            {
                Shape c = closeArrangement (sorted, n, start);
                if (c.n >= 4) { c.iv[c.n - 2] -= 12; sortShape (c); }
                shapes[ns++] = c;
            }
            break;
        }
        case Style::Quartal:
        {
            Shape s;
            switch (q)
            {
                case Quality::Min7:
                    for (int x : { 0, 5, 10, 15, 19 }) s.add (x); includesRootLow = true; break;   // "So What"
                case Quality::Maj7:
                    for (int x : { 4, 9, 14, 19, 23 }) s.add (x); break;
                case Quality::Maj6:
                    for (int x : { 4, 9, 14, 19 }) s.add (x); break;
                case Quality::Maj7Sharp11:
                    for (int x : { 11, 16, 21, 26, 30 }) s.add (x); break;
                case Quality::Dom7: case Quality::Dom7Lyd:
                    for (int x : { 10, 16, 21, 26 }) s.add (x);
                    if (q == Quality::Dom7Lyd) s.add (30);
                    break;
                case Quality::Dom7Alt:
                    for (int x : { 10, 15, 20, 25 }) s.add (x); break;
                case Quality::Dom7Sus:
                    for (int x : { 7, 12, 17, 22, 26 }) s.add (x); break;
                default:
                    return fallbackRootless();
            }
            shapes[ns++] = s;
            // variant without the top note
            Shape s2 = s; if (s2.n > 4) { s2.n--; shapes[ns++] = s2; }
            break;
        }
        case Style::UpperStructure:
        {
            int triad = 7;
            switch (q)
            {
                case Quality::Dom7: case Quality::Dom7Lyd: triad = 2; break;
                case Quality::Dom7Alt: triad = 8; break;
                case Quality::Dom7b9: triad = 9; break;
                case Quality::Dom7Sus: triad = 10; break;
                case Quality::Maj7: case Quality::Maj6: triad = 7; break;
                case Quality::Maj7Sharp11: case Quality::Maj7Sharp5: triad = 2; break;
                case Quality::Min7:
                    triad = rc.scaleId == scDorian ? 5 : (rc.scaleId == scAeolian ? 10 : 3); break;
                case Quality::Min6: triad = 5; break;
                case Quality::MinMaj7: triad = 7; break;
                case Quality::Min7b5: triad = maskHas (rc.scaleMask, rc.rootPc + 2) ? 10 : 8; break;
                case Quality::Dim7: triad = 11; break;
                default: break;
            }
            bool triadMinor = false;
            if (q == Quality::Dom7b9 && ! maskHas (rc.scaleMask, rc.rootPc + 9)) { triad = 1; triadMinor = true; }  // bII minor over phrygian dominant
            const int triadIv[3] = { triad, triad + (triadMinor ? 3 : 4), triad + 7 };
            for (int low = 0; low < 2; ++low)
            {
                Shape base;
                if (q == Quality::Dom7Sus) { base.add (low == 0 ? 7 : 0); base.add (low == 0 ? 10 : 7); }
                else if (low == 0) { base.add (t3); base.add (t7); }
                else { base.add (t7 - 12 + (t7 < t3 ? 12 : 0)); base.add (t3); if (base.iv[0] >= base.iv[1]) base.iv[0] -= 12; }
                sortShape (base);
                for (int inv = 0; inv < 3; ++inv)
                {
                    Shape s = base;
                    int prev = s.iv[s.n - 1];
                    for (int k = 0; k < 3; ++k)
                    {
                        int x = triadIv[(inv + k) % 3] % 12;
                        while (x <= prev) x += 12;
                        s.add (x); prev = x;
                    }
                    shapes[ns++] = s;
                }
            }
            break;
        }
        case Style::Spread:
        {
            Shape s; s.add (0); s.add (t7); s.add (t3 + 12);
            int prev = t3 + 12;
            int added = 0;
            for (int i = 0; i < rc.numTensions && added < 2; ++i)
            {
                int x = rc.tensions[i]; while (x <= prev) x += 12; s.add (x); prev = x; ++added;
            }
            if (added == 0 && ! rc.omitFifth) { int x = t5; while (x <= prev) x += 12; s.add (x); }
            shapes[ns++] = s;
            Shape s2; s2.add (0); s2.add (t3 + 12); s2.add (t7 + 12);   // 10ths spread
            prev = t7 + 12;
            for (int i = 0; i < rc.numTensions && i < 2; ++i) { int x = rc.tensions[i]; while (x <= prev) x += 12; s2.add (x); prev = x; }
            shapes[ns++] = s2;
            includesRootLow = true;
            break;
        }
        default:
            ns = fallbackRootless();
            break;
    }
    return ns;
}

struct RegisterRule { float centre; int low; int high; };

static RegisterRule registerFor (Style st)
{
    switch (st)
    {
        case Style::Close:          return { 62.0f, 50, 77 };
        case Style::Shell:          return { 52.0f, 38, 70 };
        case Style::Rootless:       return { 59.0f, 47, 74 };
        case Style::Drop2:          return { 60.0f, 45, 78 };
        case Style::Quartal:        return { 61.0f, 45, 82 };
        case Style::UpperStructure: return { 63.0f, 48, 84 };
        case Style::Spread:         return { 56.0f, 34, 84 };
        default:                    return { 60.0f, 46, 78 };
    }
}

static float voiceLeadingDistance (const int* a, int na, const Voicing& prev)
{
    if (prev.count == 0) return 0.0f;
    float d = 0;
    if (na == prev.count)
    {
        for (int i = 0; i < na; ++i) d += (float) std::abs (a[i] - prev.notes[i]);
        return d;
    }
    for (int i = 0; i < na; ++i)
    {
        int best = 1000; for (int k = 0; k < prev.count; ++k) best = std::min (best, std::abs (a[i] - prev.notes[k]));
        d += (float) best;
    }
    for (int k = 0; k < prev.count; ++k)
    {
        int best = 1000; for (int i = 0; i < na; ++i) best = std::min (best, std::abs (a[i] - prev.notes[k]));
        d += (float) best;
    }
    return d * 0.5f;
}

Voicing buildVoicing (const ChordSpec& spec, const Options& opt, const Voicing& previous) noexcept
{
    const ResolvedChord rc = resolve (spec, opt);
    return buildVoicing (rc, spec, opt, previous);
}

Voicing buildVoicing (const ResolvedChord& rc, const ChordSpec& spec, const Options& opt, const Voicing& previous) noexcept
{
    const Style st = effectiveStyle (spec, opt, rc);
    Shape shapes[16]; bool rootLow = false;
    const int ns = makeShapes (st, rc, opt, shapes, rootLow);

    RegisterRule reg = registerFor (st);
    const int shift = juce::jlimit (-2, 2, opt.registerShift) * 12;
    reg.centre += (float) shift; reg.low += shift; reg.high += shift;

    struct Cand { int notes[10]; int n; float cost; float pen; };
    Cand cands[96]; int nCand = 0;

    for (int s = 0; s < ns && nCand < 96; ++s)
    {
        const Shape& sh = shapes[s];
        for (int oct = 1; oct <= 8 && nCand < 96; ++oct)
        {
            const int base = rc.rootPc + oct * 12;
            Cand c; c.n = sh.n;
            int lo = 999, hi = -999; float sum = 0;
            for (int i = 0; i < sh.n; ++i)
            {
                c.notes[i] = base + sh.iv[i];
                lo = std::min (lo, c.notes[i]); hi = std::max (hi, c.notes[i]); sum += (float) c.notes[i];
            }
            if (lo < 24 || hi > 108) continue;
            std::sort (c.notes, c.notes + c.n);
            float rangePen = 0;
            if (lo < reg.low)  rangePen += (float) (reg.low - lo);
            if (hi > reg.high) rangePen += (float) (hi - reg.high);
            if (rootLow && lo < reg.low - 4) rangePen += 20.0f;
            const float centreDist = std::abs (sum / (float) c.n - reg.centre);
            if (rangePen > 14.0f) continue;

            if (opt.voiceLeading && previous.count > 0)
                c.cost = voiceLeadingDistance (c.notes, c.n, previous) + 0.45f * centreDist + 6.0f * rangePen;
            else
                c.cost = centreDist + 6.0f * rangePen + (float) s * 0.01f;
            c.pen = rangePen;
            cands[nCand++] = c;
        }
    }

    Voicing v;
    if (nCand == 0)
    {
        // Should never happen: fall back to a plain close triad + 7th.
        const int base = rc.rootPc + 48;
        v.add (base); v.add (base + qualityThird (rc.quality)); v.add (base + qualityFifth (rc.quality)); v.add (base + qualitySeventh (rc.quality));
    }
    else
    {
        // Prefer candidates that sit fully inside the register when any exist
        bool anyInRange = false; for (int i = 0; i < nCand; ++i) anyInRange |= cands[i].pen == 0.0f;
        if (anyInRange)
        {
            int w = 0; for (int i = 0; i < nCand; ++i) if (cands[i].pen == 0.0f) cands[w++] = cands[i];
            nCand = w;
        }
        std::sort (cands, cands + nCand, [] (const Cand& a, const Cand& b) { return a.cost < b.cost; });
        // Variation picks the n-th best *distinct* candidate.
        int pick = 0, distinct = 0;
        for (int i = 0; i < nCand && distinct < spec.variation; ++i)
        {
            bool same = i > 0 && cands[i].n == cands[pick].n && std::equal (cands[i].notes, cands[i].notes + cands[i].n, cands[pick].notes);
            if (i > 0 && ! same) { pick = i; ++distinct; }
        }
        for (int i = 0; i < cands[pick].n; ++i) v.add (cands[pick].notes[i]);
    }
    v.sort();

    // Slash bass replaces the low root in root-included shapes
    if (rootLow && rc.bassPc >= 0 && v.count > 0)
    {
        int b = v.notes[0] + pcOf (rc.bassPc - v.notes[0]);
        if (b - v.notes[0] > 6) b -= 12;
        if (v.count > 1 && b >= v.notes[1]) b -= 12;
        if (b < 28 && (v.count < 2 || b + 12 < v.notes[1])) b += 12;
        v.notes[0] = b; v.sort();
    }

    // Separate bass note
    if (! rootLow && (opt.bassNote || rc.bassPc >= 0) && v.count > 0)
    {
        const int bpc = rc.bassPc >= 0 ? rc.bassPc : rc.rootPc;
        int b = 36 + shift + pcOf (bpc - (36 + shift));     // within [36+shift, 47+shift]
        if (b > 43 + shift) b -= 12;                        // keep it low & warm (E1..G2 range)
        while (b > v.notes[0] - 5) b -= 12;
        v.bass = b >= 24 ? b : -1;   // voicing already sits very low: leave the bass out
    }
    return v;
}

void addVoicedTensions (ResolvedChord& rc, const Voicing& v) noexcept
{
    const Quality q = rc.quality;
    const int t3 = qualityThird (q), t5 = qualityFifth (q), t7 = qualitySeventh (q);
    auto addT = [&] (int t) { if (! rc.hasTension (t) && rc.numTensions < 5) rc.tensions[rc.numTensions++] = t; };
    rc.numTensions = 0;   // describe exactly what is sounding
    rc.omitFifth = true;

    for (int i = 0; i < v.count; ++i)
    {
        const int iv = pcOf (v.notes[i] - rc.rootPc);
        if (iv == 0 || iv == t3 || iv == t7) continue;
        if (iv == t5) { rc.omitFifth = false; continue; }
        switch (iv)
        {
            case 1: addT (13); break;
            case 2: addT (14); break;
            case 3: if (t3 == 4) addT (15); break;
            case 4: if (t3 == 5) addT (16); break;     // a 3rd sounding over a sus chord (rare)
            case 5: addT (17); break;
            case 6: addT (18); break;
            case 7: rc.omitFifth = false; break;
            case 8: addT (20); break;
            case 9: addT (21); break;
            case 10: break;
            case 11: addT (23); break;
            default: break;
        }
    }
    std::sort (rc.tensions, rc.tensions + rc.numTensions);

    // If the voicing uses colour tones outside the chord-scale (e.g. an upper-structure
    // #11 over a mixolydian V7), move to the closest scale that contains them so the
    // right hand never clashes with the left.
    uint16_t voiced = 0;
    for (int i = 0; i < v.count; ++i) voiced |= pcMask (v.notes[i]);
    if ((voiced & ~rc.scaleMask) != 0)
    {
        static const int domC[]  = { scMixolydian, scLydianDominant, scHalfWholeDim, scAltered, scPhrygianDominant };
        static const int majC[]  = { scIonian, scLydian, scLydianAugmented };
        static const int minC[]  = { scDorian, scAeolian, scMelodicMinor, scPhrygian };
        static const int hdC[]   = { scLocrian, scLocrianNat2 };
        static const int dimC[]  = { scWholeHalfDim };
        const int* list = majC; int n = 3;
        if (isDominant (q)) { list = domC; n = 5; }
        else if (q == Quality::Min7b5) { list = hdC; n = 2; }
        else if (q == Quality::Dim7) { list = dimC; n = 1; }
        else if (isMinorish (q)) { list = minC; n = 4; }
        for (int i = 0; i < n; ++i)
        {
            const uint16_t m = scaleMaskFor (list[i], rc.rootPc);
            if ((voiced & ~m) == 0) { rc.scaleId = list[i]; rc.scaleMask = m; break; }
        }
    }
}

//==============================================================================
int snapToMask (int note, uint16_t mask) noexcept
{
    if (mask == 0) return note;
    if (maskHas (mask, note)) return note;
    for (int d = 1; d <= 6; ++d)
    {
        if (maskHas (mask, note + d)) return note + d;
        if (maskHas (mask, note - d)) return note - d;
    }
    return note;
}

int mapWhiteKey (int note, uint16_t mask, int tonic) noexcept
{
    static const int whiteIndex[12] = { 0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6 };
    const int pc = pcOf (note);
    const int wi = whiteIndex[pc];
    if (wi < 0 || mask == 0) return -1;

    int list[12]; int m = 0;
    for (int k = 0; k < 12; ++k) if (maskHas (mask, tonic + k)) list[m++] = k;
    if (m == 0) return -1;

    const int octave = note / 12;
    const int base = octave * 12 + (tonic >= 6 ? tonic - 12 : tonic);
    return base + list[wi % m] + 12 * (wi / m);
}

int harmonize (Harmonize h, int melodyNote, const ResolvedChord& rc, const Options&, int* out, int maxOut) noexcept
{
    int n = 0;
    switch (h)
    {
        case Harmonize::Octaves:
            if (n < maxOut) out[n++] = melodyNote - 12;
            break;
        case Harmonize::Thirds:
        {
            int steps = 0;
            for (int x = melodyNote - 1; x > melodyNote - 12 && n < maxOut; --x)
                if (maskHas (rc.scaleMask, x) && ++steps == 2) { out[n++] = x; break; }
            break;
        }
        case Harmonize::BlockChords:
        {
            const Quality q = rc.quality;
            int core[4] = { 0, qualityThird (q), qualityFifth (q), qualitySeventh (q) };
            int found = 0;
            for (int x = melodyNote - 1; x > melodyNote - 12 && found < 3 && n < maxOut; --x)
            {
                const int iv = pcOf (x - rc.rootPc);
                for (int c : core)
                    if (iv == c) { out[n++] = x; ++found; break; }
            }
            if (n < maxOut) out[n++] = melodyNote - 12;
            break;
        }
        default: break;
    }
    return n;
}

//==============================================================================
void suggestNext (const ChordSpec& current, const ChordSpec* pads, int numPads, const Options& opt, int currentPad, float* scores) noexcept
{
    const ResolvedChord cur = resolve (current, opt);
    const int r = cur.effectiveRootOffset;
    const Quality q = cur.quality;

    for (int i = 0; i < numPads; ++i)
    {
        scores[i] = 0.0f;
        if (i == currentPad || ! pads[i].valid) continue;
        const ResolvedChord p = resolve (pads[i], opt);
        const int pr = p.effectiveRootOffset;
        const int interval = pcOf (pr - r);
        float s = 0;

        if (isDominant (q))
        {
            if (interval == 5) s += 3.0f;                                   // V → I (down a fifth)
            if (cur.substituted && interval == 11) s += 3.0f;               // subV → I (down a half step)
            if (interval == 11 && q == Quality::Dom7Lyd) s += 2.5f;         // tritone sub resolution
            if (pcOf (r) == 7 && pr == 9 && isMinorish (p.quality)) s += 1.5f; // deceptive V → vi
            if (interval == 5 && isMinorish (p.quality) && (q == Quality::Dom7b9 || q == Quality::Dom7Alt)) s += 0.5f;
        }
        else if (q == Quality::Min7 || q == Quality::Min7b5)
        {
            if (interval == 5 && isDominant (p.quality)) s += 3.0f;         // ii → V
            if (interval == 11 && isDominant (p.quality)) s += 2.0f;        // ii → subV
            if (interval == 5 && ! isDominant (p.quality)) s += 0.8f;
        }
        else if (q == Quality::Dim7)
        {
            if (interval == 1) s += 2.5f;                                    // passing dim resolves up a half step
        }
        else // major / tonic-ish
        {
            if (pr == pcOf (2) && p.quality == Quality::Min7) s += 1.8f;    // I → ii
            if (pr == pcOf (9) && (isMinorish (p.quality) || isDominant (p.quality))) s += 1.6f;
            if (pr == pcOf (5)) s += 1.4f;                                    // → IV
            if (interval == 5 && isDominant (p.quality)) s += 1.2f;          // I → I7 → IV
            if (pr == 4 && isMinorish (p.quality)) s += 0.8f;
            if (p.quality == Quality::Dim7 && interval == 1) s += 1.0f;
        }

        if (interval == 5) s += 0.3f;                                          // root motion by fourth is always strong
        scores[i] = s;
    }
}

//==============================================================================
// Strings
static bool useFlats (int tonic, bool minor)
{
    if (minor) { const int m[] = { 2, 7, 0, 5, 10, 3, 8 }; for (int x : m) if (x == tonic) return true; return false; }
    const int f[] = { 0, 5, 10, 3, 8, 1, 6 }; for (int x : f) if (x == tonic) return true; return false;
}

juce::String noteName (int pc, int keyTonic, bool minorKey)
{
    static const char* sharps[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    static const char* flats[12]  = { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };
    return useFlats (keyTonic, minorKey) ? flats[pcOf (pc)] : sharps[pcOf (pc)];
}

/** Spell a chord root by its scale degree (bII = Bb in A minor, #IV = D# ...). */
static juce::String rootName (int pc, int keyTonic, bool minorKey)
{
    static const char* majKeys[12] = { "C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
    static const char* minKeys[12] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "G#", "A", "Bb", "B" };
    static const int naturalPc[7] = { 0, 2, 4, 5, 7, 9, 11 };
    static const int steps[12] = { 0, 1, 1, 2, 2, 3, 3, 4, 5, 5, 6, 6 };
    const juce::String tonicName = (minorKey ? minKeys : majKeys)[pcOf (keyTonic)];
    const int tonicLetter = juce::String ("CDEFGAB").indexOfChar (tonicName[0]);
    const int offset = pcOf (pc - keyTonic);
    const int letter = (tonicLetter + steps[offset]) % 7;
    int acc = pcOf (pc) - naturalPc[letter];
    if (acc > 6) acc -= 12; if (acc < -6) acc += 12;
    if (acc < -1 || acc > 1) return noteName (pc, keyTonic, minorKey);
    return juce::String::charToString ((juce::juce_wchar) "CDEFGAB"[letter]) + (acc < 0 ? "b" : (acc > 0 ? "#" : ""));
}

static juce::String spellWithLetter (int pc, int letter, int keyTonic, bool minorKey)
{
    static const int naturalPc[7] = { 0, 2, 4, 5, 7, 9, 11 };
    letter = ((letter % 7) + 7) % 7;
    int acc = pcOf (pc) - naturalPc[letter];
    if (acc > 6) acc -= 12; if (acc < -6) acc += 12;
    if (acc < -2 || acc > 2) return noteName (pc, keyTonic, minorKey);
    juce::String s = juce::String::charToString ((juce::juce_wchar) "CDEFGAB"[letter]);
    if (acc == -2) s << "bb"; else if (acc == -1) s << "b"; else if (acc == 1) s << "#"; else if (acc == 2) s << "x";
    return s;
}

static int letterIndexOf (const juce::String& name) { return juce::String ("CDEFGAB").indexOfChar (name[0]); }

juce::String chordToneName (const ResolvedChord& rc, int notePc, const Options& opt)
{
    const juce::String root = rootName (rc.rootPc, opt.keyTonic, opt.minorKey);
    const int rl = letterIndexOf (root);
    const int iv = pcOf (notePc - rc.rootPc);
    const Quality q = rc.quality;
    const int t3 = qualityThird (q), t5 = qualityFifth (q), t7 = qualitySeventh (q);
    int steps;
    if (iv == 0) steps = 0;
    else if (iv == t3) steps = (t3 == 5 ? 3 : 2);
    else if (iv == t5) steps = 4;
    else if (iv == t7) steps = (t7 == 9 && q != Quality::Dim7) ? 5 : 6;
    else switch (iv)
    {
        case 1: case 2: steps = 1; break;
        case 3: steps = (t3 == 4 ? 1 : 2); break;     // #9 over a major third
        case 4: steps = 2; break;
        case 5: case 6: steps = 3; break;             // 11 / #11
        case 7: steps = 4; break;
        case 8: case 9: steps = 5; break;             // b13 / 13
        case 10: case 11: steps = 6; break;
        default: steps = 0; break;
    }
    return spellWithLetter (notePc, rl + steps, opt.keyTonic, opt.minorKey);
}

juce::String scaleNoteNames (const ResolvedChord& rc, uint16_t mask, const Options& opt)
{
    int pcs[12]; int n = 0;
    for (int k = 0; k < 12; ++k) if (maskHas (mask, rc.rootPc + k)) pcs[n++] = pcOf (rc.rootPc + k);
    juce::String out;
    if (n == 7 && maskHas (mask, rc.rootPc))
    {
        const int rl = letterIndexOf (rootName (rc.rootPc, opt.keyTonic, opt.minorKey));
        for (int i = 0; i < n; ++i) out << spellWithLetter (pcs[i], rl + i, opt.keyTonic, opt.minorKey) << " ";
    }
    else for (int i = 0; i < n; ++i) out << chordToneName (rc, pcs[i], opt) << " ";
    return out.trim();
}

juce::String noteNameWithOctave (int midiNote, int keyTonic, bool minorKey)
{
    return noteName (midiNote, keyTonic, minorKey) + juce::String (midiNote / 12 - 1);
}

juce::String chordName (const ResolvedChord& rc, const Options& opt)
{
    auto has = [&] (int t) { return rc.hasTension (t); };
    juce::String s;
    switch (rc.quality)
    {
        case Quality::Maj7:
        case Quality::Maj7Sharp11:
            s = (has (14) && has (21)) ? "maj13" : has (14) ? "maj9" : has (21) ? "maj7(13)" : "maj7";
            if (has (18) || rc.quality == Quality::Maj7Sharp11) s << "#11";
            break;
        case Quality::Maj6:       s = has (14) ? "6/9" : "6"; break;
        case Quality::Maj7Sharp5: s = has (14) ? "maj9#5" : "maj7#5"; break;
        case Quality::Dom7:
            s = (has (14) && has (21)) ? "13" : has (21) ? "7(13)" : has (14) ? "9" : "7";
            if (has (13)) s << "b9";
            if (has (15)) s << "#9";
            if (has (18)) s << "#11";
            if (has (20)) s << "b13";
            break;
        case Quality::Dom7Sus:    s = has (21) ? "13sus4" : has (14) ? "9sus4" : "7sus4"; break;
        case Quality::Dom7Lyd:    s = has (21) ? "13#11" : has (14) ? "9#11" : (has (18) ? "7#11" : "7"); break;
        case Quality::Dom7Alt:
        {
            int alts = (has (13) ? 1 : 0) + (has (15) ? 1 : 0) + (has (18) ? 1 : 0) + (has (20) ? 1 : 0);
            if (alts >= 3 || (opt.level == Level::Conservatory && alts >= 2)) s = "7alt";
            else
            {
                s = "7";
                if (has (13)) s << "b9";
                if (has (15)) s << "#9";
                if (has (18)) s << "#11";
                if (has (20)) s << "b13";
                if (alts == 0) s = "7#5";
            }
            break;
        }
        case Quality::Dom7b9:     s = has (21) ? "13b9" : has (20) ? "7b9b13" : "7b9"; break;
        case Quality::Min7:       s = has (21) ? "m13" : has (17) ? (has (14) ? "m11" : "m7(11)") : has (14) ? "m9" : "m7"; break;
        case Quality::Min6:       s = has (14) ? "m6/9" : "m6"; break;
        case Quality::MinMaj7:    s = has (14) ? "m(maj9)" : "m(maj7)"; break;
        case Quality::Min7b5:     s = has (17) ? (has (14) ? "m11b5" : "m7b5(11)") : has (14) ? "m9b5" : "m7b5"; break;
        case Quality::Dim7:       s = has (23) ? "dim7(maj7)" : has (14) ? "dim7(9)" : "dim7"; break;
        default: break;
    }
    juce::String name = rootName (rc.rootPc, opt.keyTonic, opt.minorKey) + s;
    if (rc.bassPc >= 0) name << "/" << rootName (rc.bassPc, opt.keyTonic, opt.minorKey);
    return name;
}

juce::String chordName (const ChordSpec& spec, const Options& opt)
{
    return chordName (resolve (spec, opt), opt);
}

juce::String degreeName (int rootOffset, bool)
{
    static const char* deg[12] = { "I", "bII", "II", "bIII", "III", "IV", "#IV", "V", "bVI", "VI", "bVII", "VII" };
    return deg[pcOf (rootOffset)];
}

juce::String romanNumeral (const ResolvedChord& rc, const Options& opt)
{
    juce::String d = degreeName (rc.effectiveRootOffset, opt.minorKey);
    if (isMinorish (rc.quality)) d = d.toLowerCase();
    switch (rc.quality)
    {
        case Quality::Maj7: case Quality::Maj7Sharp11: case Quality::Maj7Sharp5: d << "maj7"; break;
        case Quality::Maj6: case Quality::Min6: d << "6"; break;
        case Quality::Dom7: case Quality::Dom7Lyd: case Quality::Dom7b9: d << "7"; break;
        case Quality::Dom7Sus: d << "7sus"; break;
        case Quality::Dom7Alt: d << "7alt"; break;
        case Quality::Min7: d << "7"; break;
        case Quality::MinMaj7: d << "(maj7)"; break;
        case Quality::Min7b5: d << "7b5"; break;
        case Quality::Dim7: d << "o7"; break;
        default: break;
    }
    if (rc.bassPc >= 0) d << "/" << degreeName (pcOf (rc.bassPc - opt.keyTonic), opt.minorKey);
    return d;
}

juce::String functionDescription (const ResolvedChord& rc, const Options& opt)
{
    const int r = rc.effectiveRootOffset;
    const Quality q = rc.quality;
    const uint16_t keyMask = keyScaleMask (opt.keyTonic, opt.minorKey);
    const bool diatonic = (coreMask (rc.rootPc, q) & keyMask) == coreMask (rc.rootPc, q);

    if (rc.substituted) return "Tritone substitution - resolves down a half step";
    if (q == Quality::Dim7) return "Passing diminished - resolves up a half step";

    if (isDominant (q))
    {
        const int target = pcOf (r + 5);
        if (r == 7) return opt.minorKey ? "Dominant (V) - resolves to i" : "Dominant (V) - resolves to I";
        if (r == 1) return "Tritone sub of V (subV) - resolves to I";
        if (r == 10 && ! opt.minorKey) return "Backdoor dominant (bVII7) - resolves to I";
        if (r == 0) return "Tonic dominant (I7) - leads to IV";
        if (maskHas (keyMask, opt.keyTonic + target))
            return "Secondary dominant V7/" + degreeName (target, opt.minorKey) + " - resolves down a fifth";
        return "Chromatic dominant";
    }

    if (! diatonic) return "Modal interchange / borrowed colour";

    if (! opt.minorKey)
    {
        switch (r)
        {
            case 0: return "Tonic (I) - home";
            case 2: return "Pre-dominant (ii) - sets up V";
            case 4: return "Tonic substitute (iii)";
            case 5: return "Subdominant (IV)";
            case 9: return "Tonic substitute (vi) - relative minor";
            case 11: return "Leading-tone half-diminished (vii) - dominant function";
            default: break;
        }
    }
    else
    {
        switch (r)
        {
            case 0: return "Minor tonic (i) - home";
            case 2: return "Pre-dominant (ii half-dim) - sets up V";
            case 3: return "Relative major (bIII)";
            case 5: return "Subdominant minor (iv)";
            case 8: return "Submediant (bVI)";
            case 10: return "Subtonic (bVII)";
            default: break;
        }
    }
    return "Diatonic colour chord";
}

juce::String intervalName (int iv)
{
    switch (iv)
    {
        case 0: return "R";   case 1: return "b9";  case 2: return "9";   case 3: return "b3";
        case 4: return "3";   case 5: return "11";  case 6: return "b5";  case 7: return "5";
        case 8: return "#5";  case 9: return "6";   case 10: return "b7"; case 11: return "7";
        case 13: return "b9"; case 14: return "9";  case 15: return "#9"; case 17: return "11";
        case 18: return "#11"; case 20: return "b13"; case 21: return "13"; case 23: return "maj7";
        default: return juce::String (iv);
    }
}

juce::StringArray qualityNames()
{
    return { "maj7", "6", "maj7#11", "maj7#5", "7 (dominant)", "7sus4", "7alt", "7b9", "7#11 (lydian dom)",
             "m7", "m6", "m(maj7)", "m7b5 (half-dim)", "dim7" };
}

juce::String qualityName (Quality q) { return qualityNames()[(int) q]; }

juce::StringArray styleNames()
{
    return { "Auto (by level)", "Close position", "Shell (Bud Powell)", "Rootless A/B (Bill Evans)",
             "Drop 2", "Quartal / So What", "Upper-structure triads", "Spread (two-hand)" };
}

juce::String styleName (Style s) { return styleNames()[(int) s]; }

juce::StringArray levelNames() { return { "Beginner", "Intermediate", "Advanced", "Conservatory" }; }
juce::String levelName (Level l) { return levelNames()[(int) l]; }

juce::StringArray keyNames() { return { "C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" }; }

juce::StringArray melodyModeNames() { return { "Key scale", "Chord scale", "Chord tones", "Pentatonic", "Blues", "Bebop" }; }
juce::StringArray harmonizeNames()  { return { "Off", "Octaves", "Diatonic 3rds", "Block chords (Shearing)" }; }

juce::String scaleName (const ResolvedChord& rc, const Options& opt, MelodyMode mode)
{
    const juce::String root = rootName (rc.rootPc, opt.keyTonic, opt.minorKey);
    const juce::String key  = rootName (opt.keyTonic, opt.keyTonic, opt.minorKey);
    switch (mode)
    {
        case MelodyMode::Key:        return key + (opt.minorKey ? " Natural Minor" : " Major");
        case MelodyMode::ChordScale: return root + " " + scaleNames[juce::jlimit (0, scNum - 1, rc.scaleId)];
        case MelodyMode::ChordTones: return chordName (rc, opt) + " chord tones";
        case MelodyMode::Pentatonic:
            if (rc.quality == Quality::Dom7Alt) return noteName (rc.rootPc + 6, opt.keyTonic, opt.minorKey) + " Major Pentatonic (alt colour)";
            return root + (isMinorish (rc.quality) ? " Minor Pentatonic" : " Major Pentatonic");
        case MelodyMode::Blues:      return key + " Blues";
        case MelodyMode::Bebop:
            if (rc.quality == Quality::Dom7 || rc.quality == Quality::Dom7Sus) return root + " Bebop Dominant";
            if (rc.quality == Quality::Maj7 || rc.quality == Quality::Maj6) return root + " Bebop Major";
            if (rc.quality == Quality::Min7) return root + " Bebop Dorian";
            return root + " " + scaleNames[juce::jlimit (0, scNum - 1, rc.scaleId)];
        default: break;
    }
    return {};
}

juce::Array<Modulation> modulationSuggestions (const Options& opt)
{
    juce::Array<Modulation> m;
    const int k = opt.keyTonic;
    auto nm = [&] (int pc, bool minor) { return noteName (pcOf (pc), pcOf (pc), minor); };
    auto add = [&] (int t, bool minor, const juce::String& label)
    {
        t = pcOf (t);
        const juce::String pivot = noteName (t + 7, t, minor) + "7 (V7 of the new key)";
        m.add ({ t, minor, label + "  ->  " + nm (t, minor) + (minor ? " minor" : " major"), pivot });
    };

    if (! opt.minorKey)
    {
        add (k + 9, true,  "Relative minor");
        add (k, true,      "Parallel minor");
        add (k + 5, false, "Up a 4th (I becomes V)");
        add (k + 7, false, "Up a 5th (dominant key)");
        add (k + 3, false, "Up a minor 3rd");
        add (k + 8, false, "Down a major 3rd (Coltrane)");
        add (k + 4, false, "Up a major 3rd (Coltrane)");
        add (k + 1, false, "Up a half step (truck-driver)");
        add (k + 2, false, "Up a whole step");
    }
    else
    {
        add (k + 3, false, "Relative major");
        add (k, false,     "Parallel major");
        add (k + 5, true,  "Up a 4th (iv becomes i)");
        add (k + 7, true,  "Up a 5th");
        add (k + 8, false, "To bVI major");
        add (k + 1, true,  "Up a half step");
    }
    return m;
}

} // namespace theory
