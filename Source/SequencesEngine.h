#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

/**
    Moteur de Sequences (sans JUCE, testable seul).

    Une voix = opérateur FM (porteuse sinus + modulante à 9 formes, dont 3 « Reso », avec feedback)
    + oscillateur analogique avec hard sync (BLEP) + bruit -> distorsion (avant ou après le filtre)
    -> filtre d'état (LP / BP / HP, 12 ou 24 dB) -> ampli. Deux enveloppes ADSR (ampli, modulation).

    Modulation, à la façon du LFO SEQUENCER d'Alaskan Bites :
      - une banque de 6 LFO (période synchro au tempo, forme, quantité, STEP = LFO découpé en marches,
        phase, bipolaire / unipolaire) et 2 CHAOS (aléatoire en marches ou lissé) ;
      - un séquenceur de 1 à 32 pas : chaque pas choisit QUELLE source joue (ou OFF). À chaque pas,
        la source peut repartir de sa phase (RESTART) ; un court fondu évite les clics (XFADE) ;
      - 4 routages : source (SEQ ou une source de la banque) -> destination, profondeur ±.
    En mode NOTE, le temps part de chaque note (pas au legato). En mode HOST, il suit le morceau.
*/
namespace seqs
{

constexpr int maxSteps = 32;
constexpr int maxVoices = 8;
constexpr int numLfos = 6;
constexpr int numChaos = 2;
constexpr int numSources = numLfos + numChaos;   // 1..6 = LFO, 7..8 = CHAOS (0 = OFF / SEQ)
constexpr int numSlots = 4;
constexpr double twoPi = 6.283185307179586;
constexpr double pi = 3.141592653589793;

enum class Dest : int
{
    off = 0, cutoff, resonance, fmIndex, fmRatio, pitch, pulseWidth, drive, amp,
    fmLevel, oscLevel, noise, pan, feedback, sync, timbre, count
};

enum class Wave : int { saw = 0, square, triangle, sine };
enum class ModWave : int { sine = 0, dblSine, saw, square, pulse, sawPulse, reso1, reso2, reso3, count };
enum class LfoShape : int { sine = 0, triangle, saw, ramp, square, pulse, expDown, expUp, count };
enum class FilterType : int { lowpass = 0, bandpass, highpass };
enum class DriveType : int { soft = 0, fold, crush };

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

struct Settings
{
    // Voix
    bool mono = false;
    float glideMs = 60.0f;
    float velocity = 0.5f;
    int transpose = 0;

    // FM
    float fmLevel = 0.7f, fmRatio = 2.0f, fmIndex = 3.0f, fmFeedback = 0.0f, fmEnv = 0.3f;
    ModWave modWave = ModWave::sine;
    float timbre = 0.3f;

    // Oscillateur
    float oscLevel = 0.4f;
    Wave oscWave = Wave::saw;
    int oscOctave = 0;
    float oscDetune = 0.0f;      // centièmes
    float oscPw = 0.5f;
    float oscSync = 0.0f;        // 0..1 -> l'oscillateur esclave monte jusqu'à 4 octaves

    float noise = 0.0f;

    // Filtre
    FilterType filterType = FilterType::lowpass;
    bool slope24 = true;
    float cutoff = 1200.0f;
    float resonance = 0.35f;
    float filterEnv = 0.3f;
    float keyTrack = 0.5f;

    // Enveloppes (secondes, sustain 0..1)
    float ampA = 0.001f, ampD = 0.3f, ampS = 0.8f, ampR = 0.08f;
    float modA = 0.001f, modD = 0.18f, modS = 0.0f, modR = 0.1f;

    // Distorsion
    DriveType driveType = DriveType::soft;
    float drive = 0.0f;
    bool drivePost = false;

    float level = 1.0f;

    // Modulation
    std::array<LfoSettings, numLfos> lfo {};
    std::array<ChaosSettings, numChaos> chaos {};
    SeqSettings seq {};
    std::array<int, numSlots> slotSource {};                  // 0 = SEQ, 1..8 = source de la banque
    std::array<Dest, numSlots> slotDest { Dest::off, Dest::off, Dest::off, Dest::off };
    std::array<float, numSlots> slotAmount {};                 // -1..1
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
            default:                 return (float) std::sin (twoPi * p);
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
            return chaos (s.chaos[(size_t) c], tAbs, chaosSeed + (uint64_t) c * 0x5851F42D4C957F2Dull);
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

