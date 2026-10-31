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

    /** Routages de départ : source, destination, profondeur. */
    struct SlotDefault { int source; seqs::Dest dest; float amount; };
    const SlotDefault slotDefaults[] = {
        { seqs::srcSeq,  seqs::Dest::cutoff, 27.0f },
        { seqs::srcSeq,  seqs::Dest::aWarp,  12.0f },
        { seqs::srcEnv2, seqs::Dest::cutoff, 30.0f },
        { seqs::srcEnv2, seqs::Dest::aWarp,  15.0f } };

    const juce::StringArray warpNames[2] = { { "OFF", "SYNC", "FM (FROM B)", "AM (FROM B)", "RM (FROM B)" },
                                             { "OFF", "SYNC", "FM (FROM A)", "AM (FROM A)", "RM (FROM A)" } };

    juce::String semitoneText (float v, int)
    {
        const float r = std::round (v * 100.0f) / 100.0f;
        return (r > 0.0f ? "+" : "") + juce::String (r, std::abs (r - std::round (r)) < 0.005f ? 0 : 2) + " st";
    }

    // Les tables sont gardées en 16 bits compressés (gzip), puis en base64
    juce::String encodeCycles (const std::vector<std::vector<float>>& cycles)
    {
        juce::MemoryOutputStream packed;
        {
            juce::GZIPCompressorOutputStream gz (packed, 6);
            for (const auto& c : cycles)
            {
                gz.writeInt ((int) c.size());
                for (float v : c)
                    gz.writeShort ((short) juce::jlimit (-32767, 32767, juce::roundToInt (juce::jlimit (-1.0f, 1.0f, v) * 32767.0f)));
            }
        }
        return packed.getMemoryBlock().toBase64Encoding();
    }

    std::vector<std::vector<float>> decodeCycles (const juce::String& text)
    {
        std::vector<std::vector<float>> cycles;
        juce::MemoryBlock block;
        if (! block.fromBase64Encoding (text) || block.getSize() == 0)
            return cycles;

        juce::MemoryInputStream packed (block, false);
        juce::GZIPDecompressorInputStream gz (packed);
        while (! gz.isExhausted() && cycles.size() < 4096)
        {
            const int n = gz.readInt();
            if (n < 2 || n > 65536)
                break;
            std::vector<float> c ((size_t) n);
            for (auto& v : c)
                v = (float) gz.readShort() / 32767.0f;
            cycles.push_back (std::move (c));
        }
        return cycles;
    }

    /** Peak normalisé : les cycles d'une table importée gardés à ±1 pour la sauvegarde en 16 bits. */
    void normaliseCycles (std::vector<std::vector<float>>& cycles)
    {
        float peak = 0.0f;
        for (const auto& c : cycles) for (float v : c) peak = std::max (peak, std::abs (v));
        if (peak > 1.0e-9f)
            for (auto& c : cycles) for (float& v : c) v /= peak;
    }
}

//==============================================================================
const juce::StringArray& SequencesAudioProcessor::destinationNames()
{
    // Même ordre que seqs::Dest
    static const juce::StringArray names { "OFF", "A LEVEL", "A POS", "A COARSE", "A FINE", "A WARP",
                                           "B LEVEL", "B POS", "B COARSE", "B FINE", "B WARP",
                                           "NOISE", "PITCH", "PAN", "VOLUME", "CUTOFF", "RESO", "DRIVE" };
    return names;
}

const juce::StringArray& SequencesAudioProcessor::sourceNames()
{
    static const juce::StringArray names { "OFF", "LFO 1", "LFO 2", "LFO 3", "LFO 4", "LFO 5", "LFO 6", "CHAOS 1", "CHAOS 2" };
    return names;
}

