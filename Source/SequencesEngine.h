#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>

#include "Wavetable.h"
#include "PlaitsOsc.h"

/**
    Moteur de Sequences v0.4 (sans JUCE, testable seul), sur le modèle d'Alaskan Bites 3.

    Une voix =
      WAVETABLE (unison jusqu'à 8, detune, spread, warp SYNC / BEND+ / BEND- / MIRROR)
    + PHASE DISTORTION (formes Casio CZ, PD AMT, SHAPE, CRUSH, FOLD)
    + ALGORITHMIC OSC (Plaits de Mutable Instruments, 24 modèles, LPG)
    + SUB + NOISE
    -> matrice FM (WT, PD, NOISE modulent WT et PD ; diagonale = feedback ; mode PHASE ou LINEAR)
    -> BITE! (fold, crush, soft, asymétrie ; avant ou après le filtre)
    -> FILTRE (LADDER ou SVF ; LP / BP / HP / NOTCH ; 12 ou 24 dB)
    -> volume (ENV 1), panoramique. Tout jusqu'au filtre compris tourne suréchantillonné (1x, 2x ou 4x).

    Modulation : ENV 2 (filtre) et ENV 3, MAIN LFO (2 destinations fixes), banque de 6 LFO + 2 CHAOS,
    séquenceur qui choisit la source à chaque pas (SEQ), vélocité, molette. 24 routages source ->
    destination ; une modulation s'ajoute à la position du potard (0..1), comme dans Serum.
*/
namespace seqs
{

constexpr int maxSteps = 32;
constexpr int maxVoices = 16;
constexpr int maxUnison = 8;
constexpr int numLfos = 6;
constexpr int numChaos = 2;
constexpr int numSources = numLfos + numChaos;   // sources du séquenceur : 1..6 = LFO, 7..8 = CHAOS (0 = OFF)
constexpr int numSlots = 24;
constexpr int numFmRows = 4;                      // WT, PD, ALGO, NOISE
constexpr int numFmCols = 3;                      // vers WT, vers PD, vers ALGO
constexpr double algoFmSemitones = 48.0;          // FM de l'ALGO (par la hauteur) : écart à 100 %
constexpr double twoPi = 6.283185307179586;
constexpr double pi = 3.141592653589793;

/** Sources d'un routage. */
enum ModSource : int
{
    srcSeq = 0, srcLfo1 = 1, srcChaos1 = 7, srcEnv2 = 9, srcEnv3 = 10, srcVelocity = 11, srcMainLfo = 12, srcModWheel = 13,
    numModSources = 14
};

/** Destinations : chacune correspond à un potard de l'interface. */
enum class Dest : int
{
    off = 0,
    wtPos, wtWarp, wtSemi, wtFine, wtLevel, wtDetune, wtSpread, wtPan,
    pdAmount, pdShape, pdCrush, pdFold, pdLevel, pdSemi, pdFine,
    algoHarmonics, algoTimbre, algoMorph, algoDecay, algoColour, algoAux, algoLevel, algoSemi, algoFine, algoPan,
    subLevel, noiseLevel, fmDepth,
    biteFold, biteCrush, biteSoft,
    cutoff, resonance, filterDrive, filterEnv,
    volume,
    chorusRate, chorusDepth, chorusMix,
    delayTime, delayFeedback, delayTone, delayMix,
    reverbSize, reverbDecay, reverbDamping, reverbPredelay, reverbMix,
    count
};
constexpr int numDests = (int) Dest::count;

enum class WtWarp : int { off = 0, sync, bendPlus, bendMinus, mirror, count };
enum class PdWave : int { saw = 0, square, pulse, dblSine, sawPulse, reso1, reso2, reso3, count };
enum class SubWave : int { sine = 0, triangle, square, saw, count };
enum class NoiseType : int { white = 0, pink, brown, digital, count };
enum class FmMode : int { phase = 0, linear, count };
enum class FilterModel : int { ladder = 0, svf, count };
enum class FilterType : int { lowpass = 0, bandpass, highpass, notch, count };
enum class LfoShape : int { sine = 0, triangle, saw, ramp, square, pulse, expDown, expUp, count };
enum class MainLfoShape : int { sine = 0, triangle, saw, ramp, square, sampleHold, smoothRandom, count };
enum class VoiceMode : int { poly = 0, mono, legato, count };

/** Position du potard (0..1) -> valeur réelle, pour chaque destination. */
struct DestRange
{
    static bool isSemi (Dest d)    { return d == Dest::wtSemi || d == Dest::pdSemi || d == Dest::algoSemi; }
    static bool isFine (Dest d)    { return d == Dest::wtFine || d == Dest::pdFine || d == Dest::algoFine; }
    static bool isPan (Dest d)     { return d == Dest::wtPan || d == Dest::algoPan; }

    static float toReal (Dest d, float n)
    {
        if (isSemi (d))                return -48.0f + 96.0f * n;            // demi-tons (continu)
        if (isFine (d))                return -100.0f + 200.0f * n;          // centièmes
        if (isPan (d))                 return -1.0f + 2.0f * n;
        if (d == Dest::filterEnv)      return -1.0f + 2.0f * n;
        if (d == Dest::volume)         { const float g = n / 0.8f; return g * g; }   // gain, 0.8 = 0 dB
        if (d == Dest::cutoff)         return 20.0f * std::pow (1000.0f, n);  // Hz
        if (d == Dest::chorusRate)     return 0.05f * std::pow (100.0f, n);   // Hz
        if (d == Dest::delayTime)      return 5.0f * std::pow (400.0f, n);    // ms (mode FREE)
        return n;
    }

    static float toNorm (Dest d, float v)
    {
        if (isSemi (d))                return (v + 48.0f) / 96.0f;
        if (isFine (d))                return (v + 100.0f) / 200.0f;
        if (isPan (d) || d == Dest::filterEnv) return (v + 1.0f) * 0.5f;
        if (d == Dest::volume)         return 0.8f * std::sqrt (std::max (0.0f, v));
        if (d == Dest::cutoff)         return std::log (std::max (20.0f, v) / 20.0f) / std::log (1000.0f);
        if (d == Dest::chorusRate)     return std::log (std::max (0.05f, v) / 0.05f) / std::log (100.0f);
        if (d == Dest::delayTime)      return std::log (std::max (5.0f, v) / 5.0f) / std::log (400.0f);
        return v;
    }

    /** Hauteurs : lissage plus long, pour que SEMI glisse quand on le tourne. */
    static bool isPitch (Dest d)   { return isSemi (d); }
};

struct LfoSettings
{
    double periodBeats = 1.0;
    LfoShape shape = LfoShape::sine;
    float amount = 1.0f;
    int steps = 0;
    float phase = 0.0f;
    bool unipolar = false;
};

struct ChaosSettings
{
    double periodBeats = 0.25;
    float amount = 1.0f;
    float smooth = 0.0f;
};

struct SeqSettings
{
    bool on = true;
    int steps = 16;
    double stepBeats = 0.5;
    double swing = 0.0;
    bool hostSync = false;
    bool restart = true;
    float xfadeMs = 2.0f;
    int manual = 1;
    std::array<int, maxSteps> pattern {};
};

struct MainLfoSettings
{
    bool sync = true;
    double periodBeats = 1.0;      // si sync
    float rateHz = 1.0f;           // sinon
    MainLfoShape shape = MainLfoShape::sine;
    bool retrig = false;
    std::array<Dest, 2> dest { Dest::off, Dest::off };
    std::array<float, 2> amount {};
};

struct Settings
{
    /** Position (0..1) de chaque potard modulable. */
    std::array<float, numDests> base {};

    // Oscillateurs
    bool wtOn = true, pdOn = true, algoOn = true;
    const Wavetable* table = nullptr;     // nullptr = table intégrée BASIC SHAPES
    WtWarp wtWarp = WtWarp::off;
    int wtOctave = 0, pdOctave = 0, algoOctave = 0;
    int unison = 1;
    float wtPhase = 0.0f;                 // 0..1
    bool wtRandomPhase = false;
    PdWave pdWave = PdWave::saw;
    int algoModel = 8;                    // Plaits : 8 = VIRTUAL ANALOG
    bool algoLpg = false;
    SubWave subWave = SubWave::sine;
    int subOctave = -1;
    NoiseType noiseType = NoiseType::white;

    // Matrice FM : [ligne = WT, PD, ALGO, NOISE][colonne = vers WT, vers PD, vers ALGO], 0..1
    std::array<std::array<float, numFmCols>, numFmRows> fm {};
    FmMode fmMode = FmMode::phase;

    // Bite
    bool bitePost = false, biteAsym = false;

    // Filtre
    FilterModel filterModel = FilterModel::ladder;
    FilterType filterType = FilterType::lowpass;
    bool slope24 = false;
    float keyTrack = 0.0f;

    // Voix
    VoiceMode voiceMode = VoiceMode::poly;
    float glideMs = 0.0f;
    float bendRange = 2.0f;
    float velocity = 0.5f;
    int oversampling = 2;                 // 1, 2 ou 4

    // Enveloppes (secondes, sustain 0..1)
    float ampA = 0.005f, ampD = 0.3f, ampS = 0.7f, ampR = 0.25f;
    float fltA = 0.005f, fltD = 0.4f, fltS = 0.3f, fltR = 0.3f;
    float modA = 0.005f, modD = 0.1f, modS = 0.0f, modR = 0.2f;

    // Modulation
    MainLfoSettings mainLfo {};
    std::array<LfoSettings, numLfos> lfo {};
    std::array<ChaosSettings, numChaos> chaos {};
    SeqSettings seq {};
    std::array<int, numSlots> slotSource {};
    std::array<Dest, numSlots> slotDest {};
    std::array<float, numSlots> slotAmount {};

