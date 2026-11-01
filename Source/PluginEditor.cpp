#include "PluginEditor.h"

using namespace juce;
using LNF = ChangesLookAndFeel;

static PropertiesFile::Options settingsOptions();

#if JUCE_MAC
 #define CHANGES_COPY_KEY "Cmd"
 #define CHANGES_RIGHT_CLICK "Right-click (or Ctrl-click)"
#else
 #define CHANGES_COPY_KEY "Ctrl"
 #define CHANGES_RIGHT_CLICK "Right-click"
#endif

//==============================================================================
namespace layoutInfo
{
    struct Caption { String text; Rectangle<int> r; Colour c; };
    struct PanelInfo { String title; Rectangle<int> r; Colour accent; };
}

static std::vector<layoutInfo::Caption> gCaptions;     // filled once by layout(); read by paint
static std::vector<layoutInfo::PanelInfo> gPanels;
static String gLevelInfo;

static String levelDescription (int level)
{
    switch (level)
    {
        case 0: return "BEGINNER  -  Four-note seventh chords in close position with the root on the bottom. Learn the seven diatonic chords of the key, the ii-V-I cadence and simple secondary dominants.";
        case 1: return "INTERMEDIATE  -  Bill Evans rootless A/B voicings with 9ths, secondary dominants, the tritone substitute and minor iv. Voice leading moves every note the shortest distance.";
        case 2: return "ADVANCED  -  11ths and 13ths, altered and b9 dominants, quartal 'So What' minor chords, lydian IV, modal interchange and backdoor dominants.";
        default: return "CONSERVATORY  -  Upper-structure triads over guide-tone shells, drop-2 spreads, lydian and altered colours, locrian #2, Coltrane relationships and slash-chord reharmonisation.";
    }
}

//==============================================================================
class QuickStartOverlay : public Component
{
public:
    std::function<void()> onClose;
    void paint (Graphics& g) override
    {
        g.fillAll (Colours::black.withAlpha (0.72f));
        const int ts = LNF::getTextSize();
        auto r = getLocalBounds().toFloat().withSizeKeepingCentre (ts == 0 ? 860.0f : (ts == 1 ? 960.0f : 1060.0f),
                                                                   ts == 0 ? 620.0f : (ts == 1 ? 700.0f : 780.0f));
        LNF::drawPanel (g, r, "QUICK START");
        auto t = r.reduced (40.0f, 40.0f);
        auto footer = t.removeFromBottom (LNF::scaledHeight (12.5f) + 6.0f);

        g.setFont (LNF::displayFont (30.0f));
        g.setColour (colours::goldLight);
        g.drawText ("Welcome to Changes", t.removeFromTop (40), Justification::centredLeft);
        g.setFont (LNF::font (14.0f));
        g.setColour (colours::textDim);
        g.drawFittedText ("A playable jazz harmony instrument - one key per chord in the left hand, an in-key melody in the right.",
                          t.removeFromTop (LNF::scaledHeight (14.0f) + 8.0f).toNearestInt(), Justification::centredLeft, 1, 0.8f);
        t.removeFromTop (10);
        const float rowH = t.getHeight() / 6.0f;

        const char* items[][2] = {
            { "1  Pick a key and a level", "Choose the KEY and MAJOR/MINOR at the top, then a theory level from Beginner to Conservatory. Each level changes the chord palette and the way chords are voiced." },
            { "2  Left hand: one key = one chord", "The 24 pads map to MIDI notes C2-B3 (change it under CHORD ZONE). Click a pad, play those keys on a MIDI keyboard, or use Z S X D C V G B H N J M on your computer keyboard (TAB switches to pads 13-24)." },
            { "3  Right hand: melody that can't miss", "Notes above the chord zone are snapped to the scale of the current chord (Dorian on ii, altered on V7alt ...). Computer keys Q 2 W 3 E R 5 T 6 Y 7 U I 9 O 0 P play melody." },
            { "4  Follow the NEXT hints", "Teal-outlined pads are strong next chords (ii -> V, V -> I, tritone subs). Hover anything to read what it does in the help bar at the bottom." },
            { "5  Build and export progressions", "Drag pads into the progression strip (or arm REC and play). Press PLAY to loop it with a comping rhythm, then drag 'DRAG MIDI' straight into your DAW." },
            { "6  Make the palette your own", "Drag a pad onto another to swap them (hold " CHANGES_COPY_KEY " to copy). " CHANGES_RIGHT_CLICK " a pad to change its root, quality, voicing or slash bass. Save palettes from the MENU." },
        };
        for (auto& it : items)
        {
            auto row = t.removeFromTop (rowH).withTrimmedBottom (4.0f);
            g.setFont (LNF::font (15.5f, true)); g.setColour (colours::gold);
            g.drawText (it[0], row.removeFromTop (LNF::scaledHeight (15.5f) + 5.0f), Justification::centredLeft);
            const String txt = it[1];
            LNF::drawFittedLayout (g, row, [txt] (float s)
            {
                AttributedString as; as.append (txt, LNF::font (13.5f).withHeight (LNF::scaledHeight (13.5f) * s), colours::text);
                return as;
            });
        }
        g.setFont (LNF::font (12.5f, true)); g.setColour (colours::teal);
        g.drawText ("Click anywhere to start playing.   Open this again from MENU > Quick start guide.", footer, Justification::bottomLeft);
    }
    void mouseDown (const MouseEvent&) override { if (onClose) onClose(); }
};

