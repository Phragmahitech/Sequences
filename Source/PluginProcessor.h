#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PresetLibrary.h"
#include "SequencesEngine.h"
#include "Arp.h"
#include "Fx.h"

/**
    Sequences v0.4, sur le modèle d'Alaskan Bites 3 :
    WAVETABLE + PHASE DISTORTION + ALGORITHMIC (Plaits) + SUB/NOISE, matrice FM, BITE!, filtre LADDER / SVF,
    3 enveloppes, MAIN LFO, banque de 6 LFO + 2 CHAOS pilotée par le séquenceur, 24 routages,
    arpégiateur et effets (chorus, delay, reverb).
    Modulation à la Serum : on glisse une source sur un potard.
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
    double getTailLengthSeconds() const override         { return 8.0; }

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
    /** Case de la matrice FM : ligne (WT, PD, NOISE) -> colonne (vers WT, vers PD). */
    static juce::String fmId (int row, int col);

    static const juce::StringArray& destinationNames();   // OFF + une par destination
    static const juce::StringArray& sourceNames();        // OFF, LFO 1..6, CHAOS 1..2 (pas du séquenceur)
    static const juce::StringArray& modSourceNames();     // SEQ, LFO 1..6, CHAOS 1..2, ENV 2, ENV 3, VELO, MAIN LFO, MOD WHEEL
    static const juce::StringArray& lfoRateNames();
    static double lfoRateBeats (int index);
    static const juce::StringArray& plaitsModelNames();

    /** Paramètre lié à une destination (le potard qu'on module). */
    static juce::String paramForDest (seqs::Dest d);
    /** Destination d'un paramètre (Dest::off s'il n'est pas modulable). */
    static seqs::Dest destForParam (const juce::String& id);

    /** Lit tous les paramètres (aussi depuis l'interface, pour les aperçus). */
    void readSettings (seqs::Settings& s) const;
    void readArpSettings (seqs::ArpSettings& a) const;
    /** modulation : moteur dont on prend la modulation des effets (nullptr = potards seuls). */
    void readFxSettings (seqs::FxSettings& f, const seqs::Engine* modulation = nullptr) const;

    //==========================================================================
    // Modulation
    /** Ajoute (ou retrouve) le routage source -> dest. Renvoie le numéro du slot, -1 si les 24 sont pris. */
    int addModulation (int source, seqs::Dest dest);
    void removeModulation (int slot);
    void setModulationAmount (int slot, float amount);    // -1..1
    float getModulationAmount (int slot) const;
    int getModulationSource (int slot) const;
    seqs::Dest getModulationDest (int slot) const;
    std::vector<int> slotsForDest (seqs::Dest dest) const;
    int usedSlotCount() const;
    /** Modulation actuelle d'un potard (dernière voix jouée), en fraction de sa course. */
    float getLiveOffset (seqs::Dest d) const noexcept     { return liveOffsets[(size_t) d].load (std::memory_order_relaxed); }

    //==========================================================================
    // Wavetable
    struct TableInfo
    {
        juce::String name;
        int builtin = 0;          // index de table intégrée, -1 = table importée
        juce::File file;          // fichier d'origine d'une table importée (peut ne plus exister)
        int numFrames = 0;
    };

    TableInfo getTableInfo() const;
    const seqs::Wavetable* getTable() const               { return liveTable.load(); }
    void setBuiltinTable (int index);
    /** Importe un WAV (table Serum ou WAV ordinaire). Renvoie un message d'erreur, vide si tout va bien. */
    juce::String loadTableFile (const juce::File& file);
    juce::String setUserTable (std::vector<std::vector<float>> cycles, const juce::String& name, const juce::File& file);

    //==========================================================================
    void randomizePattern();
    void randomizeAll();
    void rotatePattern (int dir);
    void fillPattern (int source);

    int getPlayheadStep() const noexcept                 { return playheadStep.load (std::memory_order_relaxed); }
    double getPlayheadBeats() const noexcept             { return playheadBeats.load (std::memory_order_relaxed); }
    double getBpm() const noexcept                       { return bpmNow.load (std::memory_order_relaxed); }
    int getActiveVoices() const noexcept                 { return activeVoices.load (std::memory_order_relaxed); }
    float getOutputPeak() const noexcept                 { return outputPeak.load (std::memory_order_relaxed); }

    /** Les derniers échantillons joués (mono), pour l'oscilloscope. */
    static constexpr int scopeSize = 2048;
    void copyScope (std::array<float, scopeSize>& out) const;

    void setRandomSeed (uint64_t seed)                   { randomRng.reseed (seed); }

    /** Presets d'usine. */
    static int numFactoryPresets();
    static juce::String factoryPresetName (int index);

    juce::AudioProcessorValueTreeState apvts;
    PresetLibrary presets;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void setParam (const juce::String& id, float realValue);
    float realValue (const juce::String& id) const;

    void applyFactory (int index);
    int matchingFactory() const;

    std::unique_ptr<juce::XmlElement> tablesToXml() const;
    void tablesFromXml (const juce::XmlElement* xml);
    void installTable (std::unique_ptr<seqs::Wavetable> table, TableInfo info, std::vector<std::vector<float>> cycles);

    seqs::Engine engine;
    seqs::Settings settings;
    seqs::Arpeggiator arp;
    seqs::ArpSettings arpSettings;
    seqs::Fx fx;
    seqs::FxSettings fxSettings;
    double currentSampleRate = 48000.0;
    double lastBpm = 120.0;

    struct NoteEvent { int offset; int kind; int note; float velocity; };
    std::vector<NoteEvent> noteEvents;

    std::atomic<int> playheadStep { -1 };
    std::atomic<double> playheadBeats { -1.0 };
    std::atomic<double> bpmNow { 120.0 };
    std::atomic<int> activeVoices { 0 };
    std::atomic<float> outputPeak { 0.0f };
    std::array<std::atomic<float>, seqs::numDests> liveOffsets {};
    std::array<std::atomic<float>, scopeSize> scope {};
    std::atomic<int> scopeWrite { 0 };
    seqs::Rng randomRng { (uint64_t) juce::Time::getHighResolutionTicks() };
    int pendingFactoryTable = -1;

    // Table : celle que lit le fil audio, et celles qu'on garde en vie un moment après remplacement
    TableInfo tableInfo;
    std::vector<std::vector<float>> tableCycles;
    std::unique_ptr<seqs::Wavetable> ownedTable;
    std::atomic<const seqs::Wavetable*> liveTable { nullptr };
    std::vector<std::pair<juce::uint32, std::unique_ptr<seqs::Wavetable>>> retired;
    mutable juce::CriticalSection tableLock;

    std::map<juce::String, std::atomic<float>*> raw;

    /** Pointeurs vers les valeurs des paramètres, relevés une fois (lecture rapide à chaque bloc). */
    struct Cache
    {
        using P = std::atomic<float>*;
        struct Lfo { P rate, shape, amount, step, phase, polarity; };
        struct Chaos { P rate, amount, smooth; };
        struct Slot { P source, dest, amount; };

        std::array<juce::RangedAudioParameter*, seqs::numDests> dest {};
        P wtOn {}, pdOn {}, algoOn {}, wtWarpMode {}, wtOct {}, pdOct {}, algoOct {}, unison {}, wtPhase {}, wtRandPhase {};
        P pdWave {}, algoModel {}, algoLpg {}, subWave {}, subOct {}, noiseType {};
        std::array<std::array<P, seqs::numFmCols>, seqs::numFmRows> fm {};
        P fmMode {}, bitePost {}, biteAsym {};
        P filterModel {}, filterType {}, filterSlope {}, keyTrack {};
        P voiceMode {}, glide {}, bend {}, velocity {}, oversampling {};
        P ampA {}, ampD {}, ampS {}, ampR {}, fltA {}, fltD {}, fltS {}, fltR {}, modA {}, modD {}, modS {}, modR {};
        P mlSync {}, mlRate {}, mlHz {}, mlShape {}, mlRetrig {}, mlDest1 {}, mlDest2 {}, mlAmt1 {}, mlAmt2 {};
        P seqOn {}, seqSteps {}, seqDiv {}, seqFeel {}, seqSwing {}, seqSync {}, seqRestart {}, seqXfade {}, seqManual {};
        std::array<Lfo, seqs::numLfos> lfo {};
        std::array<Chaos, seqs::numChaos> chaos {};
        std::array<P, seqs::maxSteps> steps {};
        std::array<Slot, seqs::numSlots> slots {};
        P arpOn {}, arpMode {}, arpRate {}, arpOct {}, arpGate {}, arpSwing {}, arpLatch {};
        P chorusOn {}, delayOn {}, delaySync {}, delayTime {}, delayPingPong {}, reverbOn {};

        void build (SequencesAudioProcessor&);
    };

    Cache cache;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SequencesAudioProcessor)
};