    inline float noteToHz (double note)     { return (float) (440.0 * std::exp2 ((note - 69.0) / 12.0)); }

    /** Formes de la modulante FM. p = phase 0..1, timbre 0..1. */
    inline double modWave (ModWave w, double p, double timbre)
    {
        switch (w)
        {
            case ModWave::dblSine:  return 0.57 * (std::sin (twoPi * p) + std::sin (2.0 * twoPi * p));
            case ModWave::saw:      return 2.0 * p - 1.0;
            case ModWave::square:   return p < 0.5 ? 1.0 : -1.0;
            case ModWave::pulse:    return p < 0.5 - 0.42 * timbre ? 1.0 : -1.0;
            case ModWave::sawPulse: return p < 0.5 ? 4.0 * p - 1.0 : -1.0;
            case ModWave::reso1:
            case ModWave::reso2:
            case ModWave::reso3:
            {
                // Résonance façon Casio CZ : un cosinus rapide dans une fenêtre qui se referme
                const double r = 1.0 + 15.0 * timbre;
                double window = 1.0 - p;                                          // RESO 1 : dent de scie
                if (w == ModWave::reso2) window = p < 0.5 ? 2.0 * p : 2.0 - 2.0 * p;   // RESO 2 : triangle
                if (w == ModWave::reso3) window = p < 0.75 ? 1.0 : 4.0 - 4.0 * p;      // RESO 3 : trapèze
                return std::cos (twoPi * r * p) * window;
            }
            case ModWave::sine:
            case ModWave::count:
            default:                return std::sin (twoPi * p);
        }
    }
}

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
/** Oscillateur avec hard sync, sauts corrigés par polyBLEP (sortie retardée d'un échantillon). */
class SyncOscillator
{
public:
    void reset()          { master = slave = 0.0; pending = 0.0; }

    /** dtMaster : fréquence de l'oscillateur maître / fe ; dtSlave : celle de l'esclave (>= maître). */
    double process (Wave wave, double dtMaster, double dtSlave, double pw, bool sync)
    {
        double current = 0.0;   // corrections pour l'échantillon qui sort au prochain appel
        double tStart = 0.0;    // temps déjà parcouru dans cet échantillon (0..1)

        if (sync)
        {
            const double masterNext = master + dtMaster;
            if (masterNext >= 1.0)
            {
                const double ts = std::clamp ((1.0 - master) / dtMaster, 0.0, 1.0);
                advance (wave, dtSlave, pw, tStart, ts, current);
                // Saut vers la phase 0 au moment du sync
                const double before = naive (wave, slave, pw), after = naive (wave, 0.0, pw);
                addStep (after - before, 1.0 - ts, current);
                slave = 0.0;
                tStart = ts;
                master = masterNext - 1.0;
            }
            else
                master = masterNext;
        }
        else
        {
            master = slave;   // reste aligné pour un sync sans clic quand on l'allume
        }

        advance (wave, dtSlave, pw, tStart, 1.0, current);

        const double value = naive (wave, slave, pw) + current;
        const double out = pending;
        pending = value;
        return out;
    }

private:
    static double naive (Wave wave, double p, double pw)
    {
        switch (wave)
        {
            case Wave::saw:      return 2.0 * p - 1.0;
            case Wave::square:   return p < pw ? 1.0 : -1.0;
            case Wave::triangle: return p < 0.5 ? 4.0 * p - 1.0 : 3.0 - 4.0 * p;
            case Wave::sine:
            default:             return std::sin (twoPi * p);
        }
    }

    /** Saut de hauteur h ; d = avance de l'échantillon qui sort ensuite après le saut (0..1). */
    void addStep (double h, double d, double& current)
    {
        d = std::clamp (d, 0.0, 1.0);
        pending += 0.5 * h * d * d;
        current -= 0.5 * h * (1.0 - d) * (1.0 - d);
    }

    /** Avance la phase esclave de t0 à t1 (fractions de l'échantillon), en corrigeant ses sauts. */
    void advance (Wave wave, double dt, double pw, double t0, double t1, double& current)
    {
        double remaining = (t1 - t0) * dt;
        double t = t0;

        for (int guard = 0; guard < 4 && remaining > 0.0; ++guard)
        {
            // Prochain saut naturel : passage par pw (carré) ou fin de cycle
            double edge = 1.0;
            double h = 0.0;
            bool wrap = true;

            if (wave == Wave::square && slave < pw)
            {
                edge = pw;
                wrap = false;
            }

            const double toEdge = edge - slave;
            if (toEdge > remaining)
            {
                slave += remaining;
                return;
            }

            t += toEdge / dt;
            remaining -= toEdge;

            if (wave == Wave::saw)              h = -2.0;
            else if (wave == Wave::square)      h = wrap ? 2.0 : -2.0;

            if (h != 0.0)
                addStep (h, 1.0 - t, current);

            slave = wrap ? 0.0 : edge;
        }

        slave = std::min (slave + remaining, 0.999999);
    }