//==============================================================================
void EditorContent::paint (Graphics& g)
{
    const auto W = (float) getWidth(), H = (float) getHeight();
    ColourGradient bg (Colour (0xff141c2e), W * 0.5f, 0, Colour (0xff070a12), W * 0.5f, H, false);
    g.setGradientFill (bg);
    g.fillAll();
    if (noise.isValid()) g.drawImageAt (noise, 0, 0);

    // walnut end cheeks
    for (int side = 0; side < 2; ++side)
    {
        Rectangle<float> r (side == 0 ? 0.0f : W - 22.0f, 0.0f, 22.0f, H);
        ColourGradient wood (Colour (0xff5b3418), r.getX(), 0, Colour (0xff2a1608), r.getRight(), 0, false);
        wood.addColour (0.5, Colour (0xff7a4720));
        g.setGradientFill (wood);
        g.fillRect (r);
        Random rnd (side + 3);
        for (int i = 0; i < 26; ++i)
        {
            Path grain;
            const float x = r.getX() + rnd.nextFloat() * r.getWidth();
            grain.startNewSubPath (x, 0);
            for (float y = 0; y <= H; y += 40.0f)
                grain.lineTo (x + std::sin (y * 0.013f + i) * (1.5f + rnd.nextFloat() * 2.0f), y);
            g.setColour (Colours::black.withAlpha (0.10f + rnd.nextFloat() * 0.12f));
            g.strokePath (grain, PathStrokeType (0.6f + rnd.nextFloat()));
        }
        g.setColour (Colours::black.withAlpha (0.6f));
        g.drawLine (side == 0 ? r.getRight() : r.getX(), 0, side == 0 ? r.getRight() : r.getX(), H, 2.0f);
    }

    // logo
    {
        auto lr = Rectangle<float> (34, 10, 200, 50);
        Font f = LNF::displayFont (34.0f);
        f.setExtraKerningFactor (0.12f);
        g.setFont (f);
        ColourGradient lg (colours::goldLight, lr.getX(), lr.getY(), colours::goldDark, lr.getX(), lr.getBottom(), false);
        g.setColour (Colours::black.withAlpha (0.6f));
        g.drawText ("CHANGES", lr.translated (1.5f, 2.0f).withHeight (36), Justification::centredLeft);
        g.setGradientFill (lg);
        g.drawText ("CHANGES", lr.withHeight (36), Justification::centredLeft);
        Font s = LNF::font (9.5f, true); s = s.withHeight (jmin (s.getHeight(), 12.0f)); s.setExtraKerningFactor (0.2f);
        g.setFont (s);
        g.setColour (colours::textDim);
        g.drawFittedText ("JAZZ HARMONY INSTRUMENT", lr.withTrimmedTop (36).withHeight (14).toNearestInt(), Justification::centredLeft, 1, 0.6f);
    }

    for (auto& p : gPanels) LNF::drawPanel (g, p.r.toFloat(), p.title, p.accent);

    for (auto& c : gCaptions)
    {
        Font f = LNF::font (10.0f, true); f.setExtraKerningFactor (0.08f);
        g.setFont (f);
        g.setColour (c.c);
        g.drawText (c.text, c.r, Justification::centredLeft);
    }

    // level info box
    {
        auto r = Rectangle<float> (44, 432, 226, 116);
        LNF::drawRecess (g, r, 6.0f);
        const String head = gLevelInfo.upToFirstOccurrenceOf ("  -  ", false, false);
        const String body = gLevelInfo.fromFirstOccurrenceOf ("  -  ", false, false);
        LNF::drawFittedLayout (g, r.reduced (8.0f, 7.0f), [head, body] (float s)
        {
            const float h = LNF::scaledHeight (11.5f) * s;
            AttributedString as;
            as.append (head + "\n", LNF::font (11.5f, true).withHeight (h), colours::gold);
            as.append (body, LNF::font (11.5f).withHeight (h), colours::text.withAlpha (0.85f));
            return as;
        });
    }

    // melody scale readout
    {
        auto r = Rectangle<float> (642, 590, 328, 86);
        LNF::drawRecess (g, r, 6.0f);
        auto t = r.reduced (10.0f, 6.0f);
        g.setFont (LNF::font (10.0f, true)); g.setColour (colours::textFaint);
        g.drawText ("RIGHT-HAND SCALE NOW", t.removeFromTop (14), Justification::centredLeft);
        g.setFont (LNF::font (15.0f, true)); g.setColour (colours::blue);
        g.drawFittedText (melodyScaleText.upToFirstOccurrenceOf ("|", false, false), t.removeFromTop (22).toNearestInt(), Justification::centredLeft, 1, 0.7f);
        g.setFont (LNF::font (13.0f)); g.setColour (colours::text);
        g.drawFittedText (melodyScaleText.fromFirstOccurrenceOf ("|", false, false), t.removeFromTop (18).toNearestInt(), Justification::centredLeft, 1, 0.7f);
        g.setFont (LNF::font (10.5f)); g.setColour (colours::textFaint);
        g.drawFittedText ("Keys: Z-M chords | Q-P melody | TAB pad bank | SPACE play | ESC panic", t.toNearestInt(), Justification::centredLeft, 1, 0.7f);
    }

    if (hostTempo)
    {
        g.setFont (LNF::font (9.5f, true)); g.setColour (colours::teal);
        g.drawText ("HOST SYNC", Rectangle<int> (904, 416, 70, 12), Justification::centred);
    }
}

