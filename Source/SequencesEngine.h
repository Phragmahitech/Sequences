#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

/**
    Moteur de Sequences (sans JUCE, testable seul).

    Une voix = opérateur FM (porteuse + modulante avec feedback) + oscillateur analogique (polyBLEP)
    + bruit -> distorsion (avant ou après le filtre) -> filtre d'état (LP / BP / HP, 12 ou 24 dB)
    -> ampli. Deux enveloppes ADSR (ampli, modulation).

    Deux séquenceurs de LFO : une rangée de 1 à 32 pas, chaque pas a une valeur (0..1) et un drapeau
    « glide » (la valeur glisse vers celle du pas suivant pendant le pas). Le séquenceur avance au
    tempo de l'hôte ; chaque séquenceur envoie sa valeur vers 3 destinations, avec une profondeur ±.
    En mode NOTE, chaque voix a sa propre position, remise à zéro à chaque note (pas au legato).
    En mode HOST, toutes les voix lisent la position du morceau (horloge libre quand l'hôte est arrêté).
*/
namespace seqs
{

constexpr int maxSteps = 32;
constexpr int maxVoices = 8;
constexpr int numSeqs = 2;
constexpr int numSlots = 3;
constexpr double twoPi = 6.283185307179586;
constexpr double pi = 3.141592653589793;

enum class Dest : int
{
    off = 0, cutoff, resonance, fmIndex, fmRatio, pitch, fine, pulseWidth, drive, amp,
    fmLevel, oscLevel, noise, pan, feedback, count
};

enum class Wave : int { saw = 0, square, triangle, sine };
enum class FilterType : int { lowpass = 0, bandpass, highpass };
enum class DriveType : int { soft = 0, fold, crush };

struct SeqSettings
{
    bool on = false;
    int steps = 16;
    double stepBeats = 0.25;     // durée d'un pas en noires (triolet / pointé compris)
    double swing = 0.0;          // 0..0.75 : le pas pair dure (1 + swing), l'impair (1 - swing)
    float smooth = 0.0f;         // 0..1 : lissage, en fraction de la durée du pas
    bool hostSync = false;       // false = NOTE (repart à chaque note), true = HOST (position du morceau)
    std::array<Dest, numSlots> dest { Dest::off, Dest::off, Dest::off };
    std::array<float, numSlots> amount {};
    std::array<float, maxSteps> value {};
    std::array<bool, maxSteps> glide {};
};

struct Settings
{
    // Voix
    bool mono = false;
    float glideMs = 60.0f;
    float velocity = 0.5f;       // 0 = la vélocité ne compte pas, 1 = pleine plage
    int transpose = 0;

    // FM
    float fmLevel = 0.7f, fmRatio = 2.0f, fmIndex = 3.0f, fmFeedback = 0.0f, fmEnv = 0.3f;

    // Oscillateur
    float oscLevel = 0.4f;
    Wave oscWave = Wave::saw;
    int oscOctave = 0;
    float oscDetune = 0.0f;      // centièmes
    float oscPw = 0.5f;

    float noise = 0.0f;

    // Filtre
    FilterType filterType = FilterType::lowpass;
    bool slope24 = true;
    float cutoff = 1200.0f;      // Hz
    float resonance = 0.35f;     // 0..1
    float filterEnv = 0.3f;      // -1..1 -> ±6 octaves
    float keyTrack = 0.5f;

    // Enveloppes (secondes, sustain 0..1)
    float ampA = 0.001f, ampD = 0.3f, ampS = 0.8f, ampR = 0.08f;
    float modA = 0.001f, modD = 0.18f, modS = 0.0f, modR = 0.1f;

    // Distorsion
    DriveType driveType = DriveType::soft;
    float drive = 0.0f;
    bool drivePost = false;

    float level = 1.0f;          // gain linéaire de sortie

    std::array<SeqSettings, numSeqs> seq {};
};

//==============================================================================
/** Position dans un séquenceur : pas en cours, avancée dans le pas (0..1) et durée du pas (noires). */
struct StepPos
{
    int step = 0;
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

    if (r < first)
    {
        pos.frac = r / first;
        pos.lengthBeats = first;
    }
    else
    {
        ++index;
        r -= first;
        pos.frac = second > 0.0 ? r / second : 0.0;
        pos.lengthBeats = second;
    }

