#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    using Layout = juce::AudioProcessorValueTreeState::ParameterLayout;
    using seqs::Dest;

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
        if (v < 10.0f)    return juce::String (v, 2) + " Hz";
        return juce::String (juce::roundToInt (v)) + " Hz";
    }

    juce::String semitoneText (float v, int)
    {
        const float r = std::round (v * 100.0f) / 100.0f;
        return (r > 0.0f ? "+" : "") + juce::String (r, std::abs (r - std::round (r)) < 0.005f ? 0 : 2) + " st";
    }

    juce::String centText (float v, int)                 { const int i = juce::roundToInt (v); return (i > 0 ? "+" : "") + juce::String (i) + " ct"; }

    juce::String panText (float v, int)
    {
        const int i = juce::roundToInt (v);
        return i == 0 ? juce::String ("C") : juce::String (std::abs (i)) + (i < 0 ? " L" : " R");
    }

    juce::String octText (int v, int)                    { return (v > 0 ? "+" : "") + juce::String (v); }

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
        addFloat (layout, id, name, { bipolar ? -100.0f : 0.0f, 100.0f, 0.0f }, defaultValue, bipolar ? signedPercentText : percentText);
    }

    void addMs (Layout& layout, const juce::String& id, const juce::String& name, float lo, float hi, float defaultValue)
    {
        juce::NormalisableRange<float> range (lo, hi, 0.0f);
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

    //==========================================================================
    // Potards modulables : unité du paramètre <-> position (0..1) du moteur
    enum class Unit { percent, semi, fine, pan, bipolar, cutoff, volume };

    Unit unitOf (Dest d)
    {
        if (seqs::DestRange::isSemi (d)) return Unit::semi;
        if (seqs::DestRange::isFine (d)) return Unit::fine;
        if (seqs::DestRange::isPan (d))  return Unit::pan;
        if (d == Dest::filterEnv)        return Unit::bipolar;
        if (d == Dest::cutoff)           return Unit::cutoff;
        if (d == Dest::volume)           return Unit::volume;
        return Unit::percent;
    }

    /** Valeur du paramètre pour une position du moteur. */
    float paramFromNorm (Dest d, float n)
    {
        switch (unitOf (d))
        {
            case Unit::percent: return 100.0f * n;
            case Unit::pan: case Unit::bipolar: return 100.0f * seqs::DestRange::toReal (d, n);
            case Unit::volume: return n;
            case Unit::semi: case Unit::fine: case Unit::cutoff: default: return seqs::DestRange::toReal (d, n);
        }
    }

    struct DestInfo { const char* id; const char* name; };
    constexpr DestInfo destInfo[] = {
        { "", "OFF" },
        { "wtPos", "WT POS" }, { "wtWarp", "WT WARP" }, { "wtSemi", "WT SEMI" }, { "wtFine", "WT FINE" }, { "wtLevel", "WT LEVEL" },
        { "wtDetune", "WT DETUNE" }, { "wtSpread", "WT SPREAD" }, { "wtPan", "WT PAN" },
        { "pdAmount", "PD AMT" }, { "pdShape", "PD SHAPE" }, { "pdCrush", "PD CRUSH" }, { "pdFold", "PD FOLD" }, { "pdLevel", "PD LEVEL" },
        { "pdSemi", "PD SEMI" }, { "pdFine", "PD FINE" },
        { "algoHarmonics", "ALGO HARM" }, { "algoTimbre", "ALGO TIMBRE" }, { "algoMorph", "ALGO MORPH" }, { "algoDecay", "LPG DECAY" },
        { "algoColour", "LPG COLOUR" }, { "algoAux", "ALGO AUX" }, { "algoLevel", "ALGO LEVEL" }, { "algoSemi", "ALGO SEMI" },
        { "algoFine", "ALGO FINE" }, { "algoPan", "ALGO PAN" },
        { "subLevel", "SUB LEVEL" }, { "noiseLevel", "NOISE LEVEL" }, { "fmDepth", "FM DEPTH" },
        { "biteFold", "BITE FOLD" }, { "biteCrush", "BITE CRUSH" }, { "biteSoft", "BITE SOFT" },
        { "cutoff", "CUTOFF" }, { "resonance", "RESO" }, { "filterDrive", "DRIVE" }, { "filterEnv", "ENV AMT" },
        { "volume", "VOLUME" } };
    static_assert (std::size (destInfo) == (size_t) seqs::numDests, "une entrée par destination");

    //==========================================================================
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

    constexpr double divisionBeats[] = { 4.0, 2.0, 1.0, 0.5, 0.25, 0.125, 0.0625 };
    const juce::StringArray divisionNames { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/64" };

    constexpr Rate arpRates[] = { { "1/4", 1.0 }, { "1/8", 0.5 }, { "1/8T", 1.0 / 3.0 }, { "1/16", 0.25 }, { "1/16T", 1.0 / 6.0 }, { "1/32", 0.125 } };
    constexpr Rate delayTimes[] = { { "1/32", 0.125 }, { "1/16", 0.25 }, { "1/8T", 1.0 / 3.0 }, { "1/8", 0.5 }, { "3/16", 0.75 },
                                    { "1/4T", 2.0 / 3.0 }, { "1/4", 1.0 }, { "3/8", 1.5 }, { "1/2", 2.0 } };

    template <size_t N>
    juce::StringArray rateNames (const Rate (&rates)[N])
    {
        juce::StringArray n;
        for (const auto& r : rates) n.add (r.name);
        return n;
    }

    const juce::StringArray lfoShapeNames { "SINE", "TRIANGLE", "SAW", "RAMP", "SQUARE", "PULSE", "EXP DOWN", "EXP UP" };
    const juce::StringArray mainLfoShapeNames { "SINE", "TRIANGLE", "SAW", "RAMP", "SQUARE", "S&H", "SMOOTH RND" };
    const juce::StringArray stepNames { "OFF", "2", "3", "4", "6", "8", "12", "16", "24", "32" };
    constexpr int stepValues[] = { 0, 2, 3, 4, 6, 8, 12, 16, 24, 32 };

    struct LfoDefault { const char* rate; int shape; float amount; int steps; float phase; bool uni; };
    constexpr LfoDefault lfoDefaults[seqs::numLfos] = {
        { "1/16",  3, 100.0f, 0, 0.0f, false },   // RAMP rapide
        { "1/32",  0, 100.0f, 0, 0.0f, false },   // SINE très rapide
        { "1/8T",  6, 100.0f, 0, 0.0f, false },   // EXP DOWN : « pew »
        { "1/4",   1, 100.0f, 5, 0.0f, false },   // TRIANGLE en 8 marches
        { "1/64",  4, 100.0f, 0, 0.0f, false },   // SQUARE : trille
        { "1 BAR", 2, 100.0f, 0, 0.0f, false } }; // SAW lente

    constexpr int defaultPattern[16] = { 1, 1, 2, 3, 1, 4, 2, 5, 1, 1, 3, 7, 6, 2, 8, 3 };

    const juce::StringArray fmRowIds { "Wt", "Pd", "Noise" };
    const juce::StringArray fmColIds { "Wt", "Pd" };

    //==========================================================================
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

    void normaliseCycles (std::vector<std::vector<float>>& cycles)
    {
        float peak = 0.0f;
        for (const auto& c : cycles) for (float v : c) peak = std::max (peak, std::abs (v));
        if (peak > 1.0e-9f)
            for (auto& c : cycles) for (float& v : c) v /= peak;
    }

    //==========================================================================
    // Presets d'usine : valeurs (en unités des paramètres) qui changent par rapport à INIT
    struct SlotSet { int source; Dest dest; float percent; };
    struct FactoryPreset
    {
        const char* name;
        int table;
        std::vector<std::pair<juce::String, float>> values;
        std::vector<SlotSet> slots;
    };

    const std::vector<FactoryPreset>& factoryPresets()
    {
        static const std::vector<FactoryPreset> list = []
        {
            const float r16 = (float) rateIndex ("1/16"), r32 = (float) rateIndex ("1/32"), r8 = (float) rateIndex ("1/8");
            std::vector<FactoryPreset> l;

            l.push_back ({ "INIT", 0, {}, {} });

            l.push_back ({ "HITECH ZAPS", 0,
                           { { "voiceMode", 1 }, { "wtPos", 50 }, { "wtLevel", 70 },
                             { "pdLevel", 45 }, { "pdWave", 6 }, { "pdAmount", 60 }, { "pdSemi", 12 },
                             { "filterSlope", 1 }, { "cutoff", 520 }, { "resonance", 55 }, { "filterEnv", 55 },
                             { "fltAttack", 1 }, { "fltDecay", 90 }, { "fltSustain", 0 }, { "fltRelease", 80 },
                             { "ampAttack", 1 }, { "ampDecay", 300 }, { "ampSustain", 80 }, { "ampRelease", 60 },
                             { "biteSoft", 25 }, { "seqDiv", 4 },
                             { "delayOn", 1 }, { "delayTime", 4 }, { "delayFeedback", 30 }, { "delayMix", 15 } },
                           { { seqs::srcSeq, Dest::cutoff, 35 }, { seqs::srcSeq, Dest::pdAmount, 30 }, { seqs::srcSeq, Dest::wtPos, 20 } } });

            l.push_back ({ "PD ACID", 0,
                           { { "voiceMode", 2 }, { "glide", 70 }, { "wtLevel", 0 },
                             { "pdLevel", 85 }, { "pdWave", 0 }, { "pdAmount", 75 },
                             { "filterSlope", 1 }, { "cutoff", 260 }, { "resonance", 72 }, { "filterEnv", 45 },
                             { "fltAttack", 1 }, { "fltDecay", 180 }, { "fltSustain", 0 },
                             { "ampAttack", 1 }, { "ampSustain", 100 }, { "ampRelease", 40 },
                             { "biteSoft", 40 }, { "biteAsym", 1 }, { "seqDiv", 4 } },
                           { { seqs::srcSeq, Dest::pdAmount, 25 }, { seqs::srcSeq, Dest::cutoff, 15 } } });

            l.push_back ({ "PLAITS PLUCKS", 0,
                           { { "wtLevel", 0 }, { "algoOn", 1 }, { "algoModel", 8 }, { "algoLpg", 1 }, { "algoDecay", 35 },
                             { "algoColour", 60 }, { "algoLevel", 80 }, { "algoHarmonics", 40 }, { "algoTimbre", 50 }, { "algoMorph", 30 },
                             { "ampSustain", 100 }, { "ampRelease", 300 }, { "seqDiv", 4 },
                             { "delayOn", 1 }, { "delayTime", 4 }, { "delayFeedback", 40 }, { "delayMix", 25 },
                             { "reverbOn", 1 }, { "reverbMix", 15 }, { "reverbDecay", 45 } },
                           { { seqs::srcSeq, Dest::algoTimbre, 40 }, { seqs::srcSeq, Dest::algoMorph, 30 } } });

            l.push_back ({ "FM GROWL", 0,
                           { { "voiceMode", 1 }, { "wtPos", 25 }, { "pdLevel", 0 }, { "pdWave", 0 }, { "fmPdWt", 100 }, { "fmDepth", 25 },
                             { "filterSlope", 1 }, { "cutoff", 1200 }, { "resonance", 30 }, { "biteFold", 30 },
                             { "ampAttack", 1 }, { "ampSustain", 100 }, { "ampRelease", 50 }, { "seqDiv", 4 } },
                           { { seqs::srcSeq, Dest::fmDepth, 35 }, { seqs::srcSeq, Dest::cutoff, 25 } } });

            l.push_back ({ "CHAOS BLIPS", 0,
                           { { "wtLevel", 0 }, { "pdLevel", 70 }, { "pdWave", 1 }, { "pdAmount", 40 },
                             { "ampAttack", 1 }, { "ampDecay", 90 }, { "ampSustain", 0 }, { "ampRelease", 80 },
                             { "filterModel", 1 }, { "cutoff", 3000 }, { "resonance", 35 },
                             { "chaos1Rate", r16 }, { "chaos2Rate", r32 }, { "arpOn", 1 }, { "arpRate", 3 }, { "arpGate", 35 },
                             { "delayOn", 1 }, { "delayTime", 2 }, { "delayFeedback", 45 }, { "delayMix", 25 },
                             { "reverbOn", 1 }, { "reverbMix", 20 } },
                           { { seqs::srcChaos1, Dest::pdSemi, 12.5f }, { seqs::srcChaos1 + 1, Dest::cutoff, 30 } } });

            l.push_back ({ "SUB BASS", 0,
                           { { "voiceMode", 1 }, { "wtLevel", 35 }, { "wtPos", 50 }, { "subLevel", 80 }, { "subWave", 0 },
                             { "filterSlope", 1 }, { "cutoff", 280 }, { "resonance", 15 }, { "filterEnv", 20 }, { "fltDecay", 200 },
                             { "ampAttack", 1 }, { "ampSustain", 100 }, { "ampRelease", 40 }, { "seqOn", 0 } },
                           {} });

            l.push_back ({ "SUPERSAW", 0,
                           { { "unison", 7 }, { "wtDetune", 30 }, { "wtSpread", 90 }, { "wtRandPhase", 1 }, { "wtPos", 50 },
                             { "filterModel", 1 }, { "cutoff", 6000 },
                             { "ampAttack", 10 }, { "ampDecay", 600 }, { "ampSustain", 80 }, { "ampRelease", 400 },
                             { "chorusOn", 1 }, { "chorusMix", 25 },
                             { "delayOn", 1 }, { "delayTime", 7 }, { "delayFeedback", 35 }, { "delayMix", 20 },
                             { "reverbOn", 1 }, { "reverbMix", 25 }, { "reverbDecay", 60 }, { "seqOn", 0 } },
                           {} });

            l.push_back ({ "WOBBLE LFO", 0,
                           { { "voiceMode", 1 }, { "wtPos", 75 }, { "filterSlope", 1 }, { "cutoff", 400 }, { "resonance", 45 },
                             { "mlSync", 1 }, { "mlRate", r8 }, { "mlShape", 0 }, { "mlRetrig", 1 },
                             { "mlDest1", (float) Dest::cutoff }, { "mlAmt1", 35 }, { "mlDest2", (float) Dest::wtPos }, { "mlAmt2", -20 },
                             { "ampAttack", 1 }, { "ampSustain", 100 }, { "ampRelease", 60 }, { "biteSoft", 30 }, { "seqOn", 0 } },
                           {} });
            return l;
        }();
        return list;
    }
}

//==============================================================================
juce::String SequencesAudioProcessor::fmId (int row, int col)
{
    return "fm" + fmRowIds[juce::jlimit (0, seqs::numFmRows - 1, row)] + fmColIds[juce::jlimit (0, seqs::numFmCols - 1, col)];
}

const juce::StringArray& SequencesAudioProcessor::destinationNames()
{
    static const juce::StringArray names = []
    {
        juce::StringArray n;
        for (const auto& d : destInfo) n.add (d.name);
        return n;
    }();
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
                                           "ENV 2", "ENV 3", "VELO", "MAIN LFO", "MOD WHEEL" };
    return names;
}

