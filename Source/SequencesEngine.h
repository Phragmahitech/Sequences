#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include "Wavetable.h"

/**
    Moteur de Sequences (sans JUCE, testable seul).

    Une voix = 2 oscillateurs wavetable (A et B) calculés en suréchantillonnage x2, chacun avec
    OCT (octaves), COARSE (demi-tons, continu : il glisse quand on le tourne ou le module), FINE,
    POS (position dans la table) et un WARP à la façon de Serum : SYNC, FM / AM / RM depuis l'autre
    oscillateur (A est modulé par B, B par A). + bruit -> distorsion (avant ou après le filtre)
    -> filtre d'état (LP / BP / HP, 12 ou 24 dB) -> volume. Deux enveloppes ADSR (ENV 1 = ampli, ENV 2).

    Modulation :
      - une banque de 6 LFO et 2 CHAOS, un séquenceur qui choisit la source à chaque pas (SEQ),
        plus ENV 2 et la vélocité ;
      - 16 routages source -> destination. Une modulation s'ajoute à la position du potard
        (0..1, sur toute sa course), comme dans Serum : +100 % = toute la course vers la droite.
    En mode NOTE, le temps des LFO part de chaque note (pas au legato). En mode HOST, il suit le morceau.
*/
namespace seqs
{

constexpr int maxSteps = 32;
constexpr int maxVoices = 8;
constexpr int numLfos = 6;
constexpr int numChaos = 2;
constexpr int numSources = numLfos + numChaos;   // sources du séquenceur : 1..6 = LFO, 7..8 = CHAOS (0 = OFF)
constexpr int numSlots = 16;
constexpr int numOscs = 2;
constexpr double twoPi = 6.283185307179586;
constexpr double pi = 3.141592653589793;

/** Sources d'un routage. */
enum ModSource : int { srcSeq = 0, srcLfo1 = 1, srcChaos1 = 7, srcEnv2 = 9, srcVelocity = 10, numModSources = 11 };

/** Destinations : chacune correspond à un potard de l'interface. */
enum class Dest : int
{
    off = 0,
    aLevel, aPos, aCoarse, aFine, aWarp,
    bLevel, bPos, bCoarse, bFine, bWarp,
    noise, pitch, pan, volume,
    cutoff, resonance, drive,
    count
};
constexpr int numDests = (int) Dest::count;

enum class Warp : int { off = 0, sync, fm, am, rm, count };
enum class LfoShape : int { sine = 0, triangle, saw, ramp, square, pulse, expDown, expUp, count };
enum class FilterType : int { lowpass = 0, bandpass, highpass };
enum class DriveType : int { soft = 0, fold, crush };

/** Position du potard (0..1) -> valeur réelle, pour chaque destination. */
struct DestRange
{
    static float toReal (Dest d, float n)
    {
        if (d == Dest::aCoarse || d == Dest::bCoarse)   return -48.0f + 96.0f * n;          // demi-tons
        if (d == Dest::aFine || d == Dest::bFine)       return -100.0f + 200.0f * n;        // centièmes
        if (d == Dest::pitch)                           return -24.0f + 48.0f * n;          // demi-tons
        if (d == Dest::pan)                             return -1.0f + 2.0f * n;
        if (d == Dest::volume)                          { const float g = n / 0.8f; return g * g; }   // gain, 0.8 = 0 dB
        if (d == Dest::cutoff)                          return 20.0f * std::pow (1000.0f, n);  // Hz
        return n;
    }

    static float toNorm (Dest d, float v)
    {
        if (d == Dest::aCoarse || d == Dest::bCoarse)   return (v + 48.0f) / 96.0f;
        if (d == Dest::aFine || d == Dest::bFine)       return (v + 100.0f) / 200.0f;
        if (d == Dest::pitch)                           return (v + 24.0f) / 48.0f;
        if (d == Dest::pan)                             return (v + 1.0f) * 0.5f;
        if (d == Dest::volume)                          return 0.8f * std::sqrt (std::max (0.0f, v));
        if (d == Dest::cutoff)                          return std::log (std::max (20.0f, v) / 20.0f) / std::log (1000.0f);
        return v;
    }

