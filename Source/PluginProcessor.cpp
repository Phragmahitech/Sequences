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

    // Périodes des LFO, de la plus courte à la plus longue (en noires)
    struct Rate { const char* name; double beats; };
    constexpr Rate lfoRates[] = {
        { "1/64", 0.0625 }, { "1/32T", 1.0 / 12.0 }, { "1/32", 0.125 }, { "1/16T", 1.0 / 6.0 }, { "1/16", 0.25 },
        { "1/8T", 1.0 / 3.0 }, { "1/16D", 0.375 }, { "1/8", 0.5 }, { "1/4T", 2.0 / 3.0 }, { "1/8D", 0.75 },
        { "1/4", 1.0 }, { "1/2T", 4.0 / 3.0 }, { "1/4D", 1.5 }, { "1/2", 2.0 }, { "1 BAR", 4.0 },
        { "2 BAR", 8.0 }, { "4 BAR", 16.0 }, { "8 BAR", 32.0 } };
    constexpr int numLfoRates = (int) std::size (lfoRates);

    int rateIndex (const char* name)
    {
        for (int i = 0; i < numLfoRates; ++i)
            if (juce::String (lfoRates[i].name) == name)
                return i;
        return 10;
    }

    // Division du séquenceur
    constexpr double divisionBeats[] = { 4.0, 2.0, 1.0, 0.5, 0.25, 0.125, 0.0625 };
    const juce::StringArray divisionNames { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/64" };

    const juce::StringArray lfoShapeNames { "SINE", "TRIANGLE", "SAW", "RAMP", "SQUARE", "PULSE", "EXP DOWN", "EXP UP" };
    const juce::StringArray stepNames { "OFF", "2", "3", "4", "6", "8", "12", "16", "24", "32" };
    constexpr int stepValues[] = { 0, 2, 3, 4, 6, 8, 12, 16, 24, 32 };
    const juce::StringArray modWaveNames { "SINE", "DBL SINE", "SAW", "SQUARE", "PULSE", "SAW-PULSE", "RESO 1", "RESO 2", "RESO 3" };

    // Banque de départ : des mouvements typiques de séquences hi-tech
    struct LfoDefault { const char* rate; int shape; float amount; int steps; float phase; bool uni; };
    constexpr LfoDefault lfoDefaults[seqs::numLfos] = {
        { "1/16",  3, 100.0f, 0, 0.0f, false },   // RAMP rapide
        { "1/32",  0, 100.0f, 0, 0.0f, false },   // SINE très rapide
        { "1/8T",  6, 100.0f, 0, 0.0f, false },   // EXP DOWN : « pew »
        { "1/4",   1, 100.0f, 5, 0.0f, false },   // TRIANGLE en 8 marches
        { "1/64",  4, 100.0f, 0, 0.0f, false },   // SQUARE : trille
        { "1 BAR", 2, 100.0f, 0, 0.0f, false } }; // SAW lente

    constexpr int defaultPattern[16] = { 1, 1, 2, 3, 1, 4, 2, 5, 1, 1, 3, 7, 6, 2, 8, 3 };
}

//==============================================================================
const juce::StringArray& SequencesAudioProcessor::destinationNames()
{
    // Même ordre que seqs::Dest
    static const juce::StringArray names { "OFF", "CUTOFF", "RESO", "FM INDEX", "FM RATIO", "PITCH", "PW", "DRIVE",
                                           "AMP", "FM LEVEL", "OSC LEVEL", "NOISE", "PAN", "FEEDBACK", "SYNC", "TIMBRE" };
    return names;
}

const juce::StringArray& SequencesAudioProcessor::sourceNames()
{
    static const juce::StringArray names { "OFF", "LFO 1", "LFO 2", "LFO 3", "LFO 4", "LFO 5", "LFO 6", "CHAOS 1", "CHAOS 2" };
    return names;
}

const juce::StringArray& SequencesAudioProcessor::slotSourceNames()
{
    static const juce::StringArray names { "SEQ", "LFO 1", "LFO 2", "LFO 3", "LFO 4", "LFO 5", "LFO 6", "CHAOS 1", "CHAOS 2" };
    return names;
}

const juce::StringArray& SequencesAudioProcessor::lfoRateNames()
{
    static const juce::StringArray names = []
    {
        juce::StringArray n;
        for (const auto& r : lfoRates) n.add (r.name);
        return n;
    }();
    return names;
}

