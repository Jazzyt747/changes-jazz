#include "Components.h"

using namespace juce;
using LNF = ChangesLookAndFeel;

//==============================================================================
// LedMeter
LedMeter::LedMeter (bool v, int segs, bool st, Colour t) : vertical (v), stereo (st), segments (segs), tint (t) {}

float LedMeter::toNorm (float gain)
{
    const float db = Decibels::gainToDecibels (gain, -80.0f);
    return jlimit (0.0f, 1.0f, jmap (db, -48.0f, 3.0f, 0.0f, 1.0f));
}

void LedMeter::setLevels (float l, float r)
{
    const float in[2] = { toNorm (l), toNorm (r) };
    bool changed = false;
    for (int c = 0; c < 2; ++c)
    {
        const float before = disp[c];
        disp[c] = jmax (in[c], disp[c] - 0.035f);
        if (in[c] >= hold[c]) { hold[c] = in[c]; holdTimer[c] = 30; }
        else if (--holdTimer[c] <= 0) hold[c] = jmax (0.0f, hold[c] - 0.02f);
        changed |= std::abs (before - disp[c]) > 0.001f || holdTimer[c] > 0;
    }
    if (changed) repaint();
}

void LedMeter::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    LNF::drawRecess (g, r, 3.0f);
    r = r.reduced (3.0f);

    ColourGradient grad (Colour (0xff1fd36a), 0, 0, Colour (0xffff3434), 1, 0, false);
    grad.addColour (0.55, Colour (0xff9df04a));
    grad.addColour (0.75, Colour (0xffffd23c));
    grad.addColour (0.88, Colour (0xffff8a2a));

    const int channels = stereo ? 2 : 1;
    const float gap = 1.5f;
    for (int c = 0; c < channels; ++c)
    {
        Rectangle<float> lane = vertical ? r.withWidth ((r.getWidth() - (channels - 1) * 2.0f) / channels).withX (r.getX() + c * ((r.getWidth() + 2.0f) / channels))
                                         : r.withHeight ((r.getHeight() - (channels - 1) * 2.0f) / channels).withY (r.getY() + c * ((r.getHeight() + 2.0f) / channels));
        const float len = vertical ? lane.getHeight() : lane.getWidth();
        const float segLen = (len - gap * (segments - 1)) / (float) segments;
        const int holdSeg = (int) std::ceil (hold[c] * segments) - 1;
        for (int s = 0; s < segments; ++s)
        {
            const float f = (s + 0.5f) / (float) segments;
            Colour col = grad.getColourAtPosition (f);
            if (! tint.isTransparent()) col = col.interpolatedWith (tint, 0.0f);
            const bool lit = disp[c] >= (s + 1) / (float) segments - 0.5f / segments;
            const bool isHold = s == holdSeg && hold[c] > 0.02f;
            Rectangle<float> seg = vertical ? Rectangle<float> (lane.getX(), lane.getBottom() - (s + 1) * segLen - s * gap, lane.getWidth(), segLen)
                                            : Rectangle<float> (lane.getX() + s * (segLen + gap), lane.getY(), segLen, lane.getHeight());
            if (lit || isHold)
            {
                g.setColour (col.withAlpha (0.35f));
                g.fillRoundedRectangle (seg.expanded (1.0f), 1.5f);
                ColourGradient sg (col.brighter (0.5f), seg.getX(), seg.getY(), col, seg.getRight(), seg.getBottom(), false);
                g.setGradientFill (sg);
                g.fillRoundedRectangle (seg, 1.0f);
            }
            else
            {
                g.setColour (col.withAlpha (0.10f));
                g.fillRoundedRectangle (seg, 1.0f);
            }
        }
    }
}

//==============================================================================
// Knob
Knob::Knob (AudioProcessorValueTreeState& s, const String& id, const String& t, Format fmt, Colour accent)
{
    slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    slider.setRotaryParameters (MathConstants<float>::pi * 1.22f, MathConstants<float>::pi * 2.78f, true);
    slider.setTextBoxStyle (Slider::TextBoxBelow, false, 70, 16);
    slider.getProperties().set ("accent", accent.toString());
    slider.setVelocityBasedMode (false);
    slider.setMouseDragSensitivity (220);
    addAndMakeVisible (slider);
    attachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (s, id, slider);

    switch (fmt)
    {
        case Format::Percent:      slider.textFromValueFunction = [] (double v) { return String (roundToInt (v * 100.0)) + "%"; };
                                   slider.valueFromTextFunction = [] (const String& t) { return t.getDoubleValue() / 100.0; }; break;
        case Format::Milliseconds: slider.textFromValueFunction = [] (double v) { return String (v, v < 10 ? 1 : 0) + " ms"; }; break;
        case Format::Decibels:     slider.textFromValueFunction = [] (double v) { return v <= -59.9 ? String ("-inf") : String (v, 1) + " dB"; }; break;
        case Format::Bpm:          slider.textFromValueFunction = [] (double v) { return String (roundToInt (v)) + " bpm"; }; break;
        case Format::Swing:        slider.textFromValueFunction = [] (double v) { return String (roundToInt (v * 100.0)) + "%"; };
                                   slider.valueFromTextFunction = [] (const String& t) { return t.getDoubleValue() / 100.0; }; break;
        default: break;
    }
    slider.updateText();

    title.setText (t.toUpperCase(), dontSendNotification);
    title.setFont (LNF::font (10.5f, true));
    title.setColour (Label::textColourId, colours::textDim);
    title.setJustificationType (Justification::centred);
    title.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (title);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    title.setBounds (r.removeFromTop (14));
    slider.setBounds (r);
}