    /** Hauteurs : lissage plus long, pour que COARSE glisse quand on le tourne. */
    static bool isPitch (Dest d)  { return d == Dest::aCoarse || d == Dest::bCoarse || d == Dest::pitch; }
};

struct LfoSettings
{
    double periodBeats = 1.0;    // durée d'un cycle, en noires
    LfoShape shape = LfoShape::sine;
    float amount = 1.0f;         // 0..1
    int steps = 0;               // 0 = continu, sinon nombre de marches par cycle
    float phase = 0.0f;          // 0..1
    bool unipolar = false;       // false : -1..1, true : 0..1
};

struct ChaosSettings
{
    double periodBeats = 0.25;   // une nouvelle valeur par période
    float amount = 1.0f;
    float smooth = 0.0f;         // 0 = marches nettes, 1 = glisse sur toute la période
};

struct SeqSettings
{
    bool on = true;
    int steps = 16;
    double stepBeats = 0.5;      // durée d'un pas en noires (triolet / pointé compris)
    double swing = 0.0;          // 0..0.75
    bool hostSync = false;       // false = NOTE, true = HOST
    bool restart = true;         // chaque pas fait repartir sa source de sa phase
    float xfadeMs = 2.0f;        // fondu entre deux sources
    int manual = 1;              // source jouée quand le séquenceur est éteint (1..8)
    std::array<int, maxSteps> pattern {};   // 0 = OFF, 1..8 = source
};

struct OscSettings
{
    const Wavetable* table = nullptr;   // nullptr = table intégrée BASIC SHAPES
    int octave = 0;
    Warp warp = Warp::off;
};

struct Settings
{
    /** Position (0..1) de chaque potard modulable. */
    std::array<float, numDests> base {};

    std::array<OscSettings, numOscs> osc {};

    bool mono = false;
    float glideMs = 60.0f;
    float velocity = 0.5f;       // sensibilité à la vélocité

    FilterType filterType = FilterType::lowpass;
    bool slope24 = true;
    float keyTrack = 0.5f;

    float ampA = 0.001f, ampD = 0.3f, ampS = 0.8f, ampR = 0.08f;
    float modA = 0.001f, modD = 0.18f, modS = 0.0f, modR = 0.1f;

    DriveType driveType = DriveType::soft;
    bool drivePost = false;

    std::array<LfoSettings, numLfos> lfo {};
    std::array<ChaosSettings, numChaos> chaos {};
    SeqSettings seq {};

    std::array<int, numSlots> slotSource {};
    std::array<Dest, numSlots> slotDest {};
    std::array<float, numSlots> slotAmount {};   // -1..1

    Settings()
    {
        slotDest.fill (Dest::off);
        auto set = [this] (Dest d, float real) { base[(size_t) d] = DestRange::toNorm (d, real); };
        set (Dest::aLevel, 0.8f);  set (Dest::aPos, 0.0f); set (Dest::aCoarse, 0.0f); set (Dest::aFine, 0.0f); set (Dest::aWarp, 0.0f);
        set (Dest::bLevel, 0.0f);  set (Dest::bPos, 0.0f); set (Dest::bCoarse, 0.0f); set (Dest::bFine, 0.0f); set (Dest::bWarp, 0.0f);
        set (Dest::noise, 0.0f);   set (Dest::pitch, 0.0f); set (Dest::pan, 0.0f);    set (Dest::volume, 1.0f);
        set (Dest::cutoff, 20000.0f); set (Dest::resonance, 0.0f); set (Dest::drive, 0.0f);
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
namespace detail
{
    inline float fastTanh (float x)
    {
        x = std::clamp (x, -4.0f, 4.0f);
        const float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }

    inline double noteToHz (double note)     { return 440.0 * std::exp2 ((note - 69.0) / 12.0); }
}

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
            case FilterType::lowpass:
            default:                   return v2;
        }
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

//==============================================================================
class Voice
{
public:
    void prepare (double sr)
    {
        sampleRate = sr;
        ampEnv.setSampleRate (sr);
        modEnv.setSampleRate (sr);
        slotCoef = (float) (1.0 - std::exp (-1.0 / (0.0004 * sr)));   // anti-clic de 0,4 ms
        reset();
    }

    void reset()
    {
        ampEnv.reset();
        modEnv.reset();
        note = -1;
        held = false;
        phase.fill (0.0);
        lastOut.fill (0.0f);
        decimator.reset();
        for (auto& f : filters) f.reset();
        crushHold = 0.0f;
        crushCounter = 0.0;
        slotValue.fill (0.0f);
        offsets.fill (0.0f);
        seqBeats = 0.0;
        lastStep = -1;
    }

    bool isActive() const noexcept           { return ampEnv.isActive(); }
    bool isReleasing() const noexcept        { return ampEnv.isReleasing(); }