double SequencesAudioProcessor::lfoRateBeats (int index)
{
    return lfoRates[juce::jlimit (0, numLfoRates - 1, index)].beats;
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
    addChoice (layout, "modWave", "Mod Wave", modWaveNames, 0);
    addPercent (layout, "timbre", "Timbre", 30.0f);

    // --- Oscillateur ---
    addChoice (layout, "oscWave", "Osc Wave", { "SAW", "SQUARE", "TRIANGLE", "SINE" }, 0);
    addPercent (layout, "oscLevel", "Osc Level", 35.0f);
    addInt (layout, "oscOctave", "Osc Octave", -2, 2, 0, [] (int v, int) { return (v > 0 ? "+" : "") + juce::String (v) + " oct"; });
    addFloat (layout, "oscDetune", "Osc Detune", { -100.0f, 100.0f, 0.1f }, 7.0f,
              [] (float v, int) { const int i = juce::roundToInt (v); return (i > 0 ? "+" : "") + juce::String (i) + " ct"; });
    addFloat (layout, "oscPw", "Osc PW", { 5.0f, 95.0f, 0.1f }, 50.0f, percentText);
    addPercent (layout, "oscSync", "Osc Sync", 0.0f);
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

    // --- Banque de LFO ---
    for (int i = 0; i < seqs::numLfos; ++i)
    {
        const auto& d = lfoDefaults[i];
        const juce::String name = "LFO " + juce::String (i + 1) + " ";
        addChoice (layout, lfoId (i, "Rate"), name + "Rate", lfoRateNames(), rateIndex (d.rate));
        addChoice (layout, lfoId (i, "Shape"), name + "Shape", lfoShapeNames, d.shape);
        addPercent (layout, lfoId (i, "Amt"), name + "Amount", d.amount);
        addChoice (layout, lfoId (i, "Step"), name + "Step", stepNames, d.steps);
        addFloat (layout, lfoId (i, "Phase"), name + "Phase", { 0.0f, 360.0f, 1.0f }, d.phase,
                  [] (float v, int) { return juce::String (juce::roundToInt (v)) + juce::String (juce::CharPointer_UTF8 ("\xc2\xb0")); });
        addChoice (layout, lfoId (i, "Pol"), name + "Polarity", { "BI", "UNI" }, d.uni ? 1 : 0);
    }

    for (int c = 0; c < seqs::numChaos; ++c)
    {
        const juce::String name = "Chaos " + juce::String (c + 1) + " ";
        addChoice (layout, chaosId (c, "Rate"), name + "Rate", lfoRateNames(), rateIndex (c == 0 ? "1/16" : "1/32"));
        addPercent (layout, chaosId (c, "Amt"), name + "Amount", 100.0f);
        addPercent (layout, chaosId (c, "Smooth"), name + "Smooth", c == 0 ? 0.0f : 60.0f);
    }

    // --- Séquenceur ---
    addBool (layout, "seqOn", "Seq On", true);
    addInt (layout, "seqSteps", "Seq Steps", 1, numSteps, 16);
    addChoice (layout, "seqDiv", "Seq Division", divisionNames, 3);
    addChoice (layout, "seqFeel", "Seq Feel", { "STRAIGHT", "TRIPLET", "DOTTED" }, 0);
    addFloat (layout, "seqSwing", "Seq Swing", { 0.0f, 75.0f, 0.1f }, 0.0f, percentText);
    addChoice (layout, "seqSync", "Seq Sync", { "NOTE", "HOST" }, 0);
    addBool (layout, "seqRestart", "Seq Restart", true);
    addFloat (layout, "seqXfade", "Seq Crossfade", { 0.0f, 50.0f, 0.1f }, 2.0f, msText);
    addChoice (layout, "seqManual", "Seq Manual Source", sourceNames(), 1);

    for (int i = 0; i < numSteps; ++i)
        addChoice (layout, stepId (i), "Seq Step " + juce::String (i + 1), sourceNames(), defaultPattern[i % 16]);

    // --- Routages ---
    for (int k = 0; k < seqs::numSlots; ++k)
    {
        int dest = 0;
        float amount = 0.0f;
        if (k == 0) { dest = (int) seqs::Dest::cutoff;  amount = 45.0f; }
        if (k == 1) { dest = (int) seqs::Dest::fmIndex; amount = 30.0f; }
        const juce::String name = "Mod " + juce::String (k + 1) + " ";
        addChoice (layout, slotId (k, "Src"), name + "Source", slotSourceNames(), 0);
        addChoice (layout, slotId (k, "Dest"), name + "Destination", destinationNames(), dest);
        addPercent (layout, slotId (k, "Amt"), name + "Amount", amount, true);
    }

    return layout;
}