    pos.step = (int) (index % steps);
    pos.frac = std::clamp (pos.frac, 0.0, 1.0);
    return pos;
}

/** Valeur brute du séquenceur (avant lissage) : la valeur du pas, ou un glissement vers le suivant. */
inline float rawSeqValue (const SeqSettings& s, const StepPos& p)
{
    const int n = std::clamp (s.steps, 1, maxSteps);
    const float a = s.value[(size_t) p.step];
    if (! s.glide[(size_t) p.step])
        return a;

    const float b = s.value[(size_t) ((p.step + 1) % n)];
    return a + (b - a) * (float) p.frac;
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

//==============================================================================
/** Motifs aléatoires « musicaux » pour un séquenceur : valeurs + glides sur les 32 pas. */
struct RandomPattern
{
    enum Style { levels = 0, walk, ramps, accents, mirror, chaos, numStyles };

    static const char* styleName (int s)
    {
        static const char* names[] = { "LEVELS", "WALK", "RAMPS", "ACCENTS", "MIRROR", "CHAOS" };
        return names[std::clamp (s, 0, (int) numStyles - 1)];
    }

    /** Remplit values / glides. steps = longueur actuelle (le motif est pensé pour elle,
        les pas au-delà prolongent le motif). Renvoie le style tiré. */
    static int generate (Rng& rng, int steps, std::array<float, maxSteps>& values, std::array<bool, maxSteps>& glides,
                         int forcedStyle = -1)
    {
        steps = std::clamp (steps, 1, maxSteps);
        const int style = forcedStyle >= 0 ? std::min (forcedStyle, (int) numStyles - 1) : rng.range (0, numStyles - 1);
        double glideChance = 0.15;

        switch (style)
        {
            case levels:
            {
                // Quelques niveaux bien distincts, sauts francs
                const int numLevels = rng.range (3, 6);
                for (int i = 0; i < maxSteps; ++i)
                    values[(size_t) i] = (float) rng.range (0, numLevels - 1) / (float) (numLevels - 1);
                glideChance = 0.1;
                break;
            }

            case walk:
            {
                // Marche aléatoire : la valeur bouge un peu à chaque pas
                float v = rng.uniformf();
                const float stepSize = 0.12f + 0.3f * rng.uniformf();
                for (int i = 0; i < maxSteps; ++i)
                {
                    values[(size_t) i] = v;
                    v += (rng.uniformf() * 2.0f - 1.0f) * stepSize;
                    if (v < 0.0f) v = -v;
                    if (v > 1.0f) v = 2.0f - v;
                    v = std::clamp (v, 0.0f, 1.0f);
                }
                glideChance = 0.35;
                break;
            }

            case ramps:
            {
                // Rampes qui montent ou descendent sur 2, 3, 4, 6 ou 8 pas (polyrythmie contre la mesure)
                static const int periods[] = { 2, 3, 4, 6, 8 };
                const int period = periods[rng.range (0, 4)];
                const bool up = rng.chance (0.5);
                const float lo = 0.1f * rng.uniformf(), hi = 0.7f + 0.3f * rng.uniformf();
                for (int i = 0; i < maxSteps; ++i)
                {
                    float t = (float) (i % period) / (float) std::max (1, period - 1);
                    if (! up) t = 1.0f - t;
                    values[(size_t) i] = lo + (hi - lo) * t;
                }
                glideChance = 0.6;
                break;
            }

            case accents:
            {
                // Base basse + pics répartis à la façon d'un rythme euclidien
                const int pulses = std::max (1, rng.range (std::max (1, steps / 5), std::max (1, steps / 2)));
                const int rotation = rng.range (0, steps - 1);
                const float base = 0.05f + 0.25f * rng.uniformf();
                for (int i = 0; i < maxSteps; ++i)
                {
                    const int k = (i + rotation) % steps;
                    const bool hit = ((k * pulses) % steps) < pulses;
                    values[(size_t) i] = hit ? 0.65f + 0.35f * rng.uniformf() : base + 0.1f * rng.uniformf();
                }
                glideChance = 0.08;
                break;
            }

            case mirror:
            {
                // Un motif court répété, la seconde moitié en miroir
                const int half = std::max (2, (steps + 1) / 2);
                std::array<float, maxSteps> motif {};
                for (int i = 0; i < half; ++i)
                    motif[(size_t) i] = rng.uniformf();
                for (int i = 0; i < maxSteps; ++i)
                {
                    const int j = i % (2 * half);
                    values[(size_t) i] = j < half ? motif[(size_t) j] : motif[(size_t) (2 * half - 1 - j)];
                }
                glideChance = 0.2;
                break;
            }

            default:
            {
                // Suite logistique : chaotique mais pas un bruit blanc
                double x = 0.1 + 0.8 * rng.uniform();
                const double r = 3.6 + 0.399 * rng.uniform();
                for (int i = 0; i < maxSteps; ++i)
                {
                    x = r * x * (1.0 - x);
                    values[(size_t) i] = (float) std::clamp (x, 0.0, 1.0);
                }
                glideChance = 0.15;
                break;
            }
        }

        for (int i = 0; i < maxSteps; ++i)
            glides[(size_t) i] = rng.chance (glideChance);

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

    inline double polyBlep (double t, double dt)
    {
        if (t < dt)
        {
            t /= dt;
            return t + t - t * t - 1.0;
        }
        if (t > 1.0 - dt)
        {
            t = (t - 1.0) / dt;
            return t * t + t + t + 1.0;
        }
        return 0.0;
    }

    inline float noteToHz (double note)     { return (float) (440.0 * std::exp2 ((note - 69.0) / 12.0)); }
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
    Stage getStage() const noexcept               { return stage; }

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
                // Le sustain peut bouger pendant que la note est tenue
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
        // -80 dB au bout de la durée choisie
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
            case FilterType::bandpass: return v1 * k;      // gain 1 à la fréquence de coupure
            case FilterType::highpass: return x - k * v1 - v2;
            case FilterType::lowpass:
            default:                   return v2;
        }
    }
};

//==============================================================================
/** Valeurs lissées partagées par toutes les voix (évite les « escaliers » quand on tourne un potard). */
struct Smoothed
{
    float fmLevel = 0, fmRatio = 1, fmIndex = 0, fmFeedback = 0, fmEnv = 0;
    float oscLevel = 0, oscPw = 0.5f, oscDetune = 0, noise = 0;
    float logCutoff = 10, resonance = 0, filterEnv = 0, keyTrack = 0;
    float drive = 0, level = 1;