    void start (int midiNote, float vel, const Settings& s, bool retrigger, bool glide, uint64_t order, float panPosition,
                uint64_t newChaosSeed)
    {
        const bool wasActive = isActive();
        note = midiNote;
        targetNote = (double) midiNote;
        if (! glide || ! wasActive)
            currentNote = targetNote;

        held = true;
        startOrder = order;
        spread = panPosition;

        if (retrigger || ! wasActive)
        {
            velocity = std::clamp (vel, 0.0f, 1.0f);
            if (! wasActive)
            {
                // Voix au repos : phases à zéro -> attaques identiques d'une note à l'autre
                phase.fill (0.0);
                lastOut.fill (0.0f);
                decimator.reset();
                for (auto& f : filters) f.reset();
            }
            ampEnv.noteOn (s.ampA, s.ampD, s.ampS);
            modEnv.noteOn (s.modA, s.modD, s.modS);
            ampEnv.setTimes (s.ampA, s.ampD, s.ampS, s.ampR);
            modEnv.setTimes (s.modA, s.modD, s.modS, s.modR);
            seqBeats = 0.0;
            chaosSeed = newChaosSeed;
            firstSample = true;
        }
    }

    void release (const Settings& s)
    {
        held = false;
        ampEnv.noteOff (s.ampR);
        modEnv.noteOff (s.modR);
    }

    void kill()                              { reset(); }

    void updateTimes (const Settings& s)
    {
        ampEnv.setTimes (s.ampA, s.ampD, s.ampS, s.ampR);
        modEnv.setTimes (s.modA, s.modD, s.modS, s.modR);
    }

    void setGlideTarget (int midiNote)       { note = midiNote; targetNote = (double) midiNote; }

