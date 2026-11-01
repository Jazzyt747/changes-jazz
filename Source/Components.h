#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"
#include "LookAndFeel.h"

//==============================================================================
/** Segmented LED meter with a green→yellow→red gradient and peak hold. */
class LedMeter : public juce::Component
{
public:
    LedMeter (bool vertical, int segments, bool stereo, juce::Colour tint = {});
    void setLevels (float left, float right);
    void paint (juce::Graphics&) override;
private:
    bool vertical, stereo; int segments; juce::Colour tint;
    float disp[2] {}, hold[2] {}; int holdTimer[2] {};
    static float toNorm (float gain);
};

//==============================================================================
class Knob : public juce::Component
{
public:
    enum class Format { Percent, Milliseconds, Decibels, Bpm, Plain, Swing };
    Knob (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& title, Format, juce::Colour accent = colours::gold);
    void resized() override;
    void lookAndFeelChanged() override { title.setFont (ChangesLookAndFeel::font (10.5f, true)); resized(); }
    juce::Slider slider;
    juce::Label title;
private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

class Fader : public juce::Component
{
public:
    Fader (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& title, bool stereoMeter, juce::Colour accent = colours::gold);
    void resized() override;
    void lookAndFeelChanged() override { title.setFont (ChangesLookAndFeel::font (10.5f, true)); resized(); }
    juce::Slider slider;
    juce::Label title;
    LedMeter meter;
private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

//==============================================================================
class HelpBar : public juce::Component
{
public:
    void setHelp (const juce::String& title, const juce::String& text);
    void paint (juce::Graphics&) override;
private:
    juce::String title { "Welcome to Changes" }, text { "Hover over any control for an explanation. Play chords with the left hand (pads), melody with the right hand." };
};

//==============================================================================
struct DisplayData
{
    juce::String name, roman, function, notes, intervals, scale, next, level, banner;
    bool hasChord = false;
};

class ChordDisplay : public juce::Component
{
public:
    void setData (const DisplayData& d) { data = d; repaint(); }
    void paint (juce::Graphics&) override;
private:
    DisplayData data;
};

//==============================================================================
class ChordPadGrid : public juce::Component, public juce::DragAndDropTarget
{
public:
    explicit ChordPadGrid (ChangesProcessor&);
    void refreshNames();
    void setLiveState (int currentPad, const float* suggestionScores, int bank, bool showSuggestions);

    std::function<void (int pad)> onHover;
    std::function<void (int pad)> onAddToProgression;

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragEnter (const SourceDetails&) override;
    void itemDragMove (const SourceDetails&) override;
    void itemDragExit (const SourceDetails&) override;
    void itemDropped (const SourceDetails&) override;

    juce::Rectangle<float> padBounds (int pad) const;
    int padAt (juce::Point<float>) const;

private:
    ChangesProcessor& proc;
    juce::String names[palettes::numPads], romans[palettes::numPads];
    float scores[palettes::numPads] {};
    int current = -1, hovered = -1, pressed = -1, dropTarget = -1, bank = 0;
    bool showSuggest = true, dragStarted = false;
    static theory::ChordSpec clipboard; static bool hasClipboard;
    void showPadMenu (int pad);
};

//==============================================================================
class ProgressionStrip : public juce::Component, public juce::DragAndDropTarget
{
public:
    explicit ProgressionStrip (ChangesProcessor&);
    void refresh();
    void setPlayingIndex (int i) { if (i != playing) { playing = i; repaint(); } }
    std::function<void (const juce::String&, const juce::String&)> onHelp;

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragMove (const SourceDetails&) override;
    void itemDragExit (const SourceDetails&) override;
    void itemDropped (const SourceDetails&) override;

    static constexpr int chipW = 88, chipGap = 6;
private:
    ChangesProcessor& proc;
    juce::StringArray names;
    juce::Array<int> beats;
    int playing = -1, hovered = -1, insertAt = -1, pressed = -1;
    bool dragStarted = false;
    juce::Rectangle<float> chipBounds (int i) const;
    juce::Rectangle<float> deleteBounds (int i) const;
    int chipAt (juce::Point<float>) const;
    int insertionIndexFor (juce::Point<float>) const;
    void showChipMenu (int i);
};

/** Drag this handle into your DAW (or desktop) to drop the progression as a .mid file. */
class DragMidiHandle : public juce::Component
{
public:
    explicit DragMidiHandle (ChangesProcessor& p) : proc (p) { setMouseCursor (juce::MouseCursor::DraggingHandCursor); }
    void paint (juce::Graphics&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
private:
    ChangesProcessor& proc;
    bool dragging = false;
};

//==============================================================================
class KeyboardView : public juce::Component
{
public:
    explicit KeyboardView (ChangesProcessor&);
    void setScaleMask (uint16_t melodyMask, uint16_t chordMask, bool show) { scaleMask = melodyMask; chordToneMask = chordMask; showScale = show; }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    std::function<void (int note)> onHover;
    void mouseMove (const juce::MouseEvent&) override;

    static constexpr int lowNote = 24, highNote = 96;
private:
    ChangesProcessor& proc;
    uint16_t scaleMask = 0, chordToneMask = 0; bool showScale = true;
    int mouseNote = -1;
    juce::Rectangle<float> keyBounds (int note) const;
    int noteAt (juce::Point<float>, float* velocity = nullptr) const;
    int whiteCount() const;
    static float topBand() { return juce::jmax (12.0f, ChangesLookAndFeel::scaledHeight (8.5f) + 4.0f); }
};
