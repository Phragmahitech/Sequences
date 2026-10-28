#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PresetLibrary.h"
#include "SequencesEngine.h"

/**
    Sequences : synthé FM + analogique (hard sync), polyphonique (8 voix) ou mono,
    avec une banque de 6 LFO + 2 CHAOS et un séquenceur qui choisit la source à chaque pas.
*/
class SequencesAudioProcessor final : public juce::AudioProcessor
{
public:
    SequencesAudioProcessor();
    ~SequencesAudioProcessor() override = default;

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

    static const juce::StringArray& destinationNames();
    static const juce::StringArray& sourceNames();        // OFF, LFO 1..6, CHAOS 1..2
    static const juce::StringArray& slotSourceNames();    // SEQ, LFO 1..6, CHAOS 1..2
    static const juce::StringArray& lfoRateNames();
    static double lfoRateBeats (int index);

    /** Lit tous les paramètres (utilisable depuis l'interface pour l'aperçu). */
    void readSettings (seqs::Settings& s) const;

    /** RANDOM : nouveau motif de sources (parmi les sources dont la quantité n'est pas nulle). */
    void randomizePattern();
    /** RANDOM ALL : nouvelle banque de LFO / CHAOS + nouveau motif. */
    void randomizeAll();
    /** Décale le motif d'un pas (+1 vers la droite, -1 vers la gauche), dans la longueur choisie. */
    void rotatePattern (int dir);
    /** Tous les pas sur une même source. */
    void fillPattern (int source);

    /** Pour l'interface. */
    int getPlayheadStep() const noexcept                 { return playheadStep.load (std::memory_order_relaxed); }
    double getPlayheadBeats() const noexcept             { return playheadBeats.load (std::memory_order_relaxed); }
    double getBpm() const noexcept                       { return bpmNow.load (std::memory_order_relaxed); }

    /** Pour les tests : graine des tirages RANDOM. */
    void setRandomSeed (uint64_t seed)                   { randomRng.reseed (seed); }

    juce::AudioProcessorValueTreeState apvts;
    PresetLibrary presets;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void setParam (const juce::String& id, float realValue);
    float realValue (const juce::String& id) const;

    seqs::Engine engine;
    seqs::Settings settings;
    double currentSampleRate = 48000.0;
    double lastBpm = 120.0;

    std::atomic<int> playheadStep { -1 };
    std::atomic<double> playheadBeats { -1.0 };
    std::atomic<double> bpmNow { 120.0 };
    seqs::Rng randomRng { (uint64_t) juce::Time::getHighResolutionTicks() };

    std::map<juce::String, std::atomic<float>*> raw;
    std::atomic<float>* p (const juce::String& id) const   { return raw.at (id); }

    /** Pointeurs vers les valeurs des paramètres, relevés une fois (lecture rapide à chaque bloc). */
    struct Cache
    {
        using P = std::atomic<float>*;
        struct Lfo { P rate, shape, amount, step, phase, polarity; };
        struct Chaos { P rate, amount, smooth; };
        struct Slot { P source, dest, amount; };

        std::map<juce::String, P> named;
        P voiceMode {}, glide {}, velocity {}, transpose {}, level {};
        P fmLevel {}, fmRatio {}, fmIndex {}, fmFeedback {}, fmEnv {}, modWave {}, timbre {};
        P oscWave {}, oscLevel {}, oscOctave {}, oscDetune {}, oscPw {}, oscSync {}, noise {};
        P filterType {}, filterSlope {}, cutoff {}, resonance {}, filterEnv {}, keyTrack {};
        P ampAttack {}, ampDecay {}, ampSustain {}, ampRelease {}, modAttack {}, modDecay {}, modSustain {}, modRelease {};
        P driveType {}, drive {}, drivePos {};
        P seqOn {}, seqSteps {}, seqDiv {}, seqFeel {}, seqSwing {}, seqSync {}, seqRestart {}, seqXfade {}, seqManual {};
        std::array<Lfo, seqs::numLfos> lfo {};
        std::array<Chaos, seqs::numChaos> chaos {};
        std::array<P, seqs::maxSteps> steps {};
        std::array<Slot, seqs::numSlots> slots {};

        void build (const SequencesAudioProcessor&);
    };

    Cache cache;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SequencesAudioProcessor)
};