    /** neededSources : bit k = la source k sert dans un routage. */
    void render (const Settings& s, const Smoothed& sm, float& outL, float& outR, double beatsInc, double bpm,
                 double hostBeats, double glideCoef, float pitchBend, uint64_t& noiseState, uint64_t hostChaosSeed,
                 uint32_t neededSources)
    {
        if (! isActive())
            return;

        currentNote += (targetNote - currentNote) * glideCoef;

        const float ampLevel = ampEnv.next();
        const float modLevel = modEnv.next();

        // --- Modulation ---------------------------------------------------------
        offsets.fill (0.0f);
        {
            const bool host = s.seq.hostSync;
            const double t = host ? hostBeats : seqBeats;
            const uint64_t seed = host ? hostChaosSeed : chaosSeed;

            std::array<float, numModSources> value {};
            {
                // Le séquenceur tourne toujours (tête de lecture affichée même sans routage)
                const auto out = Mod::seq (s, t, bpm, seed);
                value[srcSeq] = out.value;
                lastStep = out.step;
            }
            for (int k = srcLfo1; k < srcEnv2; ++k)
                if (neededSources & (1u << k))
                    value[(size_t) k] = Mod::source (s, k, t, t, seed);
            value[srcEnv2] = modLevel;
            value[srcVelocity] = velocity;

            for (int k = 0; k < numSlots; ++k)
            {
                const auto d = s.slotDest[(size_t) k];
                float& sv = slotValue[(size_t) k];
                if (d == Dest::off)
                {
                    sv = 0.0f;
                    continue;
                }

                const int src = std::clamp (s.slotSource[(size_t) k], 0, numModSources - 1);
                const float target = s.slotAmount[(size_t) k] * value[(size_t) src];
                sv = firstSample ? target : sv + (target - sv) * slotCoef;
                offsets[(size_t) d] += sv;
            }
        }

        firstSample = false;
        seqBeats += beatsInc;

        auto norm = [&] (Dest d) { return std::clamp (sm.base[(size_t) d] + offsets[(size_t) d], 0.0f, 1.0f); };
        auto real = [&] (Dest d) { return DestRange::toReal (d, norm (d)); };

        // --- Oscillateurs (suréchantillonnés x2) -----------------------------------
        const double osRate = 2.0 * sampleRate;
        const double baseNote = currentNote + pitchBend + real (Dest::pitch);

        struct OscFrame
        {
            const Wavetable* table;
            double hz, inc, framePos, levelPos, syncRatio;
            float warp, level;
            Warp mode;
        };
        std::array<OscFrame, numOscs> o {};

        for (int i = 0; i < numOscs; ++i)
        {
            const auto& os = s.osc[(size_t) i];
            const Dest coarse = i == 0 ? Dest::aCoarse : Dest::bCoarse;
            const Dest fine = i == 0 ? Dest::aFine : Dest::bFine;
            const Dest pos = i == 0 ? Dest::aPos : Dest::bPos;
            const Dest warp = i == 0 ? Dest::aWarp : Dest::bWarp;
            const Dest level = i == 0 ? Dest::aLevel : Dest::bLevel;

            auto& f = o[(size_t) i];
            f.table = os.table != nullptr ? os.table : BuiltinTables::get (0);
            f.mode = os.warp;
            f.warp = norm (warp);
            f.level = real (level);

            f.hz = std::min (detail::noteToHz (baseNote + 12.0 * os.octave + real (coarse) + real (fine) * 0.01), osRate * 0.45);
            f.inc = f.hz / osRate;
            f.syncRatio = f.mode == Warp::sync ? std::exp2 (4.0 * f.warp) : 1.0;
            f.framePos = (double) norm (pos) * (double) (f.table->numFrames - 1);
        }

        for (int i = 0; i < numOscs; ++i)
        {
            // Niveau de la table : harmoniques jusqu'à ~0,75 fe (le décimateur coupe au-dessus de 0,5 fe).
            // En FM, chaque harmonique h balaie jusqu'à h × (f + 2π·D·fm) : on prend moins d'harmoniques.
            auto& f = o[(size_t) i];
            double effective = f.hz * f.syncRatio;
            if (f.mode == Warp::fm && f.warp > 0.0f)
            {
                const double fm = o[(size_t) (1 - i)].hz;
                effective = f.hz + twoPi * fmDepth * (double) f.warp * fm + fm;
            }
            f.levelPos = std::clamp (std::log2 (std::max (1.0e-9, effective * 1024.0 / (0.75 * sampleRate))), 0.0, (double) (wt::numLevels - 1));
        }

        std::array<float, 2> sub {};
        for (int n = 0; n < 2; ++n)
        {
            // B d'abord (il lit la dernière sortie de A), puis A (il lit la sortie de B qui vient d'être calculée)
            float outs[numOscs] {};
            for (int i = numOscs - 1; i >= 0; --i)
            {
                const auto& f = o[(size_t) i];
                const float other = i == 0 ? outs[1] : lastOut[0];

                double p = phase[(size_t) i];
                if (f.mode == Warp::sync)
                    p = p * f.syncRatio;
                else if (f.mode == Warp::fm)
                    p += fmDepth * f.warp * other;
                p -= std::floor (p);

                float v = f.table->read (f.framePos, f.levelPos, p);
                if (f.mode == Warp::am)
                    v *= 1.0f - f.warp * 0.5f * (1.0f - other);
                else if (f.mode == Warp::rm)
                    v *= (1.0f - f.warp) + f.warp * other;

                outs[i] = v;
                phase[(size_t) i] += f.inc;
                phase[(size_t) i] -= std::floor (phase[(size_t) i]);
            }

            lastOut[0] = outs[0];
            lastOut[1] = outs[1];
            sub[(size_t) n] = outs[0] * o[0].level + outs[1] * o[1].level;
        }

        double x = decimator.process (sub[0], sub[1]);

        // --- Bruit ------------------------------------------------------------------
        const float noiseLevel = real (Dest::noise);
        if (noiseLevel > 0.0f)
        {
            noiseState ^= noiseState << 13;
            noiseState ^= noiseState >> 7;
            noiseState ^= noiseState << 17;
            x += ((double) (noiseState >> 11) * (2.0 / 9007199254740992.0) - 1.0) * noiseLevel;
        }

        x *= 0.5;

        // --- Distorsion / filtre ------------------------------------------------------
        const float driveAmount = real (Dest::drive);
        if (! s.drivePost)
            x = shape (x, s.driveType, driveAmount);

        {
            const double tracked = (double) real (Dest::cutoff) * std::exp2 (s.keyTrack * (currentNote - 60.0) / 12.0);
            const double cutoff = std::clamp (tracked, 16.0, sampleRate * 0.45);
            const double g = std::tan (pi * cutoff / sampleRate);
            const double k = 2.0 - 1.96 * (double) real (Dest::resonance);
            x = filters[0].process (x, g, k, s.filterType);
            if (s.slope24)
                x = filters[1].process (x, g, s.filterType == FilterType::bandpass ? std::max (k, 0.7) : std::max (k, 1.0), s.filterType);
        }

        if (s.drivePost)
            x = shape (x, s.driveType, driveAmount);

        // --- Volume + panoramique ------------------------------------------------------
        const float velGain = 1.0f - s.velocity * (1.0f - velocity);
        const float gain = ampLevel * velGain * real (Dest::volume);
        const float p = std::clamp (spread + real (Dest::pan), -1.0f, 1.0f);
        if (std::abs (p - cachedPan) > 1.0e-7f)
        {
            cachedPan = p;
            const float angle = (float) (0.25 * pi) * (p + 1.0f);
            panL = std::cos (angle) * 1.41421356f;
            panR = std::sin (angle) * 1.41421356f;
        }
        const float y = (float) x * gain;
        outL += y * panL;
        outR += y * panR;

        if (! std::isfinite (y))
            reset();
    }

