#include "PluginEditor.h"
#include <map>

using namespace keim;

// ============================================================ Hilfen
juce::Font ui::font (float height, bool bold)
{
    return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
}

juce::Font ui::mono (float height)
{
    return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), height, juce::Font::plain));
}

juce::String ui::upper (const juce::String& s)
{
    // Großschreibung inklusive Umlaute (unabhängig von der System-Locale)
    juce::String out;
    for (auto t = s.getCharPointer(); ! t.isEmpty();)
    {
        const juce::juce_wchar c = t.getAndAdvance();
        switch (c)
        {
            case 0xe4: out << juce::String::charToString (0xc4); break;
            case 0xf6: out << juce::String::charToString (0xd6); break;
            case 0xfc: out << juce::String::charToString (0xdc); break;
            case 0xdf: out << "SS"; break;
            default:   out << juce::String::charToString (juce::CharacterFunctions::toUpperCase (c)); break;
        }
    }
    return out;
}

void ui::drawLed (juce::Graphics& g, juce::Point<float> c, float r, bool on)
{
    if (on)
    {
        g.setColour (col::accent.withAlpha (0.35f));
        g.fillEllipse (c.x - r * 2.2f, c.y - r * 2.2f, r * 4.4f, r * 4.4f);
        g.setColour (col::accent);
    }
    else g.setColour (col::ledOff);
    g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
    g.setColour (juce::Colours::black.withAlpha (0.25f));
    g.drawEllipse (c.x - r, c.y - r, r * 2, r * 2, 0.6f);
}

static juce::String uiName (int id)
{
    switch (id)
    {
        case pFA: case pAA: return "Attack";
        case pFD: case pAD: return "Decay";
        case pFS: case pAS: return "Sustain";
        case pFR: case pAR: return "Release";
        case pLfoRate:      return "Rate";
        case pLfoFilter:    return juce::String::fromUTF8 ("→ Filter");
        case pLfoPitch:     return juce::String::fromUTF8 ("→ Tonhöhe");
        default:            return juce::String::fromUTF8 (spec (id).name);
    }
}

// ============================================================ Look & Feel
KeimLookAndFeel::KeimLookAndFeel()
{
    using namespace ui::col;
    setColour (juce::PopupMenu::backgroundColourId, plateHi);
    setColour (juce::PopupMenu::textColourId, ink);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, ink);
    setColour (juce::PopupMenu::highlightedTextColourId, ivory);
    setColour (juce::PopupMenu::headerTextColourId, inkSoft);
    setColour (juce::TextButton::textColourOffId, ink);
    setColour (juce::TextButton::textColourOnId, ink);
}

void KeimLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                        float start, float end, juce::Slider& s)
{
    using namespace ui::col;
    const float size = (float) juce::jmin (w, h);
    const float cx = (float) x + (float) w * 0.5f, cy = (float) y + (float) h * 0.5f;
    const float r = size * 0.5f - 3.0f;
    const float angle = start + pos * (end - start);
    const juce::PathStrokeType stroke (3.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

    juce::Path track; track.addCentredArc (cx, cy, r, r, 0, start, end, true);
    g.setColour (groove); g.strokePath (track, stroke);

    const bool bipolar = (bool) s.getProperties().getWithDefault ("bipolar", false);
    const float from = bipolar ? (start + end) * 0.5f : start;
    if (std::abs (angle - from) > 0.01f)
    {
        juce::Path val; val.addCentredArc (cx, cy, r, r, 0, juce::jmin (from, angle), juce::jmax (from, angle), true);
        g.setColour (accent); g.strokePath (val, stroke);
    }

    const float cr = r - 6.5f;
    g.setColour (juce::Colours::black.withAlpha (0.28f));
    g.drawEllipse (cx - cr - 1.5f, cy - cr - 1.5f, (cr + 1.5f) * 2, (cr + 1.5f) * 2, 1.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff555a5f), cx - cr * 0.35f, cy - cr * 0.45f,
                                             juce::Colour (0xff101214), cx + cr * 0.6f, cy + cr * 0.7f, true));
    g.fillEllipse (cx - cr, cy - cr, cr * 2, cr * 2);

    const float sx = std::sin (angle), sy = -std::cos (angle);
    g.setColour (ivory);
    g.drawLine (cx + sx * cr * 0.35f, cy + sy * cr * 0.35f, cx + sx * cr * 0.85f, cy + sy * cr * 0.85f, 2.2f);
}

void KeimLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    using namespace ui::col;
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    if (b.getProperties().getWithDefault ("chip", false))
    {
        g.setColour (over ? ink.withAlpha (0.08f) : juce::Colours::white.withAlpha (0.18f));
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
        g.setColour (ink.withAlpha (0.4f));
        g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.0f);
        return;
    }
    g.setGradientFill (juce::ColourGradient (down ? juce::Colour (0xffd0d1cb) : juce::Colour (0xffeeefe9), 0, r.getY(),
                                             down ? juce::Colour (0xffe2e3dd) : juce::Colour (0xffd4d5cf), 0, r.getBottom(), false));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (ink.withAlpha (over ? 0.55f : 0.3f));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
}

void KeimLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setColour (ui::col::ink);
    g.setFont (ui::font (15.0f));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (8, 0), juce::Justification::centred, 1, 0.7f);
}

// ============================================================ Regler
KeimKnob::KeimKnob (KeimProcessor& p, int id, std::function<void (const juce::String&)> readout)
    : name (ui::upper (uiName (id))), onReadout (std::move (readout))
{
    slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.setMouseDragSensitivity (190);
    slider.getProperties().set ("bipolar", spec (id).bipolar);
    addAndMakeVisible (slider);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, spec (id).id, slider);
    auto* prm = p.param (id);
    slider.setDoubleClickReturnValue (true, (double) prm->convertFrom0to1 (prm->getDefaultValue()));
    slider.onValueChange = [this] { repaint(); if (slider.isMouseButtonDown()) showReadout(); };
    slider.onDragStart = [this] { showReadout(); };
}

void KeimKnob::showReadout()
{
    if (onReadout) onReadout (name + "   " + slider.getTextFromValue (slider.getValue()));
}

void KeimKnob::resized()
{
    auto r = getLocalBounds();
    slider.setBounds (r.removeFromTop (50).withSizeKeepingCentre (50, 50));
}

void KeimKnob::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();
    r.removeFromTop (51);
    g.setColour (ui::col::ink);
    g.setFont (ui::font (11.5f));
    g.drawFittedText (name, r.removeFromTop (14).reduced (3, 0), juce::Justification::centred, 1, 0.6f);
    g.setColour (ui::col::inkSoft);
    g.setFont (ui::mono (11.0f));
    g.drawFittedText (slider.getTextFromValue (slider.getValue()), r.removeFromTop (13), juce::Justification::centred, 1, 0.8f);
}

// ============================================================ Tastenreihe
SegControl::SegControl (juce::RangedAudioParameter& p, const char* const* names, int count, bool waveIcons)
    : waves (waveIcons)
{
    for (int i = 0; i < count; ++i) labels.add (juce::String::fromUTF8 (names[i]));
    attachment = std::make_unique<juce::ParameterAttachment> (p, [this] (float v) { selected = juce::roundToInt (v); repaint(); }, nullptr);
    attachment->sendInitialUpdate();
}

static void drawWave (juce::Graphics& g, int wave, juce::Rectangle<float> r)
{
    juce::Path p;
    const float x0 = r.getX(), x1 = r.getRight(), y0 = r.getY(), y1 = r.getBottom(), xm = r.getCentreX(), ym = r.getCentreY();
    switch (wave)
    {
        case 0: p.startNewSubPath (x0, y1); p.lineTo (xm, y0); p.lineTo (xm, y1); p.lineTo (x1, y0); break;
        case 1: p.startNewSubPath (x0, y1); p.lineTo (x0, y0); p.lineTo (xm, y0); p.lineTo (xm, y1); p.lineTo (x1, y1); p.lineTo (x1, y0); break;
        case 2: p.startNewSubPath (x0, ym); p.lineTo (x0 + r.getWidth() * 0.25f, y0); p.lineTo (x0 + r.getWidth() * 0.75f, y1); p.lineTo (x1, ym); break;
        default:
            p.startNewSubPath (x0, ym);
            for (int i = 1; i <= 24; ++i)
                p.lineTo (x0 + r.getWidth() * (float) i / 24.0f, ym - std::sin ((float) i / 24.0f * juce::MathConstants<float>::twoPi) * r.getHeight() * 0.5f);
            break;
    }
    g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void SegControl::paint (juce::Graphics& g)
{
    using namespace ui::col;
    const int n = labels.size();
    const float gap = 4.0f, w = ((float) getWidth() - gap * (float) (n - 1)) / (float) n;
    for (int i = 0; i < n; ++i)
    {
        juce::Rectangle<float> r ((float) i * (w + gap), 0.5f, w, (float) getHeight() - 1.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffeeefe9), 0, r.getY(), juce::Colour (0xffd4d5cf), 0, r.getBottom(), false));
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (ink.withAlpha (0.28f));
        g.drawRoundedRectangle (r, 4.0f, 1.0f);
        ui::drawLed (g, { r.getX() + 9.0f, r.getY() + 9.0f }, 3.0f, i == selected);
        g.setColour (ink);
        if (waves)
        {
            drawWave (g, i, juce::Rectangle<float> (r.getCentreX() - 11.0f, r.getY() + 6.0f, 22.0f, 11.0f));
            g.setFont (ui::font (12.5f));
            g.drawFittedText (labels[i], r.withTrimmedTop (r.getHeight() * 0.5f).toNearestInt().reduced (2, 0), juce::Justification::centred, 1, 0.7f);
        }
        else
        {
            g.setFont (ui::font (13.5f));
            g.drawFittedText (labels[i], r.toNearestInt().reduced (12, 0).withTrimmedLeft (4), juce::Justification::centred, 1, 0.7f);
        }
    }
}

