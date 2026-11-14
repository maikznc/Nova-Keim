#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include "dsp/Engine.h"
#include "dsp/Presets.h"

// Ringpuffer für die Wellenform-Anzeige (Audio-Thread schreibt, Oberfläche liest)
class ScopeBuffer
{
public:
    static constexpr int size = 4096, mask = size - 1;

    void push (const float* l, const float* r, int n)
    {
        int w = writePos.load (std::memory_order_relaxed);
        for (int i = 0; i < n; ++i)
            data[(size_t) ((w + i) & mask)].store (0.5f * (l[i] + (r != nullptr ? r[i] : l[i])), std::memory_order_relaxed);
        writePos.store ((w + n) & mask, std::memory_order_release);
    }

    void read (float* out, int n) const
    {
        const int w = writePos.load (std::memory_order_acquire);
        for (int i = 0; i < n; ++i)
            out[i] = data[(size_t) ((w - n + i) & mask)].load (std::memory_order_relaxed);
    }

private:
    std::array<std::atomic<float>, size> data {};
    std::atomic<int> writePos { 0 };
};

class KeimProcessor : public juce::AudioProcessor,
                      public juce::ChangeBroadcaster
{
public:
    KeimProcessor();
    ~KeimProcessor() override = default;

    // Audio
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    // Editor
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    // Infos
    const juce::String getName() const override { return "KEIM-1"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 6.0; }

    // Werkssounds als Programme (in Logic und Ableton im Preset-Menü sichtbar)
    int getNumPrograms() override { return keim::numFactoryPresets(); }
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    // Song-Zustand
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ----- für die Oberfläche (Message-Thread) -----
    keim::Patch currentPatch() const;
    void applyPatch (const keim::Patch& p);
    void loadFactory (int index);
    void randomize();

    void beginMorph();
    void morphTo (float x, float y);
    void endMorph();
    void setCorner (int index, const keim::Patch& p, const juce::String& name);
    juce::String getCornerName (int index) const;
    juce::Point<float> getMorphPos() const { return { morphX, morphY }; }
    bool isMorphActive() const { return morphOn; }
    void setMorphActive (bool on) { morphOn = on; }
    juce::String getSoundName() const { return soundName; }
    void setSoundName (const juce::String& n) { soundName = n; sendChangeMessage(); }

    juce::RangedAudioParameter* param (int id) const { return params[(size_t) id]; }

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;
    ScopeBuffer scope;
    std::atomic<int> uiVoices { 0 }, uiArpStep { -1 };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void handleMidi (const juce::MidiMessage& m);
    static juce::String patchToString (const keim::Patch& p);
    static keim::Patch patchFromString (const juce::String& s);

    keim::Engine engine;
    std::array<juce::RangedAudioParameter*, keim::kNumParams> params {};
    std::array<std::atomic<float>*, keim::kNumParams> raw {};
    std::vector<float> scratch;
    int currentProgram = 3;

    // Morph-Zustand (wird mit dem Song gespeichert)
    std::array<keim::Patch, 4> corners;
    std::array<juce::String, 4> cornerNames;
    float morphX = 1.0f, morphY = 0.0f;
    bool morphOn = true;
    juce::String soundName;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KeimProcessor)
};