    /** FM : excursion de phase maximale, en cycles, à WARP = 100 %. */
    static constexpr double fmDepth = 3.0;

    int note = -1;
    bool held = false;
    uint64_t startOrder = 0;
    int lastStep = -1;
    double seqBeats = 0.0;
    std::array<float, numDests> offsets {};   // décalage de modulation de chaque potard (dernier échantillon)

private:
    double shape (double x, DriveType type, float amount)
    {
        if (amount <= 0.0f)
            return x;

        switch (type)
        {
            case DriveType::fold:
            {
                const double g = 1.0 + 7.0 * amount;
                const double folded = std::sin (g * x * 1.5707963267948966);
                const double mix = std::min (1.0, (double) amount * 5.0);
                return x + (folded - x) * mix;
            }
            case DriveType::crush:
            {
                const double factor = 1.0 + (double) amount * amount * 31.0;
                crushCounter += 1.0;
                if (crushCounter >= factor)
                {
                    crushCounter -= factor;
                    const double levels = std::exp2 (16.0 - 13.0 * amount);
                    crushHold = (float) (std::round (x * levels) / levels);
                }
                return crushHold;
            }
            case DriveType::soft:
            default:
            {
                const float g = 1.0f + 30.0f * amount;
                return detail::fastTanh ((float) x * g) / detail::fastTanh (g) * (1.0f - 0.25f * amount);
            }
        }
    }

    double sampleRate = 48000.0;
    Envelope ampEnv, modEnv;
    double targetNote = 60.0, currentNote = 60.0;
    float velocity = 1.0f, spread = 0.0f;
    float cachedPan = 2.0f, panL = 1.0f, panR = 1.0f;
    std::array<double, numOscs> phase {};
    std::array<float, numOscs> lastOut {};
    HalfbandDecimator decimator;
    std::array<Svf, 2> filters {};
    float crushHold = 0.0f;
    double crushCounter = 0.0;
    std::array<float, numSlots> slotValue {};
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
        freeBeats = 0.0;
        dcCoef = (float) std::exp (-twoPi * 8.0 / sampleRate);
        dcInL = dcInR = dcOutL = dcOutR = 0.0f;
        prepared = true;
    }

    void reset()
    {
        for (auto& v : voices)
            v.reset();
        heldNotes.clear();
        pitchBend = 0.0f;
        dcInL = dcInR = dcOutL = dcOutR = 0.0f;
    }

    void setSettings (const Settings& s)
    {
        const bool modeChanged = s.mono != settings.mono;
        settings = s;
        if (modeChanged)
            allNotesOff (true);
    }

    const Settings& getSettings() const noexcept     { return settings; }

    void setHostChaosSeed (uint64_t seed) noexcept    { hostChaosSeed = seed; }

    //==========================================================================
    void noteOn (int midiNote, float velocity)
    {
        midiNote = std::clamp (midiNote, 0, 127);
        ++orderCounter;
        const uint64_t seed = noteRng.next();

        if (settings.mono)
        {
            heldNotes.remove (midiNote);
            const bool legato = ! heldNotes.empty() && voices[0].isActive() && voices[0].held;
            heldNotes.push (midiNote);

            for (size_t i = 1; i < voices.size(); ++i)
                if (voices[i].isActive()) voices[i].release (settings);

            voices[0].start (midiNote, velocity, settings, ! legato, legato, orderCounter, 0.0f, seed);
            return;
        }

        Voice* chosen = nullptr;
        for (auto& v : voices)
            if (v.isActive() && v.note == midiNote) { chosen = &v; break; }

        if (chosen == nullptr)
            for (auto& v : voices)
                if (! v.isActive()) { chosen = &v; break; }

        if (chosen == nullptr)
        {
            for (int pass = 0; pass < 2 && chosen == nullptr; ++pass)
                for (auto& v : voices)
                    if ((pass == 1 || v.isReleasing()) && (chosen == nullptr || v.startOrder < chosen->startOrder))
                        chosen = &v;
        }

        static const float spreads[] = { 0.0f, -0.18f, 0.18f, -0.09f, 0.09f, -0.27f, 0.27f, 0.0f };
        const auto index = (size_t) (chosen - voices.data());
        chosen->start (midiNote, velocity, settings, true, false, orderCounter, spreads[index % 8], seed);
    }