const juce::StringArray& SequencesAudioProcessor::lfoRateNames()
{
    static const juce::StringArray names = rateNames (lfoRates);
    return names;
}

double SequencesAudioProcessor::lfoRateBeats (int index)
{
    return lfoRates[juce::jlimit (0, numLfoRates - 1, index)].beats;
}

const juce::StringArray& SequencesAudioProcessor::plaitsModelNames()
{
    static const juce::StringArray names = []
    {
        juce::StringArray n;
        for (int i = 0; i < seqs::PlaitsModels::count; ++i) n.add (seqs::PlaitsModels::name (i));
        return n;
    }();
    return names;
}

juce::String SequencesAudioProcessor::paramForDest (seqs::Dest d)
{
    return destInfo[juce::jlimit (0, seqs::numDests - 1, (int) d)].id;
}

seqs::Dest SequencesAudioProcessor::destForParam (const juce::String& id)
{
    for (int d = 1; d < seqs::numDests; ++d)
        if (id == destInfo[d].id)
            return (seqs::Dest) d;
    return seqs::Dest::off;
}

int SequencesAudioProcessor::numFactoryPresets()               { return (int) factoryPresets().size(); }
juce::String SequencesAudioProcessor::factoryPresetName (int i) { return factoryPresets()[(size_t) juce::jlimit (0, numFactoryPresets() - 1, i)].name; }

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout SequencesAudioProcessor::createParameterLayout()
{
    Layout layout;
    const seqs::Settings init;

    // --- Potards modulables : exactement la course de seqs::DestRange ---
    for (int i = 1; i < seqs::numDests; ++i)
    {
        const auto d = (Dest) i;
        const juce::String id = destInfo[i].id;
        const juce::String name = juce::String (destInfo[i].name).toLowerCase();
        const float def = paramFromNorm (d, init.base[(size_t) i]);
        switch (unitOf (d))
        {
            case Unit::percent: addPercent (layout, id, name, def); break;
            case Unit::bipolar: addPercent (layout, id, name, def, true); break;
            case Unit::semi:    addFloat (layout, id, name, { -48.0f, 48.0f, 0.0f }, def, semitoneText); break;
            case Unit::fine:    addFloat (layout, id, name, { -100.0f, 100.0f, 0.0f }, def, centText); break;
            case Unit::pan:     addFloat (layout, id, name, { -100.0f, 100.0f, 0.0f }, def, panText); break;
            case Unit::cutoff:
            {
                juce::NormalisableRange<float> cutoff (20.0f, 20000.0f,
                                                       [] (float lo, float hi, float t) { return lo * std::pow (hi / lo, t); },
                                                       [] (float lo, float hi, float v) { return std::log (v / lo) / std::log (hi / lo); });
                addFloat (layout, id, name, cutoff, def, hzText);
                break;
            }
            case Unit::volume:
                addFloat (layout, id, name, { 0.0f, 1.0f, 0.0f }, def, [] (float v, int)
                {
                    const float gain = seqs::DestRange::toReal (Dest::volume, v);
                    if (gain < 1.0e-4f) return juce::String ("-INF");
                    const float db = 20.0f * std::log10 (gain);
                    return (db > 0.04f ? "+" : "") + juce::String (db, 1) + " dB";
                });
                break;
        }
    }

    // --- Oscillateurs ---
    addBool (layout, "wtOn", "wavetable on", init.wtOn);
    addChoice (layout, "wtWarpMode", "wavetable warp mode", { "OFF", "SYNC", "BEND +", "BEND -", "MIRROR" }, (int) init.wtWarp);
    addInt (layout, "wtOct", "wavetable octave", -4, 4, init.wtOctave, octText);
    addInt (layout, "unison", "unison", 1, seqs::maxUnison, init.unison);
    addPercent (layout, "wtPhase", "wavetable phase", 100.0f * init.wtPhase);
    addBool (layout, "wtRandPhase", "wavetable random phase", init.wtRandomPhase);

    addBool (layout, "pdOn", "pd on", init.pdOn);
    addChoice (layout, "pdWave", "pd wave", { "SAW", "SQUARE", "PULSE", "DBL SINE", "SAW-PULSE", "RESO 1", "RESO 2", "RESO 3" }, (int) init.pdWave);
    addInt (layout, "pdOct", "pd octave", -4, 4, init.pdOctave, octText);

    addBool (layout, "algoOn", "algo on", init.algoOn);
    addChoice (layout, "algoModel", "algo model", plaitsModelNames(), init.algoModel);
    addBool (layout, "algoLpg", "algo lpg", init.algoLpg);
    addInt (layout, "algoOct", "algo octave", -4, 4, init.algoOctave, octText);

    addChoice (layout, "subWave", "sub wave", { "SINE", "TRIANGLE", "SQUARE", "SAW" }, (int) init.subWave);
    addInt (layout, "subOct", "sub octave", -2, 0, init.subOctave, octText);
    addChoice (layout, "noiseType", "noise type", { "WHITE", "PINK", "BROWN", "DIGITAL" }, (int) init.noiseType);

    // --- Matrice FM ---
    const char* rowNames[] = { "wt", "pd", "noise" };
    const char* colNames[] = { "wt", "pd" };
    for (int r = 0; r < seqs::numFmRows; ++r)
        for (int c = 0; c < seqs::numFmCols; ++c)
            addPercent (layout, fmId (r, c), juce::String ("fm ") + rowNames[r] + " to " + colNames[c], 100.0f * init.fm[(size_t) r][(size_t) c]);
    addChoice (layout, "fmMode", "fm mode", { "PHASE", "LINEAR" }, (int) init.fmMode);

    // --- Bite, filtre ---
    addBool (layout, "bitePost", "bite post filter", init.bitePost);
    addBool (layout, "biteAsym", "bite asym", init.biteAsym);
    addChoice (layout, "filterModel", "filter model", { "LADDER", "SVF" }, (int) init.filterModel);
    addChoice (layout, "filterType", "filter type", { "LP", "BP", "HP", "NOTCH" }, (int) init.filterType);
    addChoice (layout, "filterSlope", "filter slope", { "12 dB", "24 dB" }, init.slope24 ? 1 : 0);
    addPercent (layout, "keyTrack", "key track", 100.0f * init.keyTrack);

    // --- Voix ---
    addChoice (layout, "voiceMode", "voice mode", { "POLY", "MONO", "LEGATO" }, (int) init.voiceMode);
    addMs (layout, "glide", "glide", 0.0f, 2000.0f, init.glideMs);
    addInt (layout, "bend", "bend range", 0, 24, juce::roundToInt (init.bendRange), [] (int v, int) { return juce::String (v) + " st"; });
    addPercent (layout, "velocity", "velocity", 100.0f * init.velocity);
    addChoice (layout, "oversampling", "oversampling", { "1X", "2X", "4X" }, init.oversampling >= 4 ? 2 : init.oversampling >= 2 ? 1 : 0);

    // --- Enveloppes ---
    auto env = [&layout] (const char* prefix, const char* name, float a, float d, float s, float r)
    {
        const juce::String p (prefix), n (name);
        addMs (layout, p + "Attack", n + " attack", 0.5f, 10000.0f, a * 1000.0f);
        addMs (layout, p + "Decay", n + " decay", 1.0f, 10000.0f, d * 1000.0f);
        addPercent (layout, p + "Sustain", n + " sustain", s * 100.0f);
        addMs (layout, p + "Release", n + " release", 1.0f, 10000.0f, r * 1000.0f);
    };
    env ("amp", "amp env", init.ampA, init.ampD, init.ampS, init.ampR);
    env ("flt", "filter env", init.fltA, init.fltD, init.fltS, init.fltR);
    env ("mod", "mod env", init.modA, init.modD, init.modS, init.modR);

    // --- MAIN LFO ---
    addBool (layout, "mlSync", "main lfo sync", init.mainLfo.sync);
    addChoice (layout, "mlRate", "main lfo rate", lfoRateNames(), rateIndex ("1/4"));
    {
        juce::NormalisableRange<float> hz (0.02f, 40.0f, 0.0f);
        hz.setSkewForCentre (2.0f);
        addFloat (layout, "mlHz", "main lfo hz", hz, init.mainLfo.rateHz, hzText);
    }
    addChoice (layout, "mlShape", "main lfo shape", mainLfoShapeNames, (int) init.mainLfo.shape);
    addBool (layout, "mlRetrig", "main lfo retrig", init.mainLfo.retrig);
    addChoice (layout, "mlDest1", "main lfo dest 1", destinationNames(), (int) init.mainLfo.dest[0]);
    addPercent (layout, "mlAmt1", "main lfo amount 1", 0.0f, true);
    addChoice (layout, "mlDest2", "main lfo dest 2", destinationNames(), (int) init.mainLfo.dest[1]);
    addPercent (layout, "mlAmt2", "main lfo amount 2", 0.0f, true);

    // --- Banque de LFO ---
    for (int i = 0; i < seqs::numLfos; ++i)
    {
        const auto& d = lfoDefaults[i];
        const juce::String name = "lfo " + juce::String (i + 1) + " ";
        addChoice (layout, lfoId (i, "Rate"), name + "rate", lfoRateNames(), rateIndex (d.rate));
        addChoice (layout, lfoId (i, "Shape"), name + "shape", lfoShapeNames, d.shape);
        addPercent (layout, lfoId (i, "Amt"), name + "amount", d.amount);
        addChoice (layout, lfoId (i, "Step"), name + "step", stepNames, d.steps);
        addFloat (layout, lfoId (i, "Phase"), name + "phase", { 0.0f, 360.0f, 1.0f }, d.phase,
                  [] (float v, int) { return juce::String (juce::roundToInt (v)) + juce::String (juce::CharPointer_UTF8 ("\xc2\xb0")); });
        addChoice (layout, lfoId (i, "Pol"), name + "polarity", { "BI", "UNI" }, d.uni ? 1 : 0);
    }

    for (int c = 0; c < seqs::numChaos; ++c)
    {
        const juce::String name = "chaos " + juce::String (c + 1) + " ";
        addChoice (layout, chaosId (c, "Rate"), name + "rate", lfoRateNames(), rateIndex (c == 0 ? "1/16" : "1/32"));
        addPercent (layout, chaosId (c, "Amt"), name + "amount", 100.0f);
        addPercent (layout, chaosId (c, "Smooth"), name + "smooth", c == 0 ? 0.0f : 60.0f);
    }

    // --- Séquenceur ---
    addBool (layout, "seqOn", "seq on", true);
    addInt (layout, "seqSteps", "seq steps", 1, numSteps, 16);
    addChoice (layout, "seqDiv", "seq division", divisionNames, 3);
    addChoice (layout, "seqFeel", "seq feel", { "STRAIGHT", "TRIPLET", "DOTTED" }, 0);
    addFloat (layout, "seqSwing", "seq swing", { 0.0f, 75.0f, 0.1f }, 0.0f, percentText);
    addChoice (layout, "seqSync", "seq sync", { "NOTE", "HOST" }, 0);
    addBool (layout, "seqRestart", "seq restart", true);
    addFloat (layout, "seqXfade", "seq crossfade", { 0.0f, 50.0f, 0.1f }, 2.0f, msText);
    addChoice (layout, "seqManual", "seq manual source", sourceNames(), 1);
    for (int i = 0; i < numSteps; ++i)
        addChoice (layout, stepId (i), "seq step " + juce::String (i + 1), sourceNames(), defaultPattern[i % 16]);

    // --- Routages (24) ---
    for (int k = 0; k < seqs::numSlots; ++k)
    {
        const juce::String name = "mod " + juce::String (k + 1) + " ";
        addChoice (layout, slotId (k, "Src"), name + "source", modSourceNames(), 0);
        addChoice (layout, slotId (k, "Dest"), name + "destination", destinationNames(), 0);
        addPercent (layout, slotId (k, "Amt"), name + "amount", 0.0f, true);
    }

    // --- Arpégiateur ---
    addBool (layout, "arpOn", "arp on", false);
    addChoice (layout, "arpMode", "arp mode", { "UP", "DOWN", "UP/DOWN", "DOWN/UP", "RANDOM", "AS PLAYED", "CHORD" }, 0);
    addChoice (layout, "arpRate", "arp rate", rateNames (arpRates), 3);
    addInt (layout, "arpOct", "arp octaves", 1, 4, 1);
    addPercent (layout, "arpGate", "arp gate", 50.0f);
    addFloat (layout, "arpSwing", "arp swing", { 0.0f, 75.0f, 0.1f }, 0.0f, percentText);
    addBool (layout, "arpLatch", "arp latch", false);

    // --- Effets ---
    const seqs::FxSettings fx;
    addBool (layout, "chorusOn", "chorus on", fx.chorusOn);
    {
        juce::NormalisableRange<float> hz (0.05f, 5.0f, 0.0f);
        hz.setSkewForCentre (0.6f);
        addFloat (layout, "chorusRate", "chorus rate", hz, fx.chorusRate, hzText);
    }
    addPercent (layout, "chorusDepth", "chorus depth", 100.0f * fx.chorusDepth);
    addPercent (layout, "chorusMix", "chorus mix", 100.0f * fx.chorusMix);
    addBool (layout, "delayOn", "delay on", fx.delayOn);
    addChoice (layout, "delayTime", "delay time", rateNames (delayTimes), 4);
    addFloat (layout, "delayFeedback", "delay feedback", { 0.0f, 95.0f, 0.0f }, 100.0f * fx.delayFeedback, percentText);
    addBool (layout, "delayPingPong", "delay ping pong", fx.delayPingPong);
    addPercent (layout, "delayTone", "delay tone", 100.0f * fx.delayTone);
    addPercent (layout, "delayMix", "delay mix", 100.0f * fx.delayMix);
    addBool (layout, "reverbOn", "reverb on", fx.reverbOn);
    addPercent (layout, "reverbSize", "reverb size", 100.0f * fx.reverbSize);
    addPercent (layout, "reverbDecay", "reverb decay", 100.0f * fx.reverbDecay);
    addPercent (layout, "reverbDamping", "reverb damping", 100.0f * fx.reverbDamping);
    addPercent (layout, "reverbPredelay", "reverb predelay", 100.0f * fx.reverbPredelay);
    addPercent (layout, "reverbMix", "reverb mix", 100.0f * fx.reverbMix);

    return layout;
}