    Settings()
    {
        slotDest.fill (Dest::off);
        auto set = [this] (Dest d, float real) { base[(size_t) d] = DestRange::toNorm (d, real); };
        set (Dest::wtPos, 0.0f);   set (Dest::wtWarp, 0.0f);  set (Dest::wtSemi, 0.0f); set (Dest::wtFine, 0.0f);
        set (Dest::wtLevel, 0.8f); set (Dest::wtDetune, 0.2f); set (Dest::wtSpread, 0.6f); set (Dest::wtPan, 0.0f);
        set (Dest::pdAmount, 0.5f); set (Dest::pdShape, 0.0f); set (Dest::pdCrush, 0.0f); set (Dest::pdFold, 0.0f);
        set (Dest::pdLevel, 0.0f); set (Dest::pdSemi, 0.0f);  set (Dest::pdFine, 0.0f);
        set (Dest::algoHarmonics, 0.5f); set (Dest::algoTimbre, 0.5f); set (Dest::algoMorph, 0.5f);
        set (Dest::algoDecay, 0.5f); set (Dest::algoColour, 0.5f); set (Dest::algoAux, 0.0f);
        set (Dest::algoLevel, 0.0f); set (Dest::algoSemi, 0.0f); set (Dest::algoFine, 0.0f); set (Dest::algoPan, 0.0f);
        set (Dest::subLevel, 0.0f); set (Dest::noiseLevel, 0.0f); set (Dest::fmDepth, 0.0f);
        set (Dest::biteFold, 0.0f); set (Dest::biteCrush, 0.0f); set (Dest::biteSoft, 0.0f);
        set (Dest::cutoff, 20000.0f); set (Dest::resonance, 0.1f); set (Dest::filterDrive, 0.0f); set (Dest::filterEnv, 0.0f);
        set (Dest::volume, 1.0f);
        set (Dest::chorusRate, 0.3f); set (Dest::chorusDepth, 0.5f); set (Dest::chorusMix, 0.4f);
        set (Dest::delayTime, 375.0f); set (Dest::delayFeedback, 0.35f); set (Dest::delayTone, 0.6f); set (Dest::delayMix, 0.25f);
        set (Dest::reverbSize, 0.6f); set (Dest::reverbDecay, 0.5f); set (Dest::reverbDamping, 0.5f);
        set (Dest::reverbPredelay, 0.1f); set (Dest::reverbMix, 0.2f);
    }

    float real (Dest d) const     { return DestRange::toReal (d, base[(size_t) d]); }
};

//==============================================================================
/** Position dans le séquenceur : pas, début du pas (noires), avancée (0..1), durée du pas. */
struct StepPos
{
    int step = 0;
    double start = 0.0;
    double frac = 0.0;
    double lengthBeats = 0.25;
};

inline StepPos stepAt (double beats, int steps, double stepBeats, double swing)
{
    steps = std::clamp (steps, 1, maxSteps);
    stepBeats = std::max (stepBeats, 1.0e-6);
    swing = std::clamp (swing, 0.0, 0.75);
    beats = std::max (beats, 0.0);

    const double pairLength = 2.0 * stepBeats;
    const double pair = std::floor (beats / pairLength);
    double r = beats - pair * pairLength;
    const double first = (1.0 + swing) * stepBeats;
    const double second = (1.0 - swing) * stepBeats;

    long long index = (long long) pair * 2;
    StepPos pos;
    pos.start = pair * pairLength;

    if (r < first)
    {
        pos.frac = r / first;
        pos.lengthBeats = first;
    }
    else
    {
        ++index;
        r -= first;
        pos.start += first;
        pos.frac = second > 0.0 ? r / second : 0.0;
        pos.lengthBeats = second;
    }

    pos.step = (int) (index % steps);
    pos.frac = std::clamp (pos.frac, 0.0, 1.0);
    return pos;
}

//==============================================================================
/** Générateur aléatoire déterministe (xorshift64*), identique sur toutes les machines. */
struct Rng
{
    uint64_t state = 0x9E3779B97F4A7C15ull;

    explicit Rng (uint64_t seed = 1) { reseed (seed); }

    void reseed (uint64_t seed)
    {
        state = seed * 0x9E3779B97F4A7C15ull + 0x632BE59BD9B4E019ull;
        if (state == 0)
            state = 0x9E3779B97F4A7C15ull;
        for (int i = 0; i < 4; ++i)
            next();
    }

    uint64_t next()
    {
        state ^= state >> 12;
        state ^= state << 25;
        state ^= state >> 27;
        return state * 0x2545F4914F6CDD1Dull;
    }

    double uniform()                 { return (double) (next() >> 11) * (1.0 / 9007199254740992.0); }
    float uniformf()                 { return (float) uniform(); }
    int range (int lo, int hi)       { return lo + (int) (uniform() * (double) (hi - lo + 1)) % (hi - lo + 1); }
    bool chance (double p)           { return uniform() < p; }
};

/** Valeur pseudo-aléatoire stable dans -1..1 pour un index (sert aux CHAOS). */
inline float hashValue (uint64_t seed, int64_t index)
{
    uint64_t x = seed ^ ((uint64_t) index * 0x9E3779B97F4A7C15ull);
    x ^= x >> 30; x *= 0xBF58476D1CE4E5B9ull;
    x ^= x >> 27; x *= 0x94D049BB133111EBull;
    x ^= x >> 31;
    return (float) ((double) (x >> 11) * (2.0 / 9007199254740992.0) - 1.0);
}

//==============================================================================
/** Sources de modulation et sortie du séquenceur : fonctions pures du temps (en noires). */
struct Mod
{
    static float lfoShape (LfoShape shape, double p)
    {
        switch (shape)
        {
            case LfoShape::triangle: return (float) (p < 0.25 ? 4.0 * p : p < 0.75 ? 2.0 - 4.0 * p : 4.0 * p - 4.0);
            case LfoShape::saw:      return (float) (2.0 * p - 1.0);
            case LfoShape::ramp:     return (float) (1.0 - 2.0 * p);
            case LfoShape::square:   return p < 0.5 ? 1.0f : -1.0f;
            case LfoShape::pulse:    return p < 0.25 ? 1.0f : -1.0f;
            case LfoShape::expDown:
            {
                // Chute rapide puis queue : le « pew » des lasers hi-tech
                constexpr double k = 6.0, e = 0.0024787521766663585;   // exp (-6)
                return (float) (2.0 * (std::exp (-k * p) - e) / (1.0 - e) - 1.0);
            }
            case LfoShape::expUp:
            {
                constexpr double k = 6.0, e = 0.0024787521766663585;
                return (float) (2.0 * (std::exp (-k * (1.0 - p)) - e) / (1.0 - e) - 1.0);
            }
            case LfoShape::sine:
            case LfoShape::count:
            default:                 return std::sin ((float) (twoPi * p));
        }
    }

    static float lfo (const LfoSettings& l, double t)
    {
        const double period = std::max (l.periodBeats, 1.0e-4);
        double p = t / period + (double) l.phase;
        p -= std::floor (p);
        if (l.steps > 1)
            p = std::floor (p * l.steps) / l.steps;

        float v = lfoShape (l.shape, p);
        if (l.unipolar)
            v = 0.5f * (v + 1.0f);
        return v * l.amount;
    }

    static float chaos (const ChaosSettings& c, double t, uint64_t seed)
    {
        const double period = std::max (c.periodBeats, 1.0e-4);
        const double x = std::max (0.0, t) / period;
        const double index = std::floor (x);
        const double f = x - index;
        const float a = hashValue (seed, (int64_t) index);

        float w = 0.0f;
        if (c.smooth > 0.0f)
        {
            const double s = std::clamp ((double) c.smooth, 0.0, 1.0);
            const double u = std::clamp ((f - (1.0 - s)) / s, 0.0, 1.0);
            w = (float) (0.5 - 0.5 * std::cos (pi * u));
        }

        const float b = w > 0.0f ? hashValue (seed, (int64_t) index + 1) : a;
        return (a + (b - a) * w) * c.amount;
    }

    /** src : 0 = rien, 1..6 = LFO (temps local), 7..8 = CHAOS (temps absolu). */
    static float source (const Settings& s, int src, double tLocal, double tAbs, uint64_t chaosSeed)
    {
        if (src >= 1 && src <= numLfos)
            return lfo (s.lfo[(size_t) (src - 1)], tLocal);
        if (src > numLfos && src <= numSources)
        {
            const int c = src - numLfos - 1;
            return chaos (s.chaos[(size_t) c], tAbs, chaosSeed + (uint64_t) c * (uint64_t) 0x5851F42D4C957F2Dull);
        }
        return 0.0f;
    }

    struct SeqOut
    {
        float value = 0.0f;
        int step = -1;
        int source = 0;
    };

    /** Sortie du séquenceur au temps t (noires depuis la note, ou position du morceau). */
    static SeqOut seq (const Settings& s, double t, double bpm, uint64_t chaosSeed)
    {
        const auto& q = s.seq;
        SeqOut out;
        t = std::max (0.0, t);

        if (! q.on)
        {
            out.source = std::clamp (q.manual, 0, numSources);
            out.value = source (s, out.source, t, t, chaosSeed);
            return out;
        }

        const int n = std::clamp (q.steps, 1, maxSteps);
        const auto pos = stepAt (t, n, q.stepBeats, q.swing);
        out.step = pos.step;
        out.source = std::clamp (q.pattern[(size_t) pos.step], 0, numSources);
        out.value = source (s, out.source, q.restart ? t - pos.start : t, t, chaosSeed);

        // Fondu depuis la source du pas précédent
        const double secondsPerBeat = 60.0 / std::clamp (bpm, 20.0, 999.0);
        const double xfade = 0.001 * (double) std::max (0.0f, q.xfadeMs);
        const double sinceStart = (t - pos.start) * secondsPerBeat;

        if (xfade > 0.0 && pos.start > 1.0e-9 && sinceStart < xfade)
        {
            const auto prev = stepAt (pos.start - 1.0e-7, n, q.stepBeats, q.swing);
            const int prevSource = std::clamp (q.pattern[(size_t) prev.step], 0, numSources);
            const float pv = source (s, prevSource, q.restart ? t - prev.start : t, t, chaosSeed);
            double w = sinceStart / xfade;
            w = w * w * (3.0 - 2.0 * w);
            out.value = pv + (out.value - pv) * (float) w;
        }

        return out;
    }
};

//==============================================================================
/** Motifs aléatoires pour le séquenceur : quelle source joue sur chaque pas. */
struct RandomPattern
{
    enum Style { runs = 0, alternate, euclid, scatter, mirror, numStyles };