    void snap (const Settings& s)
    {
        fmLevel = s.fmLevel; fmRatio = s.fmRatio; fmIndex = s.fmIndex; fmFeedback = s.fmFeedback; fmEnv = s.fmEnv;
        oscLevel = s.oscLevel; oscPw = s.oscPw; oscDetune = s.oscDetune; noise = s.noise;
        logCutoff = std::log2 (std::max (1.0f, s.cutoff)); resonance = s.resonance; filterEnv = s.filterEnv;
        keyTrack = s.keyTrack; drive = s.drive; level = s.level;
    }

    float logCutoffTarget = 10;

    void setTargets (const Settings& s)              { logCutoffTarget = std::log2 (std::max (1.0f, s.cutoff)); }

    void step (const Settings& s, float c)
    {
        auto f = [c] (float& v, float target) { v += (target - v) * c; };
        f (fmLevel, s.fmLevel); f (fmRatio, s.fmRatio); f (fmIndex, s.fmIndex); f (fmFeedback, s.fmFeedback); f (fmEnv, s.fmEnv);
        f (oscLevel, s.oscLevel); f (oscPw, s.oscPw); f (oscDetune, s.oscDetune); f (noise, s.noise);
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
        reset();
    }

    void reset()
    {
        ampEnv.reset();
        modEnv.reset();
        note = -1;
        held = false;
        carrierPhase = modPhase = oscPhase = 0.0;
        fbPrev1 = fbPrev2 = 0.0;
        triState = -1.0;
        for (auto& f : filters) f.reset();
        crushHold = 0.0f;
        crushCounter = 0.0;
        for (auto& v : seqSmoothed) v = 0.0f;
        seqBeats = 0.0;
    }

    bool isActive() const noexcept           { return ampEnv.isActive(); }
    bool isReleasing() const noexcept        { return ampEnv.isReleasing(); }