//==============================================================================
SequencesAudioProcessor::SequencesAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Sequences", createParameterLayout()),
      presets (apvts, "Sequences", ".seqpreset", PresetLibrary::defaultsOnly (*this, {}), {}, "UNTITLED")
{
    presets.restoreFromState();

    for (auto* param : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param))
            raw[ranged->getParameterID()] = apvts.getRawParameterValue (ranged->getParameterID());

    cache.build (*this);
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

//==============================================================================
void SequencesAudioProcessor::Cache::build (const SequencesAudioProcessor& proc)
{
    auto get = [&proc] (const juce::String& id) { return proc.raw.at (id); };

    for (const char* id : { "voiceMode", "glide", "velocity", "transpose", "level", "fmLevel", "fmRatio", "fmIndex", "fmFeedback",
                            "fmEnv", "modWave", "timbre", "oscWave", "oscLevel", "oscOctave", "oscDetune", "oscPw", "oscSync", "noise",
                            "filterType", "filterSlope", "cutoff", "resonance", "filterEnv", "keyTrack", "ampAttack", "ampDecay",
                            "ampSustain", "ampRelease", "modAttack", "modDecay", "modSustain", "modRelease", "driveType", "drive",
                            "drivePos", "seqOn", "seqSteps", "seqDiv", "seqFeel", "seqSwing", "seqSync", "seqRestart", "seqXfade",
                            "seqManual" })
        named[id] = get (id);

    for (int i = 0; i < seqs::numLfos; ++i)
        lfo[(size_t) i] = { get (lfoId (i, "Rate")), get (lfoId (i, "Shape")), get (lfoId (i, "Amt")),
                            get (lfoId (i, "Step")), get (lfoId (i, "Phase")), get (lfoId (i, "Pol")) };
    for (int c = 0; c < seqs::numChaos; ++c)
        chaos[(size_t) c] = { get (chaosId (c, "Rate")), get (chaosId (c, "Amt")), get (chaosId (c, "Smooth")) };
    for (int i = 0; i < numSteps; ++i)
        steps[(size_t) i] = get (stepId (i));
    for (int k = 0; k < seqs::numSlots; ++k)
        slots[(size_t) k] = { get (slotId (k, "Src")), get (slotId (k, "Dest")), get (slotId (k, "Amt")) };

    voiceMode = named.at ("voiceMode"); glide = named.at ("glide"); velocity = named.at ("velocity");
    transpose = named.at ("transpose"); level = named.at ("level");
    fmLevel = named.at ("fmLevel"); fmRatio = named.at ("fmRatio"); fmIndex = named.at ("fmIndex");
    fmFeedback = named.at ("fmFeedback"); fmEnv = named.at ("fmEnv"); modWave = named.at ("modWave"); timbre = named.at ("timbre");
    oscWave = named.at ("oscWave"); oscLevel = named.at ("oscLevel"); oscOctave = named.at ("oscOctave");
    oscDetune = named.at ("oscDetune"); oscPw = named.at ("oscPw"); oscSync = named.at ("oscSync"); noise = named.at ("noise");
    filterType = named.at ("filterType"); filterSlope = named.at ("filterSlope"); cutoff = named.at ("cutoff");
    resonance = named.at ("resonance"); filterEnv = named.at ("filterEnv"); keyTrack = named.at ("keyTrack");
    ampAttack = named.at ("ampAttack"); ampDecay = named.at ("ampDecay"); ampSustain = named.at ("ampSustain");
    ampRelease = named.at ("ampRelease"); modAttack = named.at ("modAttack"); modDecay = named.at ("modDecay");
    modSustain = named.at ("modSustain"); modRelease = named.at ("modRelease");
    driveType = named.at ("driveType"); drive = named.at ("drive"); drivePos = named.at ("drivePos");
    seqOn = named.at ("seqOn"); seqSteps = named.at ("seqSteps"); seqDiv = named.at ("seqDiv"); seqFeel = named.at ("seqFeel");
    seqSwing = named.at ("seqSwing"); seqSync = named.at ("seqSync"); seqRestart = named.at ("seqRestart");
    seqXfade = named.at ("seqXfade"); seqManual = named.at ("seqManual");
}