    static const char* styleName (int s)
    {
        static const char* names[] = { "RUNS", "ALTERNATE", "EUCLID", "SCATTER", "MIRROR" };
        return names[std::clamp (s, 0, (int) numStyles - 1)];
    }

    /** available : sources utilisables (1..8). Remplit les 32 pas ; renvoie le style tiré. */
    static int generate (Rng& rng, int steps, const std::array<bool, numSources + 1>& available,
                         std::array<int, maxSteps>& pattern, int forcedStyle = -1)
    {
        steps = std::clamp (steps, 1, maxSteps);

        std::array<int, numSources> pool {};
        int poolSize = 0;
        for (int i = 1; i <= numSources; ++i)
            if (available[(size_t) i])
                pool[(size_t) poolSize++] = i;
        if (poolSize == 0)
            for (int i = 1; i <= numSources; ++i)
                pool[(size_t) poolSize++] = i;

        // Mélange, puis on garde 2 à 5 sources pour que le motif ait une identité
        for (int i = poolSize - 1; i > 0; --i)
            std::swap (pool[(size_t) i], pool[(size_t) rng.range (0, i)]);
        const int used = std::min (poolSize, rng.range (2, 5));
        auto pick = [&] { return pool[(size_t) rng.range (0, std::max (0, used - 1))]; };

        const int style = forcedStyle >= 0 ? std::min (forcedStyle, (int) numStyles - 1) : rng.range (0, numStyles - 1);

        switch (style)
        {
            case runs:
            {
                // Une source tenue 1 à 4 pas, puis une autre
                int i = 0;
                int last = -1;
                while (i < maxSteps)
                {
                    int src = pick();
                    if (src == last && used > 1) src = pick();
                    const int len = rng.range (1, 4);
                    for (int k = 0; k < len && i < maxSteps; ++k)
                        pattern[(size_t) i++] = src;
                    last = src;
                }
                break;
            }

            case alternate:
            {
                // ABAB, ABAC, ABCB… sur une cellule de 2 à 4 pas
                const int cell = rng.range (2, 4);
                std::array<int, 4> motif {};
                for (int k = 0; k < cell; ++k)
                    motif[(size_t) k] = pool[(size_t) (k % std::max (1, used))];
                if (cell >= 3 && rng.chance (0.5))
                    motif[2] = motif[0];
                for (int i = 0; i < maxSteps; ++i)
                    pattern[(size_t) i] = motif[(size_t) (i % cell)];
                break;
            }

            case euclid:
            {
                // Une source de fond, une autre répartie à la façon d'un rythme euclidien
                const int base = pool[0];
                const int hit = used > 1 ? pool[1] : pool[0];
                const int third = used > 2 ? pool[2] : hit;
                const int pulses = std::max (1, rng.range (std::max (1, steps / 5), std::max (1, steps / 2)));
                const int rotation = rng.range (0, steps - 1);
                for (int i = 0; i < maxSteps; ++i)
                {
                    const int k = (i + rotation) % steps;
                    const bool on = ((k * pulses) % steps) < pulses;
                    pattern[(size_t) i] = on ? (rng.chance (0.25) ? third : hit) : base;
                }
                break;
            }

            case mirror:
            {
                const int half = std::max (2, (steps + 1) / 2);
                std::array<int, maxSteps> motif {};
                for (int i = 0; i < half; ++i)
                    motif[(size_t) i] = pick();
                for (int i = 0; i < maxSteps; ++i)
                {
                    const int j = i % (2 * half);
                    pattern[(size_t) i] = j < half ? motif[(size_t) j] : motif[(size_t) (2 * half - 1 - j)];
                }
                break;
            }

            case scatter:
            default:
            {
                for (int i = 0; i < maxSteps; ++i)
                    pattern[(size_t) i] = rng.chance (0.08) ? 0 : pick();
                break;
            }
        }

        // Jamais une boucle entièrement muette ou d'une seule source
        bool varied = false, anySound = false;
        for (int i = 0; i < steps; ++i)
        {
            anySound = anySound || pattern[(size_t) i] != 0;
            varied = varied || pattern[(size_t) i] != pattern[0];
        }
        if (! anySound)
            pattern[0] = pool[0];
        if (! varied && steps > 1 && used > 1)
            pattern[(size_t) (steps / 2)] = pattern[0] == pool[0] ? pool[1] : pool[0];

        return style;
    }
};

//==============================================================================
/** Décimateur ÷2 : filtre demi-bande à phase linéaire (95 coefficients, fenêtre de Kaiser).
    Bande passante jusqu'à 0,44 fe, coupure au-delà de 0,56 fe (fe = fréquence de sortie). */
class HalfbandDecimator
{
public:
    static constexpr int taps = 95;
    static constexpr int centre = taps / 2;

    void reset()                       { history.fill (0.0f); pos = 0; }

    /** Deux échantillons à 2 fe -> un échantillon à fe. */
    float process (float a, float b)
    {
        push (a);
        push (b);

        const auto& h = coefficients();
        // Coefficients non nuls : le centre et les décalages impairs
        // Coefficients non nuls : le centre et les décalages impairs (symétriques)
        const float* x = history.data() + pos + taps - centre;   // x[0] = l'échantillon du centre
        float sum = h[centre] * x[0];
        for (int k = 1; k <= centre; k += 2)
            sum += h[(size_t) (centre - k)] * (x[k] + x[-k]);
        return sum;
    }

    static const std::array<float, taps>& coefficients()
    {
        static const std::array<float, taps> h = []
        {
            std::array<float, taps> c {};
            const double beta = 9.0;
            auto bessel = [] (double x)
            {
                double sum = 1.0, term = 1.0;
                for (int k = 1; k < 40; ++k)
                {
                    term *= (x / (2.0 * k)) * (x / (2.0 * k));
                    sum += term;
                }
                return sum;
            };
            double total = 0.0;
            for (int n = 0; n < taps; ++n)
            {
                const double m = n - centre;
                const double sinc = n == centre ? 0.5 : std::sin (pi * m / 2.0) / (pi * m);
                const double r = m / centre;
                const double window = bessel (beta * std::sqrt (std::max (0.0, 1.0 - r * r))) / bessel (beta);
                c[(size_t) n] = (float) (sinc * window);
                total += c[(size_t) n];
            }
            for (auto& v : c)
                v = (float) (v / total);
            return c;
        }();
        return h;
    }

private:
    void push (float x)
    {
        pos = (pos + 1) % taps;
        history[(size_t) pos] = x;
        history[(size_t) (pos + taps)] = x;
    }

    /** k = 0 : le plus récent. */
    float at (int k) const             { return history[(size_t) (pos + taps - k)]; }

    std::array<float, 2 * taps> history {};
    int pos = 0;
};

//==============================================================================
class Envelope
{
public:
    enum Stage { idle, attack, decay, sustain, release };

    void setSampleRate (double sr)                { sampleRate = sr; }

    void noteOn (float a, float d, float s)
    {
        setTimes (a, d, s, releaseTime);
        stage = attack;
    }

    void noteOff (float r)
    {
        releaseTime = r;
        if (stage != idle)
        {
            stage = release;
            releaseCoef = coefFor (r);
        }
    }

    void setTimes (float a, float d, float s, float r)
    {
        attackStep = 1.0 / (std::max (0.0005, (double) a) * sampleRate);
        decayCoef = coefFor (d);
        sustainLevel = std::clamp ((double) s, 0.0, 1.0);
        releaseTime = r;
        releaseCoef = coefFor (r);
    }

    void reset()                                  { stage = idle; value = 0.0; }
    bool isActive() const noexcept                { return stage != idle; }
    bool isReleasing() const noexcept             { return stage == release; }

    float next()
    {
        switch (stage)
        {
            case attack:
                value += attackStep;
                if (value >= 1.0) { value = 1.0; stage = decay; }
                break;
            case decay:
                value = sustainLevel + (value - sustainLevel) * decayCoef;
                if (std::abs (value - sustainLevel) < 1.0e-5) { value = sustainLevel; stage = sustain; }
                break;
            case sustain:
                value += (sustainLevel - value) * 0.002;
                break;
            case release:
                value *= releaseCoef;
                if (value < 1.0e-5) { value = 0.0; stage = idle; }
                break;
            case idle:
            default:
                value = 0.0;
                break;
        }
        return (float) value;
    }

private:
    double coefFor (float seconds) const
    {
        return std::exp (-9.21 / (std::max (0.0005, (double) seconds) * sampleRate));
    }

    double sampleRate = 48000.0;
    Stage stage = idle;
    double value = 0.0, attackStep = 0.001, decayCoef = 0.999, sustainLevel = 1.0, releaseCoef = 0.999;
    float releaseTime = 0.1f;
};

//==============================================================================
/** Filtre d'état « TPT » (Zavalishin), LP / BP / HP. */
struct Svf
{
    double ic1 = 0.0, ic2 = 0.0;

    void reset() { ic1 = ic2 = 0.0; }

    inline double process (double x, double g, double k, FilterType type)
    {
        const double a1 = 1.0 / (1.0 + g * (g + k));
        const double a2 = g * a1;
        const double a3 = g * a2;
        const double v3 = x - ic2;
        const double v1 = a1 * ic1 + a2 * v3;
        const double v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0 * v1 - ic1;
        ic2 = 2.0 * v2 - ic2;

        switch (type)
        {
            case FilterType::bandpass: return v1 * k;
            case FilterType::highpass: return x - k * v1 - v2;
            case FilterType::notch:    return x - k * v1;
            case FilterType::lowpass:
            case FilterType::count:
            default:                   return v2;
        }
    }
};

//==============================================================================
namespace detail
{
    inline float fastTanh (float x)
    {
        x = std::clamp (x, -4.0f, 4.0f);
        const float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }

    inline double noteToHz (double note)     { return 440.0 * std::exp2 ((note - 69.0) / 12.0); }

    /** cos (2 pi x) par table (4096 points, interpolation linéaire : erreur < 1e-6). */
    inline float cosCycles (double x)
    {
        static constexpr int size = 4096;
        static const std::array<float, size + 1> table = []
        {
            std::array<float, size + 1> t {};
            for (int i = 0; i <= size; ++i)
                t[(size_t) i] = (float) std::cos (twoPi * i / size);
            return t;
        }();
        const double pos = (x - std::floor (x)) * size;
        const int i = std::min ((int) pos, size - 1);
        const float f = (float) (pos - i);
        return table[(size_t) i] + (table[(size_t) i + 1] - table[(size_t) i]) * f;
    }

    inline double polyBlep (double t, double dt)
    {
        if (dt <= 0.0) return 0.0;
        if (t < dt)            { t /= dt; return t + t - t * t - 1.0; }
        if (t > 1.0 - dt)      { t = (t - 1.0) / dt; return t * t + t + t + 1.0; }
        return 0.0;
    }

    inline double frac (double x)            { return x - std::floor (x); }
}

//==============================================================================
/** Oscillateur à distorsion de phase, façon Casio CZ. p = phase 0..1, amount = PD AMT, shape = courbure. */
struct PhaseDistortion
{
    static double curve (double x, double shape)
    {
        // SHAPE : les segments de la fonction de phase se courbent (plus de dents, plus de « quack »)
        return shape <= 0.0 ? x : std::pow (std::clamp (x, 0.0, 1.0), 1.0 + 3.0 * shape);
    }

    static double sawWarp (double p, double d, double shape)
    {
        const double m = 0.5 - 0.49 * d;
        return p < m ? 0.5 * curve (p / m, shape) : 0.5 + 0.5 * curve ((p - m) / (1.0 - m), shape);
    }

    static float read (PdWave wave, double p, double d, double shape)
    {
        d = std::clamp (d, 0.0, 1.0);
        const double w = 1.0 - 0.98 * d;
        double phase = p;

        switch (wave)
        {
            case PdWave::saw:       phase = sawWarp (p, d, shape); break;
            case PdWave::square:
            {
                const double x = detail::frac (2.0 * p);
                const double h = 0.5 * curve (std::min (x / w, 1.0), shape);
                phase = p < 0.5 ? h : 0.5 + h;
                break;
            }
            case PdWave::pulse:     phase = curve (std::min (p / w, 1.0), shape); break;
            case PdWave::dblSine:   phase = p + d * (detail::frac (2.0 * p) - p); phase = curve (detail::frac (phase), shape); break;
            case PdWave::sawPulse:
                phase = p < 0.5 ? 0.5 * sawWarp (2.0 * p, d, shape) : 0.5 + 0.5 * curve (std::min ((2.0 * p - 1.0) / w, 1.0), shape);
                break;
            case PdWave::reso1: case PdWave::reso2: case PdWave::reso3:
            {
                const double r = 1.0 + 15.0 * d;
                double window = 1.0 - p;
                if (wave == PdWave::reso2) window = p < 0.5 ? 2.0 * p : 2.0 - 2.0 * p;
                if (wave == PdWave::reso3) window = p < 0.75 ? 1.0 : 4.0 - 4.0 * p;
                window = curve (window, shape);
                return (float) (detail::cosCycles (r * p) * window);
            }
            case PdWave::count:
            default: break;
        }
        return detail::cosCycles (phase);
    }
};

//==============================================================================
/** Filtre ladder façon Moog (4 pôles TPT, saturation dans la boucle) avec sorties par mélange de pôles. */
struct Ladder
{
    std::array<double, 4> s {};

    void reset()   { s.fill (0.0); }

    double process (double x, double g, double k, double drive, FilterType type, bool slope24)
    {
        const double G = g / (1.0 + g);
        const double G2 = G * G, G3 = G2 * G, G4 = G3 * G;
        const double sigma = (1.0 - G) * (G3 * s[0] + G2 * s[1] + G * s[2] + s[3]);
        const double y4est = (G4 * x + sigma) / (1.0 + k * G4);

        // Saturation de l'entrée (avec la contre-réaction)
        const double gain = 1.0 + 4.0 * drive;
        const double u = (double) detail::fastTanh ((float) ((x - k * y4est) * gain)) / gain * (1.0 + 0.3 * drive);

        double in = u;
        std::array<double, 4> y {};
        for (int i = 0; i < 4; ++i)
        {
            const double v = (in - s[(size_t) i]) * G;
            y[(size_t) i] = v + s[(size_t) i];
            s[(size_t) i] = y[(size_t) i] + v;
            in = y[(size_t) i];
        }

        const double comp = 1.0 + 0.3 * k;   // la résonance creuse le grave : on le rend en partie
        switch (type)
        {
            case FilterType::highpass:  return slope24 ? u - 4.0 * y[0] + 6.0 * y[1] - 4.0 * y[2] + y[3] : u - 2.0 * y[0] + y[1];
            case FilterType::bandpass:  return comp * (slope24 ? 4.0 * (y[1] - 2.0 * y[2] + y[3]) : 2.0 * (y[0] - y[1]));
            case FilterType::notch:     return slope24 ? u - 4.0 * y[0] + 6.0 * y[1] - 4.0 * y[2] + 2.0 * y[3] : u - 2.0 * y[0] + 2.0 * y[1];
            case FilterType::lowpass:
            case FilterType::count:
            default:                    return comp * (slope24 ? y[3] : y[1]);
        }
    }
};

/** Filtre complet d'un canal : LADDER ou SVF (2 étages en 24 dB). */
struct VoiceFilter
{
    Ladder ladder;
    std::array<Svf, 2> svf {};

    void reset()   { ladder.reset(); for (auto& f : svf) f.reset(); }

    double process (double x, double g, double resonance, double drive, FilterModel model, FilterType type, bool slope24)
    {
        if (model == FilterModel::ladder)
            return ladder.process (x, g, 4.0 * resonance, drive, type, slope24);

        // SVF : la saturation avant le filtre tient lieu de DRIVE
        if (drive > 0.0)
        {
            const double gain = 1.0 + 4.0 * drive;
            x = (double) detail::fastTanh ((float) (x * gain)) / gain * (1.0 + 0.3 * drive);
        }
        const double k = 2.0 - 1.98 * resonance;
        auto stage = [&] (Svf& f, double in, double kk)
        {
            if (type == FilterType::notch)
            {
                // Notch = entrée - passe-bande normalisé
                const double bp = f.process (in, g, kk, FilterType::bandpass);
                return in - bp;
            }
            return f.process (in, g, kk, type);
        };

        double y = stage (svf[0], x, k);
        if (slope24)
            y = stage (svf[1], y, type == FilterType::bandpass ? std::max (k, 0.7) : std::max (k, 1.0));
        return y;
    }
};

//==============================================================================
/** Bite! : fold, crush, soft (+ asymétrie), stéréo. */
struct Bite
{
    std::array<float, 2> crushHold {};
    std::array<double, 2> crushCounter {};
    std::array<float, 2> dcIn {}, dcOut {};

    void reset()   { crushHold.fill (0.0f); crushCounter.fill (0.0); dcIn.fill (0.0f); dcOut.fill (0.0f); }

    float process (int ch, float x, float fold, float crush, float soft, bool asym, float dcCoef)
    {
        if (fold <= 0.0f && crush <= 0.0f && soft <= 0.0f)
            return x;

        if (asym)
            x += 0.35f;

        if (fold > 0.0f)
        {
            const float folded = std::sin (x * (1.0f + 7.0f * fold) * 1.5707963f);
            x += (folded - x) * std::min (1.0f, fold * 5.0f);
        }
        if (soft > 0.0f)
        {
            const float g = 1.0f + 20.0f * soft;
            x = detail::fastTanh (x * g) / detail::fastTanh (g);
        }
        if (crush > 0.0f)
        {
            const double factor = 1.0 + (double) crush * crush * 31.0;
            crushCounter[(size_t) ch] += 1.0;
            if (crushCounter[(size_t) ch] >= factor)
            {
                crushCounter[(size_t) ch] -= factor;
                const float levels = std::exp2 (16.0f - 13.0f * crush);
                crushHold[(size_t) ch] = std::round (x * levels) / levels;
            }
            x = crushHold[(size_t) ch];
        }

        if (asym)
        {
            // Retire le continu ajouté par l'asymétrie
            const float y = x - dcIn[(size_t) ch] + dcCoef * dcOut[(size_t) ch];
            dcIn[(size_t) ch] = x;
            dcOut[(size_t) ch] = y;
            x = y;
        }
        return x;
    }
};

//==============================================================================
/** Valeurs lissées partagées par toutes les voix (positions de potards, 0..1). */
struct Smoothed
{
    std::array<float, numDests> base {};

    void snap (const Settings& s)       { base = s.base; }