void SegControl::mouseDown (const juce::MouseEvent& e)
{
    const int i = juce::jlimit (0, labels.size() - 1, (int) ((float) e.x / (float) getWidth() * (float) labels.size()));
    attachment->setValueAsCompleteGesture ((float) i);
}

// ============================================================ Schalter
LedToggle::LedToggle (juce::RangedAudioParameter& p, const juce::String& text, bool dark)
    : label (text), darkStyle (dark)
{
    attachment = std::make_unique<juce::ParameterAttachment> (p, [this] (float v) { on = v >= 0.5f; repaint(); }, nullptr);
    attachment->sendInitialUpdate();
}

void LedToggle::paint (juce::Graphics& g)
{
    using namespace ui::col;
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    if (darkStyle) { g.setColour (juce::Colour (0xff33373b)); g.fillRoundedRectangle (r, 4.0f); g.setColour (juce::Colour (0xff4a4f53)); }
    else
    {
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffeeefe9), 0, r.getY(), juce::Colour (0xffd4d5cf), 0, r.getBottom(), false));
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (ink.withAlpha (0.3f));
    }
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
    auto t = getLocalBounds().reduced (8, 0);
    const float ly = (float) getHeight() * 0.5f;
    ui::drawLed (g, { (float) t.getX() + 4.0f, ly }, 3.5f, on);
    t.removeFromLeft (14);
    g.setColour (darkStyle ? ivory : ink);
    g.setFont (ui::font (12.5f).withExtraKerningFactor (0.04f));
    g.drawFittedText (ui::upper (label), t, juce::Justification::centred, 1, 0.7f);
}

void LedToggle::mouseDown (const juce::MouseEvent&) { attachment->setValueAsCompleteGesture (on ? 0.0f : 1.0f); }

// ============================================================ Morph-Pad
MorphPad::MorphPad (KeimProcessor& p) : proc (p) { setOpaque (true); }

void MorphPad::moveTo (juce::Point<float> pt)
{
    const auto s = screen().toFloat();
    proc.morphTo ((pt.x - s.getX()) / s.getWidth(), (pt.y - s.getY()) / s.getHeight());
}

void MorphPad::mouseDown (const juce::MouseEvent& e) { proc.beginMorph(); moveTo (e.position); }
void MorphPad::mouseDrag (const juce::MouseEvent& e) { moveTo (e.position); }
void MorphPad::mouseUp (const juce::MouseEvent&)     { proc.endMorph(); }

void MorphPad::paint (juce::Graphics& g)
{
    using namespace ui::col;
    // Rahmen
    g.fillAll (panel);
    auto outer = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff2b2e31)); g.fillRoundedRectangle (outer, 8.0f);
    g.setColour (juce::Colour (0xff121416)); g.drawRoundedRectangle (outer.reduced (0.5f), 8.0f, 1.0f);
    g.setColour (oledBg); g.fillRoundedRectangle (outer.reduced (5.0f), 5.0f);

    // Pixelbild
    const int W = img.getWidth(), H = img.getHeight();
    {
        juce::Image::BitmapData bd (img, juce::Image::BitmapData::writeOnly);
        auto px = [&] (int x, int y, float a)
        {
            if (x < 0 || y < 0 || x >= W || y >= H) return;
            bd.setPixelColour (x, y, oledBg.interpolatedWith (oled, juce::jlimit (0.0f, 1.0f, a)));
        };
        for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) bd.setPixelColour (x, y, oledBg);
        for (int x = 8; x < W; x += 16) for (int y = 8; y < H; y += 16) px (x, y, 0.16f);

        // Wellenform
        float buf[1024];
        proc.scope.read (buf, 1024);
        int s = 0;
        for (int i = 1; i < 512; ++i) if (buf[i - 1] < 0 && buf[i] >= 0) { s = i; break; }
        int prev = -1;
        for (int x = 0; x < W; ++x)
        {
            const float v = buf[juce::jmin (1023, s + x * 3)];
            const int y = juce::roundToInt ((float) H * 0.5f - juce::jlimit (-1.0f, 1.0f, v * 1.7f) * (float) H * 0.4f);
            if (prev < 0) px (x, y, 0.55f);
            else for (int yy = juce::jmin (y, prev); yy <= juce::jmax (y, prev); ++yy) px (x, yy, 0.55f);
            prev = y;
        }

        // Punkt + Fadenkreuz + Spur
        const auto m = proc.getMorphPos();
        const int dx = juce::roundToInt (m.x * (float) (W - 11)) + 5, dy = juce::roundToInt (m.y * (float) (H - 11)) + 5;
        for (int x = 0; x < W; x += 2) px (x, dy, 0.3f);
        for (int y = 0; y < H; y += 2) px (dx, y, 0.3f);
        trail[(size_t) trailHead] = { dx, dy }; trailHead = (trailHead + 1) % (int) trail.size();
        trailCount = juce::jmin ((int) trail.size(), trailCount + 1);
        for (int i = 0; i < trailCount; ++i)
        {
            const auto& t = trail[(size_t) ((trailHead - 1 - i + (int) trail.size()) % (int) trail.size())];
            const float a = 0.45f * (1.0f - (float) i / (float) trail.size());
            px (t.x - 1, t.y - 1, a); px (t.x, t.y - 1, a); px (t.x - 1, t.y, a); px (t.x, t.y, a);
        }
        if (proc.isMorphActive())
        { for (int yy = -2; yy <= 2; ++yy) for (int xx = -2; xx <= 2; ++xx) px (dx + xx, dy + yy, 1.0f); }
        else
            for (int k = -3; k <= 3; ++k) { px (dx + k, dy - 3, 1); px (dx + k, dy + 3, 1); px (dx - 3, dy + k, 1); px (dx + 3, dy + k, 1); }

        // Eckmarken nach Gewicht
        const auto w = morphWeights (m.x, m.y);
        const int cx[4] = { 0, W - 1, 0, W - 1 }, cy[4] = { 0, 0, H - 1, H - 1 };
        for (int i = 0; i < 4; ++i)
        {
            const float a = proc.isMorphActive() ? 0.25f + w[(size_t) i] * 0.75f : 0.3f;
            const int sx = i % 2 == 0 ? 1 : -1, sy = i < 2 ? 1 : -1;
            for (int k = 0; k < 6; ++k) { px (cx[i] + k * sx, cy[i], a); px (cx[i], cy[i] + k * sy, a); }
        }
    }
    g.setImageResamplingQuality (juce::Graphics::lowResamplingQuality);
    g.drawImage (img, screen().toFloat(), juce::RectanglePlacement::stretchToFit);
}