//==============================================================================
// Fader
Fader::Fader (AudioProcessorValueTreeState& s, const String& id, const String& t, bool stereoMeter, Colour accent)
    : meter (true, 26, stereoMeter)
{
    slider.setSliderStyle (Slider::LinearVertical);
    slider.setTextBoxStyle (Slider::TextBoxBelow, false, 64, 16);
    slider.getProperties().set ("accent", accent.toString());
    addAndMakeVisible (slider);
    attachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (s, id, slider);
    slider.textFromValueFunction = [] (double v) { return v <= -59.9 ? String ("-inf") : String (v, 1) + " dB"; };
    slider.updateText();

    title.setText (t.toUpperCase(), dontSendNotification);
    title.setFont (LNF::font (10.5f, true));
    title.setColour (Label::textColourId, accent.interpolatedWith (colours::textDim, 0.35f));
    title.setJustificationType (Justification::centred);
    addAndMakeVisible (title);
    addAndMakeVisible (meter);
}

void Fader::resized()
{
    auto r = getLocalBounds();
    title.setBounds (r.removeFromTop (16));
    auto m = r.removeFromRight (meter.getProperties().contains ("wide") ? 18 : 12);
    meter.setBounds (m.withTrimmedTop (8).withTrimmedBottom (26));
    slider.setBounds (r);
}

//==============================================================================
// HelpBar
void HelpBar::setHelp (const String& t, const String& x)
{
    if (t == title && x == text) return;
    title = t; text = x; repaint();
}

void HelpBar::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    LNF::drawRecess (g, r, 6.0f);
    auto inner = r.reduced (10.0f, 3.0f);
    // info glyph
    auto ic = inner.removeFromLeft (20.0f).withSizeKeepingCentre (16.0f, 16.0f);
    g.setColour (colours::gold); g.drawEllipse (ic, 1.4f);
    g.setFont (LNF::font (11.0f, true)); g.drawText ("i", ic, Justification::centred);
    inner.removeFromLeft (8.0f);

    TextLayout tl;
    for (float scale = 1.0f; scale > 0.5f; scale -= 0.05f)
    {
        AttributedString as;
        as.append (title + "   ", LNF::font (13.0f, true).withHeight (LNF::scaledHeight (13.0f) * scale), colours::goldLight);
        as.append (text, LNF::font (13.0f).withHeight (LNF::scaledHeight (13.0f) * scale), colours::text.withAlpha (0.88f));
        as.setJustification (Justification::centredLeft);
        as.setWordWrap (AttributedString::byWord);
        tl.createLayout (as, inner.getWidth());
        if (tl.getHeight() <= inner.getHeight() + 1) break;
    }
    tl.draw (g, inner.withY (inner.getCentreY() - tl.getHeight() * 0.5f).withHeight (tl.getHeight()));
}

//==============================================================================
// ChordDisplay
void ChordDisplay::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    // glass LCD
    g.setColour (Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (r.translated (0, 3), 10.0f);
    ColourGradient bg (Colour (0xff0b1424), r.getX(), r.getY(), Colour (0xff04070d), r.getX(), r.getBottom(), false);
    g.setGradientFill (bg);
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (colours::gold.withAlpha (0.35f));
    g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);
    // scan-line sheen
    ColourGradient sheen (Colours::white.withAlpha (0.06f), r.getX(), r.getY(), Colours::transparentWhite, r.getX(), r.getY() + r.getHeight() * 0.5f, false);
    g.setGradientFill (sheen);
    g.fillRoundedRectangle (r.reduced (2.0f).withHeight (r.getHeight() * 0.45f), 9.0f);

    auto inner = r.reduced (18.0f, 10.0f);
    auto left = inner.removeFromLeft (inner.getWidth() * 0.47f);

    if (! data.hasChord)
    {
        g.setColour (colours::gold.withAlpha (0.85f));
        g.setFont (LNF::displayFont (30.0f));
        g.drawText ("Play a chord", left.removeFromTop (48), Justification::bottomLeft);
        g.setColour (colours::textDim);
        g.setFont (LNF::font (13.0f));
        g.drawFittedText ("Click a pad, use keys Z-M on your computer keyboard, or play MIDI notes " + data.banner, left.toNearestInt(), Justification::topLeft, 3);
    }
    else
    {
        // glowing chord name
        Font big = LNF::displayFont (46.0f);
        g.setFont (big);
        auto nameArea = left.removeFromTop (56);
        for (int i = 3; i >= 1; --i)
        {
            g.setColour (colours::gold.withAlpha (0.07f * i));
            g.drawFittedText (data.name, nameArea.translated (0, 0).expanded ((float) i).toNearestInt(), Justification::bottomLeft, 1, 0.7f);
        }
        ColourGradient tg (colours::goldLight, nameArea.getX(), nameArea.getY(), colours::gold, nameArea.getX(), nameArea.getBottom(), false);
        g.setGradientFill (tg);
        g.drawFittedText (data.name, nameArea.toNearestInt(), Justification::bottomLeft, 1, 0.7f);

        g.setFont (LNF::font (17.0f, true));
        g.setColour (colours::blue);
        auto romanRow = left.removeFromTop (24);
        g.drawText (data.roman, romanRow, Justification::centredLeft);
        g.setFont (LNF::font (13.0f));
        g.setColour (colours::text.withAlpha (0.85f));
        g.drawFittedText (data.function, left.removeFromTop (34).toNearestInt(), Justification::topLeft, 2);
    }

    // right column: details
    auto row = [&] (const String& label, const String& value, Colour c)
    {
        auto rr = inner.removeFromTop (22);
        g.setFont (LNF::font (10.5f, true));
        g.setColour (colours::textFaint);
        g.drawText (label, rr.removeFromLeft (64), Justification::centredLeft);
        g.setFont (LNF::font (13.5f, true));
        g.setColour (c);
        g.drawFittedText (value, rr.toNearestInt(), Justification::centredLeft, 1, 0.75f);
    };
    inner.removeFromTop (6);
    row ("LEVEL", data.level, colours::goldLight);
    row ("NOTES", data.notes, colours::text);
    row ("DEGREES", data.intervals, colours::text);
    row ("SCALE", data.scale, colours::blue);
    row ("NEXT", data.next, colours::teal);
}