//==============================================================================
SequencesAudioProcessor::SequencesAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Sequences", createParameterLayout()),
      presets (apvts, "Sequences", ".seqpreset",
               PresetLibrary::Factory { [] { return numFactoryPresets(); },
                                        [] (int i) { return factoryPresetName (i); },
                                        [this] (int i) { applyFactory (i); },
                                        [this] { return matchingFactory(); } },
               {}, "UNTITLED")
{
    for (auto* param : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param))
            raw[ranged->getParameterID()] = apvts.getRawParameterValue (ranged->getParameterID());

    cache.build (*this);
    setBuiltinTable (0);

    presets.writeExtra = [this] (juce::XmlElement& xml) { xml.addChildElement (tablesToXml().release()); };
    presets.readExtra = [this] (const juce::XmlElement* xml) { tablesFromXml (xml != nullptr ? xml->getChildByName ("TABLES") : nullptr); };
    presets.restoreFromState();

    for (auto& o : liveOffsets) o.store (0.0f);
    for (auto& s : scope) s.store (0.0f);
    noteEvents.reserve (512);
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
    readArpSettings (arpSettings);
    arp.reset();
    arp.setSettings (arpSettings);
    readFxSettings (fxSettings);
    fx.setSettings (fxSettings);
    fx.prepare (currentSampleRate);
}