//==============================================================================
ChangesEditor::ChangesEditor (ChangesProcessor& p)
    : AudioProcessorEditor (p), proc (p),
      pads (p), strip (p), dragHandle (p),
      bpmKnob (p.apvts, ids::bpm, "Tempo", Knob::Format::Bpm),
      swingKnob (p.apvts, ids::swing, "Swing", Knob::Format::Swing),
      strumKnob (p.apvts, ids::strum, "Strum", Knob::Format::Milliseconds),
      humanizeKnob (p.apvts, ids::humanize, "Humanize", Knob::Format::Percent),
      dynamicsKnob (p.apvts, ids::dynamics, "Dynamics", Knob::Format::Percent),
      toneKnob (p.apvts, ids::tone, "Tone", Knob::Format::Percent),
      releaseKnob (p.apvts, ids::release, "Release", Knob::Format::Percent),
      chorusKnob (p.apvts, ids::chorus, "Chorus", Knob::Format::Percent, colours::blue),
      reverbKnob (p.apvts, ids::reverb, "Reverb", Knob::Format::Percent, colours::blue),
      motionKnob (p.apvts, ids::motion, "Tremolo", Knob::Format::Percent, colours::blue),
      chordFader (p.apvts, ids::chordVol, "Chords", false, colours::gold),
      melodyFader (p.apvts, ids::melVol, "Melody", false, colours::blue),
      masterFader (p.apvts, ids::masterVol, "Master", true, colours::text),
      keyboard (p)
{
    setLookAndFeel (&lnf);
    LookAndFeel::setDefaultLookAndFeel (&lnf);
    addAndMakeVisible (content);

    // noise texture for the background
    {
        content.noise = Image (Image::ARGB, designW, designH, true);
        Random r (42);
        for (int i = 0; i < 26000; ++i)
        {
            const int x = r.nextInt (designW), y = r.nextInt (designH);
            content.noise.setPixelAt (x, y, (r.nextBool() ? Colours::white : Colours::black).withAlpha (0.035f));
        }
    }

    // ---- header
    setupCombo (keyBox, ids::key);
    for (auto* b : { &majorBtn, &minorBtn })
    {
        b->setClickingTogglesState (false);
        content.addAndMakeVisible (*b);
    }
    majorBtn.onClick = [this] { setParam (ids::minor, 0.0f); };
    minorBtn.onClick = [this] { setParam (ids::minor, 1.0f); };
    minorBtn.getProperties().set ("accent", colours::blue.toString());
    const auto levels = theory::levelNames();
    for (int i = 0; i < 4; ++i)
    {
        levelBtns[i].setButtonText (levels[i].toUpperCase());
        levelBtns[i].onClick = [this, i] { setParam (ids::level, (float) i); };
        content.addAndMakeVisible (levelBtns[i]);
    }
    modulateBtn.onClick = [this] { showModulationMenu(); };
    modulateBtn.getProperties().set ("accent", colours::teal.toString());
    tipsBtn.setClickingTogglesState (true);
    tipsBtn.setToggleState (true, dontSendNotification);
    tipsBtn.onClick = [this] { toggleTooltips (tipsBtn.getToggleState()); };
    menuBtn.onClick = [this] { showMainMenu(); };
    for (auto* b : { &modulateBtn, &tipsBtn, &menuBtn }) content.addAndMakeVisible (*b);

    // ---- harmony
    setupCombo (styleBox, ids::style);
    setupCombo (registerBox, ids::registerShift);
    setupToggle (bassTgl, ids::bass);
    setupToggle (extTgl, ids::ext);
    setupToggle (altTgl, ids::alt);
    setupToggle (vlTgl, ids::vl);
    setupToggle (reharmTgl, ids::reharm);
    setupToggle (latchTgl, ids::latch, colours::red);
    setupToggle (suggestTgl, ids::suggest, colours::teal);
    setupToggle (lockTgl, ids::lockPalette, colours::red);
    presetBox.addItemList (palettes::presetNames(), 1);
    presetBox.setSelectedItemIndex (proc.getPresetIndex(), dontSendNotification);
    presetBox.onChange = [this]
    {
        const int i = presetBox.getSelectedItemIndex();
        if (i < 0) return;
        proc.loadPreset (i);
        helpBar.setHelp (palettes::presetNames()[i], palettes::presetDescription (i));
    };
    content.addAndMakeVisible (presetBox);
    savePalBtn.onClick = [this]
    {
        chooser = std::make_unique<FileChooser> ("Save chord palette", File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("My Palette.chgpal"), "*.chgpal");
        chooser->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::canSelectFiles | FileBrowserComponent::warnAboutOverwriting,
            [this] (const FileChooser& fc) { auto f = fc.getResult(); if (f != File()) proc.savePaletteToFile (f.withFileExtension ("chgpal")); });
    };
    loadPalBtn.onClick = [this]
    {
        chooser = std::make_unique<FileChooser> ("Load chord palette", File::getSpecialLocation (File::userDocumentsDirectory), "*.chgpal");
        chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
            [this] (const FileChooser& fc) { auto f = fc.getResult(); if (f.existsAsFile()) proc.loadPaletteFromFile (f); });
    };
    resetPalBtn.onClick = [this] { proc.loadPreset (proc.getPresetIndex()); };
    for (auto* b : { &savePalBtn, &loadPalBtn, &resetPalBtn }) content.addAndMakeVisible (*b);

    // ---- centre
    content.addAndMakeVisible (display);
    content.addAndMakeVisible (pads);
    pads.onHover = [this] (int pad)
    {
        if (pad < 0) return;
        const auto o = proc.currentOptions();
        auto rc = theory::resolve (proc.getPad (pad), o);
        helpBar.setHelp ("Pad " + String (pad + 1) + ":  " + proc.padDisplayName (pad) + "  (" + theory::romanNumeral (rc, o) + ")",
                         theory::functionDescription (rc, o) + ".  Click to play, drag to move or into the progression, double-click to add, right-click to edit.");
    };
    pads.onAddToProgression = [this] (int pad)
    {
        auto spec = proc.getPad (pad);
        static const int beatTable[5] = { 1, 2, 3, 4, 8 };
        spec.beats = beatTable[jlimit (0, 4, (int) getParam (ids::beats))];
        proc.progressionAdd (spec);
    };

    stripViewport.setViewedComponent (&strip, false);
    stripViewport.setScrollBarsShown (false, true);
    stripViewport.setScrollBarThickness (7);
    content.addAndMakeVisible (stripViewport);
    strip.onHelp = [this] (const String& t, const String& x) { helpBar.setHelp (t, x); };

    recBtn.setClickingTogglesState (true);
    recBtn.getProperties().set ("accent", colours::red.toString());
    recBtn.onClick = [this] { proc.recording = recBtn.getToggleState(); };
    playBtn.setClickingTogglesState (true);
    playBtn.getProperties().set ("accent", colours::green.toString());
    playBtn.onClick = [this] { proc.setPlaying (playBtn.getToggleState()); };
    clearBtn.onClick = [this] { proc.progressionClear(); };
    exportBtn.onClick = [this]
    {
        if (proc.getProgression().empty()) { helpBar.setHelp ("Nothing to export", "Add some chords to the progression first."); return; }
        chooser = std::make_unique<FileChooser> ("Export progression as MIDI", File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("Changes progression.mid"), "*.mid");
        chooser->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::canSelectFiles | FileBrowserComponent::warnAboutOverwriting,
            [this] (const FileChooser& fc)
            {
                auto f = fc.getResult();
                if (f == File()) return;
                f = f.withFileExtension ("mid");
                f.deleteFile();
                FileOutputStream os (f);
                if (os.openedOk()) { proc.renderProgressionMidi().writeTo (os); helpBar.setHelp ("Exported", f.getFullPathName()); }
            });
    };
    for (auto* b : { &recBtn, &playBtn, &clearBtn, &exportBtn }) content.addAndMakeVisible (*b);
    content.addAndMakeVisible (dragHandle);
    content.addAndMakeVisible (bpmKnob);
    content.addAndMakeVisible (swingKnob);
    setupCombo (beatsBox, ids::beats);
    setupCombo (compBox, ids::comp);

    // ---- right
    for (auto* k : { &strumKnob, &humanizeKnob, &dynamicsKnob, &toneKnob, &releaseKnob, &chorusKnob, &reverbKnob, &motionKnob })
        content.addAndMakeVisible (*k);
    setupCombo (strumDirBox, ids::strumDir);
    setupCombo (lhInstBox, ids::lhInst);
    setupCombo (rhInstBox, ids::rhInst);
    internalTgl.setButtonText ("Internal");
    setupToggle (internalTgl, ids::internal, colours::green);
    setupToggle (midiOutTgl, ids::midiOut, colours::blue);
    masterFader.meter.getProperties().set ("wide", true);
    for (auto* f : { &chordFader, &melodyFader, &masterFader }) content.addAndMakeVisible (*f);
    panicBtn.getProperties().set ("accent", colours::red.toString());
    panicBtn.onClick = [this] { proc.panic(); playBtn.setToggleState (false, dontSendNotification); };
    content.addAndMakeVisible (panicBtn);

    // ---- melody
    setupToggle (snapTgl, ids::melSnap, colours::blue);
    setupToggle (whiteMapTgl, ids::whiteMap, colours::blue);
    setupToggle (showScaleTgl, ids::showScale, colours::blue);
    setupCombo (melModeBox, ids::melMode);
    setupCombo (harmonizeBox, ids::harmonize);
    setupCombo (melOctaveBox, ids::melOctave);
    setupCombo (zoneBox, ids::zone);

    content.addAndMakeVisible (keyboard);
    keyboard.onHover = [this] (int n)
    {
        if (n < 0) return;
        const int zone = proc.chordZoneStart();
        if (n >= zone && n < zone + palettes::numPads)
            helpBar.setHelp ("Chord key " + theory::noteNameWithOctave (n, 0, false) + "  ->  pad " + String (n - zone + 1) + ":  " + proc.padDisplayName (n - zone),
                             "Keys in the gold zone trigger whole chords. Gold-lit keys show the notes of the voicing that is sounding.");
        else if (n >= zone + palettes::numPads)
            helpBar.setHelp ("Melody key " + theory::noteNameWithOctave (n, 0, false),
                             "Keys in the blue zone play melody snapped to the current scale. Dots show the scale (gold dots are chord tones).");
        else helpBar.setHelp ("Below the chord zone", "These keys are not used. Move the CHORD ZONE lower to use them for chords.");
    };
    content.addAndMakeVisible (helpBar);

    // ---- help texts
    addHelp (keyBox, "Key", "The home key. Every pad is stored relative to the key, so changing it transposes the whole palette.");
    addHelp (majorBtn, "Major key", "Use the major-key palette: Imaj7, ii7, iii7, IVmaj7, V7, vi7, vii-half-diminished plus colour chords.");
    addHelp (minorBtn, "Minor key", "Use the minor-key palette: i-6 / i-maj7, ii-half-diminished, V7b9, bIIImaj7, bVImaj7 and jazz-minor colours.");
    for (int i = 0; i < 4; ++i) addHelp (levelBtns[i], levels[i] + " level", levelDescription (i).fromFirstOccurrenceOf ("  -  ", false, false));
    addHelp (modulateBtn, "Modulate", "Suggests related keys (relative minor, up a 4th, Coltrane major-thirds ...) and the pivot V7 chord that leads there.");
    addHelp (tipsBtn, "Pop-up help", "Turns hover pop-up tooltips on or off. The help bar at the bottom always explains whatever is under the mouse.");
    addHelp (menuBtn, "Menu", "Quick start guide, save / load palettes, MIDI export, panic and about.");
    addHelp (styleBox, "Voicing style", "How chords are spread across the keys. Auto picks the right style for the level: close (Beginner), rootless (Intermediate), quartal & rootless (Advanced), upper-structure & drop-2 (Conservatory).");
    addHelp (registerBox, "Chord register", "Moves all chord voicings down or up an octave.");
    addHelp (bassTgl, "Bass note", "Adds the root (or the slash bass) low in the left hand under rootless and close voicings.");
    addHelp (extTgl, "Extensions", "Adds 9ths, 11ths and 13ths allowed by the chord-scale for the selected level. Off gives plain seventh chords.");
    addHelp (altTgl, "Alterations", "Colours dominant chords: b9/b13 when they resolve to minor, fully altered (b9 #9 #11 b13) at Conservatory level.");
    addHelp (vlTgl, "Voice leading", "Chooses the inversion of each chord that moves the fewest semitones from the previous chord - the way pianists comp.");
    addHelp (reharmTgl, "Auto reharm", "Replaces every dominant chord with its tritone substitute (G7 -> Db7#11). The bass then walks down by half steps.");
    addHelp (latchTgl, "Latch", "Chords keep sounding after you let go of the key until the next chord is played.");
    addHelp (suggestTgl, "Suggest next", "Outlines the pads that are strong next chords (teal): ii -> V, V -> I, tritone subs, deceptive cadences.");
    addHelp (lockTgl, "Lock palette", "Keeps your edited pads when you change level or major/minor. Unlocked, the palette is rebuilt for the new level.");
    addHelp (presetBox, "Palette preset", "Loads a teaching palette into the 24 pads: level default, ii-V-I in all keys, Rhythm Changes, Coltrane Changes, modal, blues, neo-soul.");
    addHelp (savePalBtn, "Save palette", "Saves your 24 pads to a .chgpal file.");
    addHelp (loadPalBtn, "Load palette", "Loads a .chgpal palette file. You can also drag a .chgpal file onto the window.");
    addHelp (resetPalBtn, "Reset palette", "Restores the selected preset, discarding pad edits.");
    addHelp (display, "Chord display", "Shows the chord being played, its Roman numeral, harmonic function, the exact notes and scale degrees, the melody scale and good next chords.");
    addHelp (recBtn, "Record", "When lit, every chord you play from the pads is added to the end of the progression.");
    addHelp (playBtn, "Play / stop", "Loops the progression using the comping pattern and tempo (follows the host tempo inside a DAW). Keep playing melody on top!  Shortcut: SPACE.");
    addHelp (clearBtn, "Clear", "Removes all chords from the progression.");
    addHelp (exportBtn, "Export MIDI", "Saves the progression, voiced exactly as you hear it, as a standard MIDI file.");
    addHelp (dragHandle, "Drag MIDI", "Drag this handle into a DAW track (or your desktop) to drop the progression as a MIDI clip.");
    addHelp (bpmKnob, "Tempo", "Tempo of the progression player. Inside a DAW the host tempo is used automatically.");
    addHelp (swingKnob, "Swing", "How late the off-beat 'and' hits fall in the comping patterns (50% = straight, 66% = triplet swing).");
    addHelp (beatsBox, "Beats per chord", "Length given to new chords added to the progression. Right-click a chord in the strip to change its length.");
    addHelp (compBox, "Comp pattern", "Rhythm used by PLAY: sustained, Charleston, 2 & 4 stabs, push, Freddie Green quarter notes or ballad half notes.");
    addHelp (strumKnob, "Strum (cascade)", "Rolls the notes of each chord instead of striking them together, from 0 ms (block) to a slow 250 ms harp-like roll.");
    addHelp (strumDirBox, "Strum direction", "Roll chords upward, downward, or alternate direction each chord.");
    addHelp (humanizeKnob, "Humanize", "Adds small natural variations to timing and velocity so chords sound played, not programmed.");
    addHelp (dynamicsKnob, "Dynamics", "How much your playing velocity matters. 0% gives an even touch, 100% follows your keyboard exactly.");
    addHelp (lhInstBox, "Chord sound", "Built-in instrument for the left hand: electric piano, grand, organ, vibraphone or pad.");
    addHelp (rhInstBox, "Melody sound", "Built-in instrument for the right-hand melody.");
    addHelp (toneKnob, "Tone", "Brightness of both instruments: hammer hardness, FM bark, organ drawbars, pad filter.");
    addHelp (releaseKnob, "Release", "How long notes ring after you let go.");
    addHelp (chorusKnob, "Chorus", "Stereo chorus for a classic 70s electric-piano shimmer.");
    addHelp (reverbKnob, "Reverb", "Room reverb, from a dry club to a big hall.");
    addHelp (motionKnob, "Tremolo", "Suitcase-style stereo tremolo / auto-pan.");
    addHelp (internalTgl, "Internal sound", "Turn off to use Changes only as a MIDI generator for another instrument in your DAW.");
    addHelp (midiOutTgl, "MIDI out", "Sends the voiced chords (channel 1) and melody (channel 2) to the plugin's MIDI output so you can record them or drive other instruments.");
    addHelp (chordFader, "Chord volume", "Level of the left-hand chords. The LED meter shows their output.");
    addHelp (melodyFader, "Melody volume", "Level of the right-hand melody.");
    addHelp (masterFader, "Master volume", "Overall output level with stereo peak meter. Keep the peaks out of the red.");
    addHelp (panicBtn, "Panic", "Stops every sounding note and the progression player. Shortcut: ESC.");
    addHelp (snapTgl, "Snap to scale", "Right-hand notes that are outside the scale are moved to the nearest scale note, so every note fits the chord.");
    addHelp (whiteMapTgl, "White-key map", "Play every scale with only the white keys: C plays the key's root, D the 2nd degree, etc., in any key. Black keys play chord tones.");
    addHelp (showScaleTgl, "Show scale", "Shows the current right-hand scale as dots on the on-screen keyboard (gold = chord tones).");
    addHelp (melModeBox, "Melody scale", "Which notes the right hand snaps to: the key scale, the chord-scale (Dorian, Mixolydian, Altered ...), chord tones only, pentatonic, blues or bebop.");
    addHelp (harmonizeBox, "Harmonize", "Adds notes under each melody note: an octave, a diatonic third, or George Shearing style block chords.");
    addHelp (melOctaveBox, "Melody octave", "Transposes the melody up or down by octaves.");
    addHelp (zoneBox, "Chord zone", "Where the 24 chord keys start on your MIDI keyboard. Notes above the zone play melody.");
    addHelp (keyboard, "Keyboard", "Click to play. Gold zone = chord pads, blue zone = melody. Lit keys show what is sounding.");

    for (auto* c : content.getChildren()) c->setMouseClickGrabsKeyboardFocus (false);
    for (int i = 0; i < 4; ++i) levelBtns[i].setWantsKeyboardFocus (false);

    layout();
    toggleTooltips (true);
    addMouseListener (this, true);
    proc.addChangeListener (this);
    setWantsKeyboardFocus (true);

    setResizable (true, true);
    setResizeLimits (designW * 6 / 10, designH * 6 / 10, designW * 3 / 2, designH * 3 / 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) designW / (double) designH);
    setSize (1152, 774);

    // first-run quick start
    {
        PropertiesFile settings (settingsOptions());
        applyTextSize (settings.getIntValue ("textSize", LNF::textMedium), false);
        if (! settings.getBoolValue ("seenQuickStart", false))
        {
            showQuickStart (true);
            settings.setValue ("seenQuickStart", true);
            settings.saveIfNeeded();
        }
    }

    updateDisplay();
    startTimerHz (30);
}

