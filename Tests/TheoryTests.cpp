#include "../Source/Theory.h"
#include "../Source/Palettes.h"
#include <iostream>
using namespace theory;
static int fails = 0;
#define CHECK(c, msg) do { if (!(c)) { ++fails; std::cout << "FAIL: " << msg << "\n"; } } while (0)

static juce::String notes (const Voicing& v, const Options& o)
{
    juce::String s; if (v.bass >= 0) s << "[" << noteNameWithOctave (v.bass, o.keyTonic, o.minorKey) << "] ";
    for (int i = 0; i < v.count; ++i) s << noteNameWithOctave (v.notes[i], o.keyTonic, o.minorKey) << " ";
    return s.trim();
}

int main()
{
    Options o; o.keyTonic = 0;
    // ---- names per level
    struct Case { Level lv; int root; Quality q; const char* expect; };
    Case cases[] = {
        { Level::Beginner, 2, Quality::Min7, "Dm7" }, { Level::Intermediate, 2, Quality::Min7, "Dm9" },
        { Level::Advanced, 2, Quality::Min7, "Dm11" }, { Level::Conservatory, 2, Quality::Min7, "Dm13" },
        { Level::Beginner, 7, Quality::Dom7, "G7" }, { Level::Intermediate, 7, Quality::Dom7, "G9" }, { Level::Advanced, 7, Quality::Dom7, "G13" },
        { Level::Intermediate, 0, Quality::Maj7, "Cmaj9" }, { Level::Advanced, 0, Quality::Maj7, "Cmaj13" },
        { Level::Conservatory, 5, Quality::Maj7, "Fmaj13#11" },   // IV is lydian
        { Level::Conservatory, 0, Quality::Maj7, "Cmaj13" },      // I: #11 is not in ionian
        { Level::Intermediate, 4, Quality::Min7, "Em7(11)" },     // iii phrygian: 9 avoided
        { Level::Advanced, 9, Quality::Min7, "Am11" },
        { Level::Advanced, 7, Quality::Dom7Alt, "G7alt" }, { Level::Conservatory, 7, Quality::Dom7Alt, "G7alt" }, { Level::Intermediate, 7, Quality::Dom7Alt, "G7#9b13" },
        { Level::Beginner, 11, Quality::Min7b5, "Bm7b5" }, { Level::Intermediate, 11, Quality::Min7b5, "Bm7b5(11)" },
        { Level::Advanced, 1, Quality::Dom7Lyd, "Db13#11" },
    };
    for (auto& c : cases)
    {
        o.level = c.lv; ChordSpec s; s.rootOffset = c.root; s.quality = c.q;
        auto n = chordName (s, o);
        CHECK (n == c.expect, levelName (c.lv) << " " << c.expect << " got " << n);
    }

    // ---- rootless A/B form for Dm9 & G13
    o.level = Level::Intermediate; o.voiceLeading = false; o.bassNote = false;
    { ChordSpec s; s.rootOffset = 2; s.quality = Quality::Min7; Voicing v = buildVoicing (s, o, {});
      std::cout << "Dm9 rootless: " << notes (v, o) << "\n";
      CHECK (v.count == 4, "Dm9 rootless 4 notes"); }
    o.level = Level::Advanced;
    { ChordSpec s; s.rootOffset = 7; s.quality = Quality::Dom7; Voicing v = buildVoicing (s, o, {});
      std::cout << "G13 rootless: " << notes (v, o) << "\n"; }
    { ChordSpec s; s.rootOffset = 7; s.quality = Quality::Dom7Alt; Voicing v = buildVoicing (s, o, {});
      std::cout << "G7alt rootless: " << notes (v, o) << "\n"; }

    // ---- every quality/style/level/key produces a sane voicing
    for (int key = 0; key < 12; ++key)
    for (int L = 0; L < 4; ++L)
    for (int st = 0; st < numStyles; ++st)
    for (int q = 0; q < numQualities; ++q)
    for (int r = 0; r < 12; ++r)
    for (int vl = 0; vl < 2; ++vl)
    for (int var = 0; var < 4; ++var)
    {
        Options op; op.keyTonic = key; op.level = (Level) L; op.style = (Style) st; op.voiceLeading = vl; op.bassNote = (r % 2) == 0;
        op.minorKey = (key % 3) == 0; op.alterations = (q % 2) == 0; op.autoReharm = (r == 7 && var == 3);
        ChordSpec s; s.rootOffset = r; s.quality = (Quality) q; s.variation = var; s.bassOffset = (r == 5 ? 9 : -1);
        Voicing prev; prev.add (52); prev.add (57); prev.add (60); prev.add (64);
        Voicing v = buildVoicing (s, op, vl ? prev : Voicing{});
        auto rc = resolve (s, op); addVoicedTensions (rc, v);
        CHECK (v.count >= 3 && v.count <= 6, "count " << v.count << " q" << q << " st" << st);
        for (int i = 0; i < v.count; ++i) CHECK (v.notes[i] >= 28 && v.notes[i] <= 96, "range " << v.notes[i] << " q" << q << " st" << st);
        for (int i = 1; i < v.count; ++i) CHECK (v.notes[i] > v.notes[i - 1], "dup/unsorted q" << q << " st" << st << " " << notes (v, op));
        if (v.count > 0) CHECK (v.notes[v.count - 1] - v.notes[0] <= (st == 7 ? 50 : 40), "span q" << q << " st" << st);
        if (v.bass >= 0) CHECK (v.bass < v.notes[0], "bass above voicing");
        // Every voiced note must be a chord tone or the slash bass
        const uint16_t ct = chordToneMask (rc);
        for (int i = 0; i < v.count; ++i)
            CHECK (maskHas (ct, v.notes[i]) || (rc.bassPc >= 0 && ((v.notes[i] % 12) == rc.bassPc)),
                   "non chord tone " << noteNameWithOctave (v.notes[i], 0, false) << " in " << chordName (rc, op) << " style " << st);
        // the 3rd and 7th (guide tones) should be present except quartal sus / spread etc
        auto nm = chordName (rc, op); CHECK (nm.isNotEmpty(), "name");
        if (fails > 30) { std::cout << "too many failures\n"; return 1; }
    }

    // ---- voice leading: ii-V-I in C should move little
    o = Options(); o.level = Level::Intermediate; o.voiceLeading = true;
    {
        ChordSpec ii; ii.rootOffset = 2; ii.quality = Quality::Min7;
        ChordSpec V; V.rootOffset = 7; V.quality = Quality::Dom7;
        ChordSpec I; I.rootOffset = 0; I.quality = Quality::Maj7;
        Voicing a = buildVoicing (ii, o, {}); Voicing b = buildVoicing (V, o, a); Voicing c = buildVoicing (I, o, b);
        std::cout << "ii-V-I: " << notes (a, o) << " | " << notes (b, o) << " | " << notes (c, o) << "\n";
        int move = 0; for (int i = 0; i < 4; ++i) move += std::abs (a.notes[i] - b.notes[i]) + std::abs (b.notes[i] - c.notes[i]);
        CHECK (move <= 10, "voice leading movement " << move);
    }
    // ---- conservatory showcase
    o.level = Level::Conservatory; o.alterations = true;
    for (int st : { 0, 6, 5, 4, 7 })
    {
        o.style = (Style) st;
        juce::String line = styleName ((Style) st) + ": ";
        Voicing prev;
        for (auto [r, q] : { std::pair{ 2, Quality::Min7 }, { 7, Quality::Dom7 }, { 0, Quality::Maj7 }, { 9, Quality::Dom7 } })
        {
            ChordSpec s; s.rootOffset = r; s.quality = q; prev = buildVoicing (s, o, prev);
            auto rcv = resolve (s, o); addVoicedTensions (rcv, prev); line << chordName (rcv, o) << " {" << notes (prev, o) << "}  ";
        }
        std::cout << line << "\n";
    }

    // ---- melody
    o = Options(); o.level = Level::Intermediate;
    { ChordSpec s; s.rootOffset = 7; s.quality = Quality::Dom7Alt; auto rc = resolve (s, o);
      CHECK (snapToMask (67 + 2, rc.scaleMask) != 69, "A not in G altered");
      std::cout << "G alt scale: " << scaleName (rc, o, MelodyMode::ChordScale) << "\n"; }
    { o.keyTonic = 2; ChordSpec s; s.rootOffset = 0; s.quality = Quality::Maj7; auto rc = resolve (s, o);
      int out[7]; for (int k = 0; k < 7; ++k) out[k] = mapWhiteKey (60 + (int[]){0,2,4,5,7,9,11}[k], keyScaleMask (2, false), 2);
      juce::String m; for (int x : out) m << noteNameWithOctave (x, 2, false) << " "; std::cout << "D major white-key map: " << m << "\n";
      CHECK (out[0] == 62 && out[6] == 73, "white key map");
      int h[4]; int n = harmonize (Harmonize::BlockChords, 78, rc, o, h, 4); CHECK (n == 4, "block harmonize count " << n); }

    // ---- palettes resolve & suggestions
    for (int p = 0; p < palettes::presetNames().size(); ++p)
        for (int L = 0; L < 4; ++L)
        {
            auto pal = palettes::preset (p, (Level) L, false);
            Options op; op.level = (Level) L;
            juce::String line;
            for (auto& s : pal) line << chordName (s, op) << " ";
            if (L == 2) std::cout << palettes::presetNames()[p] << ": " << line << "\n";
            float sc[24]; suggestNext (pal[7], pal.data(), 24, op, 7, sc);
        }
    { auto pal = palettes::levelDefault (Level::Intermediate, false); Options op; float sc[24];
      suggestNext (pal[2], pal.data(), 24, op, 2, sc); CHECK (sc[7] >= 3.0f, "ii suggests V"); }
    for (auto& m : modulationSuggestions (o)) std::cout << "mod: " << m.label << " pivot " << m.pivot << "\n";
    // pack round-trip
    for (uint32_t i = 0; i < 2000; ++i) { ChordSpec s; s.rootOffset = i % 12; s.quality = (Quality) (i % numQualities); s.variation = (i/7)%4; s.style = (int)(i % 9) - 1; s.bassOffset = (int)(i%13)-1; s.beats = 1 + i % 8;
        CHECK (ChordSpec::unpack (s.pack()) == s, "pack"); }

    std::cout << (fails == 0 ? "ALL THEORY TESTS PASSED\n" : "FAILURES: ") << (fails ? juce::String (fails) : juce::String()) << "\n";
    return fails == 0 ? 0 : 1;
}
