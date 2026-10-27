#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    using Layout = juce::AudioProcessorValueTreeState::ParameterLayout;

    juce::String percentText (float v, int)              { return juce::String (juce::roundToInt (v)) + " %"; }
    juce::String signedPercentText (float v, int)        { const int i = juce::roundToInt (v); return (i > 0 ? "+" : "") + juce::String (i) + " %"; }

    float textToNumber (const juce::String& text)        { return text.retainCharacters ("-+0123456789.").getFloatValue(); }

    juce::String msText (float v, int)
    {
        if (v >= 1000.0f) return juce::String (v / 1000.0f, 2) + " s";
        if (v >= 100.0f)  return juce::String (juce::roundToInt (v)) + " ms";
        return juce::String (v, 1) + " ms";
    }

    juce::String hzText (float v, int)
    {
        if (v >= 1000.0f) return juce::String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz";
        return juce::String (juce::roundToInt (v)) + " Hz";
    }

    void addFloat (Layout& layout, const juce::String& id, const juce::String& name, juce::NormalisableRange<float> range,
                   float defaultValue, std::function<juce::String (float, int)> text)
    {
        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, range, defaultValue,
                                                                 juce::AudioParameterFloatAttributes()
                                                                     .withStringFromValueFunction (std::move (text))
                                                                     .withValueFromStringFunction (textToNumber)));
    }

    void addPercent (Layout& layout, const juce::String& id, const juce::String& name, float defaultValue, bool bipolar = false)
    {
        addFloat (layout, id, name, { bipolar ? -100.0f : 0.0f, 100.0f, 0.1f }, defaultValue, bipolar ? signedPercentText : percentText);
    }

    void addMs (Layout& layout, const juce::String& id, const juce::String& name, float lo, float hi, float defaultValue)
    {
        juce::NormalisableRange<float> range (lo, hi, 0.01f);
        range.setSkewForCentre (lo > 0.0f ? std::sqrt (lo * hi) * 0.6f : hi * 0.15f);
        addFloat (layout, id, name, range, defaultValue, msText);
    }

    void addChoice (Layout& layout, const juce::String& id, const juce::String& name, const juce::StringArray& choices, int defaultIndex)
    {
        layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, choices, defaultIndex));
    }

    void addInt (Layout& layout, const juce::String& id, const juce::String& name, int lo, int hi, int defaultValue,
                 std::function<juce::String (int, int)> text = nullptr)
    {
        juce::AudioParameterIntAttributes attributes;
        if (text != nullptr)
            attributes = attributes.withStringFromValueFunction (std::move (text));
        layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { id, 1 }, name, lo, hi, defaultValue, attributes));
    }

    void addBool (Layout& layout, const juce::String& id, const juce::String& name, bool defaultValue)
    {
        layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name, defaultValue));
    }

    // Motifs de départ des deux séquenceurs (un rythme de coupure / FM, une rampe lente)
    constexpr float defaultPatternA[16] = { 1.0f, 0.15f, 0.6f, 0.1f, 0.85f, 0.3f, 0.45f, 0.1f,
                                            1.0f, 0.2f, 0.7f, 0.05f, 0.8f, 0.4f, 0.55f, 0.25f };
    constexpr float rateBeats[] = { 4.0f, 2.0f, 1.0f, 0.5f, 0.25f, 0.125f, 0.0625f };
}

//==============================================================================
const juce::StringArray& SequencesAudioProcessor::destinationNames()
{
    // Même ordre que seqs::Dest
    static const juce::StringArray names { "OFF", "CUTOFF", "RESO", "FM INDEX", "FM RATIO", "PITCH", "FINE", "PW", "DRIVE",
                                           "AMP", "FM LEVEL", "OSC LEVEL", "NOISE", "PAN", "FEEDBACK" };
    return names;
}