ChangesEditor::~ChangesEditor()
{
    stopTimer();
    proc.removeChangeListener (this);
    removeMouseListener (this);
    tooltipWindow.reset();
    LookAndFeel::setDefaultLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

//==============================================================================
void ChangesEditor::setupCombo (ComboBox& box, const String& id)
{
    if (auto* p = dynamic_cast<AudioParameterChoice*> (proc.apvts.getParameter (id)))
        box.addItemList (p->choices, 1);
    content.addAndMakeVisible (box);
    comboAttachments.push_back (std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, id, box));
}

void ChangesEditor::setupToggle (ToggleButton& b, const String& id, Colour led)
{
    b.getProperties().set ("led", led.toString());
    content.addAndMakeVisible (b);
    buttonAttachments.push_back (std::make_unique<APVTS::ButtonAttachment> (proc.apvts, id, b));
}

void ChangesEditor::setParam (const String& id, float plain)
{
    if (auto* p = proc.apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (plain));
        p->endChangeGesture();
    }
}

float ChangesEditor::getParam (const String& id) const
{
    if (auto* v = proc.apvts.getRawParameterValue (id)) return v->load();
    return 0.0f;
}

void ChangesEditor::addHelp (Component& c, const String& title, const String& text)
{
    help[&c] = { title, text };
    if (auto* t = dynamic_cast<SettableTooltipClient*> (&c)) t->setTooltip (title + ":  " + text);
    if (auto* k = dynamic_cast<Knob*> (&c)) { help[&k->slider] = { title, text }; k->slider.setTooltip (title + ":  " + text); }
    if (auto* f = dynamic_cast<Fader*> (&c)) { help[&f->slider] = { title, text }; f->slider.setTooltip (title + ":  " + text); }
}