void SequencesAudioProcessor::reset()
{
    engine.reset();
    arp.reset();
    fx.reset();
}

//==============================================================================
void SequencesAudioProcessor::Cache::build (SequencesAudioProcessor& proc)
{
    auto get = [&proc] (const juce::String& id) { return proc.raw.at (id); };

    for (int d = 1; d < seqs::numDests; ++d)
    {
        dest[(size_t) d] = proc.apvts.getParameter (paramForDest ((Dest) d));
        jassert (dest[(size_t) d] != nullptr);
    }

    wtOn = get ("wtOn"); pdOn = get ("pdOn"); algoOn = get ("algoOn"); wtWarpMode = get ("wtWarpMode");
    wtOct = get ("wtOct"); pdOct = get ("pdOct"); algoOct = get ("algoOct"); unison = get ("unison");
    wtPhase = get ("wtPhase"); wtRandPhase = get ("wtRandPhase");
    pdWave = get ("pdWave"); algoModel = get ("algoModel"); algoLpg = get ("algoLpg");
    subWave = get ("subWave"); subOct = get ("subOct"); noiseType = get ("noiseType");
    for (int r = 0; r < seqs::numFmRows; ++r)
        for (int c = 0; c < seqs::numFmCols; ++c)
            fm[(size_t) r][(size_t) c] = get (fmId (r, c));
    fmMode = get ("fmMode"); bitePost = get ("bitePost"); biteAsym = get ("biteAsym");
    filterModel = get ("filterModel"); filterType = get ("filterType"); filterSlope = get ("filterSlope"); keyTrack = get ("keyTrack");
    voiceMode = get ("voiceMode"); glide = get ("glide"); bend = get ("bend"); velocity = get ("velocity"); oversampling = get ("oversampling");
    ampA = get ("ampAttack"); ampD = get ("ampDecay"); ampS = get ("ampSustain"); ampR = get ("ampRelease");
    fltA = get ("fltAttack"); fltD = get ("fltDecay"); fltS = get ("fltSustain"); fltR = get ("fltRelease");
    modA = get ("modAttack"); modD = get ("modDecay"); modS = get ("modSustain"); modR = get ("modRelease");
    mlSync = get ("mlSync"); mlRate = get ("mlRate"); mlHz = get ("mlHz"); mlShape = get ("mlShape"); mlRetrig = get ("mlRetrig");
    mlDest1 = get ("mlDest1"); mlDest2 = get ("mlDest2"); mlAmt1 = get ("mlAmt1"); mlAmt2 = get ("mlAmt2");
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

    arpOn = get ("arpOn"); arpMode = get ("arpMode"); arpRate = get ("arpRate"); arpOct = get ("arpOct");
    arpGate = get ("arpGate"); arpSwing = get ("arpSwing"); arpLatch = get ("arpLatch");
    chorusOn = get ("chorusOn"); chorusRate = get ("chorusRate"); chorusDepth = get ("chorusDepth"); chorusMix = get ("chorusMix");
    delayOn = get ("delayOn"); delayTime = get ("delayTime"); delayFeedback = get ("delayFeedback");
    delayPingPong = get ("delayPingPong"); delayTone = get ("delayTone"); delayMix = get ("delayMix");
    reverbOn = get ("reverbOn"); reverbSize = get ("reverbSize"); reverbDecay = get ("reverbDecay");
    reverbDamping = get ("reverbDamping"); reverbPredelay = get ("reverbPredelay"); reverbMix = get ("reverbMix");
}