void SequencesAudioProcessor::readSettings (seqs::Settings& s) const
{
    const auto v = [] (const std::atomic<float>* a) { return a->load (std::memory_order_relaxed); };
    const auto i = [&v] (const std::atomic<float>* a) { return juce::roundToInt (v (a)); };
    const auto pct = [&v] (const std::atomic<float>* a) { return 0.01f * v (a); };
    const auto sec = [&v] (const std::atomic<float>* a) { return 0.001f * v (a); };
    const auto& c = cache;

    s.mono = i (c.voiceMode) == 0;
    s.glideMs = v (c.glide);
    s.velocity = pct (c.velocity);
    s.transpose = i (c.transpose);
    s.level = juce::Decibels::decibelsToGain (v (c.level));

    s.fmLevel = pct (c.fmLevel);
    s.fmRatio = v (c.fmRatio);
    s.fmIndex = v (c.fmIndex);
    s.fmFeedback = pct (c.fmFeedback);
    s.fmEnv = pct (c.fmEnv);
    s.modWave = (seqs::ModWave) juce::jlimit (0, (int) seqs::ModWave::count - 1, i (c.modWave));
    s.timbre = pct (c.timbre);

    s.oscWave = (seqs::Wave) juce::jlimit (0, 3, i (c.oscWave));
    s.oscLevel = pct (c.oscLevel);
    s.oscOctave = i (c.oscOctave);
    s.oscDetune = v (c.oscDetune);
    s.oscPw = pct (c.oscPw);
    s.oscSync = pct (c.oscSync);
    s.noise = pct (c.noise);

    s.filterType = (seqs::FilterType) juce::jlimit (0, 2, i (c.filterType));
    s.slope24 = i (c.filterSlope) == 1;
    s.cutoff = v (c.cutoff);
    s.resonance = pct (c.resonance);
    s.filterEnv = pct (c.filterEnv);
    s.keyTrack = pct (c.keyTrack);

    s.ampA = sec (c.ampAttack);  s.ampD = sec (c.ampDecay);
    s.ampS = pct (c.ampSustain); s.ampR = sec (c.ampRelease);
    s.modA = sec (c.modAttack);  s.modD = sec (c.modDecay);
    s.modS = pct (c.modSustain); s.modR = sec (c.modRelease);

    s.driveType = (seqs::DriveType) juce::jlimit (0, 2, i (c.driveType));
    s.drive = pct (c.drive);
    s.drivePost = i (c.drivePos) == 1;

    for (int n = 0; n < seqs::numLfos; ++n)
    {
        const auto& lp = c.lfo[(size_t) n];
        auto& l = s.lfo[(size_t) n];
        l.periodBeats = lfoRateBeats (i (lp.rate));
        l.shape = (seqs::LfoShape) juce::jlimit (0, (int) seqs::LfoShape::count - 1, i (lp.shape));
        l.amount = pct (lp.amount);
        l.steps = stepValues[juce::jlimit (0, (int) std::size (stepValues) - 1, i (lp.step))];
        l.phase = v (lp.phase) / 360.0f;
        l.unipolar = i (lp.polarity) == 1;
    }

    for (int n = 0; n < seqs::numChaos; ++n)
    {
        const auto& cp = c.chaos[(size_t) n];
        auto& ch = s.chaos[(size_t) n];
        ch.periodBeats = lfoRateBeats (i (cp.rate));
        ch.amount = pct (cp.amount);
        ch.smooth = pct (cp.smooth);
    }

    auto& q = s.seq;
    q.on = v (c.seqOn) > 0.5f;
    q.steps = juce::jlimit (1, numSteps, i (c.seqSteps));
    const int div = juce::jlimit (0, (int) std::size (divisionBeats) - 1, i (c.seqDiv));
    const int feel = i (c.seqFeel);
    q.stepBeats = divisionBeats[div] * (feel == 1 ? 2.0 / 3.0 : feel == 2 ? 1.5 : 1.0);
    q.swing = 0.01 * (double) v (c.seqSwing);
    q.hostSync = i (c.seqSync) == 1;
    q.restart = v (c.seqRestart) > 0.5f;
    q.xfadeMs = v (c.seqXfade);
    q.manual = juce::jlimit (0, seqs::numSources, i (c.seqManual));
    for (int n = 0; n < numSteps; ++n)
        q.pattern[(size_t) n] = juce::jlimit (0, seqs::numSources, i (c.steps[(size_t) n]));

    for (int k = 0; k < seqs::numSlots; ++k)
    {
        const auto& sp = c.slots[(size_t) k];
        s.slotSource[(size_t) k] = juce::jlimit (0, seqs::numSources, i (sp.source));
        s.slotDest[(size_t) k] = (seqs::Dest) juce::jlimit (0, (int) seqs::Dest::count - 1, i (sp.dest));
        s.slotAmount[(size_t) k] = pct (sp.amount);
    }
}

void SequencesAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    buffer.clear();

    readSettings (settings);
    engine.setSettings (settings);

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
    bpmNow.store (lastBpm, std::memory_order_relaxed);

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

    playheadStep.store (engine.getPlayheadStep(), std::memory_order_relaxed);
    playheadBeats.store (engine.getPlayheadBeats(), std::memory_order_relaxed);
}

//==============================================================================
void SequencesAudioProcessor::setParam (const juce::String& id, float value)
{
    if (auto* param = apvts.getParameter (id))
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->convertTo0to1 (value));
        param->endChangeGesture();
    }
}

float SequencesAudioProcessor::realValue (const juce::String& id) const
{
    return p (id)->load();
}

void SequencesAudioProcessor::randomizePattern()
{
    // Sources qui font quelque chose (quantité non nulle)
    std::array<bool, seqs::numSources + 1> available {};
    for (int i = 0; i < seqs::numLfos; ++i)
        available[(size_t) (i + 1)] = realValue (lfoId (i, "Amt")) > 0.5f;
    for (int c = 0; c < seqs::numChaos; ++c)
        available[(size_t) (seqs::numLfos + 1 + c)] = realValue (chaosId (c, "Amt")) > 0.5f;

    std::array<int, seqs::maxSteps> pattern {};
    seqs::RandomPattern::generate (randomRng, juce::roundToInt (realValue ("seqSteps")), available, pattern);

    for (int i = 0; i < numSteps; ++i)
        setParam (stepId (i), (float) pattern[(size_t) i]);

    if (realValue ("seqOn") < 0.5f)
        setParam ("seqOn", 1.0f);

    // Rien n'écoute le séquenceur : on le branche sur la coupure pour l'entendre tout de suite
    bool listening = false;
    for (int k = 0; k < seqs::numSlots; ++k)
        listening = listening || (juce::roundToInt (realValue (slotId (k, "Src"))) == 0
                                  && juce::roundToInt (realValue (slotId (k, "Dest"))) != 0
                                  && std::abs (realValue (slotId (k, "Amt"))) > 0.5f);
    if (! listening)
    {
        setParam (slotId (0, "Src"), 0.0f);
        setParam (slotId (0, "Dest"), (float) seqs::Dest::cutoff);
        setParam (slotId (0, "Amt"), 45.0f);
    }
}

void SequencesAudioProcessor::randomizeAll()
{
    // Périodes : surtout entre 1/32 et 1 mesure, là où vivent les séquences hi-tech
    static const int favouredRates[] = { 2, 3, 4, 4, 5, 6, 7, 7, 8, 9, 10, 10, 11, 12, 13, 14 };
    auto& rng = randomRng;

    for (int i = 0; i < seqs::numLfos; ++i)
    {
        setParam (lfoId (i, "Rate"), (float) favouredRates[rng.range (0, (int) std::size (favouredRates) - 1)]);
        setParam (lfoId (i, "Shape"), (float) rng.range (0, (int) seqs::LfoShape::count - 1));
        setParam (lfoId (i, "Amt"), rng.chance (0.75) ? 100.0f : (float) rng.range (40, 100));
        setParam (lfoId (i, "Step"), rng.chance (0.6) ? 0.0f : (float) rng.range (1, 7));
        setParam (lfoId (i, "Phase"), rng.chance (0.7) ? 0.0f : (float) (rng.range (0, 7) * 45));
        setParam (lfoId (i, "Pol"), rng.chance (0.8) ? 0.0f : 1.0f);
    }

    for (int c = 0; c < seqs::numChaos; ++c)
    {
        setParam (chaosId (c, "Rate"), (float) favouredRates[rng.range (0, 8)]);
        setParam (chaosId (c, "Amt"), (float) rng.range (50, 100));
        setParam (chaosId (c, "Smooth"), rng.chance (0.5) ? 0.0f : (float) rng.range (20, 100));
    }

    engine.setHostChaosSeed (rng.next());
    randomizePattern();
}

void SequencesAudioProcessor::rotatePattern (int dir)
{
    const int n = juce::jlimit (1, numSteps, juce::roundToInt (realValue ("seqSteps")));
    std::vector<float> values ((size_t) n);
    for (int i = 0; i < n; ++i)
        values[(size_t) i] = realValue (stepId (i));

    for (int i = 0; i < n; ++i)
        setParam (stepId (i), values[(size_t) (((i - dir) % n + n) % n)]);
}

void SequencesAudioProcessor::fillPattern (int source)
{
    for (int i = 0; i < numSteps; ++i)
        setParam (stepId (i), (float) juce::jlimit (0, seqs::numSources, source));
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
