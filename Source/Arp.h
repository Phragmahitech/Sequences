#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <vector>

/**
    Arpégiateur de Sequences (sans JUCE, testable seul).
    Reçoit les notes jouées, renvoie les notes à jouer, calées sur le tempo (position du morceau
    quand l'hôte joue, horloge libre partie de la première note sinon).
*/
namespace seqs
{

enum class ArpMode : int { up = 0, down, upDown, downUp, random, order, chord, count };

struct ArpSettings
{
    bool on = false;
    ArpMode mode = ArpMode::up;
    double stepBeats = 0.25;      // 1/16
    int octaves = 1;              // 1..4
    float gate = 0.5f;            // durée de la note, en fraction du pas
    double swing = 0.0;           // 0..0.75
    bool latch = false;
};

class Arpeggiator
{
public:
    /** type : true = note on, false = note off. */
    using Emit = std::function<void (bool on, int note, float velocity, int sampleOffset)>;

    void reset()
    {
        held.clear();
        latched.clear();
        playing.clear();
        stepIndex = -1;
        orderPos = 0;
        freeBeats = 0.0;
        clockRunning = false;
        upDownDir = 1;
    }

    void setSettings (const ArpSettings& s)
    {
        if (s.on != settings.on || s.latch != settings.latch)
        {
            settings = s;
            pendingFlush = true;
        }
        settings = s;
    }

    const ArpSettings& getSettings() const noexcept      { return settings; }

    void noteOn (int note, float velocity)
    {
        // Latch : une nouvelle frappe, toutes touches relâchées, remplace l'accord retenu
        if (settings.latch && heldCount() == 0)
            latched.clear();

        removeFrom (held, note);
        held.push_back ({ note, velocity });
        if (settings.latch)
        {
            removeFrom (latched, note);
            latched.push_back ({ note, velocity });
        }
        if (! clockRunning)
        {
            clockRunning = true;
            freeBeats = 0.0;
            stepIndex = -1;
            orderPos = 0;
            upDownDir = 1;
        }
    }

    void noteOff (int note)
    {
        removeFrom (held, note);
    }

    /** Notes retenues pour l'arpège (touches tenues, ou accord verrouillé). */
    const std::vector<std::pair<int, float>>& source() const   { return settings.latch ? latched : held; }

    /** Avance de numSamples ; émet les notes (positions dans le bloc). */
    void process (int numSamples, double sampleRate, double bpm, bool hostPlaying, double ppq, const Emit& emit)
    {
        if (pendingFlush)
        {
            pendingFlush = false;
            for (auto& p : playing)
                emit (false, p.note, 0.0f, 0);
            playing.clear();
            if (! settings.latch)
                latched.clear();
            stepIndex = -1;
        }

        if (! settings.on)
            return;

        const double beatsPerSample = bpm / 60.0 / sampleRate;
        const double stepBeats = std::max (settings.stepBeats, 1.0e-3);

        for (int i = 0; i < numSamples; ++i)
        {
            // Fin des notes dont le temps est écoulé
            for (auto it = playing.begin(); it != playing.end();)
            {
                if (--it->remaining <= 0)
                {
                    emit (false, it->note, 0.0f, i);
                    it = playing.erase (it);
                }
                else
                    ++it;
            }

            const auto& notes = source();
            if (notes.empty())
            {
                if (! playing.empty())
                {
                    for (auto& p : playing)
                        emit (false, p.note, 0.0f, i);
                    playing.clear();
                }
                clockRunning = false;
                stepIndex = -1;
                continue;
            }

            const double beats = hostPlaying ? std::max (0.0, ppq + i * beatsPerSample) : freeBeats;
            freeBeats += beatsPerSample;

            // Pas courant (swing : le pas pair dure plus longtemps)
            const double pair = std::floor (beats / (2.0 * stepBeats));
            const double inPair = beats - pair * 2.0 * stepBeats;
            const double first = (1.0 + std::clamp (settings.swing, 0.0, 0.75)) * stepBeats;
            const long long index = (long long) pair * 2 + (inPair < first ? 0 : 1);
            const double length = inPair < first ? first : 2.0 * stepBeats - first;

            if (index != stepIndex)
            {
                stepIndex = index;
                for (auto& p : playing)
                    emit (false, p.note, 0.0f, i);
                playing.clear();

                const int gateSamples = std::max (1, (int) std::lround (length / beatsPerSample * std::clamp ((double) settings.gate, 0.02, 1.0)));
                for (auto [note, velocity] : nextNotes (notes))
                {
                    if (note < 0 || note > 127)
                        continue;
                    emit (true, note, velocity, i);
                    playing.push_back ({ note, gateSamples });
                }
            }
        }
    }

    int playingCount() const noexcept   { return (int) playing.size(); }

private:
    struct Playing { int note; int remaining; };

    int heldCount() const               { return (int) held.size(); }

    static void removeFrom (std::vector<std::pair<int, float>>& v, int note)
    {
        v.erase (std::remove_if (v.begin(), v.end(), [note] (const auto& p) { return p.first == note; }), v.end());
    }

    std::vector<std::pair<int, float>> nextNotes (const std::vector<std::pair<int, float>>& notes)
    {
        const int octaves = std::clamp (settings.octaves, 1, 4);

        if (settings.mode == ArpMode::chord)
        {
            const int octave = (int) ((stepIndex < 0 ? 0 : stepIndex) % octaves);
            std::vector<std::pair<int, float>> chord;
            for (auto [n, v] : notes)
                chord.push_back ({ n + 12 * octave, v });
            return chord;
        }

        // Suite des notes (sur toutes les octaves)
        std::vector<std::pair<int, float>> base = notes;
        if (settings.mode != ArpMode::order)
            std::sort (base.begin(), base.end(), [] (const auto& a, const auto& b) { return a.first < b.first; });

        std::vector<std::pair<int, float>> run;
        for (int o = 0; o < octaves; ++o)
            for (auto [n, v] : base)
                run.push_back ({ n + 12 * o, v });

        const int count = (int) run.size();
        if (count == 0)
            return {};

        int index = 0;
        switch (settings.mode)
        {
            case ArpMode::down:
                index = count - 1 - (orderPos % count);
                ++orderPos;
                break;
            case ArpMode::upDown: case ArpMode::downUp:
            {
                if (count == 1) { index = 0; break; }
                const int period = 2 * (count - 1);
                int k = orderPos % period;
                ++orderPos;
                index = k < count ? k : period - k;
                if (settings.mode == ArpMode::downUp)
                    index = count - 1 - index;
                break;
            }
            case ArpMode::random:
                rng = rng * 6364136223846793005ull + 1442695040888963407ull;
                index = (int) ((rng >> 33) % (uint64_t) count);
                break;
            case ArpMode::up: case ArpMode::order: case ArpMode::chord: case ArpMode::count:
            default:
                index = orderPos % count;
                ++orderPos;
                break;
        }
        return { run[(size_t) index] };
    }

    ArpSettings settings;
    std::vector<std::pair<int, float>> held, latched;
    std::vector<Playing> playing;
    long long stepIndex = -1;
    int orderPos = 0;
    int upDownDir = 1;
    double freeBeats = 0.0;
    bool clockRunning = false;
    bool pendingFlush = false;
    uint64_t rng = 0x853C49E6748FEA9Bull;
};

} // namespace seqs