void ChangesEditor::showHelpFor (Component* c)
{
    while (c != nullptr && c != this)
    {
        auto it = help.find (c);
        if (it != help.end())
        {
            if (c == &pads || c == &keyboard || c == &strip) return;   // these give contextual help themselves
            helpBar.setHelp (it->second.title, it->second.text);
            return;
        }
        c = c->getParentComponent();
    }
}

void ChangesEditor::mouseEnter (const MouseEvent& e) { showHelpFor (e.eventComponent); }

void ChangesEditor::mouseDown (const MouseEvent& e)
{
    if (dynamic_cast<TextEditor*> (e.eventComponent) == nullptr && dynamic_cast<Label*> (e.eventComponent) == nullptr)
        grabKeyboardFocus();
}

void ChangesEditor::focusLost (FocusChangeType)
{
    for (auto& kv : qwertyDown) proc.keyboardState.noteOff (1, kv.second, 0.0f);
    qwertyDown.clear();
}

//==============================================================================
void ChangesEditor::toggleTooltips (bool on)
{
    if (on && tooltipWindow == nullptr) tooltipWindow = std::make_unique<TooltipWindow> (this, 900);
    if (! on) tooltipWindow.reset();
}

void ChangesEditor::showQuickStart (bool show)
{
    if (show)
    {
        quickStart = std::make_unique<QuickStartOverlay>();
        quickStart->onClose = [this] { MessageManager::callAsync ([sp = SafePointer<ChangesEditor> (this)] { if (sp != nullptr) sp->showQuickStart (false); }); };
        content.addAndMakeVisible (*quickStart);
        quickStart->setBounds (0, 0, designW, designH);
    }
    else quickStart.reset();
}

static PropertiesFile::Options settingsOptions()
{
    PropertiesFile::Options o; o.applicationName = "Changes"; o.filenameSuffix = ".settings";
    o.folderName = "JazzyT Audio" + String (File::getSeparatorString()) + "Changes";
    o.osxLibrarySubFolder = "Application Support";
    return o;
}