// ============================================================ Ecken-Beschriftung
class CornerLabel : public juce::Component
{
public:
    std::function<void()> onClick;
    juce::String name, weight;
    bool right = false, hi = false;
    void paint (juce::Graphics& g) override
    {
        using namespace ui::col;
        auto r = getLocalBounds();
        g.setFont (ui::mono (12.0f));
        g.setColour (hi ? accent : inkSoft);
        auto wr = right ? r.removeFromRight (44) : r.removeFromLeft (44);
        g.drawText (weight, wr, right ? juce::Justification::centredRight : juce::Justification::centredLeft);
        g.setFont (ui::font (16.0f));
        g.setColour (ink);
        const auto just = right ? juce::Justification::centredRight : juce::Justification::centredLeft;
        auto nr = right ? r.withTrimmedRight (6) : r.withTrimmedLeft (6);
        g.drawFittedText (name, nr, just, 1, 0.75f);
        const int tw = juce::jmin (nr.getWidth(), juce::GlyphArrangement::getStringWidthInt (ui::font (16.0f), name));
        const int x = right ? nr.getRight() - tw : nr.getX();
        g.setColour (inkSoft);
        for (int px = x; px < x + tw; px += 3) g.fillRect (px, nr.getCentreY() + 10, 1, 1);
    }
    void mouseUp (const juce::MouseEvent& e) override { if (onClick && getLocalBounds().contains (e.getPosition())) onClick(); }
};

