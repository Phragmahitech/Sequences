#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

/**
    Effets de sortie de Sequences (sans JUCE, testable seul) : CHORUS -> DELAY -> REVERB.
*/
namespace seqs
{

struct FxSettings
{
    bool chorusOn = false;
    float chorusRate = 0.3f;      // Hz
    float chorusDepth = 0.5f;     // 0..1
    float chorusMix = 0.4f;

    bool delayOn = false;
    double delayBeats = 0.75;     // durée (noires)
    float delayFeedback = 0.35f;  // 0..0.95
    bool delayPingPong = true;
    float delayTone = 0.6f;       // 0 = sombre, 1 = clair
    float delayMix = 0.25f;

    bool reverbOn = false;
    float reverbSize = 0.6f;      // 0..1
    float reverbDecay = 0.5f;     // 0..1 -> 0,3 à 12 s
    float reverbDamping = 0.5f;
    float reverbPredelay = 0.1f;  // 0..1 -> 0 à 200 ms
    float reverbMix = 0.2f;
};

/** Ligne à retard circulaire avec lecture interpolée. */
class DelayLine
{
public:
    void setMaxSamples (int n)            { buffer.assign ((size_t) std::max (4, n + 4), 0.0f); pos = 0; }
    void clear()                          { std::fill (buffer.begin(), buffer.end(), 0.0f); }
    void push (float x)                   { pos = (pos + 1) % (int) buffer.size(); buffer[(size_t) pos] = x; }

    /** Lecture d'il y a d échantillons (fractionnaire, d >= 1). */
    float read (double d) const
    {
        const int n = (int) buffer.size();
        d = std::clamp (d, 1.0, (double) n - 3.0);
        const int i = (int) d;
        const float f = (float) (d - i);
        const float a = buffer[(size_t) ((pos - i + n) % n)];
        const float b = buffer[(size_t) ((pos - i - 1 + n) % n)];
        return a + (b - a) * f;
    }

    float readInt (int d) const
    {
        const int n = (int) buffer.size();
        return buffer[(size_t) ((pos - std::clamp (d, 0, n - 1) + n) % n)];
    }

private:
    std::vector<float> buffer;
    int pos = 0;
};

class Fx
{
public:
    void prepare (double sr)
    {
        sampleRate = sr > 0.0 ? sr : 48000.0;
        for (auto& c : chorusLines) c.setMaxSamples ((int) (0.05 * sampleRate));
        for (auto& d : delayLines) d.setMaxSamples ((int) (4.1 * sampleRate));
        predelay.setMaxSamples ((int) (0.25 * sampleRate));
        for (int i = 0; i < numLines; ++i)
            fdn[(size_t) i].setMaxSamples ((int) (0.25 * sampleRate * 1.6));
        for (int i = 0; i < numDiffusers; ++i)
            diffusers[(size_t) i].setMaxSamples ((int) (0.02 * sampleRate));
        reset();
    }

    void reset()
    {
        for (auto& c : chorusLines) c.clear();
        for (auto& d : delayLines) d.clear();
        predelay.clear();
        for (auto& l : fdn) l.clear();
        for (auto& d : diffusers) d.clear();
        chorusPhase = 0.0;
        delayTimeSmoothed = -1.0;
        toneState.fill (0.0f);
        hpState.fill (0.0f);
        dampState.fill (0.0f);
    }

    void setSettings (const FxSettings& s)  { settings = s; }

