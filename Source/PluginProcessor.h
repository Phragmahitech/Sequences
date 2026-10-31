#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PresetLibrary.h"
#include "SequencesEngine.h"

/**
    Sequences : 2 oscillateurs wavetable (tables intégrées, ou WAV / tables Serum importées) avec warp
    SYNC / FM / AM / RM, filtre, distorsion, polyphonique (8 voix) ou mono.
    Modulation à la Serum : on glisse une source (LFO 1–6, CHAOS 1–2, SEQ, ENV 2, VELO) sur un potard.
*/
class SequencesAudioProcessor final : public juce::AudioProcessor
{
public:
    SequencesAudioProcessor();
    ~SequencesAudioProcessor() override;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                      { return true; }

    const juce::String getName() const override          { return "Sequences"; }
    bool acceptsMidi() const override                    { return true; }
    bool producesMidi() const override                   { return false; }
    bool isMidiEffect() const override                   { return false; }
    double getTailLengthSeconds() const override         { return 5.0; }

    int getNumPrograms() override                        { return 1; }
    int getCurrentProgram() override                     { return 0; }
    void setCurrentProgram (int) override                {}
    const juce::String getProgramName (int) override     { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    static constexpr int numSteps = seqs::maxSteps;

    static juce::String lfoId (int lfo, const char* name)     { return "lfo" + juce::String (lfo + 1) + name; }
    static juce::String chaosId (int c, const char* name)     { return "chaos" + juce::String (c + 1) + name; }
    static juce::String stepId (int step)                     { return "seqStep" + juce::String (step + 1); }
    static juce::String slotId (int slot, const char* name)   { return "mod" + juce::String (slot + 1) + name; }
    static juce::String oscId (int osc, const char* name)     { return juce::String (osc == 0 ? "a" : "b") + name; }

    static const juce::StringArray& destinationNames();   // OFF + une par destination
    static const juce::StringArray& sourceNames();        // OFF, LFO 1..6, CHAOS 1..2 (pas du séquenceur)
    static const juce::StringArray& modSourceNames();     // SEQ, LFO 1..6, CHAOS 1..2, ENV 2, VELO
    static const juce::StringArray& lfoRateNames();
    static double lfoRateBeats (int index);

    /** Paramètre lié à une destination (le potard qu'on module). */
    static juce::String paramForDest (seqs::Dest d);
    /** Destination d'un paramètre (Dest::off s'il n'est pas modulable). */
    static seqs::Dest destForParam (const juce::String& id);

    /** Lit tous les paramètres (aussi depuis l'interface, pour les aperçus). */
    void readSettings (seqs::Settings& s) const;

    //==========================================================================
    // Modulation
    /** Ajoute (ou retrouve) le routage source -> dest. Renvoie le numéro du slot, -1 si les 16 sont pris. */
    int addModulation (int source, seqs::Dest dest);
    void removeModulation (int slot);
    void setModulationAmount (int slot, float amount);    // -1..1
    float getModulationAmount (int slot) const;
    int getModulationSource (int slot) const;
    seqs::Dest getModulationDest (int slot) const;
    /** Slots qui visent une destination. */
    std::vector<int> slotsForDest (seqs::Dest dest) const;
    int usedSlotCount() const;
    /** Modulation actuelle d'un potard (dernière voix jouée), en fraction de sa course. */
    float getLiveOffset (seqs::Dest d) const noexcept     { return liveOffsets[(size_t) d].load (std::memory_order_relaxed); }

    //==========================================================================
    // Wavetables
    struct TableInfo
    {
        juce::String name;
        int builtin = 0;          // index de table intégrée, -1 = table importée
        juce::File file;          // fichier d'origine d'une table importée (peut ne plus exister)
        int numFrames = 0;
    };

    TableInfo getTableInfo (int osc) const;
    const seqs::Wavetable* getTable (int osc) const       { return liveTables[(size_t) juce::jlimit (0, 1, osc)].load(); }
    void setBuiltinTable (int osc, int index);
    /** Importe un WAV (table Serum ou WAV ordinaire). Renvoie un message d'erreur, vide si tout va bien. */
    juce::String loadTableFile (int osc, const juce::File& file);
    /** Table depuis des cycles déjà lus. */
    juce::String setUserTable (int osc, std::vector<std::vector<float>> cycles, const juce::String& name, const juce::File& file);

    //==========================================================================
    void randomizePattern();
    void randomizeAll();
    void rotatePattern (int dir);
    void fillPattern (int source);

    int getPlayheadStep() const noexcept                 { return playheadStep.load (std::memory_order_relaxed); }
    double getPlayheadBeats() const noexcept             { return playheadBeats.load (std::memory_order_relaxed); }
    double getBpm() const noexcept                       { return bpmNow.load (std::memory_order_relaxed); }

    void setRandomSeed (uint64_t seed)                   { randomRng.reseed (seed); }

    juce::AudioProcessorValueTreeState apvts;
    PresetLibrary presets;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void setParam (const juce::String& id, float realValue);
    float realValue (const juce::String& id) const;

    // Tables : écriture / lecture dans l'état du plugin et les presets
    std::unique_ptr<juce::XmlElement> tablesToXml() const;
    void tablesFromXml (const juce::XmlElement* xml);
    void installTable (int osc, std::unique_ptr<seqs::Wavetable> table, TableInfo info, std::vector<std::vector<float>> cycles);

    seqs::Engine engine;
    seqs::Settings settings;
    double currentSampleRate = 48000.0;
    double lastBpm = 120.0;

    std::atomic<int> playheadStep { -1 };
    std::atomic<double> playheadBeats { -1.0 };
    std::atomic<double> bpmNow { 120.0 };
    std::array<std::atomic<float>, seqs::numDests> liveOffsets {};
    seqs::Rng randomRng { (uint64_t) juce::Time::getHighResolutionTicks() };

    // Tables : celle que lit le fil audio, et celles qu'on garde en vie un moment après remplacement
    struct OscTable
    {
        TableInfo info;
        std::vector<std::vector<float>> cycles;      // cycles d'origine d'une table importée (pour la sauvegarde)
        std::unique_ptr<seqs::Wavetable> owned;
    };
    std::array<OscTable, seqs::numOscs> tables;
    std::array<std::atomic<const seqs::Wavetable*>, seqs::numOscs> liveTables {};
    std::vector<std::pair<juce::uint32, std::unique_ptr<seqs::Wavetable>>> retired;
    mutable juce::CriticalSection tableLock;

    std::map<juce::String, std::atomic<float>*> raw;
    std::atomic<float>* p (const juce::String& id) const   { return raw.at (id); }

    /** Pointeurs vers les valeurs des paramètres, relevés une fois (lecture rapide à chaque bloc). */
    struct Cache
    {
        using P = std::atomic<float>*;
        struct Lfo { P rate, shape, amount, step, phase, polarity; };
        struct Chaos { P rate, amount, smooth; };
        struct Slot { P source, dest, amount; };
        struct Osc { P octave, warp; };

        std::array<juce::RangedAudioParameter*, seqs::numDests> dest {};
        std::array<Osc, seqs::numOscs> osc {};
        P voiceMode {}, glide {}, velocity {};
        P filterType {}, filterSlope {}, keyTrack {};
        P ampAttack {}, ampDecay {}, ampSustain {}, ampRelease {}, modAttack {}, modDecay {}, modSustain {}, modRelease {};
        P driveType {}, drivePos {};
        P seqOn {}, seqSteps {}, seqDiv {}, seqFeel {}, seqSwing {}, seqSync {}, seqRestart {}, seqXfade {}, seqManual {};
        std::array<Lfo, seqs::numLfos> lfo {};
        std::array<Chaos, seqs::numChaos> chaos {};
        std::array<P, seqs::maxSteps> steps {};
        std::array<Slot, seqs::numSlots> slots {};

        void build (SequencesAudioProcessor&);
    };

    Cache cache;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SequencesAudioProcessor)
};