void SequencesAudioProcessor::readSettings (seqs::Settings& s) const
{
    const auto v = [] (const std::atomic<float>* a) { return a->load (std::memory_order_relaxed); };
    const auto i = [&v] (const std::atomic<float>* a) { return juce::roundToInt (v (a)); };
    const auto on = [&v] (const std::atomic<float>* a) { return v (a) > 0.5f; };
    const auto pct = [&v] (const std::atomic<float>* a) { return 0.01f * v (a); };
    const auto sec = [&v] (const std::atomic<float>* a) { return 0.001f * v (a); };
    const auto& c = cache;

    for (int d = 1; d < seqs::numDests; ++d)
        s.base[(size_t) d] = c.dest[(size_t) d]->getValue();

    s.wtOn = on (c.wtOn); s.pdOn = on (c.pdOn); s.algoOn = on (c.algoOn);
    s.table = liveTable.load();
    s.wtWarp = (seqs::WtWarp) juce::jlimit (0, (int) seqs::WtWarp::count - 1, i (c.wtWarpMode));
    s.wtOctave = juce::jlimit (-4, 4, i (c.wtOct));
    s.pdOctave = juce::jlimit (-4, 4, i (c.pdOct));
    s.algoOctave = juce::jlimit (-4, 4, i (c.algoOct));
    s.unison = juce::jlimit (1, seqs::maxUnison, i (c.unison));
    s.wtPhase = pct (c.wtPhase);
    s.wtRandomPhase = on (c.wtRandPhase);
    s.pdWave = (seqs::PdWave) juce::jlimit (0, (int) seqs::PdWave::count - 1, i (c.pdWave));
    s.algoModel = juce::jlimit (0, seqs::PlaitsModels::count - 1, i (c.algoModel));
    s.algoLpg = on (c.algoLpg);
    s.subWave = (seqs::SubWave) juce::jlimit (0, (int) seqs::SubWave::count - 1, i (c.subWave));
    s.subOctave = juce::jlimit (-2, 0, i (c.subOct));
    s.noiseType = (seqs::NoiseType) juce::jlimit (0, (int) seqs::NoiseType::count - 1, i (c.noiseType));

    for (int r = 0; r < seqs::numFmRows; ++r)
        for (int col = 0; col < seqs::numFmCols; ++col)
            s.fm[(size_t) r][(size_t) col] = pct (c.fm[(size_t) r][(size_t) col]);
    s.fmMode = (seqs::FmMode) juce::jlimit (0, 1, i (c.fmMode));
    s.bitePost = on (c.bitePost);
    s.biteAsym = on (c.biteAsym);

    s.filterModel = (seqs::FilterModel) juce::jlimit (0, 1, i (c.filterModel));
    s.filterType = (seqs::FilterType) juce::jlimit (0, 3, i (c.filterType));
    s.slope24 = i (c.filterSlope) == 1;
    s.keyTrack = pct (c.keyTrack);

    s.voiceMode = (seqs::VoiceMode) juce::jlimit (0, 2, i (c.voiceMode));
    s.glideMs = v (c.glide);
    s.bendRange = (float) i (c.bend);
    s.velocity = pct (c.velocity);
    s.oversampling = 1 << juce::jlimit (0, 2, i (c.oversampling));

    s.ampA = sec (c.ampA); s.ampD = sec (c.ampD); s.ampS = pct (c.ampS); s.ampR = sec (c.ampR);
    s.fltA = sec (c.fltA); s.fltD = sec (c.fltD); s.fltS = pct (c.fltS); s.fltR = sec (c.fltR);
    s.modA = sec (c.modA); s.modD = sec (c.modD); s.modS = pct (c.modS); s.modR = sec (c.modR);

    auto& ml = s.mainLfo;
    ml.sync = on (c.mlSync);
    ml.periodBeats = lfoRateBeats (i (c.mlRate));
    ml.rateHz = v (c.mlHz);
    ml.shape = (seqs::MainLfoShape) juce::jlimit (0, (int) seqs::MainLfoShape::count - 1, i (c.mlShape));
    ml.retrig = on (c.mlRetrig);
    ml.dest[0] = (Dest) juce::jlimit (0, seqs::numDests - 1, i (c.mlDest1));
    ml.dest[1] = (Dest) juce::jlimit (0, seqs::numDests - 1, i (c.mlDest2));
    ml.amount[0] = pct (c.mlAmt1);
    ml.amount[1] = pct (c.mlAmt2);

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
    q.on = on (c.seqOn);
    q.steps = juce::jlimit (1, numSteps, i (c.seqSteps));
    const int div = juce::jlimit (0, (int) std::size (divisionBeats) - 1, i (c.seqDiv));
    const int feel = i (c.seqFeel);
    q.stepBeats = divisionBeats[div] * (feel == 1 ? 2.0 / 3.0 : feel == 2 ? 1.5 : 1.0);
    q.swing = 0.01 * (double) v (c.seqSwing);
    q.hostSync = i (c.seqSync) == 1;
    q.restart = on (c.seqRestart);
    q.xfadeMs = v (c.seqXfade);
    q.manual = juce::jlimit (0, seqs::numSources, i (c.seqManual));
    for (int n = 0; n < numSteps; ++n)
        q.pattern[(size_t) n] = juce::jlimit (0, seqs::numSources, i (c.steps[(size_t) n]));

    for (int k = 0; k < seqs::numSlots; ++k)
    {
        const auto& sp = c.slots[(size_t) k];
        s.slotSource[(size_t) k] = juce::jlimit (0, seqs::numModSources - 1, i (sp.source));
        s.slotDest[(size_t) k] = (Dest) juce::jlimit (0, seqs::numDests - 1, i (sp.dest));
        s.slotAmount[(size_t) k] = pct (sp.amount);
    }
}

