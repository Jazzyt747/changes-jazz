#pragma once
#include "PluginProcessor.h"
#include "Components.h"
#include "LookAndFeel.h"

class QuickStartOverlay;

/** All UI lives in this fixed 1280x860 design canvas; the editor scales it. */
class EditorContent : public juce::Component
{
public:
    void paint (juce::Graphics&) override;
    juce::Image noise;
    juce::String melodyScaleText;
    bool hostTempo = false;
};

class ChangesEditor : public juce::AudioProcessorEditor,
                      public juce::DragAndDropContainer,
                      public juce::FileDragAndDropTarget,
                      private juce::Timer,
                      private juce::ChangeListener
{
public:
    explicit ChangesEditor (ChangesProcessor&);
    ~ChangesEditor() override;

    void resized() override;
    void paint (juce::Graphics&) override;
    bool keyPressed (const juce::KeyPress&) override;
    bool keyStateChanged (bool isKeyDown) override;
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void focusLost (FocusChangeType) override;
    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray&, int, int) override;

    static constexpr int designW = 1280, designH = 860;

private:
    ChangesProcessor& proc;
    ChangesLookAndFeel lnf;
    EditorContent content;

    using APVTS = juce::AudioProcessorValueTreeState;
    std::vector<std::unique_ptr<APVTS::ButtonAttachment>> buttonAttachments;
    std::vector<std::unique_ptr<APVTS::ComboBoxAttachment>> comboAttachments;

    // header
    juce::ComboBox keyBox;
    juce::TextButton majorBtn { "MAJOR" }, minorBtn { "MINOR" };
    juce::TextButton levelBtns[4];
    juce::TextButton modulateBtn { "MODULATE" }, tipsBtn { "?" }, menuBtn { "MENU" };

    // harmony panel
    juce::ComboBox styleBox, registerBox, presetBox;
    juce::ToggleButton bassTgl { "Bass Note" }, extTgl { "Extensions" }, altTgl { "Alterations" }, vlTgl { "Voice Leading" },
                       reharmTgl { "Auto Reharm" }, latchTgl { "Latch" }, suggestTgl { "Suggest Next" }, lockTgl { "Lock Palette" };
    juce::TextButton savePalBtn { "SAVE" }, loadPalBtn { "LOAD" }, resetPalBtn { "RESET" };

    // centre
    ChordDisplay display;
    ChordPadGrid pads;
    juce::Viewport stripViewport;
    ProgressionStrip strip;
    juce::TextButton recBtn { "REC" }, playBtn { "PLAY" }, clearBtn { "CLEAR" }, exportBtn { "EXPORT .MID" };
    DragMidiHandle dragHandle;
    Knob bpmKnob, swingKnob;
    juce::ComboBox beatsBox, compBox;

    // right
    Knob strumKnob, humanizeKnob, dynamicsKnob;
    juce::ComboBox strumDirBox, lhInstBox, rhInstBox;
    Knob toneKnob, releaseKnob, chorusKnob, reverbKnob, motionKnob;
    juce::ToggleButton internalTgl { "Internal Sound" }, midiOutTgl { "MIDI Out" };
    Fader chordFader, melodyFader, masterFader;
    juce::TextButton panicBtn { "PANIC" };

    // melody
    juce::ToggleButton snapTgl { "Snap to Scale" }, whiteMapTgl { "White-Key Map" }, showScaleTgl { "Show Scale" };
    juce::ComboBox melModeBox, harmonizeBox, melOctaveBox, zoneBox;

    KeyboardView keyboard;
    HelpBar helpBar;
    std::unique_ptr<juce::TooltipWindow> tooltipWindow;
    std::unique_ptr<QuickStartOverlay> quickStart;
    std::unique_ptr<juce::FileChooser> chooser;

    // help registry
    struct HelpEntry { juce::String title, text; };
    std::map<juce::Component*, HelpEntry> help;
    void addHelp (juce::Component&, const juce::String& title, const juce::String& text);
    void showHelpFor (juce::Component*);

    // live state
    int lastSerial = -1; juce::int64 lastOptionsHash = -1; uint32_t lastBits[8] {}; int bank = 0;
    int lastPlayStep = -2;
    juce::Array<int> lastVoicedNotes;
    juce::String modulationBanner;
    std::map<int, int> qwertyDown;   // keycode -> note

    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void setupCombo (juce::ComboBox&, const juce::String& paramId);
    void setupToggle (juce::ToggleButton&, const juce::String& paramId, juce::Colour led = colours::gold);
    void setParam (const juce::String& id, float plainValue);
    float getParam (const juce::String& id) const;
    void updateDisplay();
    void showMainMenu();
    void showModulationMenu();
    void layout();
    void toggleTooltips (bool on);
    void applyTextSize (int size, bool save);
    void showQuickStart (bool show);
    juce::int64 optionsHash() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChangesEditor)
};