const juce::StringArray& SequencesAudioProcessor::rateNames()
{
    static const juce::StringArray names { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/64" };
    return names;
}

juce::AudioProcessorValueTreeState::ParameterLayout SequencesAudioProcessor::createParameterLayout()
{
    Layout layout;

    // --- Voix ---
    addChoice (layout, "voiceMode", "Voice Mode", { "MONO", "POLY" }, 1);
    addMs (layout, "glide", "Glide", 0.0f, 1000.0f, 60.0f);
    addPercent (layout, "velocity", "Velocity", 50.0f);
    addInt (layout, "transpose", "Transpose", -24, 24, 0, [] (int v, int) { return (v > 0 ? "+" : "") + juce::String (v) + " st"; });
    addFloat (layout, "level", "Level", { -24.0f, 6.0f, 0.1f }, -3.0f,
              [] (float v, int) { return (v > 0.04f ? "+" : "") + juce::String (v, 1) + " dB"; });

    // --- FM ---
    addPercent (layout, "fmLevel", "FM Level", 70.0f);
    {
        juce::NormalisableRange<float> ratio (0.25f, 16.0f, 0.001f);
        ratio.setSkewForCentre (2.0f);
        addFloat (layout, "fmRatio", "FM Ratio", ratio, 2.0f, [] (float v, int) { return juce::String (v, v < 10.0f ? 2 : 1); });
    }
    addFloat (layout, "fmIndex", "FM Index", { 0.0f, 10.0f, 0.01f }, 3.0f, [] (float v, int) { return juce::String (v, 2); });
    addPercent (layout, "fmFeedback", "FM Feedback", 0.0f);
    addPercent (layout, "fmEnv", "FM Env", 30.0f, true);

    // --- Oscillateur ---
    addChoice (layout, "oscWave", "Osc Wave", { "SAW", "SQUARE", "TRIANGLE", "SINE" }, 0);
    addPercent (layout, "oscLevel", "Osc Level", 35.0f);
    addInt (layout, "oscOctave", "Osc Octave", -2, 2, 0, [] (int v, int) { return (v > 0 ? "+" : "") + juce::String (v) + " oct"; });
    addFloat (layout, "oscDetune", "Osc Detune", { -100.0f, 100.0f, 0.1f }, 7.0f,
              [] (float v, int) { const int i = juce::roundToInt (v); return (i > 0 ? "+" : "") + juce::String (i) + " ct"; });
    addFloat (layout, "oscPw", "Osc PW", { 5.0f, 95.0f, 0.1f }, 50.0f, percentText);
    addPercent (layout, "noise", "Noise", 0.0f);

    // --- Filtre ---
    addChoice (layout, "filterType", "Filter Type", { "LP", "BP", "HP" }, 0);
    addChoice (layout, "filterSlope", "Filter Slope", { "12 dB", "24 dB" }, 1);
    {
        juce::NormalisableRange<float> cutoff (20.0f, 20000.0f,
                                               [] (float lo, float hi, float t) { return lo * std::pow (hi / lo, t); },
                                               [] (float lo, float hi, float v) { return std::log (v / lo) / std::log (hi / lo); });
        addFloat (layout, "cutoff", "Cutoff", cutoff, 900.0f, hzText);
    }
    addPercent (layout, "resonance", "Resonance", 40.0f);
    addPercent (layout, "filterEnv", "Filter Env", 35.0f, true);
    addPercent (layout, "keyTrack", "Key Track", 50.0f);

    // --- Enveloppes ---
    addMs (layout, "ampAttack", "Amp Attack", 0.5f, 5000.0f, 1.0f);
    addMs (layout, "ampDecay", "Amp Decay", 1.0f, 5000.0f, 400.0f);
    addPercent (layout, "ampSustain", "Amp Sustain", 70.0f);
    addMs (layout, "ampRelease", "Amp Release", 1.0f, 5000.0f, 60.0f);
    addMs (layout, "modAttack", "Mod Attack", 0.5f, 5000.0f, 1.0f);
    addMs (layout, "modDecay", "Mod Decay", 1.0f, 5000.0f, 180.0f);
    addPercent (layout, "modSustain", "Mod Sustain", 0.0f);
    addMs (layout, "modRelease", "Mod Release", 1.0f, 5000.0f, 100.0f);

    // --- Distorsion ---
    addChoice (layout, "driveType", "Drive Type", { "SOFT", "FOLD", "CRUSH" }, 0);
    addPercent (layout, "drive", "Drive", 0.0f);
    addChoice (layout, "drivePos", "Drive Position", { "PRE", "POST" }, 0);

    // --- Séquenceurs de LFO ---
    for (int q = 0; q < seqs::numSeqs; ++q)
    {
        const juce::String label = q == 0 ? "Seq A " : "Seq B ";
        addBool (layout, seqId (q, "On"), label + "On", q == 0);
        addInt (layout, seqId (q, "Steps"), label + "Steps", 1, numSteps, 16);
        addChoice (layout, seqId (q, "Rate"), label + "Rate", rateNames(), q == 0 ? 4 : 3);
        addChoice (layout, seqId (q, "Feel"), label + "Feel", { "STRAIGHT", "TRIPLET", "DOTTED" }, 0);
        addFloat (layout, seqId (q, "Swing"), label + "Swing", { 0.0f, 75.0f, 0.1f }, 0.0f, percentText);
        addPercent (layout, seqId (q, "Smooth"), label + "Smooth", 0.0f);
        addChoice (layout, seqId (q, "Sync"), label + "Sync", { "NOTE", "HOST" }, 0);

        for (int k = 0; k < seqs::numSlots; ++k)
        {
            int dest = 0;
            float amount = 0.0f;
            if (q == 0 && k == 0) { dest = (int) seqs::Dest::cutoff;  amount = 60.0f; }
            if (q == 0 && k == 1) { dest = (int) seqs::Dest::fmIndex; amount = 35.0f; }
            addChoice (layout, destId (q, k), label + "Dest " + juce::String (k + 1), destinationNames(), dest);
            addPercent (layout, amountId (q, k), label + "Amount " + juce::String (k + 1), amount, true);
        }

        for (int i = 0; i < numSteps; ++i)
        {
            const float value = q == 0 ? defaultPatternA[i % 16] : (float) (i % 8) / 7.0f;
            addFloat (layout, stepId (q, i), label + "Step " + juce::String (i + 1), { 0.0f, 1.0f, 0.0f }, value,
                      [] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; });
        }

        for (int i = 0; i < numSteps; ++i)
            addBool (layout, glideId (q, i), label + "Glide " + juce::String (i + 1), q == 0 && (i % 16 == 2 || i % 16 == 13));
    }

    return layout;
}

