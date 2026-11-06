#pragma once

#include <array>
#include <cmath>
#include <memory>

#include "plaits/dsp/voice.h"

/**
    L'oscillateur algorithmique : un Plaits complet (Mutable Instruments, licence MIT, code d'origine
    dans ThirdParty/plaits) par voix de Sequences.

    Plaits travaille par blocs de 12 échantillons ; on rend un bloc d'avance et on le lit échantillon par
    échantillon. Plaits suppose la fréquence de son matériel (47 872 Hz) : la hauteur est compensée selon
    la vraie fréquence d'échantillonnage.
*/
namespace seqs
{

struct PlaitsModels
{
    static constexpr int count = 24;

    static const char* name (int i)
    {
        // Ordre du firmware 1.2 de Plaits (banque verte, puis rouge, puis la troisième)
        static const char* names[count] = {
            "VA + VCF", "PHASE DISTORTION", "6-OP FM I", "6-OP FM II", "6-OP FM III", "WAVE TERRAIN",
            "STRING MACHINE", "CHIPTUNE",
            "VIRTUAL ANALOG", "WAVESHAPING", "FM", "GRAIN / FORMANT", "HARMONIC", "WAVETABLE", "CHORDS", "SPEECH",
            "SWARM", "FILTERED NOISE", "PARTICLE NOISE", "INHARMONIC STRING", "MODAL RESONATOR",
            "BASS DRUM", "SNARE DRUM", "HI-HAT" };
        return names[i < 0 ? 0 : (i >= count ? count - 1 : i)];
    }

    /** Modèles qui ne sonnent que sur un déclenchement (leur enveloppe est intégrée). */
    static bool needsTrigger (int i)    { return (i >= 2 && i <= 4) || i >= 19; }
};

class PlaitsOsc
{
public:
    PlaitsOsc()
    {
        allocator.Init (buffer.data(), buffer.size());
        voice = std::make_unique<plaits::Voice>();
        voice->Init (&allocator);
    }

    void setSampleRate (double sr)
    {
        // Note 69 -> 440 Hz à n'importe quelle fréquence d'échantillonnage
        noteOffset = 24.0f + 12.0f * (float) std::log2 (plaits::kCorrectedSampleRate / sr);
    }

    void reset()
    {
        position = blockSize;
        gate = false;
        retriggerBlocks = 0;
    }

    /** Début de note : le déclenchement repasse à 0 pendant un bloc puis remonte (legato compris). */
    void noteOn()      { gate = true; retriggerBlocks = 1; }
    void noteOff()     { gate = false; }

    struct Params
    {
        int model = 8;
        float note = 60.0f;           // demi-tons MIDI (fractionnaires)
        float harmonics = 0.5f, timbre = 0.5f, morph = 0.5f;
        float decay = 0.5f, colour = 0.5f;
        float aux = 0.0f;             // 0 = sortie OUT, 1 = sortie AUX
        bool lpg = false;
    };

    /** Un échantillon (le paramétrage est relu à chaque nouveau bloc de 12). */
    float process (const Params& p)
    {
        if (position >= blockSize)
            renderBlock (p);
        return out[(size_t) position++];
    }

private:
    static constexpr int blockSize = (int) plaits::kBlockSize;

    void renderBlock (const Params& p)
    {
        plaits::Patch patch {};
        patch.note = p.note + noteOffset;
        patch.harmonics = clamp01 (p.harmonics);
        patch.timbre = clamp01 (p.timbre);
        patch.morph = clamp01 (p.morph);
        patch.decay = clamp01 (p.decay);
        patch.lpg_colour = clamp01 (p.colour);
        patch.engine = p.model < 0 ? 0 : (p.model >= PlaitsModels::count ? PlaitsModels::count - 1 : p.model);

        plaits::Modulations m {};
        m.trigger_patched = p.lpg || PlaitsModels::needsTrigger (patch.engine);
        if (retriggerBlocks > 0)
        {
            m.trigger = 0.0f;
            --retriggerBlocks;
        }
        else
            m.trigger = gate ? 1.0f : 0.0f;

        plaits::Voice::Frame frames[blockSize];
        voice->Render (patch, m, frames, (size_t) blockSize);

        // Plaits inverse le signe en sortie (convertisseur du module) : on le remet à l'endroit
        const float aux = clamp01 (p.aux);
        for (int i = 0; i < blockSize; ++i)
            out[(size_t) i] = -((1.0f - aux) * (float) frames[i].out + aux * (float) frames[i].aux) / 32768.0f;
        position = 0;
    }

    static float clamp01 (float v)     { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

    alignas (16) std::array<char, 16384> buffer {};
    stmlib::BufferAllocator allocator;
    std::unique_ptr<plaits::Voice> voice;
    std::array<float, blockSize> out {};
    int position = blockSize;
    bool gate = false;
    int retriggerBlocks = 0;
    float noteOffset = 24.0f;
};

} // namespace seqs