void ChangesEditor::applyTextSize (int size, bool save)
{
    LNF::setTextSize (size);
    if (save)
    {
        PropertiesFile settings (settingsOptions());
        settings.setValue ("textSize", size);
        settings.saveIfNeeded();
    }
    layout();
    content.sendLookAndFeelChange();
    pads.refreshNames();
    strip.refresh();
    lastSerial = -1;
    updateDisplay();
    content.repaint();
    static const char* names[] = { "Small", "Medium", "Large" };
    helpBar.setHelp ("Text size: " + String (names[jlimit (0, 2, size)]), "Change it any time from MENU > Text size. The setting is remembered.");
}

void ChangesEditor::showMainMenu()
{
    PopupMenu m;
    m.addSectionHeader ("Changes 1.0");
    m.addItem (1, "Quick start guide");
    m.addSeparator();
    m.addItem (2, "Save palette...");
    m.addItem (3, "Load palette...");
    m.addItem (4, "Export progression as MIDI...");
    m.addSeparator();
    PopupMenu textMenu;
    const int ts = LNF::getTextSize();
    textMenu.addItem (20, "Small (original)", true, ts == LNF::textSmall);
    textMenu.addItem (21, "Medium", true, ts == LNF::textMedium);
    textMenu.addItem (22, "Large", true, ts == LNF::textLarge);
    m.addSubMenu ("Text size", textMenu);
    m.addSeparator();
    m.addItem (5, "Panic (all notes off)");
    m.addItem (6, "About Changes");
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (menuBtn), [this] (int r)
    {
        switch (r)
        {
            case 1: showQuickStart (true); break;
            case 2: savePalBtn.triggerClick(); break;
            case 3: loadPalBtn.triggerClick(); break;
            case 4: exportBtn.triggerClick(); break;
            case 5: panicBtn.triggerClick(); break;
            case 20: case 21: case 22: applyTextSize (r - 20, true); break;
            case 6:
                AlertWindow::showMessageBoxAsync (MessageBoxIconType::InfoIcon, "Changes 1.0",
                    "Changes - Jazz Harmony Instrument\nVersion 1.0.0 (VST3 + Standalone)\n\n"
                    "One key per chord, an in-key melody, and jazz theory from beginner to conservatory level.\n"
                    "All sounds are synthesized in real time.\n\nBuilt with JUCE. VST is a trademark of Steinberg Media Technologies GmbH.");
                break;
            default: break;
        }
    });
}

void ChangesEditor::showModulationMenu()
{
    const auto opt = proc.currentOptions();
    auto mods = theory::modulationSuggestions (opt);
    PopupMenu m;
    m.addSectionHeader ("Modulate from " + theory::noteName (opt.keyTonic, opt.keyTonic, opt.minorKey) + (opt.minorKey ? " minor" : " major"));
    for (int i = 0; i < mods.size(); ++i) m.addItem (i + 1, mods[i].label + "      pivot: " + mods[i].pivot.upToFirstOccurrenceOf (" ", false, false));
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (modulateBtn), [this, mods] (int r)
    {
        if (r <= 0 || r > mods.size()) return;
        const auto& md = mods[r - 1];
        setParam (ids::key, (float) md.newTonic);
        setParam (ids::minor, md.newMinor ? 1.0f : 0.0f);
        modulationBanner = "Pivot: " + md.pivot;
        helpBar.setHelp ("Modulated: " + md.label.fromFirstOccurrenceOf ("->", false, false).trim(),
                         "To make the key change sound natural, play " + md.pivot + " just before the new I chord.");
    });
}

//==============================================================================
void ChangesEditor::layout()
{
    using namespace layoutInfo;
    gPanels.clear(); gCaptions.clear();
    auto cap = [] (const String& t, int x, int y, int w, Colour c = colours::textDim) { gCaptions.push_back ({ t, { x, y, w, 12 }, c }); };

    // header
    cap ("KEY", 252, 8, 60); cap ("MODE", 336, 8, 60); cap ("THEORY LEVEL", 494, 8, 200, colours::gold);
    keyBox.setBounds (250, 22, 78, 32);
    majorBtn.setBounds (334, 22, 70, 32);
    minorBtn.setBounds (406, 22, 70, 32);
    for (int i = 0; i < 4; ++i) levelBtns[i].setBounds (492 + i * 116, 22, 112, 32);
    modulateBtn.setBounds (968, 22, 112, 32);
    tipsBtn.setBounds (1086, 22, 40, 32);
    menuBtn.setBounds (1134, 22, 112, 32);

    // harmony panel
    gPanels.push_back ({ "HARMONY", { 32, 70, 250, 490 }, colours::gold });
    cap ("VOICING STYLE", 46, 96, 200);
    styleBox.setBounds (44, 110, 226, 28);
    cap ("CHORD REGISTER", 46, 146, 200);
    registerBox.setBounds (44, 160, 226, 28);
    ToggleButton* tg[] = { &bassTgl, &extTgl, &altTgl, &vlTgl, &reharmTgl, &latchTgl, &suggestTgl, &lockTgl };
    for (int i = 0; i < 8; ++i) tg[i]->setBounds (44 + (i % 2) * 116, 200 + (i / 2) * 34, 110, 28);
    cap ("PALETTE PRESET", 46, 342, 200);
    presetBox.setBounds (44, 356, 226, 28);
    savePalBtn.setBounds (44, 392, 72, 26);
    loadPalBtn.setBounds (121, 392, 72, 26);
    resetPalBtn.setBounds (198, 392, 72, 26);

    // centre
    display.setBounds (292, 70, 690, 130);
    gPanels.push_back ({ "CHORD PADS  -  LEFT HAND", { 292, 206, 690, 200 }, colours::gold });
    pads.setBounds (302, 228, 670, 172);
    gPanels.push_back ({ "PROGRESSION", { 292, 412, 690, 148 }, colours::green });
    recBtn.setBounds (302, 434, 56, 28);
    playBtn.setBounds (362, 434, 62, 28);
    clearBtn.setBounds (428, 434, 58, 28);
    exportBtn.setBounds (490, 434, 94, 28);
    dragHandle.setBounds (588, 434, 92, 28);
    beatsBox.setBounds (684, 434, 76, 28);
    compBox.setBounds (764, 434, 132, 28);
    stripViewport.setBounds (302, 468, 594, 86);
    strip.setSize (jmax (594, strip.getWidth()), 78);
    bpmKnob.setBounds (904, 428, 70, 64);
    swingKnob.setBounds (904, 492, 70, 64);

    // melody panel
    gPanels.push_back ({ "RIGHT HAND  -  MELODY", { 32, 566, 950, 120 }, colours::blue });
    snapTgl.setBounds (44, 592, 140, 28);
    whiteMapTgl.setBounds (190, 592, 140, 28);
    showScaleTgl.setBounds (336, 592, 124, 28);
    cap ("SCALE", 46, 628, 150); cap ("HARMONIZE", 206, 628, 150); cap ("OCTAVE", 406, 628, 80); cap ("CHORD ZONE", 496, 628, 120);
    melModeBox.setBounds (44, 642, 154, 28);
    harmonizeBox.setBounds (204, 642, 194, 28);
    melOctaveBox.setBounds (404, 642, 84, 28);
    zoneBox.setBounds (494, 642, 136, 28);

    // right column
    gPanels.push_back ({ "PERFORMANCE", { 992, 70, 256, 140 }, colours::gold });
    strumKnob.setBounds (1000, 90, 78, 76);
    humanizeKnob.setBounds (1082, 90, 78, 76);
    dynamicsKnob.setBounds (1164, 90, 78, 76);
    strumDirBox.setBounds (1004, 174, 232, 26);

    gPanels.push_back ({ "SOUND", { 992, 216, 256, 214 }, colours::blue });
    cap ("CHORDS", 1004, 246, 60); cap ("MELODY", 1004, 278, 60);
    lhInstBox.setBounds (1062, 240, 176, 26);
    rhInstBox.setBounds (1062, 272, 176, 26);
    toneKnob.setBounds (1000, 302, 78, 62);
    releaseKnob.setBounds (1082, 302, 78, 62);
    chorusKnob.setBounds (1164, 302, 78, 62);
    reverbKnob.setBounds (1000, 364, 78, 62);
    motionKnob.setBounds (1082, 364, 78, 62);
    internalTgl.setBounds (1164, 370, 78, 24);
    midiOutTgl.setBounds (1164, 398, 78, 24);

    gPanels.push_back ({ "MIXER", { 992, 436, 256, 250 }, colours::gold });
    chordFader.setBounds (1002, 456, 70, 194);
    melodyFader.setBounds (1076, 456, 70, 194);
    masterFader.setBounds (1150, 456, 90, 194);
    panicBtn.setBounds (1004, 654, 232, 24);

    const int helpH = LNF::getTextSize() == LNF::textSmall ? 26 : (LNF::getTextSize() == LNF::textMedium ? 34 : 40);
    keyboard.setBounds (32, 694, 1216, 154 - helpH);
    helpBar.setBounds (32, 854 - helpH, 1216, helpH);
}