    void noteOff (int midiNote)
    {
        if (settings.mono)
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

    void setPitchBend (float semitones)               { pitchBend = semitones; }

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

        const double bpm = std::clamp (transport.bpm > 0.0 ? transport.bpm : 120.0, 20.0, 999.0);
        const double beatsInc = bpm / 60.0 / sampleRate;
        const float fast = (float) (1.0 - std::exp (-1.0 / (0.005 * sampleRate)));
        const float slow = (float) (1.0 - std::exp (-1.0 / (0.015 * sampleRate)));
        const double glideCoef = settings.mono && settings.glideMs > 0.0f
                                     ? 1.0 - std::exp (-1.0 / (settings.glideMs * 0.001 * 0.4 * sampleRate))
                                     : 1.0;

        uint32_t needed = 0;
        for (int k = 0; k < numSlots; ++k)
            if (settings.slotDest[(size_t) k] != Dest::off)
                needed |= 1u << std::clamp (settings.slotSource[(size_t) k], 0, numModSources - 1);

        for (auto& v : voices)
            if (v.isActive())
                v.updateTimes (settings);

        for (int i = 0; i < numSamples; ++i)
        {
            smoothed.step (settings, fast, slow);

            const double hostBeats = transport.playing ? std::max (0.0, transport.ppq + (double) (sampleOffset + i) * beatsInc)
                                                       : freeBeats;
            float l = 0.0f, r = 0.0f;

            for (auto& v : voices)
                v.render (settings, smoothed, l, r, beatsInc, bpm, hostBeats, glideCoef, pitchBend, noiseState, hostChaosSeed, needed);

            const float outL = l - dcInL + dcCoef * dcOutL;
            const float outR = r - dcInR + dcCoef * dcOutR;
            dcInL = l; dcInR = r;
            dcOutL = std::isfinite (outL) ? outL : 0.0f;
            dcOutR = std::isfinite (outR) ? outR : 0.0f;

            left[i] = dcOutL;
            right[i] = dcOutR;
            freeBeats += beatsInc;
            lastHostBeats = hostBeats;
        }

        updatePlayhead();
    }

    int getPlayheadStep() const noexcept                  { return playheadStep; }
    double getPlayheadBeats() const noexcept              { return playheadBeats; }
    /** Décalage de modulation actuel d'un potard (voix la plus récente ; 0 si rien ne joue). */
    float getLiveOffset (Dest d) const noexcept           { return liveOffsets[(size_t) d]; }

    int activeVoiceCount() const
    {
        int n = 0;
        for (auto& v : voices) n += v.isActive() ? 1 : 0;
        return n;
    }

private:
    void updatePlayhead()
    {
        const Voice* latest = nullptr;
        for (auto& v : voices)
            if (v.isActive() && (latest == nullptr || v.startOrder > latest->startOrder))
                latest = &v;

        playheadStep = latest != nullptr && settings.seq.on ? latest->lastStep : -1;
        playheadBeats = latest == nullptr ? -1.0 : (settings.seq.hostSync ? lastHostBeats : latest->seqBeats);
        if (latest != nullptr)
            liveOffsets = latest->offsets;
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
    Rng noteRng { 0xC0FFEEull };
    uint64_t noiseState = 0x2545F4914F6CDD1Dull;
    uint64_t hostChaosSeed = 0x1234567ull;
    float pitchBend = 0.0f;
    double freeBeats = 0.0, lastHostBeats = 0.0;
    float dcCoef = 0.999f, dcInL = 0.0f, dcInR = 0.0f, dcOutL = 0.0f, dcOutR = 0.0f;
    int playheadStep = -1;
    double playheadBeats = -1.0;
    std::array<float, numDests> liveOffsets {};
};

} // namespace seqs