//==============================================================================
// ChordPadGrid
theory::ChordSpec ChordPadGrid::clipboard;
bool ChordPadGrid::hasClipboard = false;

ChordPadGrid::ChordPadGrid (ChangesProcessor& p) : proc (p) { refreshNames(); }

void ChordPadGrid::refreshNames()
{
    const auto o = proc.currentOptions();
    for (int i = 0; i < palettes::numPads; ++i)
    {
        names[i] = proc.padDisplayName (i);
        auto rc = theory::resolve (proc.getPad (i), o);
        romans[i] = theory::romanNumeral (rc, o);
    }
    repaint();
}

void ChordPadGrid::setLiveState (int cur, const float* sc, int b, bool show)
{
    bool changed = cur != current || b != bank || show != showSuggest;
    for (int i = 0; i < palettes::numPads; ++i) { changed |= std::abs (scores[i] - sc[i]) > 0.01f; scores[i] = sc[i]; }
    current = cur; bank = b; showSuggest = show;
    if (changed) repaint();
}

Rectangle<float> ChordPadGrid::padBounds (int pad) const
{
    // Two piano-style octave blocks stacked vertically: pads 13-24 on top, 1-12 below.
    const float W = (float) getWidth(), H = (float) getHeight();
    const float gap = 6.0f;
    const float blockH = (H - gap) * 0.5f;
    const int oct = pad / 12, pc = pad % 12;
    const float top = oct == 1 ? 0.0f : blockH + gap;
    const float ww = W / 7.0f;
    static const int whiteIdx[12] = { 0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6 };
    static const int blackAfter[12] = { -1, 1, -1, 2, -1, -1, 4, -1, 5, -1, 6, -1 };
    if (whiteIdx[pc] >= 0)
        return { whiteIdx[pc] * ww + 3.0f, top + blockH * 0.45f, ww - 6.0f, blockH * 0.55f - 1.0f };
    const float bw = ww * 0.78f;
    return { blackAfter[pc] * ww - bw * 0.5f, top + 1.0f, bw, blockH * 0.42f - 1.0f };
}

int ChordPadGrid::padAt (Point<float> p) const
{
    for (int i = 0; i < palettes::numPads; ++i)
        if (palettes::isBlackPad (i) && padBounds (i).contains (p)) return i;
    for (int i = 0; i < palettes::numPads; ++i)
        if (! palettes::isBlackPad (i) && padBounds (i).contains (p)) return i;
    return -1;
}