    double master = 0.0, slave = 0.0, pending = 0.0;
};

//==============================================================================
/** Valeurs lissées partagées par toutes les voix (évite les « escaliers » quand on tourne un potard). */
struct Smoothed
{
    float fmLevel = 0, fmRatio = 1, fmIndex = 0, fmFeedback = 0, fmEnv = 0, timbre = 0;
    float oscLevel = 0, oscPw = 0.5f, oscDetune = 0, oscSync = 0, noise = 0;
    float logCutoff = 10, resonance = 0, filterEnv = 0, keyTrack = 0;
    float drive = 0, level = 1;
    float logCutoffTarget = 10;

    void snap (const Settings& s)
    {
        fmLevel = s.fmLevel; fmRatio = s.fmRatio; fmIndex = s.fmIndex; fmFeedback = s.fmFeedback; fmEnv = s.fmEnv; timbre = s.timbre;
        oscLevel = s.oscLevel; oscPw = s.oscPw; oscDetune = s.oscDetune; oscSync = s.oscSync; noise = s.noise;
        logCutoff = logCutoffTarget = std::log2 (std::max (1.0f, s.cutoff)); resonance = s.resonance; filterEnv = s.filterEnv;
        keyTrack = s.keyTrack; drive = s.drive; level = s.level;
    }

    void setTargets (const Settings& s)              { logCutoffTarget = std::log2 (std::max (1.0f, s.cutoff)); }

    void step (const Settings& s, float c)
    {
        auto f = [c] (float& v, float target) { v += (target - v) * c; };
        f (fmLevel, s.fmLevel); f (fmRatio, s.fmRatio); f (fmIndex, s.fmIndex); f (fmFeedback, s.fmFeedback); f (fmEnv, s.fmEnv);
        f (timbre, s.timbre);
        f (oscLevel, s.oscLevel); f (oscPw, s.oscPw); f (oscDetune, s.oscDetune); f (oscSync, s.oscSync); f (noise, s.noise);
        f (logCutoff, logCutoffTarget); f (resonance, s.resonance); f (filterEnv, s.filterEnv);
        f (keyTrack, s.keyTrack); f (drive, s.drive); f (level, s.level);
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
        carrierPhase = modPhase = 0.0;
        fbPrev1 = fbPrev2 = 0.0;
        osc.reset();
        for (auto& f : filters) f.reset();
        crushHold = 0.0f;
        crushCounter = 0.0;
        slotValue.fill (0.0f);
        seqBeats = 0.0;
        lastStep = -1;
        lastSeqValue = 0.0f;
    }

    bool isActive() const noexcept           { return ampEnv.isActive(); }
    bool isReleasing() const noexcept        { return ampEnv.isReleasing(); }

    /** retrigger = enveloppes et séquenceur repartent ; sinon legato (seule la hauteur change). */
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
        pan = panPosition;

