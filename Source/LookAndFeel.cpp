#include "LookAndFeel.h"
#include "BinaryData.h"

using namespace juce;

namespace
{
struct EmbeddedFonts
{
    Typeface::Ptr regular, medium, bold;
    EmbeddedFonts()
    {
        regular = Typeface::createSystemTypefaceFor (BinaryData::PoppinsRegular_ttf, (size_t) BinaryData::PoppinsRegular_ttfSize);
        medium  = Typeface::createSystemTypefaceFor (BinaryData::PoppinsMedium_ttf,  (size_t) BinaryData::PoppinsMedium_ttfSize);
        bold    = Typeface::createSystemTypefaceFor (BinaryData::PoppinsBold_ttf,    (size_t) BinaryData::PoppinsBold_ttfSize);
    }
};
EmbeddedFonts& fonts() { static EmbeddedFonts f; return f; }
}

ChangesLookAndFeel::ChangesLookAndFeel()
{
    setColour (ResizableWindow::backgroundColourId, colours::bg0);
    setColour (Label::textColourId, colours::text);
    setColour (Slider::textBoxTextColourId, colours::goldLight);
    setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    setColour (ComboBox::textColourId, colours::text);
    setColour (ComboBox::arrowColourId, colours::gold);
    setColour (PopupMenu::backgroundColourId, colours::bg1);
    setColour (PopupMenu::textColourId, colours::text);
    setColour (PopupMenu::highlightedBackgroundColourId, colours::goldDark);
    setColour (PopupMenu::highlightedTextColourId, Colours::white);
    setColour (TextButton::textColourOffId, colours::text);
    setColour (TextButton::textColourOnId, Colours::black);
    setColour (ToggleButton::textColourId, colours::text);
    setColour (TooltipWindow::textColourId, colours::text);
    setColour (ScrollBar::thumbColourId, colours::gold.withAlpha (0.5f));
    setColour (AlertWindow::backgroundColourId, colours::bg1);
    setColour (AlertWindow::textColourId, colours::text);
    setColour (TextEditor::backgroundColourId, colours::bg0);
    setColour (TextEditor::textColourId, colours::text);
    setColour (TextEditor::outlineColourId, colours::border);
    setColour (CaretComponent::caretColourId, colours::gold);
    setColour (ListBox::backgroundColourId, colours::bg1);
    setColour (DirectoryContentsDisplayComponent::highlightColourId, colours::goldDark);
    setColour (FileChooserDialogBox::titleTextColourId, colours::text);
}

static std::atomic<int> gTextSize { ChangesLookAndFeel::textSmall };

void ChangesLookAndFeel::setTextSize (int size) { gTextSize = jlimit (0, 2, size); }
int ChangesLookAndFeel::getTextSize() { return gTextSize.load(); }

float ChangesLookAndFeel::scaledHeight (float h)
{
    // Large headline text is already readable; only small and body text grows.
    // Poppins' line height is ~1.4x its em size, so these steps are deliberately generous.
    static const float factor[3]  = { 1.0f, 1.28f, 1.55f };
    static const float minimum[3] = { 0.0f, 14.0f, 16.5f };
    const int s = gTextSize.load();
    if (s == textSmall || h >= 24.0f) return h;
    return jmin (jmax (h * factor[s], minimum[s]), jmax (h, s == textLarge ? 26.0f : 24.0f));
}

Font ChangesLookAndFeel::font (float height, bool bold)
{
    height = scaledHeight (height);
    auto tf = bold ? fonts().medium : fonts().regular;
    if (tf == nullptr) return Font (height, bold ? Font::bold : Font::plain);
    return Font (tf).withHeight (height);
}

Font ChangesLookAndFeel::displayFont (float height)
{
    height = scaledHeight (height);
    if (fonts().bold == nullptr) return Font (height, Font::bold);
    return Font (fonts().bold).withHeight (height);
}

static Font shrinkToFit (Font f, const String& text, float width, float minHScale = 0.82f)
{
    while (f.getHeight() > 9.0f && f.getStringWidthFloat (text) * minHScale > width)
        f = f.withHeight (f.getHeight() - 0.5f);
    return f;
}

void ChangesLookAndFeel::drawFittedLayout (Graphics& g, Rectangle<float> area, const std::function<AttributedString (float)>& build)
{
    TextLayout tl;
    for (float scale = 1.0f; scale > 0.55f; scale -= 0.05f)
    {
        tl.createLayout (build (scale), area.getWidth());
        if (tl.getHeight() <= area.getHeight()) break;
    }
    tl.draw (g, area);
}

