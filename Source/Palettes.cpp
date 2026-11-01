#include "Palettes.h"

namespace palettes
{
using theory::ChordSpec;
using Q = theory::Quality;
using theory::Level;
using theory::Style;

static ChordSpec P (int root, Q q, int var = 0, int style = -1, int bass = -1)
{
    ChordSpec s; s.rootOffset = ((root % 12) + 12) % 12; s.quality = q; s.variation = var; s.style = style; s.bassOffset = bass; s.beats = 4; s.valid = true;
    return s;
}

static Palette fromList (std::initializer_list<std::pair<int, ChordSpec>> l)
{
    Palette p;
    for (auto& s : p) s = P (0, Q::Maj7);
    for (auto& e : l) p[(size_t) e.first] = e.second;
    return p;
}

static Palette majorDefault (Level lv)
{
    const int L = (int) lv;
    Palette p = fromList ({
        { 0,  P (0, Q::Maj7) },   { 2,  P (2, Q::Min7) },  { 4,  P (4, Q::Min7) },  { 5,  P (5, Q::Maj7) },
        { 7,  P (7, Q::Dom7) },   { 9,  P (9, Q::Min7) },  { 11, P (11, Q::Min7b5) },
        { 14, P (2, Q::Min7, 1) },{ 21, P (9, Q::Min7, 1) },{ 19, P (7, Q::Dom7Sus) },
    });

    if (L == 0)
    {
        p[1]  = P (0, Q::Maj6);     p[3]  = P (2, Q::Dom7);   p[6]  = P (5, Q::Min7);  p[8]  = P (9, Q::Dom7);  p[10] = P (10, Q::Dom7);
        p[12] = P (0, Q::Maj7, 1);  p[13] = P (0, Q::Dom7);   p[15] = P (3, Q::Maj7);  p[16] = P (4, Q::Dom7);  p[17] = P (5, Q::Maj6);
        p[18] = P (6, Q::Dim7);     p[20] = P (8, Q::Maj7);   p[22] = P (5, Q::Min6);  p[23] = P (7, Q::Dom7, 1);
    }
    else if (L == 1)
    {
        p[1]  = P (1, Q::Dom7);     p[3]  = P (2, Q::Dom7);   p[6]  = P (6, Q::Min7b5); p[8] = P (9, Q::Dom7);  p[10] = P (10, Q::Dom7);
        p[12] = P (0, Q::Maj6);     p[13] = P (0, Q::Dom7);   p[15] = P (3, Q::Maj7);   p[16] = P (4, Q::Dom7); p[17] = P (5, Q::Min6);
        p[18] = P (6, Q::Dim7);     p[20] = P (8, Q::Maj7);   p[22] = P (11, Q::Dom7);  p[23] = P (7, Q::Dom7, 1);
    }
    else if (L == 2)
    {
        p[1]  = P (1, Q::Dom7Lyd);  p[3]  = P (2, Q::Dom7);   p[6]  = P (6, Q::Min7b5); p[8] = P (9, Q::Dom7b9); p[10] = P (10, Q::Dom7);
        p[12] = P (0, Q::Maj6);     p[13] = P (0, Q::Dom7);   p[15] = P (3, Q::Maj7);   p[16] = P (4, Q::Dom7b9); p[17] = P (5, Q::Maj7Sharp11);
        p[18] = P (6, Q::Dim7);     p[20] = P (8, Q::Maj7);   p[22] = P (5, Q::Min6);   p[23] = P (7, Q::Dom7Alt);
    }
    else
    {
        p[1]  = P (1, Q::Dom7Lyd);  p[3]  = P (2, Q::Dom7);   p[6]  = P (6, Q::Min7b5); p[8] = P (9, Q::Dom7Alt); p[10] = P (10, Q::Dom7Lyd);
        p[12] = P (0, Q::Maj6);     p[13] = P (1, Q::Maj7);   p[15] = P (3, Q::Maj7Sharp11); p[16] = P (4, Q::Dom7Alt); p[17] = P (5, Q::Maj7Sharp11);
        p[18] = P (6, Q::Dim7);     p[20] = P (8, Q::Dom7Lyd);p[22] = P (5, Q::MinMaj7);  p[23] = P (7, Q::Dom7Alt);
        p[0]  = P (0, Q::Maj7);     p[19] = P (7, Q::Dom7Sus);
    }
    return p;
}

static Palette minorDefault (Level lv)
{
    const int L = (int) lv;
    Palette p = fromList ({
        { 0,  P (0, L == 0 ? Q::Min7 : (L == 3 ? Q::MinMaj7 : Q::Min6)) },
        { 2,  P (2, Q::Min7b5) }, { 4, P (3, Q::Maj7) }, { 5, P (5, Q::Min7) },
        { 7,  P (7, Q::Dom7b9) }, { 9, P (8, Q::Maj7) }, { 11, P (11, Q::Dim7) },
        { 1,  P (1, L == 0 ? Q::Dom7 : Q::Dom7Lyd) }, { 3, P (0, Q::Dom7) }, { 6, P (10, Q::Dom7) },
        { 8,  P (7, Q::Min7) },   { 10, P (2, Q::Dom7) },
        { 12, P (0, L == 0 ? Q::Min6 : Q::MinMaj7) }, { 13, P (1, Q::Maj7) }, { 14, P (2, Q::Min7b5, 1) },
        { 15, P (3, L >= 2 ? Q::Maj7Sharp5 : Q::Maj7, L >= 2 ? 0 : 1) }, { 16, P (5, Q::Min6) }, { 17, P (5, Q::Min7, 1) },
        { 18, P (6, Q::Min7b5) }, { 19, P (7, L == 0 ? Q::Dom7 : Q::Dom7Alt) }, { 20, P (8, L == 0 ? Q::Dom7 : Q::Dom7Lyd) },
        { 21, P (8, L >= 2 ? Q::Maj7Sharp11 : Q::Maj7, L >= 2 ? 0 : 1) }, { 22, P (10, Q::Dom7Sus) }, { 23, P (7, Q::Dom7Sus) },
    });
    return p;
}

Palette levelDefault (Level level, bool minorKey)
{
    return minorKey ? minorDefault (level) : majorDefault (level);
}

juce::StringArray presetNames()
{
    return { "Level Default", "ii-V-I Workshop (cycle of 4ths)", "Minor ii-V-i Workshop", "Rhythm Changes",
             "Coltrane Changes", "Modal / So What", "Jazz & Bird Blues", "Neo-Soul Colours" };
}

juce::String presetDescription (int index)
{
    switch (index)
    {
        case 0: return "The standard palette for the selected level: diatonic chords on the white pads, colour and substitute chords on the black pads.";
        case 1: return "Eight major ii-V-I cadences moving through the cycle of fourths. Play each group of three pads left to right.";
        case 2: return "Eight minor ii-V-i cadences (half-diminished, altered dominant, minor tonic) through the cycle of fourths.";
        case 3: return "The building blocks of Gershwin's 'I Got Rhythm' changes: I-vi-ii-V turnarounds and the III7-VI7-II7-V7 bridge.";
        case 4: return "Giant-Steps style changes: three tonal centres a major third apart, each with its ii, V, tritone sub and altered dominant.";
        case 5: return "Modal jazz: minor 11th 'So What' quartal voicings on the lower pads and quartal sus chords on the upper pads, on every root.";
        case 6: return "Jazz blues and Charlie Parker 'Bird blues' chords: dominant sevenths, the #IV diminished, and descending ii-V chains.";
        case 7: return "Lush maj9, m11 and 9sus4 colours with quartal options and borrowed major-seventh chords.";
        default: return {};
    }
}

Palette preset (int index, Level level, bool minorKey)
{
    switch (index)
    {
        case 1:
        {
            Palette p; const int tonics[8] = { 0, 5, 10, 3, 8, 1, 6, 11 };
            for (int g = 0; g < 8; ++g)
            {
                const int t = tonics[g];
                p[(size_t) g * 3 + 0] = P (t + 2, Q::Min7);
                p[(size_t) g * 3 + 1] = P (t + 7, Q::Dom7);
                p[(size_t) g * 3 + 2] = P (t, Q::Maj7);
            }
            return p;
        }
        case 2:
        {
            Palette p; const int tonics[8] = { 0, 5, 10, 3, 8, 1, 6, 11 };
            for (int g = 0; g < 8; ++g)
            {
                const int t = tonics[g];
                p[(size_t) g * 3 + 0] = P (t + 2, Q::Min7b5);
                p[(size_t) g * 3 + 1] = P (t + 7, level >= Level::Advanced ? Q::Dom7Alt : Q::Dom7b9);
                p[(size_t) g * 3 + 2] = P (t, level >= Level::Advanced ? Q::MinMaj7 : Q::Min6);
            }
            return p;
        }
        case 3:
            return fromList ({
                { 0, P (0, Q::Maj6) },  { 2, P (9, Q::Min7) },  { 4, P (2, Q::Min7) },  { 5, P (7, Q::Dom7) },
                { 7, P (4, Q::Min7) },  { 9, P (9, Q::Dom7) },  { 11, P (0, Q::Dom7) }, { 12, P (5, Q::Dom7) },
                { 14, P (6, Q::Dim7) }, { 16, P (4, Q::Dom7) }, { 17, P (9, Q::Dom7, 1) }, { 19, P (2, Q::Dom7) },
                { 21, P (7, Q::Dom7, 1) }, { 23, P (0, Q::Maj6, 1) },
                { 1, P (2, Q::Min7, 1) }, { 3, P (10, Q::Dom7) }, { 6, P (5, Q::Min6) }, { 8, P (8, Q::Dom7) },
                { 10, P (1, Q::Dom7) }, { 13, P (9, Q::Min7, 1) }, { 15, P (5, Q::Maj6) }, { 18, P (4, Q::Min7, 1) },
                { 20, P (7, Q::Dom7Sus) }, { 22, P (0, Q::Maj6, 0, -1, 4) },
            });
        case 4:
            return fromList ({
                { 0, P (0, Q::Maj7) },  { 2, P (7, Q::Dom7) },  { 4, P (4, Q::Maj7) },  { 5, P (11, Q::Dom7) },
                { 7, P (8, Q::Maj7) },  { 9, P (3, Q::Dom7) },  { 11, P (2, Q::Min7) }, { 12, P (6, Q::Min7) },
                { 14, P (10, Q::Min7) },{ 16, P (0, Q::Maj6) }, { 17, P (4, Q::Maj6) }, { 19, P (8, Q::Maj6) },
                { 1, P (1, Q::Dom7Lyd) },  { 3, P (5, Q::Dom7Lyd) },  { 6, P (9, Q::Dom7Lyd) },
                { 8, P (7, Q::Dom7Alt) },  { 10, P (11, Q::Dom7Alt) }, { 13, P (3, Q::Dom7Alt) },
                { 15, P (0, Q::Maj7Sharp11) }, { 18, P (4, Q::Maj7Sharp11) }, { 20, P (8, Q::Maj7Sharp11) },
                { 21, P (0, Q::Maj7, 1) }, { 22, P (7, Q::Dom7Sus) }, { 23, P (4, Q::Maj7, 1) },
            });
        case 5:
        {
            Palette p;
            for (int i = 0; i < 12; ++i) p[(size_t) i] = P (i, Q::Min7, 0, (int) Style::Quartal);
            for (int i = 0; i < 12; ++i) p[(size_t) i + 12] = P (i, Q::Dom7Sus, 0, (int) Style::Quartal);
            return p;
        }
        case 6:
            return fromList ({
                { 0, P (0, Q::Dom7) },  { 2, P (2, Q::Min7) },  { 4, P (4, Q::Min7) },  { 5, P (5, Q::Dom7) },
                { 7, P (7, Q::Dom7) },  { 9, P (9, Q::Dom7) },  { 11, P (10, Q::Dom7) },
                { 1, P (1, Q::Dom7) },  { 3, P (3, Q::Dom7) },  { 6, P (6, Q::Dim7) },  { 8, P (8, Q::Dom7) },  { 10, P (5, Q::Min7) },
                { 12, P (0, Q::Dom7, 1) }, { 13, P (9, Q::Min7) }, { 14, P (4, Q::Dom7) }, { 15, P (7, Q::Min7) },
                { 16, P (11, Q::Min7b5) }, { 17, P (5, Q::Dom7, 1) }, { 18, P (6, Q::Dim7, 1) }, { 19, P (7, Q::Dom7Alt) },
                { 20, P (2, Q::Dom7) }, { 21, P (9, Q::Dom7Alt) }, { 22, P (0, Q::Maj7) }, { 23, P (0, Q::Dom7Alt) },
            });
        case 7:
        {
            const int qs = (int) Style::Quartal;
            return fromList ({
                { 0, P (0, Q::Maj7) },  { 2, P (2, Q::Min7) },  { 4, P (4, Q::Min7) },  { 5, P (5, Q::Maj7) },
                { 7, P (7, Q::Dom7Sus) }, { 9, P (9, Q::Min7) }, { 11, P (10, Q::Maj7) },
                { 1, P (1, Q::Maj7) },  { 3, P (3, Q::Maj7) },  { 6, P (5, Q::Maj7, 0, -1, 7) }, { 8, P (8, Q::Maj7) }, { 10, P (5, Q::Min7) },
                { 12, P (0, Q::Maj7, 0, qs) }, { 14, P (2, Q::Min7, 0, qs) }, { 16, P (4, Q::Min7, 0, qs) }, { 17, P (5, Q::Maj7, 0, qs) },
                { 19, P (7, Q::Dom7Sus, 0, qs) }, { 21, P (9, Q::Min7, 0, qs) }, { 23, P (0, Q::Maj6) },
                { 13, P (4, Q::Dom7Sus) }, { 15, P (3, Q::Maj6) }, { 18, P (6, Q::Min7b5) }, { 20, P (8, Q::Maj7Sharp11) }, { 22, P (10, Q::Dom7Sus) },
            });
        }
        default:
            return levelDefault (level, minorKey);
    }
}

bool isBlackPad (int pad) noexcept
{
    const int pc = pad % 12;
    return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
}

juce::String qwertyHint (int pad)
{
    static const char* keys = "ZSXDCVGBHNJM";
    return juce::String::charToString ((juce::juce_wchar) keys[pad % 12]);
}
}