// ============================================================ Hauptfläche
class MainPanel : public juce::Component, private juce::Timer, private juce::ChangeListener
{
public:
    explicit MainPanel (KeimProcessor& p)
        : proc (p), pad (p), keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
    {
        auto showReadout = [this] (const juce::String& t) { setReadout (t); };
        const int knobIds[] = { pMix, pSemi, pDetune, pSub, pNoise, pSpread, pCutoff, pRes, pFenv, pKtrack, pFA, pFD, pFS, pFR,
                                pAA, pAD, pAS, pAR, pGlide, pLevel, pLfoRate, pLfoFilter, pLfoPitch, pDrive, pChorus, pRMix, pRSize,
                                pDMix, pDFb, pBpm, pArpGate, pArpMut };
        for (int id : knobIds) { auto* k = knobs.add (new KeimKnob (p, id, showReadout)); addAndMakeVisible (k); byId[id] = k; }

        auto addSeg = [&] (int id, bool waves)
        {
            const auto& s = spec (id);
            auto* c = segs.add (new SegControl (*p.param (id), s.choices, s.numChoices, waves));
            addAndMakeVisible (c); byId[id] = c;
        };
        addSeg (pOsc1Wave, true); addSeg (pOsc2Wave, true); addSeg (pFilterType, false); addSeg (pMode, false);
        addSeg (pDDiv, false); addSeg (pArpMode, false); addSeg (pArpRate, false); addSeg (pArpOct, false);

        auto addTog = [&] (int id, const juce::String& label)
        {
            auto* t = toggles.add (new LedToggle (*p.param (id), label));
            addAndMakeVisible (t); byId[id] = t;
        };
        addTog (pArpOn, "Arp an"); addTog (pHold, "Halten"); addTog (pSync, "DAW-Sync");

        addAndMakeVisible (pad);
        for (int i = 0; i < 4; ++i)
        {
            auto* c = cornerLabels.add (new CornerLabel());
            c->right = (i % 2) == 1;
            c->onClick = [this, i] { showCornerMenu (i); };
            addAndMakeVisible (c);
        }

        prevBtn.setButtonText (juce::String::fromUTF8 ("◀"));
        nextBtn.setButtonText (juce::String::fromUTF8 ("▶"));
        diceBtn.setButtonText (juce::String::fromUTF8 ("Würfeln"));
        prevBtn.onClick = [this] { stepPreset (-1); };
        nextBtn.onClick = [this] { stepPreset (1); };
        presetBtn.onClick = [this] { showPresetMenu(); };
        diceBtn.onClick = [this] { proc.randomize(); };
        for (auto* b : { &prevBtn, &nextBtn, &presetBtn, &diceBtn }) addAndMakeVisible (b);

        keyboard.setOctaveForMiddleC (4);
        keyboard.setAvailableRange (21, 108);
        keyboard.setLowestVisibleKey (36);
        keyboard.setColour (juce::MidiKeyboardComponent::whiteNoteColourId, ui::col::ivory);
        keyboard.setColour (juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour (0xff1c1d1f));
        keyboard.setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colour (0xff0d0e0f));
        keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, ui::col::accent.withAlpha (0.22f));
        keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, ui::col::accent.withAlpha (0.75f));
        keyboard.setColour (juce::MidiKeyboardComponent::upDownButtonBackgroundColourId, ui::col::bar);
        keyboard.setColour (juce::MidiKeyboardComponent::upDownButtonArrowColourId, ui::col::ivory);
        keyboard.setColour (juce::MidiKeyboardComponent::textLabelColourId, juce::Colour (0xff8d8b83));
        addAndMakeVisible (keyboard);

        proc.addChangeListener (this);
        refreshFromProcessor();
        startTimerHz (30);
    }

    ~MainPanel() override { proc.removeChangeListener (this); }

    void setReadout (const juce::String& t)
    {
        readout = t;
        readoutUntil = juce::Time::getMillisecondCounterHiRes() + 1600.0;
        repaint (readoutRect);
    }

    void resized() override
    {
        labels.clear(); modules.clear();
        const int top = 66, kbTop = KeimEditor::baseH - 100, bottom = kbTop - 10;

        // Kopfleiste
        prevBtn.setBounds (372, 14, 36, 34);
        presetBtn.setBounds (412, 14, 236, 34);
        nextBtn.setBounds (652, 14, 36, 34);
        diceBtn.setBounds (700, 14, 110, 34);
        readoutRect = { 826, 14, 250, 34 };
        ledRect = { 1092, 12, 134, 38 };

        // Morph-Modul
        const juce::Rectangle<int> mr (14, top, 340, bottom - top);
        modules.push_back ({ "", "Morph", juce::String::fromUTF8 ("Ziehen = überblenden"), mr });
        auto m = mr.reduced (14, 0).withTrimmedTop (44);
        auto c1 = m.removeFromTop (26);
        cornerLabels[0]->setBounds (c1.removeFromLeft (c1.getWidth() / 2));
        cornerLabels[1]->setBounds (c1);
        m.removeFromTop (6);
        pad.setBounds (m.removeFromTop (m.getWidth()));
        m.removeFromTop (6);
        auto c2 = m.removeFromTop (26);
        cornerLabels[2]->setBounds (c2.removeFromLeft (c2.getWidth() / 2));
        cornerLabels[3]->setBounds (c2);
        morphTextRect = m.withTrimmedTop (12).withTrimmedBottom (10);

        // Rechte Spalten
        const int x0 = 366, colW = (KeimEditor::baseW - 14 - x0 - 20) / 3, row1H = 334;
        auto col = [&] (int c, int row)
        {
            const int y = row == 0 ? top : top + row1H + 10;
            const int h = row == 0 ? row1H : bottom - (top + row1H + 10);
            return juce::Rectangle<int> (x0 + c * (colW + 10), y, colW, h);
        };
        auto inner = [] (juce::Rectangle<int> r) { return r.reduced (14, 0).withTrimmedTop (44).withTrimmedBottom (10); };

        auto placeKnobs = [&] (juce::Rectangle<int>& a, std::initializer_list<int> ids)
        {
            const int cw = a.getWidth() / 4; int i = 0; juce::Rectangle<int> row;
            for (int id : ids)
            {
                if (i % 4 == 0) { row = a.removeFromTop (78); a.removeFromTop (2); }
                byId[id]->setBounds (row.removeFromLeft (cw)); ++i;
            }
        };
        auto placeSeg = [&] (juce::Rectangle<int>& a, int id, int h, const juce::String& label = {}, bool sub = false)
        {
            if (label.isNotEmpty()) labels.push_back ({ label, a.removeFromTop (sub ? 18 : 15), sub });
            byId[id]->setBounds (a.removeFromTop (h));
            a.removeFromTop (8);
        };
        auto subhead = [&] (juce::Rectangle<int>& a, const juce::String& t) { labels.push_back ({ t, a.removeFromTop (18), true }); a.removeFromTop (4); };

        { auto r = col (0, 0); modules.push_back ({ "01", "Oszillatoren", "Klangquelle", r }); auto a = inner (r);
          placeSeg (a, pOsc1Wave, 40, "Osz 1"); placeSeg (a, pOsc2Wave, 40, "Osz 2");
          placeKnobs (a, { pMix, pSemi, pDetune, pSub, pNoise, pSpread }); }
        { auto r = col (1, 0); modules.push_back ({ "02", "Filter", "Klangfarbe", r }); auto a = inner (r);
          placeSeg (a, pFilterType, 30); placeKnobs (a, { pCutoff, pRes, pFenv, pKtrack });
          a.removeFromTop (4); subhead (a, juce::String::fromUTF8 ("Filter-Hüllkurve")); placeKnobs (a, { pFA, pFD, pFS, pFR }); }
        { auto r = col (2, 0); modules.push_back ({ "03", juce::String::fromUTF8 ("Verstärker"), juce::String::fromUTF8 ("Lautstärkeverlauf"), r }); auto a = inner (r);
          placeKnobs (a, { pAA, pAD, pAS, pAR }); a.removeFromTop (4);
          placeSeg (a, pMode, 30, "Spielweise"); placeKnobs (a, { pGlide, pLevel }); }
        { auto r = col (0, 1); modules.push_back ({ "04", "LFO", "Bewegung", r }); auto a = inner (r);
          placeKnobs (a, { pLfoRate, pLfoFilter, pLfoPitch }); lfoTextRect = a.withTrimmedTop (10); }
        { auto r = col (1, 1); modules.push_back ({ "05", "Effekte", "Raum und Dreck", r }); auto a = inner (r);
          placeKnobs (a, { pDrive, pChorus, pRMix, pRSize }); a.removeFromTop (2);
          subhead (a, "Delay (im Tempo)"); placeSeg (a, pDDiv, 30); placeKnobs (a, { pDMix, pDFb }); }
        { auto r = col (2, 1); modules.push_back ({ "06", "Arpeggiator", "Akkord halten", r }); auto a = inner (r);
          auto t = a.removeFromTop (30); a.removeFromTop (7);
          const int tw = (t.getWidth() - 10) / 3;
          byId[pArpOn]->setBounds (t.removeFromLeft (tw)); t.removeFromLeft (5);
          byId[pHold]->setBounds (t.removeFromLeft (tw)); t.removeFromLeft (5);
          byId[pSync]->setBounds (t);
          placeSeg (a, pArpMode, 28); placeSeg (a, pArpRate, 28); placeSeg (a, pArpOct, 28);
          placeKnobs (a, { pBpm, pArpGate, pArpMut });
          const int cw = byId[pBpm]->getWidth();
          stepRect = byId[pArpMut]->getBounds().translated (cw, 0).withTrimmedTop (6).withHeight (60); }

        keyboardBar = { 0, kbTop, KeimEditor::baseW, KeimEditor::baseH - kbTop };
        keyboard.setBounds (keyboardBar.reduced (12, 10));
        keyboard.setKeyWidth ((float) keyboard.getWidth() / 36.0f);
        keyboard.setLowestVisibleKey (36);
    }

    void paint (juce::Graphics& g) override
    {
        using namespace ui::col;
        // Gebürstetes Aluminium
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffdadbd5), 0, 0, juce::Colour (0xffcdcec8), 0, (float) getHeight(), false));
        g.fillAll();
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        for (int x = 0; x < getWidth(); x += 3) g.fillRect (x, 0, 1, getHeight());

        // Logo
        g.setColour (ink);
        const auto logoFont = ui::font (28.0f).withExtraKerningFactor (0.08f);
        g.setFont (logoFont);
        g.drawText ("KEIM", 20, 10, 120, 32, juce::Justification::centredLeft);
        g.setColour (accent);
        g.drawText ("-1", 20 + juce::GlyphArrangement::getStringWidthInt (logoFont, "KEIM"), 10, 40, 32, juce::Justification::centredLeft);
        g.setColour (inkSoft);
        g.setColour (ink); g.fillRoundedRectangle (21.0f, 41.0f, 50.0f, 15.0f, 2.0f);
        g.setColour (ivory); g.setFont (ui::font (11.0f).withExtraKerningFactor (0.3f));
        g.drawText ("NOVA", 21, 41, 52, 15, juce::Justification::centred);
        g.setColour (inkSoft); g.setFont (ui::font (11.5f).withExtraKerningFactor (0.14f));
        g.drawText ("SYNTH  VST3 / AU", 79, 40, 220, 16, juce::Justification::centredLeft);

        // Stimmen-LEDs
        g.setFont (ui::font (11.0f).withExtraKerningFactor (0.14f));
        g.drawText ("STIMMEN", ledRect.withHeight (16).withTrimmedRight (2), juce::Justification::centredRight);
        {
            auto lr = ledRect.withTrimmedTop (18);
            const int nv = proc.uiVoices.load();
            for (int i = 0; i < kPoly; ++i)
                ui::drawLed (g, { (float) lr.getRight() - 6.0f - (float) (kPoly - 1 - i) * 13.0f, (float) lr.getY() + 6.0f }, 3.5f, i < nv);
        }

        // Anzeige
        g.setColour (juce::Colour (0xff0b0f0e)); g.fillRoundedRectangle (readoutRect.toFloat(), 4.0f);
        g.setColour (juce::Colour (0xff33383a)); g.drawRoundedRectangle (readoutRect.toFloat().reduced (0.5f), 4.0f, 1.0f);
        g.setColour (oled); g.setFont (ui::mono (13.0f));
        const bool showRead = juce::Time::getMillisecondCounterHiRes() < readoutUntil;
        g.drawFittedText (showRead ? readout : proc.getSoundName(), readoutRect.reduced (10, 0), juce::Justification::centredLeft, 1, 0.8f);

        // Module
        for (const auto& mo : modules)
        {
            auto r = mo.r.toFloat();
            g.setGradientFill (juce::ColourGradient (plateHi, 0, r.getY(), plateLo, 0, r.getBottom(), false));
            g.fillRoundedRectangle (r, 6.0f);
            g.setColour (juce::Colours::black.withAlpha (0.13f)); g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
            g.setColour (juce::Colours::white.withAlpha (0.7f)); g.drawHorizontalLine ((int) r.getY() + 1, r.getX() + 6, r.getRight() - 6);
            for (float sx : { r.getX() + 10.0f, r.getRight() - 10.0f })
            {
                g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff2f2ee), sx - 1, r.getY() + 9, juce::Colour (0xff9a9b95), sx + 3, r.getY() + 13, true));
                g.fillEllipse (sx - 3.5f, r.getY() + 7.0f, 7.0f, 7.0f);
            }
            auto h = mo.r.reduced (24, 0).withTrimmedTop (12).withHeight (24);
            g.setColour (ink); g.fillRect (h.getX(), h.getBottom() + 2, h.getWidth(), 1);
            int x = h.getX();
            if (mo.no.isNotEmpty())
            {
                g.setColour (inkSoft); g.setFont (ui::mono (11.0f));
                g.drawText (mo.no, x, h.getY(), 22, h.getHeight(), juce::Justification::centredLeft);
                x += 24;
            }
            g.setColour (ink); g.setFont (ui::font (13.0f).withExtraKerningFactor (0.18f));
            g.drawText (ui::upper (mo.title), x, h.getY(), 180, h.getHeight(), juce::Justification::centredLeft);
            g.setColour (inkSoft); g.setFont (ui::font (13.5f, false));
            g.drawText (mo.hint, h, juce::Justification::centredRight);
        }

        // Zeilenbeschriftungen
        for (const auto& l : labels)
        {
            g.setColour (inkSoft);
            g.setFont (ui::font (11.5f).withExtraKerningFactor (0.12f));
            g.drawText (ui::upper (l.text), l.r, juce::Justification::centredLeft);
            if (l.sub)
            {
                const int tw = juce::GlyphArrangement::getStringWidthInt (ui::font (11.5f).withExtraKerningFactor (0.12f), ui::upper (l.text));
                g.setColour (ink.withAlpha (0.25f));
                g.fillRect (l.r.getX() + tw + 8, l.r.getCentreY(), l.r.getWidth() - tw - 8, 1);
            }
        }

        // Arp-Schritte
        if (! stepRect.isEmpty())
        {
            g.setColour (inkSoft); g.setFont (ui::font (11.5f).withExtraKerningFactor (0.1f));
            g.drawText ("SCHRITT", stepRect.withHeight (14), juce::Justification::centred);
            const int st = proc.uiArpStep.load();
            const float sx = (float) stepRect.getCentreX() - 22.5f;
            for (int i = 0; i < 8; ++i)
                ui::drawLed (g, { sx + (float) (i % 4) * 15.0f, (float) stepRect.getY() + 28.0f + (float) (i / 4) * 15.0f }, 4.0f, i == st);
        }

        // Texte
        g.setColour (inkSoft); g.setFont (ui::font (14.5f, false));
        g.drawFittedText (juce::String::fromUTF8 ("Jede Ecke hält einen kompletten Sound. Klick auf einen Namen, um die Ecke neu zu belegen, auch mit deinem aktuellen Sound. "
                                                  "Alle Regler lassen sich in der DAW automatisieren."),
                          morphTextRect, juce::Justification::topLeft, 6, 1.0f);
        g.drawFittedText (juce::String::fromUTF8 ("Der LFO bewegt Filter und Tonhöhe. Das Modrad am MIDI-Keyboard gibt zusätzlich Vibrato."),
                          lfoTextRect, juce::Justification::topLeft, 3, 1.0f);

        // Tastaturleiste
        g.setColour (bar); g.fillRect (keyboardBar);
    }