        if (retrigger || ! wasActive)
        {
            velocity = std::clamp (vel, 0.0f, 1.0f);
            if (! wasActive)
            {
                carrierPhase = modPhase = 0.0;
                fbPrev1 = fbPrev2 = 0.0;
                osc.reset();
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

    void render (const Settings& s, const Smoothed& sm, float& outL, float& outR, double beatsInc, double bpm,
                 double hostBeats, double glideCoef, float pitchBend, uint64_t& noiseState, uint64_t hostChaosSeed)
    {
        if (! isActive())
            return;

        currentNote += (targetNote - currentNote) * glideCoef;

        // --- Modulation ---------------------------------------------------------
        std::array<float, (size_t) Dest::count> add {};
        float ampGain = 1.0f;
        {
            const bool host = s.seq.hostSync;
            const double t = host ? hostBeats : seqBeats;
            const uint64_t seed = host ? hostChaosSeed : chaosSeed;

            // Le séquenceur tourne toujours (tête de lecture affichée même sans routage)
            const auto out = Mod::seq (s, t, bpm, seed);
            const float seqValue = out.value;
            lastStep = out.step;
            lastSeqValue = out.value;

            for (int k = 0; k < numSlots; ++k)
            {
                const auto d = s.slotDest[(size_t) k];
                const float amt = s.slotAmount[(size_t) k];
                float target = 0.0f;

                if (d != Dest::off && std::abs (amt) > 1.0e-6f)
                {
                    const int src = s.slotSource[(size_t) k];
                    const float v = src == 0 ? seqValue : Mod::source (s, src, t, t, seed);
                    target = amt * v;
                }

                float& sv = slotValue[(size_t) k];
                sv = firstSample ? target : sv + (target - sv) * slotCoef;

                if (d == Dest::amp && std::abs (amt) > 1.0e-6f)
                    ampGain *= std::clamp (amt > 0.0f ? 1.0f - amt + sv : 1.0f + sv, 0.0f, 1.0f);
                else if (d != Dest::off)
                    add[(size_t) d] += sv;
            }
        }

        firstSample = false;
        seqBeats += beatsInc;

        auto mod = [&add] (Dest d) { return add[(size_t) d]; };

        const float ampLevel = ampEnv.next();
        const float modLevel = modEnv.next();

        // --- Hauteur -------------------------------------------------------------
        const double playedNote = currentNote + s.transpose + pitchBend + 24.0 * mod (Dest::pitch);
        const double nyquist = 0.5 * sampleRate;
        const double freq = std::min ((double) detail::noteToHz (playedNote), nyquist * 0.98);

        // --- FM ------------------------------------------------------------------
        double fm = 0.0;
        const float fmLevel = std::clamp (sm.fmLevel + mod (Dest::fmLevel), 0.0f, 1.0f);
        if (fmLevel > 0.0f)
        {
            const double ratio = std::clamp ((double) sm.fmRatio * std::exp2 (3.0 * mod (Dest::fmRatio)), 0.0625, 32.0);
            const double modFreq = freq * ratio;
            const double index = std::clamp ((double) sm.fmIndex + 10.0 * (sm.fmEnv * modLevel + mod (Dest::fmIndex)), 0.0, 20.0);
            const double fb = std::clamp ((double) sm.fmFeedback + mod (Dest::feedback), 0.0, 1.0);
            const double timbre = std::clamp ((double) sm.timbre + mod (Dest::timbre), 0.0, 1.0);

            double modOut = 0.0;
            if (modFreq < nyquist)
            {
                double p = modPhase + fb * 1.4 * 0.5 * (fbPrev1 + fbPrev2) / twoPi;
                p -= std::floor (p);
                modOut = detail::modWave (s.modWave, p, timbre);
            }
            fbPrev2 = fbPrev1;
            fbPrev1 = modOut;

            fm = std::sin (twoPi * carrierPhase + index * modOut) * fmLevel;

            modPhase += modFreq / sampleRate;
            modPhase -= std::floor (modPhase);
        }
        carrierPhase += freq / sampleRate;
        carrierPhase -= std::floor (carrierPhase);

        // --- Oscillateur (hard sync) ---------------------------------------------------
        double analog = 0.0;
        const float oscLevel = std::clamp (sm.oscLevel + mod (Dest::oscLevel), 0.0f, 1.0f);
        {
            const double masterFreq = std::min (freq * std::exp2 ((double) s.oscOctave + sm.oscDetune / 1200.0), nyquist * 0.9);
            const double syncAmount = std::clamp ((double) sm.oscSync + mod (Dest::sync), 0.0, 1.0);
            const double slaveFreq = std::min (masterFreq * std::exp2 (4.0 * syncAmount), nyquist * 0.9);
            const double pw = std::clamp ((double) sm.oscPw + 0.45 * mod (Dest::pulseWidth), 0.05, 0.95);
            const double value = osc.process (s.oscWave, masterFreq / sampleRate, slaveFreq / sampleRate, pw, syncAmount > 1.0e-4);
            analog = value * oscLevel;
        }

        // --- Bruit ------------------------------------------------------------------
        double noise = 0.0;
        const float noiseLevel = std::clamp (sm.noise + mod (Dest::noise), 0.0f, 1.0f);
        if (noiseLevel > 0.0f)
        {
            noiseState ^= noiseState << 13;
            noiseState ^= noiseState >> 7;
            noiseState ^= noiseState << 17;
            noise = ((double) (noiseState >> 11) * (2.0 / 9007199254740992.0) - 1.0) * noiseLevel;
        }

        double x = 0.6 * (fm + analog + noise);

        // --- Distorsion / filtre ------------------------------------------------------
        const float driveAmount = std::clamp (sm.drive + mod (Dest::drive), 0.0f, 1.0f);
        if (! s.drivePost)
            x = shape (x, s.driveType, driveAmount);

        {
            const double octaves = 6.0 * (sm.filterEnv * modLevel + mod (Dest::cutoff))
                                 + sm.keyTrack * (currentNote + s.transpose - 60.0) / 12.0;
            const double cutoff = std::clamp (std::exp2 ((double) sm.logCutoff + octaves), 16.0, sampleRate * 0.45);
            const double g = std::tan (pi * cutoff / sampleRate);
            const double res = std::clamp ((double) sm.resonance + mod (Dest::resonance), 0.0, 1.0);
            const double k = 2.0 - 1.96 * res;
            x = filters[0].process (x, g, k, s.filterType);
            if (s.slope24)
                x = filters[1].process (x, g, s.filterType == FilterType::bandpass ? std::max (k, 0.7) : std::max (k, 1.0), s.filterType);
        }

        if (s.drivePost)
            x = shape (x, s.driveType, driveAmount);

        // --- Ampli + panoramique ------------------------------------------------------
        const float velGain = 1.0f - s.velocity * (1.0f - velocity);
        const float gain = ampLevel * velGain * ampGain;
        const float p = std::clamp (pan + mod (Dest::pan), -1.0f, 1.0f);
        const float angle = (float) (0.25 * pi) * (p + 1.0f);
        const float y = (float) x * gain;
        outL += y * std::cos (angle) * 1.41421356f;
        outR += y * std::sin (angle) * 1.41421356f;

        if (! std::isfinite (y))
            reset();
    }

    int note = -1;
    bool held = false;
    uint64_t startOrder = 0;
    int lastStep = -1;
    float lastSeqValue = 0.0f;
    double seqBeats = 0.0;

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
    float velocity = 1.0f, pan = 0.0f;
    double carrierPhase = 0.0, modPhase = 0.0;
    double fbPrev1 = 0.0, fbPrev2 = 0.0;
    SyncOscillator osc;
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

    /** Graine des CHAOS en mode HOST (le même morceau rejoue le même chaos). */
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

        static const float spread[] = { 0.0f, -0.18f, 0.18f, -0.09f, 0.09f, -0.27f, 0.27f, 0.0f };
        const auto index = (size_t) (chosen - voices.data());
        chosen->start (midiNote, velocity, settings, true, false, orderCounter, spread[index % 8], seed);
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
        const double smoothCoef = 1.0 - std::exp (-1.0 / (0.005 * sampleRate));
        const double glideCoef = settings.mono && settings.glideMs > 0.0f
                                     ? 1.0 - std::exp (-1.0 / (settings.glideMs * 0.001 * 0.4 * sampleRate))
                                     : 1.0;

        for (auto& v : voices)
            if (v.isActive())
                v.updateTimes (settings);

        smoothed.setTargets (settings);

        for (int i = 0; i < numSamples; ++i)
        {
            smoothed.step (settings, (float) smoothCoef);

            const double hostBeats = transport.playing ? std::max (0.0, transport.ppq + (double) (sampleOffset + i) * beatsInc)
                                                       : freeBeats;
            float l = 0.0f, r = 0.0f;

            for (auto& v : voices)
                v.render (settings, smoothed, l, r, beatsInc, bpm, hostBeats, glideCoef, pitchBend, noiseState, hostChaosSeed);

            const float outL = l - dcInL + dcCoef * dcOutL;
            const float outR = r - dcInR + dcCoef * dcOutR;
            dcInL = l; dcInR = r;
            dcOutL = std::isfinite (outL) ? outL : 0.0f;
            dcOutR = std::isfinite (outR) ? outR : 0.0f;

            left[i] = dcOutL * smoothed.level;
            right[i] = dcOutR * smoothed.level;
            freeBeats += beatsInc;
            lastHostBeats = hostBeats;
        }

        updatePlayhead();
    }

    /** Pas en cours (voix la plus récente ; -1 si rien ne joue ou séquenceur éteint). */
    int getPlayheadStep() const noexcept                  { return playheadStep; }
    /** Temps du séquenceur de la voix la plus récente, en noires (-1 si rien ne joue). */
    double getPlayheadBeats() const noexcept              { return playheadBeats; }

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
};

} // namespace seqs