    /** retrigger = enveloppes et séquenceurs repartent ; sinon legato (seule la hauteur change). */
    void start (int midiNote, float vel, const Settings& s, bool retrigger, bool glide, uint64_t order, float panPosition)
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
                // Voix au repos : phases à zéro -> attaques identiques d'une note à l'autre
                carrierPhase = modPhase = 0.0;
                oscPhase = 0.0;
                fbPrev1 = fbPrev2 = 0.0;
                triState = -1.0;
                for (auto& f : filters) f.reset();
            }
            ampEnv.noteOn (s.ampA, s.ampD, s.ampS);
            modEnv.noteOn (s.modA, s.modD, s.modS);
            ampEnv.setTimes (s.ampA, s.ampD, s.ampS, s.ampR);
            modEnv.setTimes (s.modA, s.modD, s.modS, s.modR);
            seqBeats = 0.0;
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

    /** Durées des enveloppes : suivent les potards même pendant une note. */
    void updateTimes (const Settings& s)
    {
        ampEnv.setTimes (s.ampA, s.ampD, s.ampS, s.ampR);
        modEnv.setTimes (s.modA, s.modD, s.modS, s.modR);
    }

    void setGlideTarget (int midiNote)       { note = midiNote; targetNote = (double) midiNote; }

    /** Rend un échantillon (stéréo, ajouté à l'entrée). beatsInc = noires par échantillon,
        hostBeats = position commune (mode HOST). */
    void render (const Settings& s, const Smoothed& sm, float& outL, float& outR, double beatsInc, double hostBeats,
                 double glideCoef, float pitchBend, uint64_t& noiseState)
    {
        if (! isActive())
            return;

        // --- Hauteur (glide) ----------------------------------------------------
        currentNote += (targetNote - currentNote) * glideCoef;

        // --- Séquenceurs de LFO -------------------------------------------------
        std::array<float, (size_t) Dest::count> add {};
        float ampGain = 1.0f;

        for (int q = 0; q < numSeqs; ++q)
        {
            const auto& seq = s.seq[(size_t) q];
            if (! seq.on)
                continue;

            const double beats = seq.hostSync ? hostBeats : seqBeats;
            const auto pos = stepAt (beats, seq.steps, seq.stepBeats, seq.swing);
            lastStep[(size_t) q] = pos.step;
            const float raw = rawSeqValue (seq, pos);

            // Lissage : 0,5 ms minimum (pas de clic), jusqu'à la moitié d'un pas
            const double stepSeconds = beatsInc > 0.0 ? pos.lengthBeats / (beatsInc * sampleRate) : 0.1;
            const double tau = std::max (0.0005, (double) seq.smooth * 0.5 * stepSeconds);
            if (std::abs (tau - lastTau[(size_t) q]) > 1.0e-12)
            {
                lastTau[(size_t) q] = tau;
                seqCoef[(size_t) q] = (float) (1.0 - std::exp (-1.0 / (tau * sampleRate)));
            }
            const float coef = seqCoef[(size_t) q];
            float& v = seqSmoothed[(size_t) q];
            if (firstSample)
                v = raw;
            else
                v += (raw - v) * coef;

            for (int k = 0; k < numSlots; ++k)
            {
                const auto d = seq.dest[(size_t) k];
                const float amt = seq.amount[(size_t) k];
                if (d == Dest::off || std::abs (amt) < 1.0e-6f)
                    continue;

                if (d == Dest::amp)
                    ampGain *= amt > 0.0f ? 1.0f - amt * (1.0f - v) : 1.0f + amt * v;
                else
                    add[(size_t) d] += amt * v;
            }
        }

        firstSample = false;
        seqBeats += beatsInc;

        auto mod = [&add] (Dest d) { return add[(size_t) d]; };

        // --- Enveloppes ---------------------------------------------------------
        const float ampLevel = ampEnv.next();
        const float modLevel = modEnv.next();

        // --- Hauteur finale -----------------------------------------------------
        const double pitchMod = std::round (mod (Dest::pitch) * 24.0f) + mod (Dest::fine) * 12.0f;
        const double playedNote = currentNote + s.transpose + pitchBend + pitchMod;
        const double nyquist = 0.5 * sampleRate;
        const double freq = std::min ((double) detail::noteToHz (playedNote), nyquist * 0.98);

        // --- FM ----------------------------------------------------------------
        double fm = 0.0;
        const float fmLevel = std::clamp (sm.fmLevel + mod (Dest::fmLevel), 0.0f, 1.0f);
        if (fmLevel > 0.0f)
        {
            const double ratio = std::clamp ((double) sm.fmRatio * std::exp2 (3.0 * mod (Dest::fmRatio)), 0.0625, 32.0);
            const double modFreq = freq * ratio;
            const double index = std::clamp ((double) sm.fmIndex + 10.0 * (sm.fmEnv * modLevel + mod (Dest::fmIndex)), 0.0, 20.0);
            const double fb = std::clamp ((double) sm.fmFeedback + mod (Dest::feedback), 0.0, 1.0);

            double modOut = 0.0;
            if (modFreq < nyquist)
                modOut = std::sin (twoPi * modPhase + fb * 1.4 * 0.5 * (fbPrev1 + fbPrev2));
            fbPrev2 = fbPrev1;
            fbPrev1 = modOut;

            fm = std::sin (twoPi * carrierPhase + index * modOut) * fmLevel;

            modPhase += modFreq / sampleRate;
            modPhase -= std::floor (modPhase);
        }
        carrierPhase += freq / sampleRate;
        carrierPhase -= std::floor (carrierPhase);

        // --- Oscillateur analogique ----------------------------------------------
        double osc = 0.0;
        const float oscLevel = std::clamp (sm.oscLevel + mod (Dest::oscLevel), 0.0f, 1.0f);
        {
            const double oscFreq = std::min (freq * std::exp2 ((double) s.oscOctave + sm.oscDetune / 1200.0), nyquist * 0.98);
            const double dt = oscFreq / sampleRate;

            if (oscLevel > 0.0f)
            {
                const double t = oscPhase;
                switch (s.oscWave)
                {
                    case Wave::saw:
                        osc = 2.0 * t - 1.0 - detail::polyBlep (t, dt);
                        break;
                    case Wave::square:
                    case Wave::triangle:
                    {
                        const double pw = s.oscWave == Wave::triangle ? 0.5
                                                                      : std::clamp ((double) sm.oscPw + 0.45 * mod (Dest::pulseWidth), 0.05, 0.95);
                        double sq = t < pw ? 1.0 : -1.0;
                        sq += detail::polyBlep (t, dt);
                        double t2 = t - pw;
                        if (t2 < 0.0) t2 += 1.0;
                        sq -= detail::polyBlep (t2, dt);

                        if (s.oscWave == Wave::square)
                            osc = sq;
                        else
                        {
                            // Triangle = carré intégré (fuite légère pour rester centré)
                            triState = triState * 0.9995 + 4.0 * dt * sq;
                            osc = triState;
                        }
                        break;
                    }
                    case Wave::sine:
                    default:
                        osc = std::sin (twoPi * t);
                        break;
                }
                osc *= oscLevel;
            }

            oscPhase += dt;
            oscPhase -= std::floor (oscPhase);
        }

        // --- Bruit ----------------------------------------------------------------
        double noise = 0.0;
        const float noiseLevel = std::clamp (sm.noise + mod (Dest::noise), 0.0f, 1.0f);
        if (noiseLevel > 0.0f)
        {
            noiseState ^= noiseState << 13;
            noiseState ^= noiseState >> 7;
            noiseState ^= noiseState << 17;
            noise = ((double) (noiseState >> 11) * (2.0 / 9007199254740992.0) - 1.0) * noiseLevel;
        }

        double x = 0.6 * (fm + osc + noise);

        // --- Distorsion / filtre ----------------------------------------------------
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

        // --- Ampli + panoramique -------------------------------------------------------
        const float velGain = 1.0f - s.velocity * (1.0f - velocity);
        const float gain = ampLevel * velGain * std::max (0.0f, ampGain);
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
    std::array<int, numSeqs> lastStep {};

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
                // Décimation (jusqu'à ÷32) + réduction de bits (16 -> 3)
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
    double carrierPhase = 0.0, modPhase = 0.0, oscPhase = 0.0;
    double fbPrev1 = 0.0, fbPrev2 = 0.0, triState = -1.0;
    std::array<Svf, 2> filters {};
    float crushHold = 0.0f;
    double crushCounter = 0.0;
    std::array<float, numSeqs> seqSmoothed {}, seqCoef {};
    std::array<double, numSeqs> lastTau { -1.0, -1.0 };
    double seqBeats = 0.0;
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

    //==========================================================================
    void noteOn (int midiNote, float velocity)
    {
        midiNote = std::clamp (midiNote, 0, 127);
        ++orderCounter;

        if (settings.mono)
        {
            heldNotes.remove (midiNote);
            const bool legato = ! heldNotes.empty() && voices[0].isActive() && voices[0].held;
            heldNotes.push (midiNote);

            for (size_t i = 1; i < voices.size(); ++i)
                if (voices[i].isActive()) voices[i].release (settings);

            voices[0].start (midiNote, velocity, settings, ! legato, legato, orderCounter, 0.0f);
            return;
        }

        // Polyphonique : la même note rejoue sur sa voix, sinon une voix libre, sinon la plus ancienne
        Voice* chosen = nullptr;
        for (auto& v : voices)
            if (v.isActive() && v.note == midiNote) { chosen = &v; break; }

        if (chosen == nullptr)
            for (auto& v : voices)
                if (! v.isActive()) { chosen = &v; break; }

        if (chosen == nullptr)
        {
            // La plus ancienne en relâchement, sinon la plus ancienne tout court
            for (int pass = 0; pass < 2 && chosen == nullptr; ++pass)
                for (auto& v : voices)
                    if ((pass == 1 || v.isReleasing()) && (chosen == nullptr || v.startOrder < chosen->startOrder))
                        chosen = &v;
        }

        // Petit étalement stéréo des voix
        static const float spread[] = { 0.0f, -0.18f, 0.18f, -0.09f, 0.09f, -0.27f, 0.27f, 0.0f };
        const auto index = (size_t) (chosen - voices.data());
        chosen->start (midiNote, velocity, settings, true, false, orderCounter, spread[index % 8]);
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
                v.setGlideTarget (heldNotes.back());   // legato vers la dernière note encore tenue
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
    /** Transport : tempo (BPM), lecture en cours, position (noires) au premier échantillon rendu. */
    struct Transport
    {
        double bpm = 120.0;
        bool playing = false;
        double ppq = 0.0;
    };

    /** Rend numSamples échantillons dans left/right (écrasés). sampleOffset : décalage depuis le début
        du bloc pour lequel transport.ppq est donné. */
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

            const double hostBeats = transport.playing ? transport.ppq + (double) (sampleOffset + i) * beatsInc : freeBeats;
            float l = 0.0f, r = 0.0f;

            for (auto& v : voices)
                v.render (settings, smoothed, l, r, beatsInc, hostBeats, glideCoef, pitchBend, noiseState);

            // Coupe le continu (FM avec feedback, distorsion) : passe-haut d'un pôle à ~8 Hz
            const float outL = l - dcInL + dcCoef * dcOutL;
            const float outR = r - dcInR + dcCoef * dcOutR;
            dcInL = l; dcInR = r;
            dcOutL = std::isfinite (outL) ? outL : 0.0f;
            dcOutR = std::isfinite (outR) ? outR : 0.0f;

            left[i] = dcOutL * smoothed.level;
            right[i] = dcOutR * smoothed.level;
            freeBeats += beatsInc;
        }

        updatePlayhead();
    }