const juce::StringArray& SequencesAudioProcessor::modSourceNames()
{
    static const juce::StringArray names { "SEQ", "LFO 1", "LFO 2", "LFO 3", "LFO 4", "LFO 5", "LFO 6", "CHAOS 1", "CHAOS 2",
                                           "ENV 2", "VELO" };
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

juce::String SequencesAudioProcessor::paramForDest (seqs::Dest d)
{
    static const char* ids[] = { "", "aLevel", "aPos", "aCoarse", "aFine", "aWarp", "bLevel", "bPos", "bCoarse", "bFine", "bWarp",
                                 "noise", "pitch", "pan", "volume", "cutoff", "resonance", "drive" };
    static_assert (std::size (ids) == seqs::numDests, "une entrée par destination");
    return ids[juce::jlimit (0, seqs::numDests - 1, (int) d)];
}

seqs::Dest SequencesAudioProcessor::destForParam (const juce::String& id)
{
    for (int d = 1; d < seqs::numDests; ++d)
        if (paramForDest ((seqs::Dest) d) == id)
            return (seqs::Dest) d;
    return seqs::Dest::off;
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout SequencesAudioProcessor::createParameterLayout()
{
    Layout layout;

    // --- Oscillateurs ---
    // Les potards modulables ont exactement la course de seqs::DestRange (position du potard = valeur 0..1 du moteur).
    for (int o = 0; o < seqs::numOscs; ++o)
    {
        const juce::String n = o == 0 ? "Osc A " : "Osc B ";
        addPercent (layout, oscId (o, "Level"), n + "Level", o == 0 ? 80.0f : 0.0f);
        addPercent (layout, oscId (o, "Pos"), n + "Position", 0.0f);
        addInt (layout, oscId (o, "Oct"), n + "Octave", -4, 4, o == 0 ? 0 : 1,
                [] (int v, int) { return (v > 0 ? "+" : "") + juce::String (v) + " oct"; });
        addFloat (layout, oscId (o, "Coarse"), n + "Coarse", { -48.0f, 48.0f, 0.0f }, 0.0f, semitoneText);
        addFloat (layout, oscId (o, "Fine"), n + "Fine", { -100.0f, 100.0f, 0.0f }, 0.0f,
                  [] (float v, int) { const int i = juce::roundToInt (v); return (i > 0 ? "+" : "") + juce::String (i) + " ct"; });
        addChoice (layout, oscId (o, "WarpMode"), n + "Warp Mode", warpNames[o], o == 0 ? (int) seqs::Warp::fm : 0);
        addPercent (layout, oscId (o, "Warp"), n + "Warp", o == 0 ? 15.0f : 0.0f);
    }

    // --- Voix ---
    addChoice (layout, "voiceMode", "Voice Mode", { "MONO", "POLY" }, 1);
    addMs (layout, "glide", "Glide", 0.0f, 1000.0f, 60.0f);
    addPercent (layout, "velocity", "Velocity", 50.0f);
    addFloat (layout, "pitch", "Pitch", { -24.0f, 24.0f, 0.0f }, 0.0f, semitoneText);
    addPercent (layout, "noise", "Noise", 0.0f);
    addFloat (layout, "pan", "Pan", { -100.0f, 100.0f, 0.0f }, 0.0f, [] (float v, int)
    {
        const int i = juce::roundToInt (v);
        return i == 0 ? juce::String ("C") : juce::String (std::abs (i)) + (i < 0 ? " L" : " R");
    });
    addFloat (layout, "volume", "Volume", { 0.0f, 1.0f, 0.0f }, 0.8f * std::pow (10.0f, -3.0f / 40.0f), [] (float v, int)
    {
        const float gain = seqs::DestRange::toReal (seqs::Dest::volume, v);
        if (gain < 1.0e-4f) return juce::String ("-INF");
        const float db = 20.0f * std::log10 (gain);
        return (db > 0.04f ? "+" : "") + juce::String (db, 1) + " dB";
    });

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
    addPercent (layout, "keyTrack", "Key Track", 50.0f);

    // --- Enveloppes ---
    addMs (layout, "ampAttack", "Env 1 Attack", 0.5f, 5000.0f, 1.0f);
    addMs (layout, "ampDecay", "Env 1 Decay", 1.0f, 5000.0f, 400.0f);
    addPercent (layout, "ampSustain", "Env 1 Sustain", 70.0f);
    addMs (layout, "ampRelease", "Env 1 Release", 1.0f, 5000.0f, 60.0f);
    addMs (layout, "modAttack", "Env 2 Attack", 0.5f, 5000.0f, 1.0f);
    addMs (layout, "modDecay", "Env 2 Decay", 1.0f, 5000.0f, 180.0f);
    addPercent (layout, "modSustain", "Env 2 Sustain", 0.0f);
    addMs (layout, "modRelease", "Env 2 Release", 1.0f, 5000.0f, 100.0f);

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

    // --- Routages (16) ---
    for (int k = 0; k < seqs::numSlots; ++k)
    {
        const SlotDefault d = k < (int) std::size (slotDefaults) ? slotDefaults[k] : SlotDefault { 0, seqs::Dest::off, 0.0f };
        const juce::String name = "Mod " + juce::String (k + 1) + " ";
        addChoice (layout, slotId (k, "Src"), name + "Source", modSourceNames(), d.source);
        addChoice (layout, slotId (k, "Dest"), name + "Destination", destinationNames(), (int) d.dest);
        addPercent (layout, slotId (k, "Amt"), name + "Amount", d.amount, true);
    }

    return layout;
}

//==============================================================================
SequencesAudioProcessor::SequencesAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Sequences", createParameterLayout()),
      presets (apvts, "Sequences", ".seqpreset", PresetLibrary::defaultsOnly (*this, {}), {}, "UNTITLED")
{
    for (auto* param : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param))
            raw[ranged->getParameterID()] = apvts.getRawParameterValue (ranged->getParameterID());

    cache.build (*this);

    for (int o = 0; o < seqs::numOscs; ++o)
        setBuiltinTable (o, 0);

    presets.writeExtra = [this] (juce::XmlElement& xml) { xml.addChildElement (tablesToXml().release()); };
    presets.readExtra = [this] (const juce::XmlElement* xml) { tablesFromXml (xml != nullptr ? xml->getChildByName ("TABLES") : nullptr); };
    presets.restoreFromState();

    for (auto& o : liveOffsets)
        o.store (0.0f);
}

SequencesAudioProcessor::~SequencesAudioProcessor() = default;

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
void SequencesAudioProcessor::Cache::build (SequencesAudioProcessor& proc)
{
    auto get = [&proc] (const juce::String& id) { return proc.raw.at (id); };

    for (int d = 1; d < seqs::numDests; ++d)
    {
        dest[(size_t) d] = proc.apvts.getParameter (paramForDest ((seqs::Dest) d));
        jassert (dest[(size_t) d] != nullptr);
    }

    for (int o = 0; o < seqs::numOscs; ++o)
        osc[(size_t) o] = { get (oscId (o, "Oct")), get (oscId (o, "WarpMode")) };

    voiceMode = get ("voiceMode"); glide = get ("glide"); velocity = get ("velocity");
    filterType = get ("filterType"); filterSlope = get ("filterSlope"); keyTrack = get ("keyTrack");
    ampAttack = get ("ampAttack"); ampDecay = get ("ampDecay"); ampSustain = get ("ampSustain"); ampRelease = get ("ampRelease");
    modAttack = get ("modAttack"); modDecay = get ("modDecay"); modSustain = get ("modSustain"); modRelease = get ("modRelease");
    driveType = get ("driveType"); drivePos = get ("drivePos");
    seqOn = get ("seqOn"); seqSteps = get ("seqSteps"); seqDiv = get ("seqDiv"); seqFeel = get ("seqFeel");
    seqSwing = get ("seqSwing"); seqSync = get ("seqSync"); seqRestart = get ("seqRestart");
    seqXfade = get ("seqXfade"); seqManual = get ("seqManual");

    for (int i = 0; i < seqs::numLfos; ++i)
        lfo[(size_t) i] = { get (lfoId (i, "Rate")), get (lfoId (i, "Shape")), get (lfoId (i, "Amt")),
                            get (lfoId (i, "Step")), get (lfoId (i, "Phase")), get (lfoId (i, "Pol")) };
    for (int c = 0; c < seqs::numChaos; ++c)
        chaos[(size_t) c] = { get (chaosId (c, "Rate")), get (chaosId (c, "Amt")), get (chaosId (c, "Smooth")) };
    for (int i = 0; i < numSteps; ++i)
        steps[(size_t) i] = get (stepId (i));
    for (int k = 0; k < seqs::numSlots; ++k)
        slots[(size_t) k] = { get (slotId (k, "Src")), get (slotId (k, "Dest")), get (slotId (k, "Amt")) };
}

void SequencesAudioProcessor::readSettings (seqs::Settings& s) const
{
    const auto v = [] (const std::atomic<float>* a) { return a->load (std::memory_order_relaxed); };
    const auto i = [&v] (const std::atomic<float>* a) { return juce::roundToInt (v (a)); };
    const auto pct = [&v] (const std::atomic<float>* a) { return 0.01f * v (a); };
    const auto sec = [&v] (const std::atomic<float>* a) { return 0.001f * v (a); };
    const auto& c = cache;

    for (int d = 1; d < seqs::numDests; ++d)
        s.base[(size_t) d] = c.dest[(size_t) d]->getValue();

    for (int o = 0; o < seqs::numOscs; ++o)
    {
        s.osc[(size_t) o].table = liveTables[(size_t) o].load();
        s.osc[(size_t) o].octave = juce::jlimit (-4, 4, i (c.osc[(size_t) o].octave));
        s.osc[(size_t) o].warp = (seqs::Warp) juce::jlimit (0, (int) seqs::Warp::count - 1, i (c.osc[(size_t) o].warp));
    }

    s.mono = i (c.voiceMode) == 0;
    s.glideMs = v (c.glide);
    s.velocity = pct (c.velocity);

    s.filterType = (seqs::FilterType) juce::jlimit (0, 2, i (c.filterType));
    s.slope24 = i (c.filterSlope) == 1;
    s.keyTrack = pct (c.keyTrack);

    s.ampA = sec (c.ampAttack);  s.ampD = sec (c.ampDecay);
    s.ampS = pct (c.ampSustain); s.ampR = sec (c.ampRelease);
    s.modA = sec (c.modAttack);  s.modD = sec (c.modDecay);
    s.modS = pct (c.modSustain); s.modR = sec (c.modRelease);

    s.driveType = (seqs::DriveType) juce::jlimit (0, 2, i (c.driveType));
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
        s.slotSource[(size_t) k] = juce::jlimit (0, seqs::numModSources - 1, i (sp.source));
        s.slotDest[(size_t) k] = (seqs::Dest) juce::jlimit (0, seqs::numDests - 1, i (sp.dest));
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
    for (int d = 0; d < seqs::numDests; ++d)
        liveOffsets[(size_t) d].store (engine.getLiveOffset ((seqs::Dest) d), std::memory_order_relaxed);
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

//==============================================================================
int SequencesAudioProcessor::getModulationSource (int slot) const  { return juce::roundToInt (realValue (slotId (slot, "Src"))); }
seqs::Dest SequencesAudioProcessor::getModulationDest (int slot) const
{
    return (seqs::Dest) juce::jlimit (0, seqs::numDests - 1, juce::roundToInt (realValue (slotId (slot, "Dest"))));
}
float SequencesAudioProcessor::getModulationAmount (int slot) const { return 0.01f * realValue (slotId (slot, "Amt")); }

int SequencesAudioProcessor::addModulation (int source, seqs::Dest dest)
{
    if (dest == seqs::Dest::off || source < 0 || source >= seqs::numModSources)
        return -1;

    for (int k = 0; k < seqs::numSlots; ++k)
        if (getModulationDest (k) == dest && getModulationSource (k) == source)
            return k;

    for (int k = 0; k < seqs::numSlots; ++k)
        if (getModulationDest (k) == seqs::Dest::off)
        {
            // Profondeur de départ : sensible mais pas brutale (une octave sur les hauteurs)
            float amount = 0.25f;
            if (dest == seqs::Dest::aCoarse || dest == seqs::Dest::bCoarse) amount = 12.0f / 96.0f;
            if (dest == seqs::Dest::pitch)                                   amount = 12.0f / 48.0f;

            setParam (slotId (k, "Src"), (float) source);
            setParam (slotId (k, "Amt"), amount * 100.0f);
            setParam (slotId (k, "Dest"), (float) dest);
            return k;
        }

    return -1;
}

void SequencesAudioProcessor::removeModulation (int slot)
{
    if (! juce::isPositiveAndBelow (slot, seqs::numSlots))
        return;
    setParam (slotId (slot, "Dest"), 0.0f);
    setParam (slotId (slot, "Amt"), 0.0f);
    setParam (slotId (slot, "Src"), 0.0f);
}

void SequencesAudioProcessor::setModulationAmount (int slot, float amount)
{
    if (juce::isPositiveAndBelow (slot, seqs::numSlots))
        setParam (slotId (slot, "Amt"), juce::jlimit (-1.0f, 1.0f, amount) * 100.0f);
}

std::vector<int> SequencesAudioProcessor::slotsForDest (seqs::Dest dest) const
{
    std::vector<int> slots;
    if (dest != seqs::Dest::off)
        for (int k = 0; k < seqs::numSlots; ++k)
            if (getModulationDest (k) == dest)
                slots.push_back (k);
    return slots;
}

int SequencesAudioProcessor::usedSlotCount() const
{
    int n = 0;
    for (int k = 0; k < seqs::numSlots; ++k)
        n += getModulationDest (k) != seqs::Dest::off ? 1 : 0;
    return n;
}

//==============================================================================
SequencesAudioProcessor::TableInfo SequencesAudioProcessor::getTableInfo (int osc) const
{
    const juce::ScopedLock sl (tableLock);
    return tables[(size_t) juce::jlimit (0, 1, osc)].info;
}

void SequencesAudioProcessor::installTable (int osc, std::unique_ptr<seqs::Wavetable> table, TableInfo info,
                                            std::vector<std::vector<float>> cycles)
{
    osc = juce::jlimit (0, seqs::numOscs - 1, osc);
    const juce::ScopedLock sl (tableLock);

    // Les tables remplacées restent en vie 2 s : le fil audio a pu en prendre une au début d'un bloc
    const auto now = juce::Time::getMillisecondCounter();
    retired.erase (std::remove_if (retired.begin(), retired.end(),
                                   [now] (const auto& r) { return now - r.first > 2000; }), retired.end());

    auto& slot = tables[(size_t) osc];
    const seqs::Wavetable* live = table != nullptr ? table.get() : seqs::BuiltinTables::get (juce::jmax (0, info.builtin));
    info.numFrames = live->numFrames;

    if (slot.owned != nullptr)
        retired.emplace_back (now, std::move (slot.owned));

    slot.owned = std::move (table);
    slot.info = std::move (info);
    slot.cycles = std::move (cycles);
    liveTables[(size_t) osc].store (live);
}

void SequencesAudioProcessor::setBuiltinTable (int osc, int index)
{
    index = juce::jlimit (0, seqs::BuiltinTables::count() - 1, index);
    TableInfo info;
    info.builtin = index;
    info.name = seqs::BuiltinTables::names()[(size_t) index];
    installTable (osc, nullptr, info, {});
}

juce::String SequencesAudioProcessor::setUserTable (int osc, std::vector<std::vector<float>> cycles, const juce::String& name,
                                                    const juce::File& file)
{
    normaliseCycles (cycles);
    auto table = seqs::Wavetable::build (cycles, name.toStdString());
    if (table == nullptr)
        return "EMPTY WAVETABLE";

    TableInfo info;
    info.builtin = -1;
    info.name = name;
    info.file = file;
    installTable (osc, std::move (table), info, std::move (cycles));
    return {};
}

juce::String SequencesAudioProcessor::loadTableFile (int osc, const juce::File& file)
{
    if (! file.existsAsFile())
        return "FILE NOT FOUND";
    if (file.getSize() > 64 * 1024 * 1024)
        return "FILE TOO BIG";

    juce::MemoryBlock data;
    if (! file.loadFileAsData (data))
        return "COULD NOT READ THE FILE";

    auto parsed = seqs::WavetableFile::parse ((const uint8_t*) data.getData(), data.getSize());
    if (! parsed.error.empty())
        return parsed.error;

    return setUserTable (osc, std::move (parsed.cycles), file.getFileNameWithoutExtension().toUpperCase(), file);
}

std::unique_ptr<juce::XmlElement> SequencesAudioProcessor::tablesToXml() const
{
    const juce::ScopedLock sl (tableLock);
    auto xml = std::make_unique<juce::XmlElement> ("TABLES");
    for (int o = 0; o < seqs::numOscs; ++o)
    {
        const auto& t = tables[(size_t) o];
        auto* e = xml->createNewChildElement ("TABLE");
        e->setAttribute ("osc", o);
        e->setAttribute ("name", t.info.name);
        e->setAttribute ("builtin", t.info.builtin);
        if (t.info.builtin < 0)
        {
            e->setAttribute ("file", t.info.file.getFullPathName());
            e->setAttribute ("data", encodeCycles (t.cycles));
        }
    }
    return xml;
}

void SequencesAudioProcessor::tablesFromXml (const juce::XmlElement* xml)
{
    std::array<bool, seqs::numOscs> done {};

    if (xml != nullptr)
        for (auto* e : xml->getChildWithTagNameIterator ("TABLE"))
        {
            const int o = e->getIntAttribute ("osc", -1);
            if (! juce::isPositiveAndBelow (o, seqs::numOscs))
                continue;

            const int builtin = e->getIntAttribute ("builtin", 0);
            if (builtin >= 0)
            {
                // Par le nom d'abord (l'ordre des tables intégrées peut changer d'une version à l'autre)
                const int byName = seqs::BuiltinTables::indexOf (e->getStringAttribute ("name").toStdString());
                setBuiltinTable (o, byName >= 0 ? byName : builtin);
                done[(size_t) o] = true;
            }
            else
            {
                auto cycles = decodeCycles (e->getStringAttribute ("data"));
                if (! cycles.empty())
                    done[(size_t) o] = setUserTable (o, std::move (cycles), e->getStringAttribute ("name"),
                                                     juce::File (e->getStringAttribute ("file"))).isEmpty();
            }
        }

    for (int o = 0; o < seqs::numOscs; ++o)
        if (! done[(size_t) o])
            setBuiltinTable (o, 0);
}

//==============================================================================
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
        listening = listening || (getModulationSource (k) == seqs::srcSeq && getModulationDest (k) != seqs::Dest::off
                                  && std::abs (getModulationAmount (k)) > 0.005f);
    if (! listening)
    {
        const int slot = addModulation (seqs::srcSeq, seqs::Dest::cutoff);
        if (slot >= 0)
            setModulationAmount (slot, 0.3f);
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
    {
        xml->addChildElement (tablesToXml().release());
        copyXmlToBinary (*xml, destData);
    }
}

void SequencesAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            tablesFromXml (xml->getChildByName ("TABLES"));
            xml->deleteAllChildElementsWithTagName ("TABLES");
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
