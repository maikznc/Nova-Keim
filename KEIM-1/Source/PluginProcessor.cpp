#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace keim;

KeimProcessor::KeimProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "KEIM1", createLayout())
{
    for (int id = 0; id < kNumParams; ++id)
    {
        params[(size_t) id] = apvts.getParameter (spec (id).id);
        raw[(size_t) id] = apvts.getRawParameterValue (spec (id).id);
        jassert (params[(size_t) id] != nullptr && raw[(size_t) id] != nullptr);
    }

    const auto dc = defaultCorners();
    for (size_t i = 0; i < 4; ++i)
    {
        corners[i] = factoryPatch (dc[i]);
        cornerNames[i] = juce::String::fromUTF8 (factoryPresets()[(size_t) dc[i]].name);
    }

    // Start mit "Supersaw Lead" (rechte obere Ecke)
    const auto start = factoryPatch (currentProgram);
    for (int id = 0; id < kNumParams; ++id)
        if (isMorphParam (id))
            params[(size_t) id]->setValueNotifyingHost (params[(size_t) id]->convertTo0to1 (start[id]));
    soundName = juce::String::fromUTF8 (factoryPresets()[(size_t) currentProgram].name);
}

juce::AudioProcessorValueTreeState::ParameterLayout KeimProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (int id = 0; id < kNumParams; ++id)
    {
        const auto& s = spec (id);
        const juce::ParameterID pid { s.id, 1 };
        const auto name = juce::String::fromUTF8 (s.name);

        if (s.kind == Kind::Float)
        {
            juce::NormalisableRange<float> range (s.min, s.max,
                [id] (float, float, float n) { return fromNorm (id, n); },
                [id] (float, float, float v) { return toNorm (id, v); },
                [id] (float, float, float v) { return snap (id, v); });

            auto attrs = juce::AudioParameterFloatAttributes()
                             .withStringFromValueFunction ([id] (float v, int) { return juce::String::fromUTF8 (formatValue (id, v).c_str()); })
                             .withValueFromStringFunction ([id] (const juce::String& t) { return parseValue (id, t.toStdString()); });

            layout.add (std::make_unique<juce::AudioParameterFloat> (pid, name, range, s.def, attrs));
        }
        else if (s.kind == Kind::Choice)
        {
            juce::StringArray choices;
            for (int i = 0; i < s.numChoices; ++i) choices.add (juce::String::fromUTF8 (s.choices[i]));
            layout.add (std::make_unique<juce::AudioParameterChoice> (pid, name, choices, (int) s.def));
        }
        else
        {
            layout.add (std::make_unique<juce::AudioParameterBool> (pid, name, s.def >= 0.5f));
        }
    }
    return layout;
}

// ---------- Audio ----------
void KeimProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate);
    for (int id = 0; id < kNumParams; ++id) engine.setParam (id, raw[(size_t) id]->load());
    engine.reset();
    scratch.assign ((size_t) juce::jmax (1, samplesPerBlock), 0.0f);
    keyboardState.reset();
}

bool KeimProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void KeimProcessor::handleMidi (const juce::MidiMessage& m)
{
    if (m.isNoteOn())                 engine.noteOn (m.getNoteNumber(), 0.25f + 0.75f * m.getFloatVelocity());
    else if (m.isNoteOff())           engine.noteOff (m.getNoteNumber());
    else if (m.isPitchWheel())        engine.pitchBend ((float) (m.getPitchWheelValue() - 8192) / 8192.0f * 2.0f);
    else if (m.isSustainPedalOn())    engine.setSustain (true);
    else if (m.isSustainPedalOff())   engine.setSustain (false);
    else if (m.isAllNotesOff() || m.isAllSoundOff()) engine.allNotesOff();
    else if (m.isController() && m.getControllerNumber() == 1) engine.modWheel ((float) m.getControllerValue() / 127.0f);
}

void KeimProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();
    if (n == 0) return;

    // Bildschirm-Tastatur einspeisen
    keyboardState.processNextMidiBuffer (midi, 0, n, true);

    for (int id = 0; id < kNumParams; ++id) engine.setParam (id, raw[(size_t) id]->load (std::memory_order_relaxed));

    bool tempoValid = false; double bpm = 120.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm()) { bpm = *b; tempoValid = true; }
    engine.setHostTempo (bpm, tempoValid);

    float* L = buffer.getWritePointer (0);
    float* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    if (R == nullptr)
    {
        if ((int) scratch.size() < n) scratch.resize ((size_t) n);  // nur falls der Host größere Blöcke schickt als angekündigt
        R = scratch.data();
    }

    int pos = 0;
    for (const auto meta : midi)
    {
        const int s = juce::jlimit (0, n, meta.samplePosition);
        if (s > pos) { engine.process (L + pos, R + pos, s - pos); pos = s; }
        handleMidi (meta.getMessage());
    }
    if (pos < n) engine.process (L + pos, R + pos, n - pos);

    if (buffer.getNumChannels() == 1)
        for (int i = 0; i < n; ++i) L[i] = 0.5f * (L[i] + R[i]);

    scope.push (L, buffer.getNumChannels() > 1 ? R : nullptr, n);
    uiVoices.store (engine.activeVoices(), std::memory_order_relaxed);
    uiArpStep.store (engine.arpStep(), std::memory_order_relaxed);
}

juce::AudioProcessorEditor* KeimProcessor::createEditor() { return new KeimEditor (*this); }

// ---------- Sounds ----------
Patch KeimProcessor::currentPatch() const
{
    Patch p;
    for (int id = 0; id < kNumParams; ++id) p[id] = raw[(size_t) id]->load();
    return p;
}

void KeimProcessor::applyPatch (const Patch& p)
{
    for (int id = 0; id < kNumParams; ++id)
    {
        if (! isMorphParam (id)) continue;
        auto* prm = params[(size_t) id];
        const float nv = prm->convertTo0to1 (p[id]);
        if (std::abs (prm->getValue() - nv) < 1.0e-6f) continue;
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (nv);
        prm->endChangeGesture();
    }
}

void KeimProcessor::setCurrentProgram (int index)
{
    if (index >= 0 && index < numFactoryPresets()) loadFactory (index);
}

const juce::String KeimProcessor::getProgramName (int index)
{
    if (index < 0 || index >= numFactoryPresets()) return {};
    return juce::String::fromUTF8 (factoryPresets()[(size_t) index].name);
}

void KeimProcessor::loadFactory (int index)
{
    currentProgram = juce::jlimit (0, numFactoryPresets() - 1, index);
    applyPatch (factoryPatch (currentProgram));
    soundName = getProgramName (currentProgram);
    morphOn = false;
    for (int i = 0; i < 4; ++i)
        if (cornerNames[(size_t) i] == soundName)
        {
            morphX = (float) (i % 2); morphY = i > 1 ? 1.0f : 0.0f; morphOn = true;
            break;
        }
    sendChangeMessage();
}

void KeimProcessor::randomize()
{
    std::string kind;
    const auto seed = (uint32_t) juce::Random::getSystemRandom().nextInt();
    applyPatch (randomPatch (seed, currentPatch(), &kind));
    soundName = "Zufall: " + juce::String (kind);
    morphOn = false;
    sendChangeMessage();
}

// ---------- Morph ----------
void KeimProcessor::beginMorph()
{
    for (int id = 0; id < kNumParams; ++id)
        if (isMorphParam (id)) params[(size_t) id]->beginChangeGesture();
}

void KeimProcessor::endMorph()
{
    for (int id = 0; id < kNumParams; ++id)
        if (isMorphParam (id)) params[(size_t) id]->endChangeGesture();
}

