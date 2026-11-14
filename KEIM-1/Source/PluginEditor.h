#pragma once

#include "PluginProcessor.h"

namespace ui
{
    namespace col
    {
        const juce::Colour panel   { 0xffd4d5cf };
        const juce::Colour plateHi { 0xffe3e4de };
        const juce::Colour plateLo { 0xffd6d7d1 };
        const juce::Colour groove  { 0xffb9bab3 };
        const juce::Colour ink     { 0xff17191b };
        const juce::Colour inkSoft { 0xff55595b };
        const juce::Colour accent  { 0xffe0452b };
        const juce::Colour cap     { 0xff1e2124 };
        const juce::Colour oledBg  { 0xff030605 };
        const juce::Colour oled    { 0xffc6f2e2 };
        const juce::Colour bar     { 0xff1f2225 };
        const juce::Colour ivory   { 0xfff4f2ea };
        const juce::Colour ledOff  { 0xff9d9e97 };
    }
    juce::Font font (float height, bool bold = true);
    juce::Font mono (float height);
    juce::String upper (const juce::String& s);
    void drawLed (juce::Graphics& g, juce::Point<float> c, float r, bool on);
}

class KeimLookAndFeel : public juce::LookAndFeel_V4
{
public:
    KeimLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
    juce::Font getPopupMenuFont() override { return ui::font (15.0f, false); }
};

// Drehregler mit Name und Wert darunter
class KeimKnob : public juce::Component
{
public:
    KeimKnob (KeimProcessor& p, int paramId, std::function<void (const juce::String&)> readout);
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    juce::Slider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    juce::String name;
    std::function<void (const juce::String&)> onReadout;
    void showReadout();
};

// Tastenreihe für Auswahl-Parameter (Welle, Filtertyp, Spielweise ...)
class SegControl : public juce::Component
{
public:
    SegControl (juce::RangedAudioParameter& p, const char* const* labels, int count, bool waveIcons);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    std::unique_ptr<juce::ParameterAttachment> attachment;
    juce::StringArray labels;
    bool waves;
    int selected = 0;
};

// Schalter mit LED
class LedToggle : public juce::Component
{
public:
    LedToggle (juce::RangedAudioParameter& p, const juce::String& label, bool dark = false);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    std::unique_ptr<juce::ParameterAttachment> attachment;
    juce::String label;
    bool on = false, darkStyle = false;
};

// OLED-Display mit Morph-Pad und Live-Wellenform
class MorphPad : public juce::Component
{
public:
    explicit MorphPad (KeimProcessor& p);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    KeimProcessor& proc;
    juce::Image img { juce::Image::RGB, 160, 120, true };
    std::array<juce::Point<int>, 14> trail {};
    int trailCount = 0, trailHead = 0;
    juce::Rectangle<int> screen() const { return getLocalBounds().reduced (9); }
    void moveTo (juce::Point<float> p);
};

class MainPanel;

class KeimEditor : public juce::AudioProcessorEditor
{
public:
    explicit KeimEditor (KeimProcessor&);
    ~KeimEditor() override;
    void resized() override;
    void paint (juce::Graphics&) override {}

    static constexpr int baseW = 1240, baseH = 800;

private:
    KeimLookAndFeel lnf;
    std::unique_ptr<MainPanel> panel;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KeimEditor)
};