void SequencesAudioProcessor::readArpSettings (seqs::ArpSettings& a) const
{
    const auto& c = cache;
    a.on = c.arpOn->load() > 0.5f;
    a.mode = (seqs::ArpMode) juce::jlimit (0, (int) seqs::ArpMode::count - 1, juce::roundToInt (c.arpMode->load()));
    a.stepBeats = arpRates[juce::jlimit (0, (int) std::size (arpRates) - 1, juce::roundToInt (c.arpRate->load()))].beats;
    a.octaves = juce::jlimit (1, 4, juce::roundToInt (c.arpOct->load()));
    a.gate = 0.01f * c.arpGate->load();
    a.swing = 0.01 * (double) c.arpSwing->load();
    a.latch = c.arpLatch->load() > 0.5f;
}

void SequencesAudioProcessor::readFxSettings (seqs::FxSettings& f) const
{
    const auto& c = cache;
    f.chorusOn = c.chorusOn->load() > 0.5f;
    f.chorusRate = c.chorusRate->load();
    f.chorusDepth = 0.01f * c.chorusDepth->load();
    f.chorusMix = 0.01f * c.chorusMix->load();
    f.delayOn = c.delayOn->load() > 0.5f;
    f.delayBeats = delayTimes[juce::jlimit (0, (int) std::size (delayTimes) - 1, juce::roundToInt (c.delayTime->load()))].beats;
    f.delayFeedback = 0.01f * c.delayFeedback->load();
    f.delayPingPong = c.delayPingPong->load() > 0.5f;
    f.delayTone = 0.01f * c.delayTone->load();
    f.delayMix = 0.01f * c.delayMix->load();
    f.reverbOn = c.reverbOn->load() > 0.5f;
    f.reverbSize = 0.01f * c.reverbSize->load();
    f.reverbDecay = 0.01f * c.reverbDecay->load();
    f.reverbDamping = 0.01f * c.reverbDamping->load();
    f.reverbPredelay = 0.01f * c.reverbPredelay->load();
    f.reverbMix = 0.01f * c.reverbMix->load();
}