Typeface::Ptr ChangesLookAndFeel::getTypefaceForFont (const Font& f)
{
    if (f.getTypefaceName() == Font::getDefaultSansSerifFontName() && fonts().regular != nullptr)
        return f.isBold() ? fonts().medium : fonts().regular;
    return LookAndFeel_V4::getTypefaceForFont (f);
}

//==============================================================================
void ChangesLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float start, float end, Slider& s)
{
    const auto area = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2.0f);
    const float size = jmin (area.getWidth(), area.getHeight());
    const float R = size * 0.5f;
    const auto c = area.getCentre();
    const float angle = start + pos * (end - start);
    const bool enabled = s.isEnabled();
    const Colour accent = s.getProperties().contains ("accent") ? Colour::fromString (s.getProperties()["accent"].toString()) : colours::gold;

    // 1. soft drop shadow on the panel
    {
        ColourGradient sh (Colours::black.withAlpha (0.55f), c.x, c.y + R * 0.18f, Colours::transparentBlack, c.x, c.y + R * 1.25f, true);
        g.setGradientFill (sh);
        g.fillEllipse (c.x - R * 1.05f, c.y - R * 0.85f, R * 2.1f, R * 2.2f);
    }

    // 2. recessed well that hosts the value ring
    {
        const float r = R * 0.98f;
        ColourGradient well (Colour (0xff04060b), c.x, c.y - r, Colour (0xff222b40), c.x, c.y + r, false);
        g.setGradientFill (well);
        g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
        g.setColour (Colours::white.withAlpha (0.06f));
        g.drawEllipse (c.x - r, c.y - r, r * 2, r * 2, 1.0f);
    }

    // 3. value arc (LED ring)
    {
        const float ar = R * 0.86f, th = jmax (2.5f, R * 0.11f);
        Path track; track.addCentredArc (c.x, c.y, ar, ar, 0.0f, start, end, true);
        g.setColour (Colour (0xff0d1220));
        g.strokePath (track, PathStrokeType (th, PathStrokeType::curved, PathStrokeType::rounded));

        if (enabled)
        {
            const bool bipolar = s.getProperties().contains ("bipolar");
            const float from = bipolar ? (start + end) * 0.5f : start;
            Path val; val.addCentredArc (c.x, c.y, ar, ar, 0.0f, jmin (from, angle), jmax (from, angle), true);
            g.setColour (accent.withAlpha (0.22f));
            g.strokePath (val, PathStrokeType (th * 2.6f, PathStrokeType::curved, PathStrokeType::rounded));
            ColourGradient vg (accent.darker (0.5f), c.x - R, c.y + R, accent.brighter (0.6f), c.x + R, c.y - R, false);
            g.setGradientFill (vg);
            g.strokePath (val, PathStrokeType (th, PathStrokeType::curved, PathStrokeType::rounded));
        }
    }

    // 4. knob skirt (dark machined body)
    const float skirt = R * 0.70f;
    {
        Path body; body.addEllipse (c.x - skirt, c.y - skirt, skirt * 2, skirt * 2);
        g.setColour (Colours::black.withAlpha (0.6f));
        g.fillEllipse (c.x - skirt, c.y - skirt + R * 0.06f, skirt * 2, skirt * 2);
        ColourGradient bg (Colour (0xff4a5368), c.x - skirt * 0.7f, c.y - skirt, Colour (0xff0b0e15), c.x + skirt * 0.6f, c.y + skirt, false);
        g.setGradientFill (bg);
        g.fillPath (body);

        // knurled grip
        const int teeth = 40;
        for (int i = 0; i < teeth; ++i)
        {
            const float a = angle + MathConstants<float>::twoPi * (float) i / (float) teeth;
            const float sa = std::sin (a), ca = -std::cos (a);
            const float light = 0.5f + 0.5f * std::cos (a + MathConstants<float>::pi * 0.75f);
            g.setColour (Colours::white.withAlpha (0.05f + 0.13f * light));
            g.drawLine (c.x + sa * skirt * 0.88f, c.y + ca * skirt * 0.88f, c.x + sa * skirt * 0.99f, c.y + ca * skirt * 0.99f, 1.0f);
        }
        g.setColour (Colours::white.withAlpha (0.12f));
        g.drawEllipse (c.x - skirt, c.y - skirt, skirt * 2, skirt * 2, 0.8f);
    }

    // 5. brushed metal cap
    const float cap = R * 0.54f;
    {
        ColourGradient cg (Colour (0xffe8ebf0), c.x - cap * 0.6f, c.y - cap, Colour (0xff59606e), c.x + cap * 0.5f, c.y + cap, false);
        cg.addColour (0.5, Colour (0xffaab1bd));
        g.setGradientFill (cg);
        g.fillEllipse (c.x - cap, c.y - cap, cap * 2, cap * 2);
        for (int i = 1; i < 6; ++i)
        {
            const float rr = cap * (float) i / 6.0f;
            g.setColour (Colours::black.withAlpha (0.035f));
            g.drawEllipse (c.x - rr, c.y - rr, rr * 2, rr * 2, 0.7f);
        }
        ColourGradient hl (Colours::white.withAlpha (0.75f), c.x - cap * 0.35f, c.y - cap * 0.45f, Colours::transparentWhite, c.x - cap * 0.35f, c.y + cap * 0.2f, true);
        g.setGradientFill (hl);
        g.fillEllipse (c.x - cap * 0.9f, c.y - cap * 0.95f, cap * 1.1f, cap * 0.9f);
        g.setColour (Colours::black.withAlpha (0.5f));
        g.drawEllipse (c.x - cap, c.y - cap, cap * 2, cap * 2, 1.0f);
    }

    // 6. pointer
    {
        const float sa = std::sin (angle), ca = -std::cos (angle);
        const Point<float> p1 (c.x + sa * cap * 0.25f, c.y + ca * cap * 0.25f), p2 (c.x + sa * skirt * 0.95f, c.y + ca * skirt * 0.95f);
        g.setColour (Colours::black.withAlpha (0.55f));
        g.drawLine (p1.x + 0.8f, p1.y + 0.8f, p2.x + 0.8f, p2.y + 0.8f, jmax (2.0f, R * 0.09f));
        g.setColour (enabled ? accent.brighter (0.4f) : colours::textFaint);
        g.drawLine (p1.x, p1.y, p2.x, p2.y, jmax (1.6f, R * 0.07f));
        g.setColour (accent.withAlpha (0.35f));
        g.fillEllipse (p2.x - 3.0f, p2.y - 3.0f, 6.0f, 6.0f);
    }

    if (! enabled) { g.setColour (colours::bg0.withAlpha (0.45f)); g.fillEllipse (c.x - R, c.y - R, R * 2, R * 2); }
}