    void step (const Settings& s, float fast, float slow)
    {
        for (int d = 1; d < numDests; ++d)
        {
            const float c = DestRange::isPitch ((Dest) d) ? slow : fast;
            base[(size_t) d] += (s.base[(size_t) d] - base[(size_t) d]) * c;
        }
    }
};

/** Valeur du MAIN LFO pour une phase (cycles), -1..1. */
inline float mainLfoValue (MainLfoShape shape, double cycles, uint64_t seed)
{
    const double p = detail::frac (cycles);
    switch (shape)
    {
        case MainLfoShape::triangle:     return (float) (p < 0.25 ? 4.0 * p : p < 0.75 ? 2.0 - 4.0 * p : 4.0 * p - 4.0);
        case MainLfoShape::saw:          return (float) (2.0 * p - 1.0);
        case MainLfoShape::ramp:         return (float) (1.0 - 2.0 * p);
        case MainLfoShape::square:       return p < 0.5 ? 1.0f : -1.0f;
        case MainLfoShape::sampleHold:   return hashValue (seed, (int64_t) std::floor (cycles));
        case MainLfoShape::smoothRandom:
        {
            const auto i = (int64_t) std::floor (cycles);
            const float a = hashValue (seed, i), b = hashValue (seed, i + 1);
            const float w = (float) (0.5 - 0.5 * std::cos (pi * p));
            return a + (b - a) * w;
        }
        case MainLfoShape::sine:
        case MainLfoShape::count:
        default:                         return std::sin ((float) (twoPi * p));
    }
}

//==============================================================================
class Voice
{
public:
    void prepare (double sr)
    {
        sampleRate = sr;
        ampEnv.setSampleRate (sr);
        fltEnv.setSampleRate (sr);
        modEnv.setSampleRate (sr);
        if (plaits == nullptr)
            plaits = std::make_unique<PlaitsOsc>();
        plaits->setSampleRate (sr);
        slotCoef = (float) (1.0 - std::exp (-1.0 / (0.0004 * sr)));
        slotCoefControl = (float) (1.0 - std::exp (-(double) controlInterval / (0.0004 * sr)));
        dcCoef = (float) std::exp (-twoPi * 10.0 / sr);
        reset();
    }

    void reset()
    {
        ampEnv.reset(); fltEnv.reset(); modEnv.reset();
        note = -1;
        held = false;
        resetDsp();
        slotValue.fill (0.0f);
        offsets.fill (0.0f);
        seqBeats = 0.0;
        seconds = 0.0;
        lastStep = -1;
        if (plaits != nullptr) plaits->reset();
    }

    bool isActive() const noexcept           { return ampEnv.isActive(); }
    bool isReleasing() const noexcept        { return ampEnv.isReleasing(); }

    void start (int midiNote, float vel, const Settings& s, bool retrigger, double fromNote, uint64_t order, uint64_t seed)
    {
        const bool wasActive = isActive();
        note = midiNote;
        targetNote = (double) midiNote;
        currentNote = fromNote;
        held = true;
        startOrder = order;

        if (retrigger || ! wasActive)
        {
            velocity = std::clamp (vel, 0.0f, 1.0f);
            if (! wasActive)
                resetDsp();

            // Phases de départ des oscillateurs
            Rng rng (seed ^ 0xA5A5A5A5ull);
            for (int u = 0; u < maxUnison; ++u)
                wtPhase[(size_t) u] = s.wtRandomPhase ? rng.uniform() : (double) s.wtPhase;
            if (! wasActive)
            {
                pdPhase = 0.0;
                subPhase = 0.0;
            }

            ampEnv.noteOn (s.ampA, s.ampD, s.ampS);  ampEnv.setTimes (s.ampA, s.ampD, s.ampS, s.ampR);
            fltEnv.noteOn (s.fltA, s.fltD, s.fltS);  fltEnv.setTimes (s.fltA, s.fltD, s.fltS, s.fltR);
            modEnv.noteOn (s.modA, s.modD, s.modS);  modEnv.setTimes (s.modA, s.modD, s.modS, s.modR);
            plaits->noteOn();
            seqBeats = 0.0;
            seconds = 0.0;
            chaosSeed = seed;
            firstSample = true;
            controlCountdown = 0;
        }
    }

    void release (const Settings& s)
    {
        held = false;
        ampEnv.noteOff (s.ampR);
        fltEnv.noteOff (s.fltR);
        modEnv.noteOff (s.modR);
        plaits->noteOff();
    }

    void kill()                              { reset(); }

    void updateTimes (const Settings& s)
    {
        ampEnv.setTimes (s.ampA, s.ampD, s.ampS, s.ampR);
        fltEnv.setTimes (s.fltA, s.fltD, s.fltS, s.fltR);
        modEnv.setTimes (s.modA, s.modD, s.modS, s.modR);
    }

    void setGlideTarget (int midiNote)       { note = midiNote; targetNote = (double) midiNote; }

    struct Shared
    {
        double beatsInc = 0.0, bpm = 120.0, hostBeats = 0.0, hostSeconds = 0.0;
        double glideCoef = 1.0;
        float pitchBend = 0.0f, modWheel = 0.0f;
        uint64_t hostChaosSeed = 1;
        uint32_t neededSources = 0;
        bool stereo = true;
    };

    void render (const Settings& s, const Smoothed& sm, const Shared& sh, float& outL, float& outR, uint64_t& noiseState)
    {
        if (! isActive())
            return;

        currentNote += (targetNote - currentNote) * sh.glideCoef;

        const float ampLevel = ampEnv.next();
        const float fltLevel = fltEnv.next();
        const float modLevel = modEnv.next();

        if (controlCountdown <= 0)
        {
            controlCountdown = controlInterval;
            updateControl (s, sm, sh, fltLevel, modLevel);
        }
        --controlCountdown;
        seqBeats += sh.beatsInc;
        seconds += 1.0 / sampleRate;

        [[maybe_unused]] const auto& [os, osRate, baseNote, table, unison, wtLevel, wtHz, wtWarp, framePos,
                     pdLevel, pdHz, pdAmount, pdShape, pdCrush, pdFold, subLevel, subHz, noiseLevel,
                     linearFm, wtFromWt, wtFromPd, wtFromAlgo, wtFromNoise, pdFromWt, pdFromPd, pdFromAlgo, pdFromNoise, algoFromWt, algoFromPd, algoFromAlgo, algoFromNoise, algoFm,
                     warpFactor, levelPos, algoActive, algoParams, algoGainL, algoGainR,
                     biteFold, biteCrush, biteSoft, g, resonance, drive, stereo, pdActive, wtActive, volume, dcOs] = ctl;
        float algo = 0.0f;
        if (algoActive)
        {
            if (algoFm)
            {
                // FM de l'ALGO : sa hauteur suit les modulateurs, échantillon par échantillon
                const float m = algoFromWt * lastWt + algoFromPd * lastPd + algoFromAlgo * lastAlgo + algoFromNoise * lastNoise;
                algo = plaits->processFm (algoParams, (float) algoFmSemitones * std::clamp (m, -1.5f, 1.5f));
            }
            else
                algo = plaits->process (algoParams);
        }
        const float algoL = algo * algoGainL, algoR = algo * algoGainR;

        // --- Boucle suréchantillonnée -------------------------------------------------
        std::array<float, 4> subL {}, subR {};
        for (int n = 0; n < os; ++n)
        {
            const float t = (float) (n + 1) / (float) os;
            const float algoNowL = lastAlgoL + (algoL - lastAlgoL) * t;
            const float algoNowR = lastAlgoR + (algoR - lastAlgoR) * t;
            const float algoNow = lastAlgo + (algo - lastAlgo) * t;

            // Bruit (aussi modulateur de la matrice)
            noiseState ^= noiseState << 13;
            noiseState ^= noiseState >> 7;
            noiseState ^= noiseState << 17;
            const float white = (float) ((double) (noiseState >> 11) * (2.0 / 9007199254740992.0) - 1.0);
            float noise = white;
            switch (s.noiseType)
            {
                case NoiseType::pink:
                    // Filtre de Paul Kellet (version économique)
                    pink[0] = 0.99765f * pink[0] + white * 0.0990460f;
                    pink[1] = 0.96300f * pink[1] + white * 0.2965164f;
                    pink[2] = 0.57000f * pink[2] + white * 1.0526913f;
                    noise = 0.25f * (pink[0] + pink[1] + pink[2] + white * 0.1848f);
                    break;
                case NoiseType::brown:
                    brown = (brown + 0.02f * white) * 0.998f;
                    noise = 3.5f * brown;
                    break;
                case NoiseType::digital:
                {
                    // Bruit 1 bit cadencé sur la note (façon puce sonore)
                    digitalPhase += 8.0 * subHz / osRate;
                    if (digitalPhase >= 1.0)
                    {
                        digitalPhase -= std::floor (digitalPhase);
                        lfsr = (uint16_t) ((lfsr >> 1) | (((lfsr ^ (lfsr >> 1)) & 1u) << 14));
                    }
                    noise = (lfsr & 1u) ? 0.7f : -0.7f;
                    break;
                }
                case NoiseType::white:
                case NoiseType::count:
                default: break;
            }

            lastNoise = noise;

            // PHASE DISTORTION
            float pd = 0.0f;
            if (pdActive)
            {
                const float mod = pdFromWt * lastWt + pdFromPd * lastPd + pdFromAlgo * algoNow + pdFromNoise * noise;
                double p = pdPhase;
                double inc = pdHz / osRate;
                if (linearFm)
                    inc *= 1.0 + linDepth * mod;
                else
                    p += pmDepth * mod;
                p = detail::frac (p);
                pd = PhaseDistortion::read (s.pdWave, p, pdAmount, pdShape);
                if (pdFold > 0.0f)
                {
                    const float folded = std::sin (pd * (1.0f + 7.0f * pdFold) * 1.5707963f);
                    pd += (folded - pd) * std::min (1.0f, pdFold * 5.0f);
                }
                if (pdCrush > 0.0f)
                {
                    const double factor = 1.0 + (double) pdCrush * pdCrush * 31.0;
                    pdCrushCounter += 1.0;
                    if (pdCrushCounter >= factor)
                    {
                        pdCrushCounter -= factor;
                        const float levels = std::exp2 (16.0f - 13.0f * pdCrush);
                        pdCrushHold = std::round (pd * levels) / levels;
                    }
                    pd = pdCrushHold;
                }
                pdPhase = detail::frac (pdPhase + inc);
                lastPd = pd;
            }
            else
                lastPd = 0.0f;

            // WAVETABLE (unison)
            float wtL = 0.0f, wtR = 0.0f, wtMono = 0.0f;
            if (! wtActive)
                lastWt = 0.0f;
            else
            {
                const float mod = wtFromWt * lastWt + wtFromPd * pd + wtFromAlgo * algoNow + wtFromNoise * noise;
                const double baseInc = std::min (wtHz / osRate, 0.45);
                for (int u = 0; u < unison; ++u)
                {
                    double& ph = wtPhase[(size_t) u];
                    double inc = baseInc * unisonRatio[(size_t) u];
                    double p = ph;
                    if (linearFm)
                        inc *= 1.0 + linDepth * mod;
                    else
                        p += pmDepth * mod;
                    p = detail::frac (p);

                    float v;
                    switch (s.wtWarp)
                    {
                        case WtWarp::sync:
                            v = table->read (framePos, levelPos, detail::frac (p * warpFactor));
                            break;
                        case WtWarp::bendPlus: case WtWarp::bendMinus:
                        {
                            const double e = s.wtWarp == WtWarp::bendPlus ? 1.0 + 3.0 * wtWarp : 1.0 / (1.0 + 3.0 * wtWarp);
                            const double b = p < 0.5 ? 0.5 * std::pow (2.0 * p, e) : 1.0 - 0.5 * std::pow (2.0 * (1.0 - p), e);
                            v = table->read (framePos, levelPos, std::min (b, 0.9999999));
                            break;
                        }
                        case WtWarp::mirror:
                        {
                            const float a = table->read (framePos, levelPos, p);
                            const double m = p < 0.5 ? 2.0 * p : 2.0 - 2.0 * p;
                            v = a + (table->read (framePos, levelPos, std::min (m, 0.9999999)) - a) * wtWarp;
                            break;
                        }
                        case WtWarp::off:
                        case WtWarp::count:
                        default:
                            v = table->read (framePos, levelPos, p);
                            break;
                    }

                    wtL += v * unisonL[(size_t) u];
                    wtR += v * unisonR[(size_t) u];
                    wtMono += v;
                    ph = detail::frac (ph + inc);
                }
                lastWt = wtMono / (float) unison;
            }

            // SUB
            float sub = 0.0f;
            if (subLevel > 0.0f)
            {
                const double dt = subHz / osRate;
                switch (s.subWave)
                {
                    case SubWave::triangle: sub = (float) (subPhase < 0.5 ? 4.0 * subPhase - 1.0 : 3.0 - 4.0 * subPhase); break;
                    case SubWave::square:
                    {
                        double sq = subPhase < 0.5 ? 1.0 : -1.0;
                        sq += detail::polyBlep (subPhase, dt) - detail::polyBlep (detail::frac (subPhase + 0.5), dt);
                        sub = (float) sq;
                        break;
                    }
                    case SubWave::saw:      sub = (float) (2.0 * subPhase - 1.0 - detail::polyBlep (subPhase, dt)); break;
                    case SubWave::sine:
                    case SubWave::count:
                    default:                sub = detail::cosCycles (subPhase - 0.25); break;
                }
                subPhase = detail::frac (subPhase + dt);
            }

            // Mélange
            const float centre = pd * pdLevel + sub * subLevel + noise * noiseLevel;
            float l = 0.5f * (wtL * wtLevel + centre + algoNowL);
            float r = 0.5f * (wtR * wtLevel + centre + algoNowR);

            // Bite (avant le filtre)
            if (! s.bitePost)
            {
                l = bite.process (0, l, biteFold, biteCrush, biteSoft, s.biteAsym, dcOs);
                if (stereo) r = bite.process (1, r, biteFold, biteCrush, biteSoft, s.biteAsym, dcOs);
            }

            l = (float) filters[0].process (l, g, resonance, drive, s.filterModel, s.filterType, s.slope24);
            if (stereo)
                r = (float) filters[1].process (r, g, resonance, drive, s.filterModel, s.filterType, s.slope24);

            if (s.bitePost)
            {
                l = bite.process (0, l, biteFold, biteCrush, biteSoft, s.biteAsym, dcOs);
                if (stereo) r = bite.process (1, r, biteFold, biteCrush, biteSoft, s.biteAsym, dcOs);
            }

            subL[(size_t) n] = l;
            subR[(size_t) n] = stereo ? r : l;
        }
        lastAlgoL = algoL;
        lastAlgoR = algoR;
        lastAlgo = algo;

        // --- Retour à la fréquence de base ---------------------------------------------
        float yl = subL[0], yr = subR[0];
        if (os == 2)
        {
            yl = decim[0].process (subL[0], subL[1]);
            yr = stereo ? decim[1].process (subR[0], subR[1]) : yl;
        }
        else if (os == 4)
        {
            const float a = decim[0].process (subL[0], subL[1]), b = decim[0].process (subL[2], subL[3]);
            yl = decim[2].process (a, b);
            if (stereo)
            {
                const float c = decim[1].process (subR[0], subR[1]), d = decim[1].process (subR[2], subR[3]);
                yr = decim[3].process (c, d);
            }
            else
                yr = yl;
        }

        const float velGain = 1.0f - s.velocity * (1.0f - velocity);
        const float gain = ampLevel * velGain * volume;
        outL += yl * gain;
        outR += yr * gain;

        if (! std::isfinite (yl) || ! std::isfinite (yr))
            reset();
    }