//==============================================================================
void SequencesAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    buffer.clear();
    if (numSamples <= 0)
        return;

    readSettings (settings);
    engine.setSettings (settings);
    const bool arpWasOn = arpSettings.on;
    readArpSettings (arpSettings);
    arp.setSettings (arpSettings);
    if (arpWasOn != arpSettings.on)
        engine.allNotesOff (false);        // les notes jouées par l'autre chemin s'arrêtent
    readFxSettings (fxSettings);
    fx.setSettings (fxSettings);

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

    // --- Notes : MIDI de l'hôte -> (arpégiateur) -> moteur, toutes à leur position dans le bloc ---
    noteEvents.clear();
    enum Kind { kindOff = 0, kindOn = 1, kindBend = 2, kindWheel = 3, kindAllOff = 4 };
    int arpPos = 0;
    const double beatsPerSample = transport.bpm / 60.0 / currentSampleRate;
    auto runArp = [&] (int end)
    {
        if (end > arpPos && (arpSettings.on || arp.playingCount() > 0))
        {
            const int base = arpPos;
            arp.process (end - arpPos, currentSampleRate, transport.bpm, transport.playing, transport.ppq + base * beatsPerSample,
                         [this, base] (bool noteOn, int note, float velocity, int offset)
                         {
                             noteEvents.push_back ({ base + offset, noteOn ? kindOn : kindOff, note, velocity });
                         });
        }
        arpPos = juce::jmax (arpPos, end);
    };

    for (const auto metadata : midi)
    {
        const int when = juce::jlimit (0, numSamples - 1, metadata.samplePosition);
        runArp (when);
        const auto m = metadata.getMessage();
        if (m.isNoteOn())
        {
            arp.noteOn (m.getNoteNumber(), m.getFloatVelocity());
            if (! arpSettings.on)
                noteEvents.push_back ({ when, kindOn, m.getNoteNumber(), m.getFloatVelocity() });
        }
        else if (m.isNoteOff())
        {
            arp.noteOff (m.getNoteNumber());
            if (! arpSettings.on)
                noteEvents.push_back ({ when, kindOff, m.getNoteNumber(), 0.0f });
        }
        else if (m.isAllNotesOff() || m.isAllSoundOff())
        {
            arp.reset();
            noteEvents.push_back ({ when, kindAllOff, m.isAllSoundOff() ? 1 : 0, 0.0f });
        }
        else if (m.isPitchWheel())
            noteEvents.push_back ({ when, kindBend, 0, (float) (m.getPitchWheelValue() - 8192) / 8192.0f });
        else if (m.isControllerOfType (1))
            noteEvents.push_back ({ when, kindWheel, 0, (float) m.getControllerValue() / 127.0f });
    }
    runArp (numSamples);

    std::stable_sort (noteEvents.begin(), noteEvents.end(), [] (const NoteEvent& a, const NoteEvent& b) { return a.offset < b.offset; });

    // --- Rendu ---
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
            fx.process (left + position, r, block, transport.bpm);
            if (right == nullptr)
                for (int n = 0; n < block; ++n)
                    left[position + n] = 0.5f * (left[position + n] + scratch[(size_t) n]);
            position += block;
        }
    };

    for (const auto& e : noteEvents)
    {
        renderUpTo (juce::jlimit (0, numSamples, e.offset));
        switch (e.kind)
        {
            case kindOn:     engine.noteOn (e.note, e.velocity); break;
            case kindOff:    engine.noteOff (e.note); break;
            case kindBend:   engine.setPitchWheel (e.velocity); break;
            case kindWheel:  engine.setModWheel (e.velocity); break;
            case kindAllOff: engine.allNotesOff (e.note == 1); break;
            default: break;
        }
    }
    renderUpTo (numSamples);

    // --- Pour l'interface ---
    float peak = 0.0f;
    int w = scopeWrite.load (std::memory_order_relaxed);
    for (int n = 0; n < numSamples; ++n)
    {
        const float m = right != nullptr ? 0.5f * (left[n] + right[n]) : left[n];
        peak = juce::jmax (peak, std::abs (left[n]), right != nullptr ? std::abs (right[n]) : 0.0f);
        scope[(size_t) w].store (m, std::memory_order_relaxed);
        w = (w + 1) % scopeSize;
    }
    scopeWrite.store (w, std::memory_order_relaxed);
    outputPeak.store (juce::jmax (peak, outputPeak.load (std::memory_order_relaxed) * 0.85f), std::memory_order_relaxed);

    playheadStep.store (engine.getPlayheadStep(), std::memory_order_relaxed);
    playheadBeats.store (engine.getPlayheadBeats(), std::memory_order_relaxed);
    activeVoices.store (engine.activeVoiceCount(), std::memory_order_relaxed);
    for (int d = 0; d < seqs::numDests; ++d)
        liveOffsets[(size_t) d].store (engine.getLiveOffset ((Dest) d), std::memory_order_relaxed);
}

void SequencesAudioProcessor::copyScope (std::array<float, scopeSize>& out) const
{
    const int w = scopeWrite.load (std::memory_order_relaxed);
    for (int n = 0; n < scopeSize; ++n)
        out[(size_t) n] = scope[(size_t) ((w + n) % scopeSize)].load (std::memory_order_relaxed);
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
    return raw.at (id)->load();
}

//==============================================================================
void SequencesAudioProcessor::applyFactory (int index)
{
    const auto& list = factoryPresets();
    const auto& preset = list[(size_t) juce::jlimit (0, (int) list.size() - 1, index)];

    for (auto* p : getParameters())
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->getDefaultValue());
        p->endChangeGesture();
    }
    for (const auto& [id, value] : preset.values)
    {
        jassert (apvts.getParameter (id) != nullptr);
        setParam (id, value);
    }
    for (size_t k = 0; k < preset.slots.size() && k < (size_t) seqs::numSlots; ++k)
    {
        setParam (slotId ((int) k, "Src"), (float) preset.slots[k].source);
        setParam (slotId ((int) k, "Dest"), (float) preset.slots[k].dest);
        setParam (slotId ((int) k, "Amt"), preset.slots[k].percent);
    }
    pendingFactoryTable = preset.table;
}