void ChordPadGrid::paint (Graphics& g)
{
    const int zone = proc.chordZoneStart();
    const auto o = proc.currentOptions();
    const float pulse = 0.5f + 0.5f * std::sin ((float) Time::getMillisecondCounter() * 0.006f);

    for (int pass = 0; pass < 2; ++pass)
    for (int i = 0; i < palettes::numPads; ++i)
    {
        const bool black = palettes::isBlackPad (i);
        if ((pass == 0) == black) continue;
        auto r = padBounds (i);
        const bool isCurrent = i == current;
        const bool isPressed = i == pressed || proc.keyboardState.isNoteOn (1, zone + i);
        const bool isHover = i == hovered;
        const float sug = showSuggest ? jlimit (0.0f, 1.0f, scores[i] / 3.0f) : 0.0f;
        const bool inBank = (i / 12) == bank;

        // shadow
        g.setColour (Colours::black.withAlpha (0.55f));
        g.fillRoundedRectangle (r.translated (0, isPressed ? 1.0f : 3.5f), 7.0f);
        auto body = isPressed ? r.translated (0, 1.5f) : (isHover ? r.translated (0, -1.0f) : r);

        Colour top = black ? Colour (0xff1a2236) : Colour (0xff334263);
        Colour bot = black ? Colour (0xff090d17) : Colour (0xff1a2340);
        if (isCurrent) { top = top.interpolatedWith (colours::gold, 0.35f); bot = bot.interpolatedWith (colours::goldDark, 0.35f); }
        if (isHover) { top = top.brighter (0.12f); }
        ColourGradient bg (top, body.getX(), body.getY(), bot, body.getX(), body.getBottom(), false);
        g.setGradientFill (bg);
        g.fillRoundedRectangle (body, 7.0f);

        // bevel
        g.setColour (Colours::white.withAlpha (black ? 0.08f : 0.16f));
        g.drawLine (body.getX() + 6, body.getY() + 1.2f, body.getRight() - 6, body.getY() + 1.2f, 1.2f);
        g.setColour (Colours::black.withAlpha (0.8f));
        g.drawRoundedRectangle (body, 7.0f, 1.0f);

        if (isCurrent)
        {
            g.setColour (colours::gold.withAlpha (0.35f));
            g.drawRoundedRectangle (body.expanded (2.0f), 9.0f, 3.0f);
            g.setColour (colours::goldLight);
            g.drawRoundedRectangle (body, 7.0f, 1.5f);
        }
        else if (sug > 0.05f)
        {
            g.setColour (colours::teal.withAlpha (0.15f + 0.45f * sug * (0.6f + 0.4f * pulse)));
            g.drawRoundedRectangle (body.expanded (1.5f), 8.5f, 2.5f);
        }
        if (i == dropTarget)
        {
            Path p; p.addRoundedRectangle (body.reduced (2.0f), 6.0f);
            float dashes[] = { 5.0f, 4.0f };
            PathStrokeType (2.0f).createDashedStroke (p, p, dashes, 2);
            g.setColour (colours::goldLight); g.fillPath (p);
        }

        auto tr = body.reduced (7.0f, 3.0f);
        auto bottom = tr.removeFromBottom (black ? 13.0f : 15.0f);
        // chord name
        g.setColour (black ? colours::goldLight : colours::text);
        g.setFont (LNF::font (black ? 13.5f : 15.0f, true));
        g.drawFittedText (names[i], tr.toNearestInt(), Justification::centred, 1, 0.55f);
        // roman numeral (left) and key hints (right)
        g.setFont (LNF::font (10.5f, true));
        g.setColour (colours::blue.withAlpha (0.9f));
        g.drawFittedText (romans[i], bottom.toNearestInt(), Justification::centredLeft, 1, 0.7f);
        // key hint: hidden on narrow black pads when the text is enlarged (the help bar still shows it)
        if (! black || LNF::getTextSize() == LNF::textSmall)
        {
            g.setFont (LNF::font (9.0f));
            g.setColour (colours::textFaint);
            const String keyHint = inBank ? palettes::qwertyHint (i) + " " : String();
            const String hint = keyHint + theory::noteNameWithOctave (zone + i, 0, false);
            const float romanW = LNF::font (10.5f, true).getStringWidthFloat (romans[i]) + 6.0f;
            if (g.getCurrentFont().getStringWidthFloat (hint) < bottom.getWidth() - romanW)
                g.drawText (hint, bottom, Justification::centredRight);
        }
        if (sug > 0.05f && ! isCurrent)
            LNF::drawLed (g, { body.getRight() - 7.0f, body.getY() + 7.0f }, 3.0f, colours::teal, true);
        ignoreUnused (o);
    }
}

void ChordPadGrid::mouseMove (const MouseEvent& e)
{
    const int h = padAt (e.position);
    if (h != hovered) { hovered = h; repaint(); if (onHover) onHover (h); }
}

void ChordPadGrid::mouseExit (const MouseEvent&) { hovered = -1; repaint(); if (onHover) onHover (-1); }

void ChordPadGrid::mouseDown (const MouseEvent& e)
{
    const int p = padAt (e.position);
    if (p < 0) return;
    if (e.mods.isPopupMenu()) { showPadMenu (p); return; }
    pressed = p; dragStarted = false;
    const float vel = jlimit (0.3f, 1.0f, 0.55f + 0.45f * (e.position.y - padBounds (p).getY()) / padBounds (p).getHeight());
    proc.auditionPad (p, true, vel);
    repaint();
}

void ChordPadGrid::mouseDrag (const MouseEvent& e)
{
    if (pressed < 0 || dragStarted || e.getDistanceFromDragStart() < 8) return;
    dragStarted = true;
    proc.auditionPad (pressed, false);
    if (auto* dc = DragAndDropContainer::findParentDragContainerFor (this))
    {
        auto r = padBounds (pressed).toNearestInt();
        Image img = createComponentSnapshot (r, true).convertedToFormat (Image::ARGB);
        img.multiplyAllAlphas (0.8f);
        dc->startDragging ("pad:" + String (pressed), this, ScaledImage (img), true);
    }
}

void ChordPadGrid::mouseUp (const MouseEvent&)
{
    if (pressed >= 0 && ! dragStarted) proc.auditionPad (pressed, false);
    pressed = -1; dragStarted = false;
    repaint();
}

void ChordPadGrid::mouseDoubleClick (const MouseEvent& e)
{
    const int p = padAt (e.position);
    if (p >= 0 && onAddToProgression) onAddToProgression (p);
}

bool ChordPadGrid::isInterestedInDragSource (const SourceDetails& d)
{
    const String s = d.description.toString();
    return s.startsWith ("pad:") || s.startsWith ("chip:");
}

void ChordPadGrid::itemDragEnter (const SourceDetails& d) { itemDragMove (d); }
void ChordPadGrid::itemDragMove (const SourceDetails& d) { const int t = padAt (d.localPosition.toFloat()); if (t != dropTarget) { dropTarget = t; repaint(); } }
void ChordPadGrid::itemDragExit (const SourceDetails&) { dropTarget = -1; repaint(); }