    struct Control
    {
        int os; double osRate, baseNote; const Wavetable* table; int unison; float wtLevel; double wtHz; float wtWarp; double framePos;
        float pdLevel; double pdHz, pdAmount, pdShape; float pdCrush, pdFold, subLevel; double subHz; float noiseLevel;
        bool linearFm; float wtFromWt, wtFromPd, wtFromAlgo, wtFromNoise, pdFromWt, pdFromPd, pdFromAlgo, pdFromNoise;
        float algoFromWt, algoFromPd, algoFromAlgo, algoFromNoise; bool algoFm;
        double warpFactor, levelPos; bool algoActive; PlaitsOsc::Params algoParams; float algoGainL, algoGainR;
        float biteFold, biteCrush, biteSoft; double g, resonance, drive; bool stereo, pdActive, wtActive; float volume, dcOs;
    };
    Control ctl {};
    int controlCountdown = 0;
    float slotCoefControl = 0.3f;
    static constexpr int controlInterval = 8;
    static constexpr double pmDepth = 2.0;    // excursion de phase (cycles) à 100 %
    static constexpr double linDepth = 4.0;   // FM linéaire : écart de fréquence relatif à 100 %

    /** Réglages de la voix, recalculés toutes les controlInterval périodes (modulation comprise). */
    void updateControl (const Settings& s, const Smoothed& sm, const Shared& sh, float fltLevel, float modLevel)
    {
        // --- Modulation ---------------------------------------------------------
        offsets.fill (0.0f);
        {
            const bool host = s.seq.hostSync;
            const double t = host ? sh.hostBeats : seqBeats;
            const uint64_t seed = host ? sh.hostChaosSeed : chaosSeed;

            std::array<float, numModSources> value {};
            {
                const auto out = Mod::seq (s, t, sh.bpm, seed);
                value[srcSeq] = out.value;
                lastStep = out.step;
            }
            for (int k = srcLfo1; k < srcEnv2; ++k)
                if (sh.neededSources & (1u << k))
                    value[(size_t) k] = Mod::source (s, k, t, t, seed);
            value[srcEnv2] = fltLevel;
            value[srcEnv3] = modLevel;
            value[srcVelocity] = velocity;
            value[srcModWheel] = sh.modWheel;
            {
                const auto& ml = s.mainLfo;
                const double localTime = ml.sync ? seqBeats : seconds;
                const double globalTime = ml.sync ? sh.hostBeats : sh.hostSeconds;
                const double time = ml.retrig ? localTime : globalTime;
                const double cycles = ml.sync ? time / std::max (1.0e-4, ml.periodBeats) : time * (double) ml.rateHz;
                value[srcMainLfo] = mainLfoValue (ml.shape, cycles, ml.retrig ? chaosSeed : sh.hostChaosSeed);
            }

            auto route = [&] (int k, int src, Dest d, float amount)
            {
                float& sv = slotValue[(size_t) k];
                if (d == Dest::off)
                {
                    sv = 0.0f;
                    return;
                }
                const float target = amount * value[(size_t) std::clamp (src, 0, numModSources - 1)];
                sv = firstSample ? target : sv + (target - sv) * slotCoefControl;
                offsets[(size_t) d] += sv;
            };

            for (int k = 0; k < numSlots; ++k)
                route (k, s.slotSource[(size_t) k], s.slotDest[(size_t) k], s.slotAmount[(size_t) k]);
            for (int k = 0; k < 2; ++k)
                route (numSlots + k, srcMainLfo, s.mainLfo.dest[(size_t) k], s.mainLfo.amount[(size_t) k]);
        }

        firstSample = false;

        auto norm = [&] (Dest d) { return std::clamp (sm.base[(size_t) d] + offsets[(size_t) d], 0.0f, 1.0f); };
        auto real = [&] (Dest d) { return DestRange::toReal (d, norm (d)); };

        // --- Réglages de l'échantillon ------------------------------------------------
        const int os = s.oversampling >= 4 ? 4 : (s.oversampling >= 2 ? 2 : 1);
        const double osRate = sampleRate * os;
        const double baseNote = currentNote + sh.pitchBend;

        // WAVETABLE
        const Wavetable* table = s.table != nullptr ? s.table : BuiltinTables::get (0);
        const int unison = std::clamp (s.unison, 1, maxUnison);
        const float wtLevel = s.wtOn ? real (Dest::wtLevel) : 0.0f;
        const double wtHz = detail::noteToHz (baseNote + 12.0 * s.wtOctave + real (Dest::wtSemi) + 0.01 * real (Dest::wtFine));
        const float detune = norm (Dest::wtDetune);
        const float spread = norm (Dest::wtSpread);
        const float wtPan = real (Dest::wtPan);
        const float wtWarp = norm (Dest::wtWarp);
        const double framePos = (double) norm (Dest::wtPos) * (double) (table->numFrames - 1);

        // PHASE DISTORTION
        const float pdLevel = s.pdOn ? real (Dest::pdLevel) : 0.0f;
        const double pdHz = std::min (detail::noteToHz (baseNote + 12.0 * s.pdOctave + real (Dest::pdSemi) + 0.01 * real (Dest::pdFine)), osRate * 0.45);
        const double pdAmount = norm (Dest::pdAmount), pdShape = norm (Dest::pdShape);
        const float pdCrush = norm (Dest::pdCrush), pdFold = norm (Dest::pdFold);

        // SUB / NOISE
        const float subLevel = norm (Dest::subLevel);
        const double subHz = std::min (detail::noteToHz (baseNote + 12.0 * s.subOctave), osRate * 0.45);
        const float noiseLevel = norm (Dest::noiseLevel);

        // Matrice FM
        const float depth = norm (Dest::fmDepth);
        const bool linearFm = s.fmMode == FmMode::linear;
        const float wtFromWt = depth * s.fm[0][0], wtFromPd = depth * s.fm[1][0], wtFromAlgo = depth * s.fm[2][0], wtFromNoise = depth * s.fm[3][0];
        const float pdFromWt = depth * s.fm[0][1], pdFromPd = depth * s.fm[1][1], pdFromAlgo = depth * s.fm[2][1], pdFromNoise = depth * s.fm[3][1];
        const float algoFromWt = depth * s.fm[0][2], algoFromPd = depth * s.fm[1][2], algoFromAlgo = depth * s.fm[2][2], algoFromNoise = depth * s.fm[3][2];
        const bool algoFm = algoFromWt > 0.0f || algoFromPd > 0.0f || algoFromAlgo > 0.0f || algoFromNoise > 0.0f;

        // Niveau de mipmap : harmoniques jusqu'à 0,75 fe (le décimateur coupe au-dessus de 0,5 fe) ; sans
        // suréchantillonnage, un niveau de plus pour rester sous Nyquist
        double warpFactor = 1.0;
        if (s.wtWarp == WtWarp::sync)                                 warpFactor = std::exp2 (4.0 * wtWarp);
        else if (s.wtWarp == WtWarp::bendPlus || s.wtWarp == WtWarp::bendMinus) warpFactor = 1.0 + 3.0 * wtWarp;
        else if (s.wtWarp == WtWarp::mirror)                          warpFactor = 1.0 + wtWarp;
        double effective = wtHz * warpFactor;
        if (linearFm)
            effective *= 1.0 + linDepth * (wtFromWt + wtFromPd);
        else
            effective += twoPi * pmDepth * ((double) wtFromPd * pdHz + (double) wtFromWt * wtHz
                                        + (double) wtFromAlgo * detail::noteToHz (baseNote + 12.0 * s.algoOctave));
        const double levelBias = os == 1 ? 1.0 : 0.0;
        auto levelFor = [&] (double hz)
        {
            return std::clamp (std::log2 (std::max (1.0e-9, hz * 1024.0 / (0.75 * sampleRate))) + levelBias, 0.0, (double) (wt::numLevels - 1));
        };
        const double levelPos = levelFor (effective);

        // Unison : désaccord et panoramique de chaque voix
        if (unison != cachedUnison || std::abs (detune - cachedDetune) > 1.0e-5f || std::abs (spread - cachedSpread) > 1.0e-5f
            || std::abs (wtPan - cachedWtPan) > 1.0e-5f)
        {
            cachedUnison = unison; cachedDetune = detune; cachedSpread = spread; cachedWtPan = wtPan;
            const float norm1 = 1.0f / std::sqrt ((float) unison);
            for (int u = 0; u < unison; ++u)
            {
                const double pos = unison > 1 ? (double) u / (unison - 1) * 2.0 - 1.0 : 0.0;
                unisonRatio[(size_t) u] = std::exp2 (pos * 100.0 * (double) (detune * detune) / 1200.0);
                const float pan = std::clamp ((float) pos * spread + wtPan, -1.0f, 1.0f);
                const float angle = (float) (0.25 * pi) * (pan + 1.0f);
                unisonL[(size_t) u] = std::cos (angle) * 1.41421356f * norm1;
                unisonR[(size_t) u] = std::sin (angle) * 1.41421356f * norm1;
            }
        }

        // ALGORITHMIC OSC (Plaits, à la fréquence de base, interpolé) : réglages
        const float algoLevel = s.algoOn ? real (Dest::algoLevel) : 0.0f;
        const bool algoActive = s.algoOn && (algoLevel > 0.0f || wtFromAlgo > 0.0f || pdFromAlgo > 0.0f);
        PlaitsOsc::Params algoParams;
        algoParams.model = s.algoModel;
        algoParams.note = (float) (baseNote + 12.0 * s.algoOctave + real (Dest::algoSemi) + 0.01 * real (Dest::algoFine));
        algoParams.harmonics = norm (Dest::algoHarmonics);
        algoParams.timbre = norm (Dest::algoTimbre);
        algoParams.morph = norm (Dest::algoMorph);
        algoParams.decay = norm (Dest::algoDecay);
        algoParams.colour = norm (Dest::algoColour);
        algoParams.aux = norm (Dest::algoAux);
        algoParams.lpg = s.algoLpg;
        const float algoPan = std::clamp (real (Dest::algoPan), -1.0f, 1.0f);
        const float algoAngle = (float) (0.25 * pi) * (algoPan + 1.0f);
        const float algoGainL = algoLevel * std::cos (algoAngle) * 1.41421356f;
        const float algoGainR = algoLevel * std::sin (algoAngle) * 1.41421356f;

        // Bite et filtre
        const float biteFold = norm (Dest::biteFold), biteCrush = norm (Dest::biteCrush), biteSoft = norm (Dest::biteSoft);
        const double tracked = (double) real (Dest::cutoff)
                             * std::exp2 (s.keyTrack * (currentNote - 60.0) / 12.0 + 7.0 * (double) real (Dest::filterEnv) * fltLevel);
        const double cutoff = std::clamp (tracked, 16.0, std::min (20000.0, osRate * 0.45));
        const double g = std::tan (pi * cutoff / osRate);
        const double resonance = norm (Dest::resonance);
        const double drive = norm (Dest::filterDrive);
        const bool stereo = sh.stereo;
        const bool pdActive = s.pdOn && (pdLevel > 0.0f || wtFromPd > 0.0f || (algoActive && algoFromPd > 0.0f));
        const bool wtActive = s.wtOn && (wtLevel > 0.0f || pdFromWt > 0.0f || (algoActive && algoFromWt > 0.0f));
        const float volume = real (Dest::volume);
        const float dcOs = dcCoefOs (os);

        ctl = Control { os, osRate, baseNote, table, unison, wtLevel, wtHz, wtWarp, framePos, pdLevel, pdHz, pdAmount, pdShape, pdCrush, pdFold, subLevel, subHz, noiseLevel, linearFm, wtFromWt, wtFromPd, wtFromAlgo, wtFromNoise, pdFromWt, pdFromPd, pdFromAlgo, pdFromNoise, algoFromWt, algoFromPd, algoFromAlgo, algoFromNoise, algoFm, warpFactor, levelPos, algoActive, algoParams, algoGainL, algoGainR, biteFold, biteCrush, biteSoft, g, resonance, drive, stereo, pdActive, wtActive, volume, dcOs };
    }