int SequencesAudioProcessor::matchingFactory() const
{
    const auto& list = factoryPresets();
    for (int i = 0; i < (int) list.size(); ++i)
    {
        std::map<juce::String, float> target;
        for (const auto& [id, value] : list[(size_t) i].values)
            target[id] = value;
        for (size_t k = 0; k < list[(size_t) i].slots.size(); ++k)
        {
            target[slotId ((int) k, "Src")] = (float) list[(size_t) i].slots[k].source;
            target[slotId ((int) k, "Dest")] = (float) list[(size_t) i].slots[k].dest;
            target[slotId ((int) k, "Amt")] = list[(size_t) i].slots[k].percent;
        }

        bool same = true;
        for (auto* p : getParameters())
        {
            auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p);
            if (ranged == nullptr)
                continue;
            const auto it = target.find (ranged->getParameterID());
            const float want = it != target.end() ? ranged->convertTo0to1 (it->second) : ranged->getDefaultValue();
            if (std::abs (ranged->getValue() - want) > 1.0e-4f)
            {
                same = false;
                break;
            }
        }
        if (same)
            return i;
    }
    return -1;
}

//==============================================================================
int SequencesAudioProcessor::getModulationSource (int slot) const  { return juce::roundToInt (realValue (slotId (slot, "Src"))); }
seqs::Dest SequencesAudioProcessor::getModulationDest (int slot) const
{
    return (Dest) juce::jlimit (0, seqs::numDests - 1, juce::roundToInt (realValue (slotId (slot, "Dest"))));
}
float SequencesAudioProcessor::getModulationAmount (int slot) const { return 0.01f * realValue (slotId (slot, "Amt")); }

int SequencesAudioProcessor::addModulation (int source, seqs::Dest dest)
{
    if (dest == Dest::off || source < 0 || source >= seqs::numModSources)
        return -1;

    for (int k = 0; k < seqs::numSlots; ++k)
        if (getModulationDest (k) == dest && getModulationSource (k) == source)
            return k;

    for (int k = 0; k < seqs::numSlots; ++k)
        if (getModulationDest (k) == Dest::off)
        {
            // Profondeur de départ : sensible mais pas brutale (une octave sur les hauteurs)
            const float amount = seqs::DestRange::isSemi (dest) ? 12.0f / 96.0f : 0.25f;
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
    if (dest != Dest::off)
        for (int k = 0; k < seqs::numSlots; ++k)
            if (getModulationDest (k) == dest)
                slots.push_back (k);
    return slots;
}

int SequencesAudioProcessor::usedSlotCount() const
{
    int n = 0;
    for (int k = 0; k < seqs::numSlots; ++k)
        n += getModulationDest (k) != Dest::off ? 1 : 0;
    return n;
}

//==============================================================================
SequencesAudioProcessor::TableInfo SequencesAudioProcessor::getTableInfo() const
{
    const juce::ScopedLock sl (tableLock);
    return tableInfo;
}

void SequencesAudioProcessor::installTable (std::unique_ptr<seqs::Wavetable> table, TableInfo info, std::vector<std::vector<float>> cycles)
{
    const juce::ScopedLock sl (tableLock);

    // Les tables remplacées restent en vie 2 s : le fil audio a pu en prendre une au début d'un bloc
    const auto now = juce::Time::getMillisecondCounter();
    retired.erase (std::remove_if (retired.begin(), retired.end(),
                                   [now] (const auto& r) { return now - r.first > 2000; }), retired.end());

    const seqs::Wavetable* live = table != nullptr ? table.get() : seqs::BuiltinTables::get (juce::jmax (0, info.builtin));
    info.numFrames = live->numFrames;

    if (ownedTable != nullptr)
        retired.emplace_back (now, std::move (ownedTable));

    ownedTable = std::move (table);
    tableInfo = std::move (info);
    tableCycles = std::move (cycles);
    liveTable.store (live);
}

void SequencesAudioProcessor::setBuiltinTable (int index)
{
    index = juce::jlimit (0, seqs::BuiltinTables::count() - 1, index);
    TableInfo info;
    info.builtin = index;
    info.name = seqs::BuiltinTables::names()[(size_t) index];
    installTable (nullptr, info, {});
}

juce::String SequencesAudioProcessor::setUserTable (std::vector<std::vector<float>> cycles, const juce::String& name, const juce::File& file)
{
    normaliseCycles (cycles);
    auto table = seqs::Wavetable::build (cycles, name.toStdString());
    if (table == nullptr)
        return "EMPTY WAVETABLE";

    TableInfo info;
    info.builtin = -1;
    info.name = name;
    info.file = file;
    installTable (std::move (table), info, std::move (cycles));
    return {};
}

juce::String SequencesAudioProcessor::loadTableFile (const juce::File& file)
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

    return setUserTable (std::move (parsed.cycles), file.getFileNameWithoutExtension().toUpperCase(), file);
}

std::unique_ptr<juce::XmlElement> SequencesAudioProcessor::tablesToXml() const
{
    const juce::ScopedLock sl (tableLock);
    auto xml = std::make_unique<juce::XmlElement> ("TABLES");
    auto* e = xml->createNewChildElement ("TABLE");
    e->setAttribute ("osc", 0);
    e->setAttribute ("name", tableInfo.name);
    e->setAttribute ("builtin", tableInfo.builtin);
    if (tableInfo.builtin < 0)
    {
        e->setAttribute ("file", tableInfo.file.getFullPathName());
        e->setAttribute ("data", encodeCycles (tableCycles));
    }
    return xml;
}

void SequencesAudioProcessor::tablesFromXml (const juce::XmlElement* xml)
{
    if (xml == nullptr && pendingFactoryTable >= 0)
    {
        setBuiltinTable (pendingFactoryTable);
        pendingFactoryTable = -1;
        return;
    }

    if (xml != nullptr)
        if (auto* e = xml->getChildByName ("TABLE"))
        {
            const int builtin = e->getIntAttribute ("builtin", 0);
            if (builtin >= 0)
            {
                // Par le nom d'abord (l'ordre des tables intégrées peut changer d'une version à l'autre)
                const int byName = seqs::BuiltinTables::indexOf (e->getStringAttribute ("name").toStdString());
                setBuiltinTable (byName >= 0 ? byName : builtin);
                return;
            }

            auto cycles = decodeCycles (e->getStringAttribute ("data"));
            if (! cycles.empty()
                && setUserTable (std::move (cycles), e->getStringAttribute ("name"), juce::File (e->getStringAttribute ("file"))).isEmpty())
                return;
        }

    setBuiltinTable (0);
}

//==============================================================================
void SequencesAudioProcessor::randomizePattern()
{
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

    // Rien n'écoute le séquenceur : on le branche sur la coupure (et on la baisse si elle est grande ouverte)
    bool listening = false;
    for (int k = 0; k < seqs::numSlots; ++k)
        listening = listening || (getModulationSource (k) == seqs::srcSeq && getModulationDest (k) != Dest::off
                                  && std::abs (getModulationAmount (k)) > 0.005f);
    if (! listening)
    {
        if (realValue ("cutoff") > 5000.0f)
        {
            setParam ("cutoff", 900.0f);
            setParam ("resonance", 40.0f);
        }
        const int slot = addModulation (seqs::srcSeq, Dest::cutoff);
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