void ChordPadGrid::itemDropped (const SourceDetails& d)
{
    const String s = d.description.toString();
    const int target = padAt (d.localPosition.toFloat());
    dropTarget = -1;
    if (target < 0) { repaint(); return; }
    if (s.startsWith ("pad:"))
    {
        const int from = s.fromFirstOccurrenceOf ("pad:", false, false).getIntValue();
        if (ModifierKeys::currentModifiers.isCommandDown() || ModifierKeys::currentModifiers.isAltDown()) proc.copyPad (from, target);   // Ctrl on Windows, Cmd on Mac
        else proc.swapPads (from, target);
    }
    else if (s.startsWith ("chip:"))
    {
        const int idx = s.fromFirstOccurrenceOf ("chip:", false, false).getIntValue();
        const auto& prog = proc.getProgression();
        if (idx >= 0 && idx < (int) prog.size()) { auto spec = prog[(size_t) idx]; spec.beats = 4; proc.setPad (target, spec); }
    }
    repaint();
}

void ChordPadGrid::showPadMenu (int pad)
{
    using namespace theory;
    const auto o = proc.currentOptions();
    const ChordSpec spec = proc.getPad (pad);
    PopupMenu m;
    m.addSectionHeader ("Pad " + String (pad + 1) + ":  " + names[pad]);

    PopupMenu root, qual, var, style, bass;
    for (int r = 0; r < 12; ++r)
        root.addItem (100 + r, degreeName (r, o.minorKey) + "   (" + noteName (o.keyTonic + r, o.keyTonic, o.minorKey) + ")", true, spec.rootOffset == r);
    for (int q = 0; q < numQualities; ++q)
        qual.addItem (200 + q, qualityNames()[q], true, (int) spec.quality == q);
    const char* vn[] = { "Best voice-leading (auto)", "Alternative 1", "Alternative 2", "Alternative 3" };
    for (int v = 0; v < 4; ++v) var.addItem (300 + v, vn[v], true, spec.variation == v);
    style.addItem (400, "Use global style", true, spec.style < 0);
    for (int s = 1; s < numStyles; ++s) style.addItem (401 + s, styleNames()[s], true, spec.style == s);
    bass.addItem (500, "Root in the bass (no slash)", true, spec.bassOffset < 0);
    for (int b = 0; b < 12; ++b)
        bass.addItem (501 + b, "/" + noteName (o.keyTonic + b, o.keyTonic, o.minorKey) + "   (" + degreeName (b, o.minorKey) + ")", true, spec.bassOffset == b);

    m.addSubMenu ("Root", root);
    m.addSubMenu ("Quality", qual);
    m.addSubMenu ("Voicing variation", var);
    m.addSubMenu ("Voicing style", style);
    m.addSubMenu ("Slash bass", bass);
    m.addSeparator();
    m.addItem (1, "Add to progression");
    m.addItem (2, "Copy chord");
    m.addItem (3, "Paste chord", hasClipboard);
    m.addItem (4, "Reset pad to palette default");

    SafePointer<ChordPadGrid> safe (this);
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (this).withTargetScreenArea (localAreaToGlobal (padBounds (pad).toNearestInt())),
        [safe, pad] (int r)
        {
            if (safe == nullptr || r == 0) return;
            auto& p = safe->proc;
            ChordSpec s = p.getPad (pad);
            if (r >= 100 && r < 112) s.rootOffset = r - 100;
            else if (r >= 200 && r < 200 + numQualities) s.quality = (Quality) (r - 200);
            else if (r >= 300 && r < 304) s.variation = r - 300;
            else if (r == 400) s.style = -1;
            else if (r > 400 && r < 400 + numStyles + 1) s.style = r - 401;
            else if (r == 500) s.bassOffset = -1;
            else if (r > 500 && r < 513) s.bassOffset = r - 501;
            else if (r == 1) { if (safe->onAddToProgression) safe->onAddToProgression (pad); return; }
            else if (r == 2) { clipboard = s; hasClipboard = true; return; }
            else if (r == 3) { s = clipboard; }
            else if (r == 4)
            {
                const auto o2 = p.currentOptions();
                s = palettes::preset (p.getPresetIndex(), o2.level, o2.minorKey)[(size_t) pad];
            }
            p.setPad (pad, s);
        });
}

//==============================================================================
// ProgressionStrip
ProgressionStrip::ProgressionStrip (ChangesProcessor& p) : proc (p) { refresh(); }

void ProgressionStrip::refresh()
{
    names.clear(); beats.clear();
    const auto o = proc.currentOptions();
    theory::Voicing prev;
    for (auto& s : proc.getProgression())
    {
        auto rc = theory::resolve (s, o);
        prev = theory::buildVoicing (rc, s, o, prev);
        theory::addVoicedTensions (rc, prev);
        prev.bass = -1;
        names.add (theory::chordName (rc, o));
        beats.add (s.beats);
    }
    const int parentW = getParentComponent() != nullptr ? getParentComponent()->getWidth() : 594;
    const int w = juce::jmax (parentW, (int) names.size() * (chipW + chipGap) + chipGap + 40);
    setSize (w, getHeight());
    repaint();
}

Rectangle<float> ProgressionStrip::chipBounds (int i) const
{
    return { (float) (chipGap + i * (chipW + chipGap)), 6.0f, (float) chipW, (float) getHeight() - 12.0f };
}

Rectangle<float> ProgressionStrip::deleteBounds (int i) const
{
    auto r = chipBounds (i);
    return { r.getRight() - 16.0f, r.getY() + 3.0f, 13.0f, 13.0f };
}

int ProgressionStrip::chipAt (Point<float> p) const
{
    for (int i = 0; i < names.size(); ++i) if (chipBounds (i).contains (p)) return i;
    return -1;
}

int ProgressionStrip::insertionIndexFor (Point<float> p) const
{
    return jlimit (0, names.size(), (int) std::floor ((p.x - chipGap + (chipW + chipGap) * 0.5f) / (float) (chipW + chipGap)));
}