//==============================================================================
SequencesAudioProcessor::SequencesAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Sequences", createParameterLayout()),
      presets (apvts, "Sequences", ".seqpreset", PresetLibrary::defaultsOnly (*this, {}), {}, "UNTITLED")
{
    presets.restoreFromState();   // projet neuf : DEFAULT

    for (auto* param : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param))
            raw[ranged->getParameterID()] = apvts.getRawParameterValue (ranged->getParameterID());

    for (int q = 0; q < seqs::numSeqs; ++q)
    {
        auto& sp = seqParams[(size_t) q];
        sp.on = raw.at (seqId (q, "On"));
        sp.steps = raw.at (seqId (q, "Steps"));
        sp.rate = raw.at (seqId (q, "Rate"));
        sp.feel = raw.at (seqId (q, "Feel"));
        sp.swing = raw.at (seqId (q, "Swing"));
        sp.smooth = raw.at (seqId (q, "Smooth"));
        sp.sync = raw.at (seqId (q, "Sync"));
        for (int k = 0; k < seqs::numSlots; ++k)
        {
            sp.dest[(size_t) k] = raw.at (destId (q, k));
            sp.amount[(size_t) k] = raw.at (amountId (q, k));
        }
        for (int i = 0; i < numSteps; ++i)
        {
            sp.value[(size_t) i] = raw.at (stepId (q, i));
            sp.glide[(size_t) i] = raw.at (glideId (q, i));
        }
        playhead[(size_t) q].store (-1);
    }
}

bool SequencesAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void SequencesAudioProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 48000.0;
    readSettings (settings);
    engine.setSettings (settings);
    engine.prepare (currentSampleRate);
}

void SequencesAudioProcessor::reset()
{
    engine.reset();
}

