#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace colours
{
    const juce::Colour bg0       { 0xff0a0e18 };
    const juce::Colour bg1       { 0xff111827 };
    const juce::Colour bg2       { 0xff1a2438 };
    const juce::Colour bg3       { 0xff243049 };
    const juce::Colour border    { 0xff2a3650 };
    const juce::Colour gold      { 0xffe2a94b };
    const juce::Colour goldLight { 0xffffd889 };
    const juce::Colour goldDark  { 0xff8f5f1c };
    const juce::Colour blue      { 0xff4fb9ff };
    const juce::Colour blueDark  { 0xff1b5f8f };
    const juce::Colour teal      { 0xff4fe0c8 };
    const juce::Colour red       { 0xffff4b4b };
    const juce::Colour green     { 0xff3ce67a };
    const juce::Colour text      { 0xffece6d6 };
    const juce::Colour textDim   { 0xff8d97ad };
    const juce::Colour textFaint { 0xff5b6680 };
}

/** The premium look: machined 3D knobs, recessed faders, LED toggles. */
class ChangesLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ChangesLookAndFeel();

    static juce::Font font (float height, bool bold = false);

    /** Global text size: 0 = Small (original), 1 = Medium, 2 = Large. */
    enum TextSize { textSmall = 0, textMedium = 1, textLarge = 2 };
    static void setTextSize (int size);
    static int getTextSize();
    static float scaledHeight (float designHeight);
    /** Lays out wrapped text, shrinking it until it fits the area, then draws it. */
    static void drawFittedLayout (juce::Graphics&, juce::Rectangle<float> area,
                                  const std::function<juce::AttributedString (float scale)>& build);
    juce::Typeface::Ptr getTypefaceForFont (const juce::Font&) override;
    static juce::Font displayFont (float height);

    // Knobs & faders
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
    juce::Slider::SliderLayout getSliderLayout (juce::Slider&) override;

    // Buttons
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;

    // Combo & menus
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>&, bool isSeparator, bool isActive, bool isHighlighted,
                            bool isTicked, bool hasSubMenu, const juce::String& text, const juce::String& shortcut,
                            const juce::Drawable* icon, const juce::Colour* textColour) override;
    juce::Font getPopupMenuFont() override;
    void drawPopupMenuSectionHeader (juce::Graphics&, const juce::Rectangle<int>&, const juce::String&) override;

    // Tooltips
    juce::Rectangle<int> getTooltipBounds (const juce::String& text, juce::Point<int> pos, juce::Rectangle<int> parentArea) override;
    void drawTooltip (juce::Graphics&, const juce::String& text, int width, int height) override;

    juce::Font getLabelFont (juce::Label&) override;
    void drawLabel (juce::Graphics&, juce::Label&) override;

    // Shared helpers used by the custom components
    static void drawPanel (juce::Graphics&, juce::Rectangle<float> r, const juce::String& title, juce::Colour accent = colours::gold);
    static void drawScrew (juce::Graphics&, juce::Point<float> c, float radius, float angle);
    static void drawRecess (juce::Graphics&, juce::Rectangle<float> r, float corner);
    static void drawLed (juce::Graphics&, juce::Point<float> c, float radius, juce::Colour colour, bool on);
};