void ProgressionStrip::paint (Graphics& g)
{
    if (names.isEmpty())
    {
        g.setColour (colours::textFaint);
        g.setFont (LNF::font (13.5f));
        g.drawText ("Drag chord pads here, double-click a pad, or arm REC and play chords to build a progression.",
                    getLocalBounds().withWidth (getParentComponent() != nullptr ? getParentComponent()->getWidth() : getWidth()).reduced (12, 0), Justification::centred);
    }
    for (int i = 0; i < names.size(); ++i)
    {
        auto r = chipBounds (i);
        const bool isPlaying = i == playing, isHover = i == hovered;
        g.setColour (Colours::black.withAlpha (0.5f));
        g.fillRoundedRectangle (r.translated (0, 2.5f), 6.0f);
        ColourGradient bg (isPlaying ? colours::gold.withAlpha (0.9f) : Colour (0xff2c3858).brighter (isHover ? 0.12f : 0.0f), r.getX(), r.getY(),
                           isPlaying ? colours::goldDark : Colour (0xff141b2e), r.getX(), r.getBottom(), false);
        g.setGradientFill (bg);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (Colours::white.withAlpha (0.12f));
        g.drawLine (r.getX() + 5, r.getY() + 1.2f, r.getRight() - 5, r.getY() + 1.2f, 1.0f);
        g.setColour (isPlaying ? colours::goldLight : Colours::black.withAlpha (0.8f));
        g.drawRoundedRectangle (r, 6.0f, 1.0f);

        g.setColour (isPlaying ? Colour (0xff140d04) : colours::text);
        g.setFont (LNF::font (14.0f, true));
        g.drawFittedText (names[i], r.reduced (5, 4).withTrimmedBottom (14).toNearestInt(), Justification::centred, 2, 0.7f);
        g.setFont (LNF::font (10.0f));
        g.setColour (isPlaying ? Colour (0xff3a2a10) : colours::textDim);
        g.drawText (String (i + 1) + "  |  " + String (beats[i]) + (beats[i] == 1 ? " beat" : " beats"), r.reduced (5, 3).removeFromBottom (13), Justification::centred);

        if (isHover)
        {
            auto d = deleteBounds (i);
            g.setColour (colours::red.withAlpha (0.85f)); g.fillEllipse (d);
            g.setColour (Colours::white); g.drawLine (d.getX() + 3.5f, d.getY() + 3.5f, d.getRight() - 3.5f, d.getBottom() - 3.5f, 1.4f);
            g.drawLine (d.getRight() - 3.5f, d.getY() + 3.5f, d.getX() + 3.5f, d.getBottom() - 3.5f, 1.4f);
        }
    }
    if (insertAt >= 0)
    {
        const float x = (float) (chipGap * 0.5f + insertAt * (chipW + chipGap));
        g.setColour (colours::goldLight);
        g.fillRoundedRectangle (x - 1.5f, 3.0f, 3.0f, getHeight() - 6.0f, 1.5f);
    }
}

void ProgressionStrip::mouseMove (const MouseEvent& e)
{
    const int h = chipAt (e.position);
    if (h != hovered) { hovered = h; repaint(); }
    if (onHelp)
    {
        if (h >= 0) onHelp ("Progression chord " + String (h + 1), "Click to hear it. Drag to reorder, drag onto a pad to store it there. Right-click to change its length or remove it.");
        else onHelp ("Progression strip", "Your chord progression. Drop pads here, then press PLAY to loop it with the selected comping rhythm, or drag the MIDI handle into your DAW.");
    }
}

void ProgressionStrip::mouseExit (const MouseEvent&) { hovered = -1; repaint(); }

void ProgressionStrip::mouseDown (const MouseEvent& e)
{
    const int c = chipAt (e.position);
    if (c < 0) return;
    if (e.mods.isPopupMenu()) { showChipMenu (c); return; }
    if (deleteBounds (c).contains (e.position)) { proc.progressionRemove (c); hovered = -1; return; }
    pressed = c; dragStarted = false;
    proc.auditionSpec (proc.getProgression()[(size_t) c], true);
}

void ProgressionStrip::mouseDrag (const MouseEvent& e)
{
    if (pressed < 0 || dragStarted || e.getDistanceFromDragStart() < 8) return;
    dragStarted = true;
    proc.auditionSpec ({}, false);
    if (auto* dc = DragAndDropContainer::findParentDragContainerFor (this))
    {
        Image img = createComponentSnapshot (chipBounds (pressed).toNearestInt(), true).convertedToFormat (Image::ARGB);
        img.multiplyAllAlphas (0.8f);
        dc->startDragging ("chip:" + String (pressed), this, ScaledImage (img), true);
    }
}

void ProgressionStrip::mouseUp (const MouseEvent&)
{
    if (pressed >= 0 && ! dragStarted) proc.auditionSpec ({}, false);
    pressed = -1; dragStarted = false;
}

bool ProgressionStrip::isInterestedInDragSource (const SourceDetails& d)
{
    const String s = d.description.toString();
    return s.startsWith ("pad:") || s.startsWith ("chip:");
}

void ProgressionStrip::itemDragMove (const SourceDetails& d)
{
    const int ia = insertionIndexFor (d.localPosition.toFloat());
    if (ia != insertAt) { insertAt = ia; repaint(); }
}

void ProgressionStrip::itemDragExit (const SourceDetails&) { insertAt = -1; repaint(); }