void ChangesEditor::resized()
{
    const float scale = (float) getWidth() / (float) designW;
    content.setBounds (0, 0, designW, designH);
    content.setTransform (AffineTransform::scale (scale));
}

void ChangesEditor::paint (Graphics& g) { g.fillAll (colours::bg0); }

bool ChangesEditor::isInterestedInFileDrag (const StringArray& files)
{
    for (auto& f : files) if (f.endsWithIgnoreCase (".chgpal")) return true;
    return false;
}

void ChangesEditor::filesDropped (const StringArray& files, int, int)
{
    for (auto& f : files)
        if (f.endsWithIgnoreCase (".chgpal") && proc.loadPaletteFromFile (File (f)))
            helpBar.setHelp ("Palette loaded", File (f).getFileNameWithoutExtension());
}

//==============================================================================
juce::int64 ChangesEditor::optionsHash() const
{
    const auto o = proc.currentOptions();
    juce::int64 h = o.keyTonic;
    h = h * 3 + (o.minorKey ? 1 : 0);
    h = h * 5 + (int) o.level;
    h = h * 11 + (int) o.style;
    h = h * 2 + o.bassNote; h = h * 2 + o.extensions; h = h * 2 + o.alterations; h = h * 2 + o.voiceLeading; h = h * 2 + o.autoReharm;
    h = h * 5 + o.registerShift + 2;
    h = h * 7 + (int) getParam (ids::melMode);
    h = h * 4 + (int) getParam (ids::zone);
    h = h * 2 + (getParam (ids::suggest) > 0.5f);
    return h;
}

void ChangesEditor::changeListenerCallback (ChangeBroadcaster*)
{
    pads.refreshNames();
    strip.refresh();
    presetBox.setSelectedItemIndex (proc.getPresetIndex(), dontSendNotification);
    lastSerial = -1;
}

void ChangesEditor::updateDisplay()
{
    using namespace theory;
    const auto o = proc.currentOptions();
    const int padIdx = proc.currentPad.load();
    const ChordSpec spec = ChordSpec::unpack (proc.currentSpec.load());
    const bool hasChord = proc.chordSerial.load() > 0;
    const MelodyMode mode = proc.melodyMode();

    DisplayData d;
    d.hasChord = hasChord;
    d.level = levelName (o.level);
    d.banner = modulationBanner.isNotEmpty() ? "(" + modulationBanner + ")" : String();

    ResolvedChord rc;
    if (hasChord) rc = resolve (spec, o);
    else { ChordSpec s; s.quality = o.minorKey ? Quality::Min6 : Quality::Maj7; rc = resolve (s, o); }

    // voicing from what is actually sounding (or the last one we saw)
    Array<int> sounding;
    for (int n = 0; n < 128; ++n) if (proc.isNoteSounding (proc.soundingChord, n)) sounding.add (n);
    if (! sounding.isEmpty()) lastVoicedNotes = sounding;
    Voicing v;
    for (int n : lastVoicedNotes) v.add (n);
    if (hasChord && v.count > 0) addVoicedTensions (rc, v);

    if (hasChord)
    {
        d.name = chordName (rc, o);
        d.roman = romanNumeral (rc, o) + "   in " + noteName (o.keyTonic, o.keyTonic, o.minorKey) + (o.minorKey ? " minor" : " major");
        d.function = functionDescription (rc, o);
        d.level << "  |  " << styleName (effectiveStyle (spec, o, rc));
        String notes, degs;
        for (int n : lastVoicedNotes)
        {
            notes << chordToneName (rc, n, o) << "  ";
            const int iv = ((n - rc.rootPc) % 12 + 12) % 12;
            static const char* tension[12] = { "R", "b9", "9", "#9", "3", "11", "#11", "5", "b13", "13", "b7", "7" };
            String nm;
            const int t3 = qualityThird (rc.quality), t5 = qualityFifth (rc.quality), t7 = qualitySeventh (rc.quality);
            if (iv == 0) nm = "R";
            else if (iv == t3) nm = t3 == 3 ? "b3" : (t3 == 5 ? "4" : "3");
            else if (iv == t5) nm = t5 == 6 ? "b5" : (t5 == 8 ? "#5" : "5");
            else if (iv == t7) nm = t7 == 9 ? (rc.quality == Quality::Dim7 ? "bb7" : "6") : (t7 == 10 ? "b7" : "7");
            else nm = tension[iv];
            degs << nm << "  ";
        }
        d.notes = notes.trim();
        d.intervals = degs.trim();
    }

    const uint16_t mm = melodyMask (mode, rc, o);
    d.scale = scaleName (rc, o, mode);
    String scaleNotes;
    if (mode == MelodyMode::Key || mode == MelodyMode::Blues)
    {
        for (int k = 0; k < 12; ++k) if (maskHas (mm, o.keyTonic + k)) scaleNotes << noteName (o.keyTonic + k, o.keyTonic, o.minorKey) << " ";
        if (mode == MelodyMode::Key) { ChordSpec tonic; tonic.quality = o.minorKey ? Quality::Min7 : Quality::Maj7; scaleNotes = scaleNoteNames (resolve (tonic, o), mm, o); }
    }
    else scaleNotes = scaleNoteNames (rc, mm, o);
    content.melodyScaleText = scaleName (rc, o, mode) + "|" + scaleNotes.trim();

    // suggestions
    float scores[palettes::numPads] {};
    const bool suggest = getParam (ids::suggest) > 0.5f;
    if (hasChord && suggest)
    {
        palettes::Palette pal;
        for (int i = 0; i < palettes::numPads; ++i) pal[(size_t) i] = proc.getPad (i);
        suggestNext (spec, pal.data(), palettes::numPads, o, padIdx, scores);
        Array<int> order; for (int i = 0; i < palettes::numPads; ++i) if (scores[i] > 0.9f) order.add (i);
        std::sort (order.begin(), order.end(), [&] (int a, int b) { return scores[a] > scores[b]; });
        StringArray names; for (int i = 0; i < jmin (4, order.size()); ++i) names.addIfNotAlreadyThere (proc.padDisplayName (order[i]));
        d.next = names.isEmpty() ? String ("-") : names.joinIntoString ("   ");
    }
    else d.next = suggest ? "Play a chord to see suggestions" : "(Suggest Next is off)";
    if (! suggest) for (auto& s : scores) s = 0.0f;

    display.setData (d);
    pads.setLiveState (padIdx, scores, bank, suggest);
    keyboard.setScaleMask (mm, chordToneMask (rc), getParam (ids::showScale) > 0.5f);
    gLevelInfo = levelDescription ((int) o.level);
    content.repaint (Rectangle<int> (642, 590, 328, 86));
    content.repaint (Rectangle<int> (44, 432, 226, 116));
}