    /** Pas en cours affiché pour un séquenceur (voix la plus récente ; -1 si rien ne joue). */
    int getPlayheadStep (int seqIndex) const noexcept     { return playhead[(size_t) std::clamp (seqIndex, 0, numSeqs - 1)]; }

    int activeVoiceCount() const
    {
        int n = 0;
        for (auto& v : voices) n += v.isActive() ? 1 : 0;
        return n;
    }

    double getSampleRate() const noexcept                 { return sampleRate; }

private:
    void updatePlayhead()
    {
        const Voice* latest = nullptr;
        for (auto& v : voices)
            if (v.isActive() && (latest == nullptr || v.startOrder > latest->startOrder))
                latest = &v;

        for (int q = 0; q < numSeqs; ++q)
            playhead[(size_t) q] = (latest != nullptr && settings.seq[(size_t) q].on) ? latest->lastStep[(size_t) q] : -1;
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
    uint64_t noiseState = 0x2545F4914F6CDD1Dull;
    float pitchBend = 0.0f;
    double freeBeats = 0.0;
    float dcCoef = 0.999f, dcInL = 0.0f, dcInR = 0.0f, dcOutL = 0.0f, dcOutR = 0.0f;
    std::array<int, numSeqs> playhead { -1, -1 };
};

} // namespace seqs