void ProgressionStrip::itemDropped (const SourceDetails& d)
{
    const String s = d.description.toString();
    const int at = insertionIndexFor (d.localPosition.toFloat());
    insertAt = -1;
    if (s.startsWith ("pad:"))
    {
        auto spec = proc.getPad (s.fromFirstOccurrenceOf ("pad:", false, false).getIntValue());
        static const int beatTable[5] = { 1, 2, 3, 4, 8 };
        spec.beats = beatTable[jlimit (0, 4, (int) proc.apvts.getRawParameterValue (ids::beats)->load())];
        proc.progressionAdd (spec, at);
    }
    else if (s.startsWith ("chip:"))
    {
        const int from = s.fromFirstOccurrenceOf ("chip:", false, false).getIntValue();
        proc.progressionMove (from, at > from ? at - 1 : at);
    }
    repaint();
}

void ProgressionStrip::showChipMenu (int i)
{
    PopupMenu m, len;
    m.addSectionHeader ("Chord " + String (i + 1) + ":  " + names[i]);
    for (int b : { 1, 2, 3, 4, 6, 8 }) len.addItem (100 + b, String (b) + (b == 1 ? " beat" : " beats"), true, beats[i] == b);
    m.addSubMenu ("Length", len);
    m.addItem (1, "Duplicate");
    m.addItem (2, "Remove");
    m.addSeparator();
    m.addItem (3, "Clear whole progression");
    SafePointer<ProgressionStrip> safe (this);
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (this).withTargetScreenArea (localAreaToGlobal (chipBounds (i).toNearestInt())),
        [safe, i] (int r)
        {
            if (safe == nullptr || r == 0) return;
            auto& p = safe->proc;
            if (r > 100) p.progressionSetBeats (i, r - 100);
            else if (r == 1) p.progressionAdd (p.getProgression()[(size_t) i], i + 1);
            else if (r == 2) p.progressionRemove (i);
            else if (r == 3) p.progressionClear();
        });
}

//==============================================================================
// DragMidiHandle
void DragMidiHandle::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    const bool over = isMouseOver();
    ColourGradient bg (over ? colours::blue.withAlpha (0.55f) : Colour (0xff1f3a58), r.getX(), r.getY(), Colour (0xff0e1a2a), r.getX(), r.getBottom(), false);
    g.setGradientFill (bg);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (colours::blue.withAlpha (0.8f));
    g.drawRoundedRectangle (r, 5.0f, 1.0f);
    // grip dots
    for (int row = 0; row < 3; ++row)
        for (int col = 0; col < 2; ++col)
            g.fillEllipse (r.getX() + 7.0f + col * 5.0f, r.getCentreY() - 6.0f + row * 5.0f, 2.4f, 2.4f);
    g.setColour (colours::text);
    g.setFont (LNF::font (11.5f, true));
    g.drawFittedText ("DRAG MIDI", r.withTrimmedLeft (18).toNearestInt(), Justification::centred, 1);
}

void DragMidiHandle::mouseDrag (const MouseEvent& e)
{
    if (dragging || e.getDistanceFromDragStart() < 6) return;
    if (proc.getProgression().empty()) return;
    dragging = true;
    const File f = proc.writeProgressionMidiTemp();
    if (f.existsAsFile())
        DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this, [this] { dragging = false; });
    else dragging = false;
}

//==============================================================================
// KeyboardView
KeyboardView::KeyboardView (ChangesProcessor& p) : proc (p) {}

static bool isBlackNote (int n) { const int pc = n % 12; return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10; }

int KeyboardView::whiteCount() const
{
    int c = 0; for (int n = lowNote; n <= highNote; ++n) if (! isBlackNote (n)) ++c; return c;
}

Rectangle<float> KeyboardView::keyBounds (int note) const
{
    const float top = topBand();
    const float H = (float) getHeight() - top;
    const float ww = (float) getWidth() / (float) whiteCount();
    int whiteIndex = 0;
    for (int n = lowNote; n < note; ++n) if (! isBlackNote (n)) ++whiteIndex;
    if (! isBlackNote (note)) return { whiteIndex * ww, top, ww, H };
    const float bw = ww * 0.62f;
    return { whiteIndex * ww - bw * 0.5f, top, bw, H * 0.6f };
}

int KeyboardView::noteAt (Point<float> p, float* velocity) const
{
    for (int n = lowNote; n <= highNote; ++n)
        if (isBlackNote (n) && keyBounds (n).contains (p)) { if (velocity) *velocity = jlimit (0.2f, 1.0f, (p.y - topBand()) / keyBounds (n).getHeight()); return n; }
    for (int n = lowNote; n <= highNote; ++n)
        if (! isBlackNote (n) && keyBounds (n).contains (p)) { if (velocity) *velocity = jlimit (0.2f, 1.0f, (p.y - topBand()) / keyBounds (n).getHeight()); return n; }
    return -1;
}