    void process (float* left, float* right, int numSamples, double bpm)
    {
        const auto& s = settings;
        if (! s.chorusOn && ! s.delayOn && ! s.reverbOn)
            return;

        // --- Réglages du bloc ---
        const double chorusInc = (double) s.chorusRate / sampleRate;
        const double chorusBase = 0.012 * sampleRate, chorusDepthSamples = 0.006 * sampleRate * s.chorusDepth;

        const double delayTarget = std::clamp (s.delayBeats * 60.0 / std::max (20.0, bpm), 0.005, 4.0) * sampleRate;
        if (delayTimeSmoothed < 0.0)
            delayTimeSmoothed = delayTarget;
        const float feedback = std::clamp (s.delayFeedback, 0.0f, 0.95f);
        const float toneCoef = (float) (1.0 - std::exp (-twoPiF * (500.0 + 15000.0 * s.delayTone * s.delayTone) / sampleRate));
        const float hpCoef = (float) std::exp (-twoPiF * 80.0 / sampleRate);

        // FDN : longueurs (nombres premiers), gains selon le temps de décroissance
        static constexpr std::array<int, numLines> primes { 1427, 1601, 1867, 2053, 2251, 2399, 2647, 2801 };
        const double scale = (0.35 + 1.25 * s.reverbSize) * sampleRate / 44100.0;
        const double rt60 = 0.3 * std::pow (40.0, (double) s.reverbDecay);
        std::array<int, numLines> lengths {};
        std::array<float, numLines> gains {};
        for (int i = 0; i < numLines; ++i)
        {
            lengths[(size_t) i] = std::max (8, (int) (primes[(size_t) i] * scale));
            gains[(size_t) i] = (float) std::pow (10.0, -3.0 * lengths[(size_t) i] / (rt60 * sampleRate));
        }
        const float damp = 0.05f + 0.9f * s.reverbDamping;
        const int predelaySamples = (int) (s.reverbPredelay * 0.2 * sampleRate);
        static constexpr std::array<int, numDiffusers> diffLengths { 142, 107, 379, 277 };

        for (int i = 0; i < numSamples; ++i)
        {
            float l = left[i], r = right[i];

            // CHORUS : deux voix modulées en quadrature
            if (s.chorusOn)
            {
                chorusLines[0].push (l);
                chorusLines[1].push (r);
                const double m0 = std::sin (twoPiF * chorusPhase), m1 = std::cos (twoPiF * chorusPhase);
                const float wl = chorusLines[0].read (chorusBase + chorusDepthSamples * m0);
                const float wr = chorusLines[1].read (chorusBase + chorusDepthSamples * m1);
                chorusPhase += chorusInc;
                chorusPhase -= std::floor (chorusPhase);
                l += (wl - l) * s.chorusMix;
                r += (wr - r) * s.chorusMix;
            }

            // DELAY : synchro au tempo, ping-pong, filtre dans la boucle
            if (s.delayOn)
            {
                delayTimeSmoothed += (delayTarget - delayTimeSmoothed) * 0.0005;
                float dl = delayLines[0].read (delayTimeSmoothed);
                float dr = delayLines[1].read (delayTimeSmoothed);

                // Couleur des répétitions : passe-bas + coupe le grave
                toneState[0] += (dl - toneState[0]) * toneCoef;
                toneState[1] += (dr - toneState[1]) * toneCoef;
                dl = toneState[0]; dr = toneState[1];
                hpState[0] = hpCoef * hpState[0] + (1.0f - hpCoef) * dl;
                hpState[1] = hpCoef * hpState[1] + (1.0f - hpCoef) * dr;
                dl -= hpState[0]; dr -= hpState[1];

                if (s.delayPingPong)
                {
                    delayLines[0].push (0.5f * (l + r) + feedback * dr);
                    delayLines[1].push (feedback * dl);
                }
                else
                {
                    delayLines[0].push (l + feedback * dl);
                    delayLines[1].push (r + feedback * dr);
                }

                l += dl * s.delayMix;
                r += dr * s.delayMix;
            }

            // REVERB : pré-délai, diffusion, FDN 8 lignes (matrice de Householder)
            if (s.reverbOn)
            {
                predelay.push (0.5f * (l + r));
                float x = predelay.readInt (predelaySamples);
                for (int d = 0; d < numDiffusers; ++d)
                {
                    const int len = (int) (diffLengths[(size_t) d] * sampleRate / 44100.0);
                    const float delayed = diffusers[(size_t) d].readInt (len);
                    const float v = x + 0.6f * delayed;
                    diffusers[(size_t) d].push (v);
                    x = delayed - 0.6f * v;
                }

                std::array<float, numLines> out {};
                float sum = 0.0f;
                for (int k = 0; k < numLines; ++k)
                {
                    float y = fdn[(size_t) k].readInt (lengths[(size_t) k]);
                    dampState[(size_t) k] += (y - dampState[(size_t) k]) * (1.0f - damp);
                    out[(size_t) k] = dampState[(size_t) k] * gains[(size_t) k];
                    sum += out[(size_t) k];
                }
                const float h = sum * (2.0f / numLines);
                for (int k = 0; k < numLines; ++k)
                    fdn[(size_t) k].push (out[(size_t) k] - h + (k % 2 == 0 ? x : -x) * 0.35f);

                const float wetL = 0.6f * (out[0] + out[2] + out[4] + out[6]);
                const float wetR = 0.6f * (out[1] + out[3] + out[5] + out[7]);
                l += (wetL - l) * s.reverbMix;
                r += (wetR - r) * s.reverbMix;
            }

            left[i] = std::isfinite (l) ? l : 0.0f;
            right[i] = std::isfinite (r) ? r : 0.0f;
        }
    }

private:
    static constexpr int numLines = 8;
    static constexpr int numDiffusers = 4;
    static constexpr double twoPiF = 6.283185307179586;

    double sampleRate = 48000.0;
    FxSettings settings;

    std::array<DelayLine, 2> chorusLines;
    double chorusPhase = 0.0;

    std::array<DelayLine, 2> delayLines;
    double delayTimeSmoothed = -1.0;
    std::array<float, 2> toneState {}, hpState {};

    DelayLine predelay;
    std::array<DelayLine, numDiffusers> diffusers;
    std::array<DelayLine, numLines> fdn;
    std::array<float, numLines> dampState {};
};

} // namespace seqs