private:
    struct Module { juce::String no, title, hint; juce::Rectangle<int> r; };
    struct Label  { juce::String text; juce::Rectangle<int> r; bool sub; };

    KeimProcessor& proc;
    juce::OwnedArray<KeimKnob> knobs;
    juce::OwnedArray<SegControl> segs;
    juce::OwnedArray<LedToggle> toggles;
    juce::OwnedArray<CornerLabel> cornerLabels;
    std::map<int, juce::Component*> byId;
    MorphPad pad;
    juce::TextButton prevBtn, nextBtn, presetBtn, diceBtn;
    juce::MidiKeyboardComponent keyboard;
    std::vector<Module> modules;
    std::vector<Label> labels;
    juce::Rectangle<int> readoutRect, ledRect, stepRect, morphTextRect, lfoTextRect, keyboardBar;
    juce::String readout;
    double readoutUntil = 0;
    int lastVoices = -1, lastStep = -2;
    bool readoutShown = false;
    juce::Point<float> lastMorph { -1, -1 };

    void timerCallback() override
    {
        pad.repaint();
        const int nv = proc.uiVoices.load(), st = proc.uiArpStep.load();
        if (nv != lastVoices) { lastVoices = nv; repaint (ledRect.expanded (4)); }
        if (st != lastStep)   { lastStep = st; repaint (stepRect.expanded (4)); }
        const bool showRead = juce::Time::getMillisecondCounterHiRes() < readoutUntil;
        if (showRead != readoutShown) { readoutShown = showRead; repaint (readoutRect); }
        if (proc.getMorphPos() != lastMorph) { lastMorph = proc.getMorphPos(); updateCorners(); }
    }

    void changeListenerCallback (juce::ChangeBroadcaster*) override { refreshFromProcessor(); }

    void refreshFromProcessor()
    {
        presetBtn.setButtonText (proc.getSoundName() + juce::String::fromUTF8 ("  ▾"));
        updateCorners();
        repaint (readoutRect);
    }

    void updateCorners()
    {
        const auto m = proc.getMorphPos();
        const auto w = morphWeights (m.x, m.y);
        for (int i = 0; i < 4; ++i)
        {
            auto* c = cornerLabels[i];
            c->name = proc.getCornerName (i);
            c->weight = proc.isMorphActive() ? juce::String (juce::roundToInt (w[(size_t) i] * 100.0f)) + " %" : juce::String::fromUTF8 ("–");
            c->hi = proc.isMorphActive() && w[(size_t) i] > 0.5f;
            c->repaint();
        }
    }

    void stepPreset (int d)
    {
        const int n = numFactoryPresets();
        proc.loadFactory ((proc.getCurrentProgram() + d + n) % n);
    }

    void showPresetMenu()
    {
        juce::PopupMenu menu;
        menu.addSectionHeader ("Werkssounds");
        for (int i = 0; i < numFactoryPresets(); ++i)
            menu.addItem (i + 1, proc.getProgramName (i), true, proc.getSoundName() == proc.getProgramName (i));
        menu.addSeparator();
        menu.addItem (1000, juce::String::fromUTF8 ("Würfeln"));
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetBtn), [this] (int r)
        {
            if (r == 1000) proc.randomize();
            else if (r > 0) proc.loadFactory (r - 1);
        });
    }

    void showCornerMenu (int corner)
    {
        static const char* names[] = { "oben links", "oben rechts", "unten links", "unten rechts" };
        juce::PopupMenu menu;
        menu.addSectionHeader ("Ecke " + juce::String (names[corner]) + " belegen");
        menu.addItem (500, "Aktueller Sound");
        menu.addSeparator();
        for (int i = 0; i < numFactoryPresets(); ++i) menu.addItem (i + 1, proc.getProgramName (i));
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (cornerLabels[corner]), [this, corner] (int r)
        {
            if (r == 500)
            {
                const auto name = proc.getSoundName().isNotEmpty() ? proc.getSoundName() : juce::String ("Eigener Sound");
                proc.setCorner (corner, proc.currentPatch(), name);
            }
            else if (r > 0) proc.setCorner (corner, factoryPatch (r - 1), proc.getProgramName (r - 1));
        });
    }
};

// ============================================================ Editor
KeimEditor::KeimEditor (KeimProcessor& p) : AudioProcessorEditor (&p)
{
    setLookAndFeel (&lnf);
    panel = std::make_unique<MainPanel> (p);
    addAndMakeVisible (*panel);
    setResizable (true, true);
    setResizeLimits (baseW / 2, baseH / 2, baseW * 2, baseH * 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) baseW / (double) baseH);
    setSize ((int) (baseW * 0.9f), (int) (baseH * 0.9f));
}

KeimEditor::~KeimEditor()
{
    setLookAndFeel (nullptr);
    panel.reset();
}

void KeimEditor::resized()
{
    panel->setBounds (0, 0, baseW, baseH);
    panel->setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) baseW));
}