void KeyboardView::paint (Graphics& g)
{
    const int zone = proc.chordZoneStart();
    const int melStart = zone + palettes::numPads;

    // zone bars
    {
        auto bar = [&] (int from, int to, Colour c, const String& label)
        {
            from = jmax (from, lowNote); to = jmin (to, highNote + 1);
            if (to <= from) return;
            const float x1 = keyBounds (from).getX();
            const float x2 = isBlackNote (to - 1) ? keyBounds (to - 1).getRight() : keyBounds (to - 1).getRight();
            Rectangle<float> r (x1 + 1, 1.0f, x2 - x1 - 2, topBand() - 3.0f);
            ColourGradient bg (c.withAlpha (0.85f), r.getX(), r.getY(), c.darker (0.6f), r.getX(), r.getBottom(), false);
            g.setGradientFill (bg);
            g.fillRoundedRectangle (r, 3.0f);
            g.setColour (Colours::black.withAlpha (0.8f));
            g.setFont (LNF::font (8.5f, true));
            g.drawFittedText (label, r.toNearestInt(), Justification::centred, 1, 0.7f);
        };
        bar (zone, zone + 12, colours::gold, "CHORD PADS 1-12");
        bar (zone + 12, zone + 24, colours::goldDark.brighter (0.4f), "CHORD PADS 13-24");
        bar (melStart, highNote + 1, colours::blue, "MELODY  (right hand, snapped to the scale)");
    }

    auto drawKey = [&] (int n)
    {
        auto r = keyBounds (n);
        const bool black = isBlackNote (n);
        const bool chordOn = proc.isNoteSounding (proc.soundingChord, n);
        const bool melOn = proc.isNoteSounding (proc.soundingMelody, n);
        const bool held = proc.keyboardState.isNoteOn (1, n);
        const bool inChordZone = n >= zone && n < melStart;

        Colour top = black ? Colour (0xff2a2f3a) : Colour (0xfff4efe4);
        Colour bot = black ? Colour (0xff050608) : Colour (0xffc9c2b2);
        if (inChordZone && ! black) { top = top.interpolatedWith (colours::gold, 0.10f); }
        if (chordOn) { top = colours::goldLight; bot = colours::gold.darker (0.25f); }
        if (melOn)   { top = colours::blue.brighter (0.5f); bot = colours::blueDark; }
        if (held && ! chordOn && ! melOn) { top = top.darker (0.25f); bot = bot.darker (0.25f); }

        auto body = r.reduced (black ? 0.0f : 0.6f, 0.0f);
        ColourGradient kg (top, body.getX(), body.getY(), bot, body.getX(), body.getBottom(), false);
        g.setGradientFill (kg);
        g.fillRoundedRectangle (body.withTrimmedTop (-4.0f), 3.0f);
        if (black)
        {
            g.setColour (Colours::white.withAlpha (chordOn || melOn ? 0.4f : 0.16f));
            g.drawLine (body.getX() + 2.5f, body.getY() + 2, body.getX() + 2.5f, body.getBottom() - 6, 1.0f);
            g.setColour (Colours::black.withAlpha (0.4f));
            g.fillRect (body.withTop (body.getBottom() - 5));
        }
        else
        {
            g.setColour (Colours::black.withAlpha (0.45f));
            g.drawRect (body, 0.7f);
        }
        if (chordOn || melOn)
        {
            g.setColour ((chordOn ? colours::gold : colours::blue).withAlpha (0.35f));
            g.drawRoundedRectangle (body.expanded (1.0f), 3.0f, 2.0f);
        }

        // scale dots (teaching aid)
        if (showScale && n >= melStart)
        {
            const bool inScale = theory::maskHas (scaleMask, n);
            const bool chordTone = theory::maskHas (chordToneMask, n);
            if (inScale)
            {
                const float d = black ? 5.0f : 6.5f;
                auto c = Point<float> (body.getCentreX(), body.getBottom() - (black ? 10.0f : 12.0f));
                g.setColour (chordTone ? colours::gold : colours::blue.withAlpha (0.8f));
                g.fillEllipse (c.x - d * 0.5f, c.y - d * 0.5f, d, d);
            }
        }
        if (! black && n % 12 == 0)
        {
            g.setColour (Colours::black.withAlpha (0.55f));
            g.setFont (LNF::font (9.0f, true));
            g.drawText ("C" + String (n / 12 - 1), body.removeFromBottom (24).removeFromTop (12), Justification::centred);
        }
    };
    for (int n = lowNote; n <= highNote; ++n) if (! isBlackNote (n)) drawKey (n);
    for (int n = lowNote; n <= highNote; ++n) if (isBlackNote (n)) drawKey (n);

    // top shadow (key bed felt)
    const float tb = topBand();
    ColourGradient felt (Colours::black.withAlpha (0.7f), 0, tb, Colours::transparentBlack, 0, tb + 10.0f, false);
    g.setGradientFill (felt);
    g.fillRect (0.0f, tb, (float) getWidth(), 10.0f);
    g.setColour (Colour (0xff6b1d1d));
    g.fillRect (0.0f, tb - 1.0f, (float) getWidth(), 2.0f);
}

void KeyboardView::mouseDown (const MouseEvent& e)
{
    float v = 0.8f;
    mouseNote = noteAt (e.position, &v);
    if (mouseNote >= 0) proc.keyboardState.noteOn (1, mouseNote, v);
}

void KeyboardView::mouseDrag (const MouseEvent& e)
{
    float v = 0.8f;
    const int n = noteAt (e.position, &v);
    if (n != mouseNote)
    {
        if (mouseNote >= 0) proc.keyboardState.noteOff (1, mouseNote, 0.0f);
        mouseNote = n;
        if (n >= 0) proc.keyboardState.noteOn (1, n, v);
    }
}

void KeyboardView::mouseUp (const MouseEvent&)
{
    if (mouseNote >= 0) proc.keyboardState.noteOff (1, mouseNote, 0.0f);
    mouseNote = -1;
}

void KeyboardView::mouseMove (const MouseEvent& e)
{
    if (onHover) onHover (noteAt (e.position));
}
