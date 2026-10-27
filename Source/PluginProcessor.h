#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PresetLibrary.h"
#include "SequencesEngine.h"

/**
    Sequences : synthé FM + analogique, polyphonique (8 voix) ou mono, avec deux séquenceurs de LFO.
    Les pas des séquenceurs sont des paramètres (automatisables, gardés dans les presets).
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

    /** Identifiant d'un paramètre de séquenceur : seqId (0, "on") -> "seqAOn". */
    static juce::String seqId (int seq, const juce::String& name)  { return juce::String (seq == 0 ? "seqA" : "seqB") + name; }
    static juce::String stepId (int seq, int step)                 { return seqId (seq, "Step" + juce::String (step + 1)); }
    static juce::String glideId (int seq, int step)                { return seqId (seq, "Glide" + juce::String (step + 1)); }
    static juce::String destId (int seq, int slot)                 { return seqId (seq, "Dest" + juce::String (slot + 1)); }
    static juce::String amountId (int seq, int slot)               { return seqId (seq, "Amt" + juce::String (slot + 1)); }

    static const juce::StringArray& destinationNames();
    static const juce::StringArray& rateNames();

    /** RANDOM : nouveau motif (valeurs + glides) pour un séquenceur. Fil de l'interface. */
    void randomizeSequence (int seq);
    /** Décale le motif d'un pas (dir = +1 vers la droite, -1 vers la gauche), dans la longueur choisie. */
    void rotateSequence (int seq, int dir);
    /** Met les pas d'un séquenceur à une même valeur (et retire les glides). */
    void clearSequence (int seq, float value);

    /** Pour l'interface : pas en cours (-1 = rien ne joue). */
    int getPlayheadStep (int seq) const noexcept         { return playhead[(size_t) juce::jlimit (0, 1, seq)].load (std::memory_order_relaxed); }

    /** Pour les tests : graine des tirages RANDOM. */
    void setRandomSeed (uint64_t seed)                   { randomRng.reseed (seed); }

    juce::AudioProcessorValueTreeState apvts;
    PresetLibrary presets;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void readSettings (seqs::Settings& s) const;
    void setParam (const juce::String& id, float realValue);

    seqs::Engine engine;
    seqs::Settings settings;
    double currentSampleRate = 48000.0;
    double lastBpm = 120.0;

    std::array<std::atomic<int>, seqs::numSeqs> playhead {};
    seqs::Rng randomRng { (uint64_t) juce::Time::getHighResolutionTicks() };

    // Accès rapide aux paramètres (fil audio)
    struct SeqParams
    {
        std::atomic<float>* on = nullptr;
        std::atomic<float>* steps = nullptr;
        std::atomic<float>* rate = nullptr;
        std::atomic<float>* feel = nullptr;
        std::atomic<float>* swing = nullptr;
        std::atomic<float>* smooth = nullptr;
        std::atomic<float>* sync = nullptr;
        std::array<std::atomic<float>*, seqs::numSlots> dest {}, amount {};
        std::array<std::atomic<float>*, seqs::maxSteps> value {}, glide {};
    };

    std::array<SeqParams, seqs::numSeqs> seqParams;
    std::map<juce::String, std::atomic<float>*> raw;
    std::atomic<float>* p (const char* id) const         { return raw.at (id); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SequencesAudioProcessor)
};