void KeimProcessor::morphTo (float x, float y)
{
    morphX = juce::jlimit (0.0f, 1.0f, x);
    morphY = juce::jlimit (0.0f, 1.0f, y);
    morphOn = true;
    const Patch p = morphPatches (corners, morphX, morphY, currentPatch());
    for (int id = 0; id < kNumParams; ++id)
    {
        if (! isMorphParam (id)) continue;
        auto* prm = params[(size_t) id];
        const float nv = prm->convertTo0to1 (p[id]);
        if (std::abs (prm->getValue() - nv) > 1.0e-5f) prm->setValueNotifyingHost (nv);
    }
    const auto w = morphWeights (morphX, morphY);
    int a = 0; for (int i = 1; i < 4; ++i) if (w[(size_t) i] > w[(size_t) a]) a = i;
    soundName = w[(size_t) a] > 0.97f ? cornerNames[(size_t) a] : juce::String ("Morph-Mischung");
    sendChangeMessage();
}

void KeimProcessor::setCorner (int index, const Patch& p, const juce::String& name)
{
    if (index < 0 || index > 3) return;
    corners[(size_t) index] = p;
    cornerNames[(size_t) index] = name;
    if (morphOn) morphTo (morphX, morphY);
    sendChangeMessage();
}

juce::String KeimProcessor::getCornerName (int index) const
{
    return index >= 0 && index < 4 ? cornerNames[(size_t) index] : juce::String();
}

// ---------- Speichern ----------
juce::String KeimProcessor::patchToString (const Patch& p)
{
    juce::StringArray parts;
    for (int id = 0; id < kNumParams; ++id)
        if (isMorphParam (id)) parts.add (juce::String (spec (id).id) + "=" + juce::String (p[id], 5));
    return parts.joinIntoString (";");
}

Patch KeimProcessor::patchFromString (const juce::String& s)
{
    Patch p = defaultPatch();
    for (const auto& part : juce::StringArray::fromTokens (s, ";", ""))
    {
        const auto key = part.upToFirstOccurrenceOf ("=", false, false);
        const auto val = part.fromFirstOccurrenceOf ("=", false, false).getFloatValue();
        for (int id = 0; id < kNumParams; ++id)
            if (key == spec (id).id) { p[id] = snap (id, val); break; }
    }
    return p;
}

void KeimProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    juce::ValueTree ui ("UI");
    ui.setProperty ("morphX", morphX, nullptr);
    ui.setProperty ("morphY", morphY, nullptr);
    ui.setProperty ("morphOn", morphOn, nullptr);
    ui.setProperty ("sound", soundName, nullptr);
    ui.setProperty ("program", currentProgram, nullptr);
    for (int i = 0; i < 4; ++i)
    {
        ui.setProperty ("cornerName" + juce::String (i), cornerNames[(size_t) i], nullptr);
        ui.setProperty ("corner" + juce::String (i), patchToString (corners[(size_t) i]), nullptr);
    }
    state.removeChild (state.getChildWithName ("UI"), nullptr);
    state.appendChild (ui, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void KeimProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    auto tree = juce::ValueTree::fromXml (*xml);
    const auto ui = tree.getChildWithName ("UI");
    if (ui.isValid())
    {
        morphX = (float) ui.getProperty ("morphX", 1.0f);
        morphY = (float) ui.getProperty ("morphY", 0.0f);
        morphOn = (bool) ui.getProperty ("morphOn", false);
        soundName = ui.getProperty ("sound", "").toString();
        currentProgram = (int) ui.getProperty ("program", 3);
        for (int i = 0; i < 4; ++i)
        {
            const auto s = ui.getProperty ("corner" + juce::String (i)).toString();
            if (s.isNotEmpty())
            {
                corners[(size_t) i] = patchFromString (s);
                cornerNames[(size_t) i] = ui.getProperty ("cornerName" + juce::String (i)).toString();
            }
        }
        tree.removeChild (ui, nullptr);
    }
    apvts.replaceState (tree);
    sendChangeMessage();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new KeimProcessor(); }