    int note = -1;
    bool held = false;
    uint64_t startOrder = 0;
    int lastStep = -1;
    double seqBeats = 0.0, seconds = 0.0;
    double currentNote = 60.0;
    std::array<float, numDests> offsets {};

private:
    float dcCoefOs (int os) const            { return os == 1 ? dcCoef : (os == 2 ? std::sqrt (dcCoef) : std::sqrt (std::sqrt (dcCoef))); }

    void resetDsp()
    {
        wtPhase.fill (0.0);
        pdPhase = subPhase = digitalPhase = 0.0;
        lastWt = lastPd = lastAlgo = lastNoise = 0.0f;
        lastAlgoL = lastAlgoR = 0.0f;
        pink.fill (0.0f);
        brown = 0.0f;
        lfsr = 0x7FFF;
        pdCrushHold = 0.0f;
        pdCrushCounter = 0.0;
        for (auto& f : filters) f.reset();
        for (auto& d : decim) d.reset();
        bite.reset();
        cachedUnison = -1;
    }

    double sampleRate = 48000.0;
    Envelope ampEnv, fltEnv, modEnv;
    double targetNote = 60.0;
    float velocity = 1.0f;

    std::array<double, maxUnison> wtPhase {};
    std::array<double, maxUnison> unisonRatio {};
    std::array<float, maxUnison> unisonL {}, unisonR {};
    int cachedUnison = -1;
    float cachedDetune = -1.0f, cachedSpread = -1.0f, cachedWtPan = -2.0f;

    double pdPhase = 0.0, subPhase = 0.0, digitalPhase = 0.0;
    float lastWt = 0.0f, lastPd = 0.0f, lastAlgo = 0.0f, lastNoise = 0.0f, lastAlgoL = 0.0f, lastAlgoR = 0.0f;
    std::array<float, 3> pink {};
    float brown = 0.0f;
    uint16_t lfsr = 0x7FFF;
    float pdCrushHold = 0.0f;
    double pdCrushCounter = 0.0;

    std::unique_ptr<PlaitsOsc> plaits;
    std::array<VoiceFilter, 2> filters {};
    std::array<HalfbandDecimator, 4> decim {};
    Bite bite;
    float dcCoef = 0.999f;

    std::array<float, numSlots + 2> slotValue {};
    float slotCoef = 0.05f;
    uint64_t chaosSeed = 1;
    bool firstSample = true;
};

//==============================================================================
class Engine
{
public:
    void prepare (double sr)
    {
        sampleRate = sr > 0.0 ? sr : 48000.0;
        for (auto& v : voices)
            v.prepare (sampleRate);
        smoothed.snap (settings);
        heldNotes.clear();
        freeBeats = freeSeconds = 0.0;
        dcCoef = (float) std::exp (-twoPi * 8.0 / sampleRate);
        dcInL = dcInR = dcOutL = dcOutR = 0.0f;
        prepared = true;
    }