void ChangesLookAndFeel::drawLinearSlider (Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                           Slider::SliderStyle style, Slider& s)
{
    const Colour accent = s.getProperties().contains ("accent") ? Colour::fromString (s.getProperties()["accent"].toString()) : colours::gold;
    if (style != Slider::LinearVertical)
    {
        // compact horizontal slider
        auto r = Rectangle<float> ((float) x, (float) y + h * 0.5f - 3.0f, (float) w, 6.0f);
        drawRecess (g, r, 3.0f);
        auto filled = r.withRight (pos);
        g.setColour (accent); g.fillRoundedRectangle (filled.reduced (1.0f), 2.0f);
        const float tx = pos;
        Rectangle<float> thumb (tx - 7.0f, (float) y + h * 0.5f - 9.0f, 14.0f, 18.0f);
        ColourGradient tg (Colour (0xffd9dde5), thumb.getX(), thumb.getY(), Colour (0xff59606e), thumb.getX(), thumb.getBottom(), false);
        g.setGradientFill (tg); g.fillRoundedRectangle (thumb, 3.0f);
        return;
    }

    const float cx = x + w * 0.5f;
    const auto slot = Rectangle<float> (cx - 3.5f, (float) y, 7.0f, (float) h);

    // scale ticks
    for (int i = 0; i <= 10; ++i)
    {
        const float ty = y + h * (float) i / 10.0f;
        const float len = (i % 5 == 0) ? 9.0f : 5.0f;
        g.setColour (colours::textFaint.withAlpha (i % 5 == 0 ? 0.9f : 0.5f));
        g.drawLine (cx - 12.0f - len, ty, cx - 12.0f, ty, 1.0f);
        g.drawLine (cx + 12.0f, ty, cx + 12.0f + len, ty, 1.0f);
    }

    drawRecess (g, slot, 3.5f);
    g.setColour (accent.withAlpha (0.55f));
    g.fillRoundedRectangle (Rectangle<float> (cx - 1.5f, pos, 3.0f, (float) (y + h) - pos), 1.5f);

    // fader cap
    const float tw = jmin (40.0f, (float) w - 4.0f), th = 22.0f;
    Rectangle<float> cap (cx - tw * 0.5f, pos - th * 0.5f, tw, th);
    g.setColour (Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (cap.translated (0, 3.0f).expanded (1.0f), 4.0f);
    ColourGradient cg (Colour (0xff626b7d), cap.getX(), cap.getY(), Colour (0xff1a1f2b), cap.getX(), cap.getBottom(), false);
    cg.addColour (0.45, Colour (0xff3a4152));
    cg.addColour (0.5, Colour (0xff151922));
    cg.addColour (0.55, Colour (0xff2c3240));
    g.setGradientFill (cg);
    g.fillRoundedRectangle (cap, 4.0f);
    g.setColour (Colours::white.withAlpha (0.25f));
    g.drawLine (cap.getX() + 3, cap.getY() + 1.2f, cap.getRight() - 3, cap.getY() + 1.2f, 1.0f);
    g.setColour (accent.brighter (0.3f));
    g.fillRect (cap.getX() + 4, cap.getCentreY() - 1.0f, cap.getWidth() - 8, 2.0f);
    g.setColour (accent.withAlpha (0.25f));
    g.fillRect (cap.getX() + 2, cap.getCentreY() - 3.0f, cap.getWidth() - 4, 6.0f);
    g.setColour (Colours::black.withAlpha (0.8f));
    g.drawRoundedRectangle (cap, 4.0f, 1.0f);
}

Label* ChangesLookAndFeel::createSliderTextBox (Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (font (11.5f, true));
    l->setMinimumHorizontalScale (0.7f);
    l->setColour (Label::textColourId, colours::goldLight);
    l->setColour (Label::backgroundColourId, Colours::transparentBlack);
    l->setColour (Label::outlineColourId, Colours::transparentBlack);
    l->setColour (Label::textWhenEditingColourId, colours::text);
    l->setColour (Label::backgroundWhenEditingColourId, colours::bg0);
    l->setJustificationType (Justification::centred);
    return l;
}

Slider::SliderLayout ChangesLookAndFeel::getSliderLayout (Slider& s)
{
    auto layout = LookAndFeel_V4::getSliderLayout (s);
    return layout;
}

//==============================================================================
void ChangesLookAndFeel::drawRecess (Graphics& g, Rectangle<float> r, float corner)
{
    ColourGradient rg (Colour (0xff020308), r.getX(), r.getY(), Colour (0xff1d2537), r.getX(), r.getBottom(), false);
    g.setGradientFill (rg);
    g.fillRoundedRectangle (r, corner);
    g.setColour (Colours::white.withAlpha (0.07f));
    g.drawLine (r.getX() + corner, r.getBottom() - 0.5f, r.getRight() - corner, r.getBottom() - 0.5f, 1.0f);
    g.setColour (Colours::black.withAlpha (0.7f));
    g.drawRoundedRectangle (r, corner, 1.0f);
}

void ChangesLookAndFeel::drawLed (Graphics& g, Point<float> c, float radius, Colour colour, bool on)
{
    if (on)
    {
        ColourGradient halo (colour.withAlpha (0.55f), c.x, c.y, colour.withAlpha (0.0f), c.x + radius * 3.2f, c.y, true);
        g.setGradientFill (halo);
        g.fillEllipse (c.x - radius * 3.2f, c.y - radius * 3.2f, radius * 6.4f, radius * 6.4f);
    }
    ColourGradient body (on ? colour.brighter (0.9f) : colour.darker (2.2f), c.x - radius * 0.3f, c.y - radius * 0.4f,
                         on ? colour : colour.darker (3.2f), c.x + radius, c.y + radius, true);
    g.setGradientFill (body);
    g.fillEllipse (c.x - radius, c.y - radius, radius * 2, radius * 2);
    g.setColour (Colours::white.withAlpha (on ? 0.8f : 0.18f));
    g.fillEllipse (c.x - radius * 0.45f, c.y - radius * 0.6f, radius * 0.6f, radius * 0.45f);
    g.setColour (Colours::black.withAlpha (0.7f));
    g.drawEllipse (c.x - radius, c.y - radius, radius * 2, radius * 2, 0.8f);
}

void ChangesLookAndFeel::drawToggleButton (Graphics& g, ToggleButton& b, bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    const Colour led = b.getProperties().contains ("led") ? Colour::fromString (b.getProperties()["led"].toString()) : colours::gold;

    ColourGradient bg (Colour (0xff1c2538).brighter (highlighted ? 0.12f : 0.0f), r.getX(), r.getY(),
                       Colour (0xff0d121d), r.getX(), r.getBottom(), false);
    g.setGradientFill (bg);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (on ? led.withAlpha (0.45f) : colours::border);
    g.drawRoundedRectangle (r, 5.0f, 1.0f);
    g.setColour (Colours::white.withAlpha (0.06f));
    g.drawLine (r.getX() + 5, r.getY() + 1, r.getRight() - 5, r.getY() + 1, 1.0f);
    if (down) { g.setColour (Colours::black.withAlpha (0.25f)); g.fillRoundedRectangle (r, 5.0f); }

    drawLed (g, { r.getX() + 12.0f, r.getCentreY() }, 4.2f, led, on);

    g.setColour (on ? colours::text : colours::textDim);
    auto tr = r.withTrimmedLeft (24.0f).withTrimmedRight (4.0f);
    g.setFont (shrinkToFit (font (jmin (13.0f, r.getHeight() * 0.48f), on), b.getButtonText(), tr.getWidth()));
    g.drawFittedText (b.getButtonText(), tr.toNearestInt(), Justification::centredLeft, 1, 0.82f);
}

void ChangesLookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    const Colour accent = b.getProperties().contains ("accent") ? Colour::fromString (b.getProperties()["accent"].toString()) : colours::gold;
    const float corner = 5.0f;

    g.setColour (Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (r.translated (0, down ? 0.5f : 1.5f), corner);

    if (on)
    {
        ColourGradient og (accent.brighter (0.35f), r.getX(), r.getY(), accent.darker (0.45f), r.getX(), r.getBottom(), false);
        g.setGradientFill (og);
        g.fillRoundedRectangle (r, corner);
        g.setColour (accent.withAlpha (0.35f));
        g.drawRoundedRectangle (r.expanded (1.5f), corner + 1.5f, 2.0f);
    }
    else
    {
        ColourGradient bg (Colour (0xff2c3650).brighter (highlighted ? 0.15f : 0.0f), r.getX(), r.getY(),
                           Colour (0xff121826), r.getX(), r.getBottom(), false);
        g.setGradientFill (bg);
        g.fillRoundedRectangle (r, corner);
    }
    g.setColour (Colours::white.withAlpha (on ? 0.45f : 0.12f));
    g.drawLine (r.getX() + corner, r.getY() + 1.0f, r.getRight() - corner, r.getY() + 1.0f, 1.0f);
    g.setColour (Colours::black.withAlpha (0.8f));
    g.drawRoundedRectangle (r, corner, 1.0f);
    if (down) { g.setColour (Colours::black.withAlpha (0.2f)); g.fillRoundedRectangle (r, corner); }
}

void ChangesLookAndFeel::drawButtonText (Graphics& g, TextButton& b, bool, bool)
{
    const bool on = b.getToggleState();
    auto tr = b.getLocalBounds().reduced (4, 2);
    g.setFont (shrinkToFit (getTextButtonFont (b, b.getHeight()), b.getButtonText(), (float) tr.getWidth(), 0.8f));
    g.setColour (on ? Colour (0xff15100a) : (b.isEnabled() ? colours::text : colours::textFaint));
    g.drawFittedText (b.getButtonText(), tr, Justification::centred, 1, 0.8f);
}

Font ChangesLookAndFeel::getTextButtonFont (TextButton&, int h) { return font (jmin (13.5f, h * 0.48f), true); }

//==============================================================================
void ChangesLookAndFeel::drawComboBox (Graphics& g, int w, int h, bool, int, int, int, int, ComboBox& box)
{
    auto r = Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    ColourGradient bg (Colour (0xff222c42), 0, 0, Colour (0xff0f1420), 0, (float) h, false);
    g.setGradientFill (bg);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (box.hasKeyboardFocus (true) || box.isMouseOver (true) ? colours::gold.withAlpha (0.6f) : colours::border);
    g.drawRoundedRectangle (r, 5.0f, 1.0f);
    g.setColour (Colours::white.withAlpha (0.07f));
    g.drawLine (5, 1.2f, w - 5.0f, 1.2f, 1.0f);

    Path arrow;
    const float ax = w - 14.0f, ay = h * 0.5f;
    arrow.addTriangle (ax - 4.5f, ay - 2.5f, ax + 4.5f, ay - 2.5f, ax, ay + 3.0f);
    g.setColour (colours::gold);
    g.fillPath (arrow);
}

Font ChangesLookAndFeel::getComboBoxFont (ComboBox& b) { return font (jmin (13.5f, b.getHeight() * 0.5f)).withHeight (jmin (scaledHeight (jmin (13.5f, b.getHeight() * 0.5f)), b.getHeight() * 0.68f)); }

void ChangesLookAndFeel::positionComboBoxText (ComboBox& box, Label& label)
{
    label.setBounds (6, 1, box.getWidth() - 26, box.getHeight() - 2);
    label.setFont (shrinkToFit (getComboBoxFont (box), box.getText(), (float) box.getWidth() - 30.0f, 0.75f));
    label.setMinimumHorizontalScale (0.75f);
}

void ChangesLookAndFeel::drawPopupMenuBackground (Graphics& g, int w, int h)
{
    ColourGradient bg (colours::bg2, 0, 0, colours::bg0, 0, (float) h, false);
    g.setGradientFill (bg);
    g.fillAll();
    g.setColour (colours::gold.withAlpha (0.35f));
    g.drawRect (0, 0, w, h, 1);
}

void ChangesLookAndFeel::drawPopupMenuItem (Graphics& g, const Rectangle<int>& area, bool isSeparator, bool isActive, bool isHighlighted,
                                            bool isTicked, bool hasSubMenu, const String& text, const String& shortcut,
                                            const Drawable* icon, const Colour* textColour)
{
    if (isSeparator)
    {
        g.setColour (colours::border);
        g.fillRect (area.reduced (8, 0).withHeight (1).withY (area.getCentreY()));
        return;
    }
    auto r = area.reduced (2, 1).toFloat();
    if (isHighlighted && isActive)
    {
        ColourGradient hg (colours::gold.withAlpha (0.9f), r.getX(), r.getY(), colours::goldDark, r.getX(), r.getBottom(), false);
        g.setGradientFill (hg);
        g.fillRoundedRectangle (r, 3.0f);
    }
    g.setColour (! isActive ? colours::textFaint : (isHighlighted ? Colour (0xff120c05) : (textColour != nullptr ? *textColour : colours::text)));
    g.setFont (getPopupMenuFont());
    auto tr = area.reduced (26, 0);
    g.drawFittedText (text, tr, Justification::centredLeft, 1);
    if (isTicked) drawLed (g, { (float) area.getX() + 13.0f, (float) area.getCentreY() }, 4.0f, colours::gold, true);
    if (hasSubMenu)
    {
        Path p; const float x = (float) area.getRight() - 14.0f, y = (float) area.getCentreY();
        p.addTriangle (x, y - 4, x, y + 4, x + 5, y);
        g.fillPath (p);
    }
    if (shortcut.isNotEmpty()) { g.setFont (font (12.0f)); g.drawText (shortcut, area.reduced (12, 0), Justification::centredRight); }
    ignoreUnused (icon);
}

Font ChangesLookAndFeel::getPopupMenuFont() { return font (14.0f); }

void ChangesLookAndFeel::drawPopupMenuSectionHeader (Graphics& g, const Rectangle<int>& area, const String& name)
{
    g.setFont (font (12.5f, true));
    g.setColour (colours::gold);
    g.drawFittedText (name.toUpperCase(), area.reduced (12, 0), Justification::bottomLeft, 1);
}

//==============================================================================
Rectangle<int> ChangesLookAndFeel::getTooltipBounds (const String& text, Point<int> pos, Rectangle<int> parent)
{
    AttributedString s; s.append (text, font (13.5f), colours::text);
    TextLayout tl; tl.createLayoutWithBalancedLineLengths (s, 320.0f);
    const int w = (int) tl.getWidth() + 22, h = (int) tl.getHeight() + 16;
    return Rectangle<int> (pos.x > parent.getCentreX() ? pos.x - (w + 12) : pos.x + 18,
                           pos.y > parent.getCentreY() ? pos.y - (h + 10) : pos.y + 18, w, h).constrainedWithin (parent);
}

void ChangesLookAndFeel::drawTooltip (Graphics& g, const String& text, int w, int h)
{
    auto r = Rectangle<float> (0, 0, (float) w, (float) h);
    g.setColour (colours::bg0.withAlpha (0.97f));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (colours::gold.withAlpha (0.7f));
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
    AttributedString s; s.append (text, font (13.5f), colours::text);
    TextLayout tl; tl.createLayoutWithBalancedLineLengths (s, 320.0f);
    tl.draw (g, r.reduced (11.0f, 8.0f));
}

Font ChangesLookAndFeel::getLabelFont (Label& l) { return l.getFont(); }

void ChangesLookAndFeel::drawLabel (Graphics& g, Label& label)
{
    if (dynamic_cast<Slider*> (label.getParentComponent()) != nullptr)
    {
        // value read-out as a small recessed LCD
        auto r = label.getLocalBounds().toFloat().reduced (3.0f, 0.5f);
        drawRecess (g, r, 4.0f);
        if (! label.isBeingEdited())
        {
            g.setColour (label.findColour (Label::textColourId));
            g.setFont (label.getFont());
            g.drawFittedText (label.getText(), r.toNearestInt().reduced (2, 0), Justification::centred, 1, 0.8f);
        }
        return;
    }
    LookAndFeel_V4::drawLabel (g, label);
}

//==============================================================================
void ChangesLookAndFeel::drawScrew (Graphics& g, Point<float> c, float radius, float angle)
{
    ColourGradient sg (Colour (0xffa8afba), c.x - radius, c.y - radius, Colour (0xff353b47), c.x + radius, c.y + radius, false);
    g.setGradientFill (sg);
    g.fillEllipse (c.x - radius, c.y - radius, radius * 2, radius * 2);
    g.setColour (Colours::black.withAlpha (0.7f));
    g.drawEllipse (c.x - radius, c.y - radius, radius * 2, radius * 2, 0.8f);
    const float dx = std::cos (angle) * radius * 0.7f, dy = std::sin (angle) * radius * 0.7f;
    g.setColour (Colour (0xff1a1d24));
    g.drawLine (c.x - dx, c.y - dy, c.x + dx, c.y + dy, 1.4f);
}

void ChangesLookAndFeel::drawPanel (Graphics& g, Rectangle<float> r, const String& title, Colour accent)
{
    // shadow
    g.setColour (Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (r.translated (0, 3.0f), 9.0f);

    ColourGradient bg (Colour (0xff18213a), r.getX(), r.getY(), Colour (0xff0d1220), r.getX(), r.getBottom(), false);
    g.setGradientFill (bg);
    g.fillRoundedRectangle (r, 9.0f);

    g.setColour (Colours::white.withAlpha (0.07f));
    g.drawLine (r.getX() + 9, r.getY() + 1.0f, r.getRight() - 9, r.getY() + 1.0f, 1.0f);
    g.setColour (Colour (0xff050811));
    g.drawRoundedRectangle (r, 9.0f, 1.2f);
    g.setColour (colours::border.withAlpha (0.6f));
    g.drawRoundedRectangle (r.reduced (1.5f), 8.0f, 0.8f);

    if (title.isNotEmpty())
    {
        g.setFont (font (11.5f, true));
        const int tw = g.getCurrentFont().getStringWidth (title) + 10;
        auto tr = Rectangle<float> (r.getX() + 16, r.getY() + 7, (float) tw, 14.0f);
        g.setColour (accent);
        g.fillRoundedRectangle (r.getX() + 9, tr.getCentreY() - 1.5f, 3.0f, 3.0f, 1.5f);
        g.setColour (accent.withAlpha (0.95f));
        g.drawText (title, tr, Justification::centredLeft);
        g.setColour (accent.withAlpha (0.18f));
        g.drawLine (tr.getRight() + 2, tr.getCentreY(), r.getRight() - 22, tr.getCentreY(), 1.0f);
    }
    drawScrew (g, { r.getRight() - 10, r.getY() + 10 }, 3.2f, 0.7f);
    drawScrew (g, { r.getRight() - 10, r.getBottom() - 10 }, 3.2f, 2.1f);
    drawScrew (g, { r.getX() + 10, r.getBottom() - 10 }, 3.2f, 1.2f);
}