void ChangesEditor::timerCallback()
{
    // meters
    { const float c = proc.meterChord.exchange (0.0f); chordFader.meter.setLevels (c, c); }
    { const float c = proc.meterMelody.exchange (0.0f); melodyFader.meter.setLevels (c, c); }
    { const float l = proc.meterL.exchange (0.0f), r = proc.meterR.exchange (0.0f); masterFader.meter.setLevels (l, r); }

    // header state
    const bool minor = getParam (ids::minor) > 0.5f;
    majorBtn.setToggleState (! minor, dontSendNotification);
    minorBtn.setToggleState (minor, dontSendNotification);
    const int level = (int) getParam (ids::level);
    for (int i = 0; i < 4; ++i) levelBtns[i].setToggleState (i == level, dontSendNotification);
    playBtn.setToggleState (proc.isPlaying(), dontSendNotification);
    recBtn.setToggleState (proc.recording.load(), dontSendNotification);
    const bool hostTempo = proc.hostBpm.load() > 1.0f && ! JUCEApplicationBase::isStandaloneApp();
    if (hostTempo != content.hostTempo) { content.hostTempo = hostTempo; content.repaint (900, 412, 80, 20); }

    const int step = proc.playStep.load();
    if (step != lastPlayStep)
    {
        lastPlayStep = step;
        strip.setPlayingIndex (step);
        if (step >= 0) stripViewport.setViewPosition (jmax (0, step * (ProgressionStrip::chipW + ProgressionStrip::chipGap) - 200), 0);
    }

    // option changes rename pads
    const auto h = optionsHash();
    const bool optsChanged = h != lastOptionsHash;
    if (optsChanged) { lastOptionsHash = h; pads.refreshNames(); strip.refresh(); }

    // sounding notes
    bool bitsChanged = false;
    for (int i = 0; i < 4; ++i)
    {
        const uint32_t c = proc.soundingChord[i].load(), m = proc.soundingMelody[i].load();
        bitsChanged |= c != lastBits[i] || m != lastBits[4 + i];
        lastBits[i] = c; lastBits[4 + i] = m;
    }

    const int serial = proc.chordSerial.load();
    if (serial != lastSerial || optsChanged || bitsChanged)
    {
        lastSerial = serial;
        updateDisplay();
    }
    if (bitsChanged || optsChanged) keyboard.repaint();
    static int tick = 0;
    if (++tick % 2 == 0 && getParam (ids::suggest) > 0.5f) pads.repaint();   // gentle pulse on suggested pads
}

//==============================================================================
static const char* chordKeys = "zsxdcvgbhnjm";
static const char* melodyKeys = "q2w3er5t6y7ui9o0p";

bool ChangesEditor::keyStateChanged (bool)
{
    bool used = false;
    const int zone = proc.chordZoneStart();
    auto handle = [&] (int keyCode, int note)
    {
        const bool down = KeyPress::isKeyCurrentlyDown (keyCode);
        const auto it = qwertyDown.find (keyCode);
        if (down && it == qwertyDown.end())
        {
            if (ModifierKeys::currentModifiers.isCommandDown() || ModifierKeys::currentModifiers.isAltDown()) return;
            qwertyDown[keyCode] = note;
            proc.keyboardState.noteOn (1, note, 0.8f);
            used = true;
        }
        else if (! down && it != qwertyDown.end())
        {
            proc.keyboardState.noteOff (1, it->second, 0.0f);
            qwertyDown.erase (it);
            used = true;
        }
    };
    for (int i = 0; chordKeys[i] != 0; ++i) handle (chordKeys[i], zone + bank * 12 + i);
    for (int i = 0; melodyKeys[i] != 0; ++i) handle (melodyKeys[i], zone + palettes::numPads + i);
    return used;
}

bool ChangesEditor::keyPressed (const KeyPress& k)
{
    if (k == KeyPress::tabKey)
    {
        bank = 1 - bank;
        lastSerial = -1;
        helpBar.setHelp ("Pad bank " + String (bank + 1), bank == 0 ? "Computer keys Z-M now play pads 1-12." : "Computer keys Z-M now play pads 13-24.");
        return true;
    }
    if (k == KeyPress::spaceKey) { proc.setPlaying (! proc.isPlaying()); return true; }
    if (k == KeyPress::escapeKey) { proc.panic(); if (quickStart) showQuickStart (false); return true; }
    if (k == KeyPress::upKey) { setParam (ids::melOctave, jmin (4.0f, getParam (ids::melOctave) + 1)); return true; }
    if (k == KeyPress::downKey) { setParam (ids::melOctave, jmax (0.0f, getParam (ids::melOctave) - 1)); return true; }
    const juce_wchar c = CharacterFunctions::toLowerCase (k.getTextCharacter());
    if (c != 0 && (String (chordKeys).containsChar (c) || String (melodyKeys).containsChar (c))) return true;
    return false;
}

//==============================================================================
juce::AudioProcessorEditor* ChangesProcessor::createEditor() { return new ChangesEditor (*this); }