    void reset()
    {
        for (auto& v : voices)
            v.reset();
        heldNotes.clear();
        pitchWheel = 0.0f;
        fxOffsets.fill (0.0f);
        dcInL = dcInR = dcOutL = dcOutR = 0.0f;
    }

    void setSettings (const Settings& s)
    {
        const bool modeChanged = (s.voiceMode == VoiceMode::poly) != (settings.voiceMode == VoiceMode::poly);
        settings = s;
        if (modeChanged)
            allNotesOff (true);
    }

    const Settings& getSettings() const noexcept     { return settings; }

    void setHostChaosSeed (uint64_t seed) noexcept    { hostChaosSeed = seed; }
    void setPitchWheel (float v) noexcept             { pitchWheel = std::clamp (v, -1.0f, 1.0f); }
    void setModWheel (float v) noexcept               { modWheel = std::clamp (v, 0.0f, 1.0f); }

    //==========================================================================
    void noteOn (int midiNote, float velocity)
    {
        midiNote = std::clamp (midiNote, 0, 127);
        ++orderCounter;
        const uint64_t seed = noteRng.next();
        const bool glide = settings.glideMs > 0.0f && lastNote >= 0;
        const double from = glide ? lastPlayedPitch() : (double) midiNote;

        if (settings.voiceMode != VoiceMode::poly)
        {
            heldNotes.remove (midiNote);
            const bool overlapping = ! heldNotes.empty() && voices[0].isActive() && voices[0].held;
            heldNotes.push (midiNote);

            for (size_t i = 1; i < voices.size(); ++i)
                if (voices[i].isActive()) voices[i].release (settings);

            // MONO : chaque note relance les enveloppes ; LEGATO : pas si une note est déjà tenue
            const bool retrigger = settings.voiceMode == VoiceMode::mono || ! overlapping;
            const double start = voices[0].isActive() && glide ? voices[0].currentNote : from;
            voices[0].start (midiNote, velocity, settings, retrigger, start, orderCounter, seed);
            lastNote = midiNote;
            return;
        }

        Voice* chosen = nullptr;
        for (auto& v : voices)
            if (v.isActive() && v.note == midiNote) { chosen = &v; break; }

        if (chosen == nullptr)
            for (auto& v : voices)
                if (! v.isActive()) { chosen = &v; break; }

        if (chosen == nullptr)
            for (int pass = 0; pass < 2 && chosen == nullptr; ++pass)
                for (auto& v : voices)
                    if ((pass == 1 || v.isReleasing()) && (chosen == nullptr || v.startOrder < chosen->startOrder))
                        chosen = &v;

        chosen->start (midiNote, velocity, settings, true, from, orderCounter, seed);
        lastNote = midiNote;
    }

    void noteOff (int midiNote)
    {
        if (settings.voiceMode != VoiceMode::poly)
        {
            heldNotes.remove (midiNote);
            auto& v = voices[0];
            if (! v.isActive() || ! v.held)
                return;

            if (heldNotes.empty())
                v.release (settings);
            else if (v.note == midiNote)
                v.setGlideTarget (heldNotes.back());
            return;
        }

        for (auto& v : voices)
            if (v.isActive() && v.held && v.note == midiNote)
                v.release (settings);
    }

    void allNotesOff (bool immediate)
    {
        heldNotes.clear();
        for (auto& v : voices)
        {
            if (immediate)
                v.kill();
            else if (v.isActive())
                v.release (settings);
        }
    }

    //==========================================================================
    struct Transport
    {
        double bpm = 120.0;
        bool playing = false;
        double ppq = 0.0;
    };

    void render (float* left, float* right, int numSamples, const Transport& transport, int sampleOffset = 0)
    {
        if (! prepared)
            prepare (sampleRate);

        Voice::Shared sh;
        sh.bpm = std::clamp (transport.bpm > 0.0 ? transport.bpm : 120.0, 20.0, 999.0);
        sh.beatsInc = sh.bpm / 60.0 / sampleRate;
        sh.pitchBend = pitchWheel * settings.bendRange;
        sh.modWheel = modWheel;
        sh.hostChaosSeed = hostChaosSeed;
        sh.glideCoef = settings.glideMs > 0.0f ? 1.0 - std::exp (-1.0 / (settings.glideMs * 0.001 * 0.4 * sampleRate)) : 1.0;

        const float fast = (float) (1.0 - std::exp (-1.0 / (0.005 * sampleRate)));
        const float slow = (float) (1.0 - std::exp (-1.0 / (0.015 * sampleRate)));

        sh.neededSources = 0;
        for (int k = 0; k < numSlots; ++k)
            if (settings.slotDest[(size_t) k] != Dest::off)
                sh.neededSources |= 1u << std::clamp (settings.slotSource[(size_t) k], 0, numModSources - 1);

        // Stéréo seulement si quelque chose écarte les canaux (sinon un seul filtre par voix)
        sh.stereo = (settings.wtOn && settings.unison > 1 && settings.base[(size_t) Dest::wtSpread] > 0.0f)
                  || std::abs (settings.base[(size_t) Dest::wtPan] - 0.5f) > 1.0e-4f
                  || (settings.algoOn && std::abs (settings.base[(size_t) Dest::algoPan] - 0.5f) > 1.0e-4f);
        for (int k = 0; k < numSlots; ++k)
        {
            const auto d = settings.slotDest[(size_t) k];
            sh.stereo = sh.stereo || d == Dest::wtPan || d == Dest::algoPan || d == Dest::wtSpread;
        }
        for (auto d : settings.mainLfo.dest)
            sh.stereo = sh.stereo || d == Dest::wtPan || d == Dest::algoPan || d == Dest::wtSpread;

        for (auto& v : voices)
            if (v.isActive())
                v.updateTimes (settings);

        for (int i = 0; i < numSamples; ++i)
        {
            smoothed.step (settings, fast, slow);
            sh.hostBeats = transport.playing ? std::max (0.0, transport.ppq + (double) (sampleOffset + i) * sh.beatsInc) : freeBeats;
            sh.hostSeconds = sh.hostBeats * 60.0 / sh.bpm;

            float l = 0.0f, r = 0.0f;
            for (auto& v : voices)
                v.render (settings, smoothed, sh, l, r, noiseState);

            const float outL = l - dcInL + dcCoef * dcOutL;
            const float outR = r - dcInR + dcCoef * dcOutR;
            dcInL = l; dcInR = r;
            dcOutL = std::isfinite (outL) ? outL : 0.0f;
            dcOutR = std::isfinite (outR) ? outR : 0.0f;

            left[i] = dcOutL;
            right[i] = dcOutR;
            freeBeats += sh.beatsInc;
            lastHostBeats = sh.hostBeats;
        }

        updatePlayhead();
    }

    int getPlayheadStep() const noexcept                  { return playheadStep; }
    double getPlayheadBeats() const noexcept              { return playheadBeats; }
    float getLiveOffset (Dest d) const noexcept           { return liveOffsets[(size_t) d]; }
    /** Modulation des effets (dernière voix jouée, gardée quand plus aucune voix ne joue). */
    float getFxOffset (Dest d) const noexcept             { return fxOffsets[(size_t) d]; }

    int activeVoiceCount() const
    {
        int n = 0;
        for (auto& v : voices) n += v.isActive() ? 1 : 0;
        return n;
    }

private:
    double lastPlayedPitch() const
    {
        const Voice* latest = nullptr;
        for (auto& v : voices)
            if (v.isActive() && (latest == nullptr || v.startOrder > latest->startOrder))
                latest = &v;
        return latest != nullptr ? latest->currentNote : (double) lastNote;
    }

    void updatePlayhead()
    {
        const Voice* latest = nullptr;
        for (auto& v : voices)
            if (v.isActive() && (latest == nullptr || v.startOrder > latest->startOrder))
                latest = &v;

        playheadStep = latest != nullptr && settings.seq.on ? latest->lastStep : -1;
        playheadBeats = latest == nullptr ? -1.0 : (settings.seq.hostSync ? lastHostBeats : latest->seqBeats);
        if (latest != nullptr)
            liveOffsets = fxOffsets = latest->offsets;   // les effets gardent la dernière modulation pendant les queues
        else
            liveOffsets.fill (0.0f);
    }

    double sampleRate = 48000.0;
    bool prepared = false;
    Settings settings;
    Smoothed smoothed;
    std::array<Voice, maxVoices> voices {};

    struct NoteStack
    {
        std::array<int, 128> notes {};
        int count = 0;
        bool empty() const noexcept { return count == 0; }
        void clear() noexcept       { count = 0; }
        int back() const noexcept   { return notes[(size_t) (count - 1)]; }
        void remove (int n) noexcept
        {
            int w = 0;
            for (int i = 0; i < count; ++i)
                if (notes[(size_t) i] != n) notes[(size_t) w++] = notes[(size_t) i];
            count = w;
        }
        void push (int n) noexcept  { remove (n); if (count < 128) notes[(size_t) count++] = n; }
    };

    NoteStack heldNotes;
    uint64_t orderCounter = 0;
    int lastNote = -1;
    Rng noteRng { 0xC0FFEEull };
    uint64_t noiseState = 0x2545F4914F6CDD1Dull;
    uint64_t hostChaosSeed = 0x1234567ull;
    float pitchWheel = 0.0f, modWheel = 0.0f;
    double freeBeats = 0.0, freeSeconds = 0.0, lastHostBeats = 0.0;
    float dcCoef = 0.999f, dcInL = 0.0f, dcInR = 0.0f, dcOutL = 0.0f, dcOutR = 0.0f;
    int playheadStep = -1;
    double playheadBeats = -1.0;
    std::array<float, numDests> liveOffsets {}, fxOffsets {};
};

} // namespace seqs