void SequencesAudioProcessor::readSettings (seqs::Settings& s) const
{
    const auto v = [] (const std::atomic<float>* a) { return a->load (std::memory_order_relaxed); };
    const auto i = [&v] (const std::atomic<float>* a) { return juce::roundToInt (v (a)); };
    const auto pct = [&v] (const std::atomic<float>* a) { return 0.01f * v (a); };
    const auto sec = [&v] (const std::atomic<float>* a) { return 0.001f * v (a); };

    s.mono = i (p ("voiceMode")) == 0;
    s.glideMs = v (p ("glide"));
    s.velocity = pct (p ("velocity"));
    s.transpose = i (p ("transpose"));
    s.level = juce::Decibels::decibelsToGain (v (p ("level")));

    s.fmLevel = pct (p ("fmLevel"));
    s.fmRatio = v (p ("fmRatio"));
    s.fmIndex = v (p ("fmIndex"));
    s.fmFeedback = pct (p ("fmFeedback"));
    s.fmEnv = pct (p ("fmEnv"));

    s.oscWave = (seqs::Wave) juce::jlimit (0, 3, i (p ("oscWave")));
    s.oscLevel = pct (p ("oscLevel"));
    s.oscOctave = i (p ("oscOctave"));
    s.oscDetune = v (p ("oscDetune"));
    s.oscPw = pct (p ("oscPw"));
    s.noise = pct (p ("noise"));

    s.filterType = (seqs::FilterType) juce::jlimit (0, 2, i (p ("filterType")));
    s.slope24 = i (p ("filterSlope")) == 1;
    s.cutoff = v (p ("cutoff"));
    s.resonance = pct (p ("resonance"));
    s.filterEnv = pct (p ("filterEnv"));
    s.keyTrack = pct (p ("keyTrack"));

    s.ampA = sec (p ("ampAttack"));  s.ampD = sec (p ("ampDecay"));
    s.ampS = pct (p ("ampSustain")); s.ampR = sec (p ("ampRelease"));
    s.modA = sec (p ("modAttack"));  s.modD = sec (p ("modDecay"));
    s.modS = pct (p ("modSustain")); s.modR = sec (p ("modRelease"));

    s.driveType = (seqs::DriveType) juce::jlimit (0, 2, i (p ("driveType")));
    s.drive = pct (p ("drive"));
    s.drivePost = i (p ("drivePos")) == 1;

    for (int q = 0; q < seqs::numSeqs; ++q)
    {
        const auto& sp = seqParams[(size_t) q];
        auto& out = s.seq[(size_t) q];
        out.on = v (sp.on) > 0.5f;
        out.steps = juce::jlimit (1, numSteps, i (sp.steps));
        const int rate = juce::jlimit (0, (int) std::size (rateBeats) - 1, i (sp.rate));
        const int feel = i (sp.feel);
        out.stepBeats = (double) rateBeats[rate] * (feel == 1 ? 2.0 / 3.0 : feel == 2 ? 1.5 : 1.0);
        out.swing = 0.01 * (double) v (sp.swing);
        out.smooth = pct (sp.smooth);
        out.hostSync = i (sp.sync) == 1;

        for (int k = 0; k < seqs::numSlots; ++k)
        {
            out.dest[(size_t) k] = (seqs::Dest) juce::jlimit (0, (int) seqs::Dest::count - 1, i (sp.dest[(size_t) k]));
            out.amount[(size_t) k] = pct (sp.amount[(size_t) k]);
        }

        for (int n = 0; n < numSteps; ++n)
        {
            out.value[(size_t) n] = juce::jlimit (0.0f, 1.0f, v (sp.value[(size_t) n]));
            out.glide[(size_t) n] = v (sp.glide[(size_t) n]) > 0.5f;
        }
    }
}

void SequencesAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    buffer.clear();

    readSettings (settings);
    engine.setSettings (settings);

    // --- Transport ---
    seqs::Engine::Transport transport;
    if (auto* head = getPlayHead())
        if (const auto position = head->getPosition())
        {
            if (const auto bpm = position->getBpm())
                if (*bpm > 1.0)
                    lastBpm = *bpm;

            if (const auto ppq = position->getPpqPosition())
            {
                transport.ppq = *ppq;
                transport.playing = position->getIsPlaying();
            }
        }
    transport.bpm = lastBpm;

    // --- Rendu, coupé aux événements MIDI (précis à l'échantillon) ---
    float* left = buffer.getWritePointer (0);
    float* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    std::array<float, 512> scratch {};

    int position = 0;
    auto renderUpTo = [&] (int end)
    {
        while (position < end)
        {
            const int block = juce::jmin (end - position, (int) scratch.size());
            float* r = right != nullptr ? right + position : scratch.data();
            engine.render (left + position, r, block, transport, position);
            if (right == nullptr)
                for (int n = 0; n < block; ++n)
                    left[position + n] = 0.5f * (left[position + n] + scratch[(size_t) n]);
            position += block;
        }
    };

    for (const auto metadata : midi)
    {
        const int when = juce::jlimit (0, numSamples, metadata.samplePosition);
        renderUpTo (when);

        const auto m = metadata.getMessage();
        if (m.isNoteOn())
            engine.noteOn (m.getNoteNumber(), m.getFloatVelocity());
        else if (m.isNoteOff())
            engine.noteOff (m.getNoteNumber());
        else if (m.isAllNotesOff() || m.isAllSoundOff())
            engine.allNotesOff (m.isAllSoundOff());
        else if (m.isPitchWheel())
            engine.setPitchBend (2.0f * (float) (m.getPitchWheelValue() - 8192) / 8192.0f);
    }

    renderUpTo (numSamples);

    for (int q = 0; q < seqs::numSeqs; ++q)
        playhead[(size_t) q].store (engine.getPlayheadStep (q), std::memory_order_relaxed);
}

//==============================================================================
void SequencesAudioProcessor::setParam (const juce::String& id, float realValue)
{
    if (auto* param = apvts.getParameter (id))
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->convertTo0to1 (realValue));
        param->endChangeGesture();
    }
}

void SequencesAudioProcessor::randomizeSequence (int seq)
{
    seq = juce::jlimit (0, seqs::numSeqs - 1, seq);
    const int steps = juce::roundToInt (seqParams[(size_t) seq].steps->load());

    std::array<float, seqs::maxSteps> values {};
    std::array<bool, seqs::maxSteps> glides {};
    seqs::RandomPattern::generate (randomRng, steps, values, glides);

    for (int i = 0; i < numSteps; ++i)
    {
        setParam (stepId (seq, i), values[(size_t) i]);
        setParam (glideId (seq, i), glides[(size_t) i] ? 1.0f : 0.0f);
    }

    // Rien n'est branché : on branche la coupure, pour entendre le motif tout de suite
    bool anyDestination = false;
    for (int k = 0; k < seqs::numSlots; ++k)
        anyDestination = anyDestination || (juce::roundToInt (seqParams[(size_t) seq].dest[(size_t) k]->load()) != 0
                                            && std::abs (seqParams[(size_t) seq].amount[(size_t) k]->load()) > 0.5f);
    if (! anyDestination)
    {
        setParam (destId (seq, 0), (float) seqs::Dest::cutoff);
        setParam (amountId (seq, 0), 60.0f);
    }

    if (seqParams[(size_t) seq].on->load() < 0.5f)
        setParam (seqId (seq, "On"), 1.0f);
}

void SequencesAudioProcessor::rotateSequence (int seq, int dir)
{
    seq = juce::jlimit (0, seqs::numSeqs - 1, seq);
    const auto& sp = seqParams[(size_t) seq];
    const int n = juce::jlimit (1, numSteps, juce::roundToInt (sp.steps->load()));

    std::vector<float> values ((size_t) n), glides ((size_t) n);
    for (int i = 0; i < n; ++i)
    {
        values[(size_t) i] = sp.value[(size_t) i]->load();
        glides[(size_t) i] = sp.glide[(size_t) i]->load();
    }

    for (int i = 0; i < n; ++i)
    {
        const int from = ((i - dir) % n + n) % n;
        setParam (stepId (seq, i), values[(size_t) from]);
        setParam (glideId (seq, i), glides[(size_t) from]);
    }
}

void SequencesAudioProcessor::clearSequence (int seq, float value)
{
    seq = juce::jlimit (0, seqs::numSeqs - 1, seq);
    for (int i = 0; i < numSteps; ++i)
    {
        setParam (stepId (seq, i), value);
        setParam (glideId (seq, i), 0.0f);
    }
}

//==============================================================================
void SequencesAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void SequencesAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            presets.restoreFromState();
        }
}

juce::AudioProcessorEditor* SequencesAudioProcessor::createEditor()
{
    return new SequencesAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SequencesAudioProcessor();
}
