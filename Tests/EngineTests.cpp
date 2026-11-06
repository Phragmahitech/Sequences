// Tests du moteur de Sequences (sans JUCE).
#include "SequencesEngine.h"
#include "Arp.h"
#include "Fx.h"

#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using namespace seqs;

static int failures = 0, checks = 0;

#define CHECK(cond, ...)                                                    \
    do {                                                                    \
        ++checks;                                                           \
        if (! (cond)) {                                                     \
            ++failures;                                                     \
            std::printf ("ECHEC %s:%d : %s | ", __FILE__, __LINE__, #cond); \
            std::printf (__VA_ARGS__);                                      \
            std::printf ("\n");                                             \
        }                                                                   \
    } while (0)

static bool near (double a, double b, double tol)   { return std::abs (a - b) <= tol; }

static const Wavetable* sineTable()     { return BuiltinTables::get (BuiltinTables::indexOf ("SINE")); }

static void setReal (Settings& s, Dest d, float v)   { s.base[(size_t) d] = DestRange::toNorm (d, v); }

static std::vector<float> render (Engine& e, int n, Engine::Transport t = {}, std::vector<float>* right = nullptr)
{
    std::vector<float> l ((size_t) n), r ((size_t) n);
    int done = 0;
    while (done < n)
    {
        const int block = std::min (512, n - done);
        e.render (l.data() + done, r.data() + done, block, t, done);
        done += block;
    }
    if (right) *right = r;
    return l;
}

static double measureFreq (const std::vector<float>& x, int from, int to, double sr)
{
    double first = -1.0, last = -1.0;
    int count = 0;
    for (int i = std::max (1, from); i < to; ++i)
        if (x[(size_t) i - 1] < 0.0f && x[(size_t) i] >= 0.0f)
        {
            const double t = (double) (i - 1) + (double) (-x[(size_t) i - 1]) / (double) (x[(size_t) i] - x[(size_t) i - 1]);
            if (first < 0.0) first = t; else ++count;
            last = t;
        }
    if (count < 1) return 0.0;
    return (double) count * sr / (last - first);
}

static double rms (const std::vector<float>& x, int from, int to)
{
    double s = 0.0;
    for (int i = from; i < to; ++i) s += (double) x[(size_t) i] * x[(size_t) i];
    return std::sqrt (s / std::max (1, to - from));
}

/** Amplitude d'une fréquence (Goertzel avec fenêtre de Hann). */
static double toneLevel (const std::vector<float>& x, int from, int len, double freq, double sr)
{
    double re = 0.0, im = 0.0, wsum = 0.0;
    for (int i = 0; i < len; ++i)
    {
        const double w = 0.5 - 0.5 * std::cos (twoPi * i / (len - 1));
        const double a = twoPi * freq * i / sr;
        re += w * x[(size_t) (from + i)] * std::cos (a);
        im -= w * x[(size_t) (from + i)] * std::sin (a);
        wsum += w;
    }
    return 2.0 * std::sqrt (re * re + im * im) / wsum;
}

static int periodByAutocorrelation (const std::vector<float>& x, int from, int len, int lo, int hi)
{
    int best = lo;
    double bestScore = -1e30;
    for (int lag = lo; lag <= hi; ++lag)
    {
        double sum = 0.0, e1 = 0.0, e2 = 0.0;
        for (int i = from; i < from + len; ++i)
        {
            sum += (double) x[(size_t) i] * x[(size_t) (i + lag)];
            e1 += (double) x[(size_t) i] * x[(size_t) i];
            e2 += (double) x[(size_t) (i + lag)] * x[(size_t) (i + lag)];
        }
        const double score = sum / std::sqrt (e1 * e2 + 1e-12);
        if (score > bestScore) { bestScore = score; best = lag; }
    }
    return best;
}

/** WAVETABLE = sinus pur, tout le reste coupé, filtre ouvert. */
static Settings cleanSine()
{
    Settings s;
    s.table = sineTable();
    s.unison = 1;
    s.pdOn = true;
    s.algoOn = false;
    setReal (s, Dest::wtLevel, 1.0f);
    setReal (s, Dest::pdLevel, 0.0f);
    setReal (s, Dest::algoLevel, 0.0f);
    setReal (s, Dest::wtSpread, 0.0f);
    setReal (s, Dest::cutoff, 20000.0f);
    setReal (s, Dest::resonance, 0.0f);
    setReal (s, Dest::volume, 1.0f);
    s.filterModel = FilterModel::svf;
    s.slope24 = false;
    s.keyTrack = 0.0f;
    s.ampA = 0.0005f; s.ampD = 1.0f; s.ampS = 1.0f; s.ampR = 0.01f;
    s.velocity = 0.0f;
    s.seq.on = false;
    s.seq.manual = 1;
    return s;
}

static Settings onlyPd (PdWave w, float amount)
{
    auto s = cleanSine();
    setReal (s, Dest::wtLevel, 0.0f);
    setReal (s, Dest::pdLevel, 1.0f);
    s.pdWave = w;
    setReal (s, Dest::pdAmount, amount);
    return s;
}

static std::vector<float> play (Settings s, int note, int samples, double sr = 48000.0, std::vector<float>* right = nullptr, float velocity = 1.0f)
{
    Engine e;
    e.setSettings (s);
    e.prepare (sr);
    e.noteOn (note, velocity);
    return render (e, samples, {}, right);
}

static double brightness (const std::vector<float>& x, size_t from = 2400)
{
    double d = 0.0, e = 0.0;
    for (size_t i = from; i < x.size(); ++i) { d += (double) (x[i] - x[i - 1]) * (x[i] - x[i - 1]); e += (double) x[i] * x[i]; }
    return std::sqrt (d / (e + 1e-20));
}

/** Énergie hors des harmoniques de f0, en dB par rapport aux harmoniques. */
static double inharmonicDb (const std::vector<float>& x, int from, double f0, double sr)
{
    const int n = 32768;
    std::vector<std::complex<double>> spec ((size_t) n);
    for (int i = 0; i < n; ++i)
    {
        const double win = 0.35875 - 0.48829 * std::cos (twoPi * i / n) + 0.14128 * std::cos (2 * twoPi * i / n) - 0.01168 * std::cos (3 * twoPi * i / n);
        spec[(size_t) i] = { x[(size_t) (from + i)] * win, 0.0 };
    }
    wt::fft (spec, false);
    double harmonic = 0.0, other = 0.0;
    for (int b = 1; b < n / 2; ++b)
    {
        const double f = b * sr / n;
        const double nearest = std::round (f / f0) * f0;
        (std::abs (f - nearest) < 12.0 ? harmonic : other) += std::norm (spec[(size_t) b]);
    }
    return 10.0 * std::log10 (other / harmonic);
}

static void testStepPositions()
{
    CHECK (stepAt (0.0, 16, 0.25, 0.0).step == 0, "pas 0");
    CHECK (stepAt (0.25, 16, 0.25, 0.0).step == 1, "pas 1");
    CHECK (stepAt (3.99, 16, 0.25, 0.0).step == 15, "pas 15");
    CHECK (stepAt (4.0, 16, 0.25, 0.0).step == 0, "boucle");
    CHECK (near (stepAt (0.125, 16, 0.25, 0.0).frac, 0.5, 1e-9), "milieu du pas");
    CHECK (near (stepAt (1.3, 16, 0.25, 0.0).start, 1.25, 1e-9), "début du pas");
    CHECK (stepAt (0.75, 3, 0.25, 0.0).step == 0, "3 pas");
    CHECK (stepAt (0.3, 16, 0.25, 0.5).step == 0, "swing pair");
    CHECK (stepAt (0.38, 16, 0.25, 0.5).step == 1, "swing impair");
    CHECK (near (stepAt (0.38, 16, 0.25, 0.5).start, 0.375, 1e-9), "début du pas swingué");
    CHECK (stepAt (0.5, 16, 0.25, 0.5).step == 2, "swing : la paire dure une croche");
}

static void testLfoShapes()
{
    LfoSettings l;
    l.periodBeats = 1.0;
    l.shape = LfoShape::sine;
    CHECK (near (Mod::lfo (l, 0.25), 1.0, 1e-6), "sinus au quart : %f", Mod::lfo (l, 0.25));
    CHECK (near (Mod::lfo (l, 1.75), -1.0, 1e-6), "sinus, 2e cycle");

    l.shape = LfoShape::saw;
    CHECK (near (Mod::lfo (l, 0.0), -1.0, 1e-6) && near (Mod::lfo (l, 0.5), 0.0, 1e-6), "saw");
    l.shape = LfoShape::ramp;
    CHECK (near (Mod::lfo (l, 0.0), 1.0, 1e-6) && near (Mod::lfo (l, 0.75), -0.5, 1e-6), "ramp");
    l.shape = LfoShape::triangle;
    CHECK (near (Mod::lfo (l, 0.25), 1.0, 1e-6) && near (Mod::lfo (l, 0.75), -1.0, 1e-6) && near (Mod::lfo (l, 0.0), 0.0, 1e-6), "triangle");
    l.shape = LfoShape::square;
    CHECK (Mod::lfo (l, 0.2) == 1.0f && Mod::lfo (l, 0.7) == -1.0f, "carré");
    l.shape = LfoShape::pulse;
    CHECK (Mod::lfo (l, 0.2) == 1.0f && Mod::lfo (l, 0.3) == -1.0f, "pulse");
    l.shape = LfoShape::expDown;
    CHECK (near (Mod::lfo (l, 0.0), 1.0, 1e-5) && near (Mod::lfo (l, 0.9999), -1.0, 1e-3) && Mod::lfo (l, 0.2) < 0.0f, "exp down");
    l.shape = LfoShape::expUp;
    CHECK (near (Mod::lfo (l, 0.0), -1.0, 1e-3) && near (Mod::lfo (l, 0.9999), 1.0, 3e-3), "exp up");

    // Période
    l.shape = LfoShape::saw;
    l.periodBeats = 0.25;
    CHECK (near (Mod::lfo (l, 0.125), 0.0, 1e-6), "période 1/16");

    // STEP : marches
    l.periodBeats = 1.0;
    l.steps = 4;
    CHECK (near (Mod::lfo (l, 0.30), Mod::lfo (l, 0.45), 1e-9), "même marche");
    CHECK (near (Mod::lfo (l, 0.30), -0.5, 1e-6), "valeur de la marche : %f", Mod::lfo (l, 0.30));
    CHECK (! near (Mod::lfo (l, 0.45), Mod::lfo (l, 0.55), 1e-3), "marche suivante");
    l.steps = 0;

    // Phase, quantité, unipolaire
    l.phase = 0.5f;
    CHECK (near (Mod::lfo (l, 0.0), 0.0, 1e-6), "phase");
    l.phase = 0.0f;
    l.amount = 0.5f;
    CHECK (near (Mod::lfo (l, 0.0), -0.5, 1e-6), "quantité");
    l.unipolar = true;
    CHECK (near (Mod::lfo (l, 0.0), 0.0, 1e-6) && near (Mod::lfo (l, 0.9999), 0.5, 1e-3), "unipolaire");
}

static void testChaos()
{
    ChaosSettings c;
    c.periodBeats = 0.5;
    c.smooth = 0.0f;
    const float a = Mod::chaos (c, 0.1, 42), b = Mod::chaos (c, 0.45, 42);
    CHECK (a == b, "constant dans la période");
    CHECK (Mod::chaos (c, 0.55, 42) != a, "change à la période suivante");
    CHECK (Mod::chaos (c, 0.1, 42) == a, "déterministe");
    CHECK (Mod::chaos (c, 0.1, 43) != a, "autre graine, autre suite");

    bool inRange = true;
    double mean = 0.0;
    for (int i = 0; i < 2000; ++i)
    {
        const float v = Mod::chaos (c, i * 0.5 + 0.1, 7);
        inRange = inRange && v >= -1.0f && v <= 1.0f;
        mean += v;
    }
    CHECK (inRange, "dans -1..1");
    CHECK (std::abs (mean / 2000.0) < 0.06, "centré : %f", mean / 2000.0);

    // Lissé : passe continûment d'une valeur à la suivante
    c.smooth = 1.0f;
    float maxJump = 0.0f, prev = Mod::chaos (c, 0.0, 9);
    for (int i = 1; i < 4000; ++i)
    {
        const float v = Mod::chaos (c, i * 0.001, 9);
        maxJump = std::max (maxJump, std::abs (v - prev));
        prev = v;
    }
    CHECK (maxJump < 0.02f, "lissé sans saut : %f", maxJump);

    c.amount = 0.25f;
    CHECK (std::abs (Mod::chaos (c, 3.3, 9)) <= 0.25f, "quantité");
}

static Settings seqSettings()
{
    Settings s = cleanSine();
    s.seq.on = true;
    s.seq.steps = 2;
    s.seq.stepBeats = 0.25;
    s.seq.xfadeMs = 0.0f;
    s.seq.restart = true;
    s.lfo[0].shape = LfoShape::saw;    s.lfo[0].periodBeats = 1.0;
    s.lfo[1].shape = LfoShape::sine;   s.lfo[1].periodBeats = 0.125;
    s.seq.pattern = {};
    s.seq.pattern[0] = 1;
    s.seq.pattern[1] = 2;
    return s;
}

static void testSequencerSources()
{
    auto s = seqSettings();

    // Pas 0 -> LFO 1, temps local depuis le début du pas
    auto o = Mod::seq (s, 0.1, 120.0, 1);
    CHECK (o.step == 0 && o.source == 1, "pas 0, source 1");
    CHECK (near (o.value, Mod::lfo (s.lfo[0], 0.1), 1e-6), "valeur LFO 1");

    // Pas 1 -> LFO 2, RESTART : repart de sa phase au début du pas
    o = Mod::seq (s, 0.25 + 0.03125, 120.0, 1);
    CHECK (o.step == 1 && o.source == 2, "pas 1, source 2");
    CHECK (near (o.value, 1.0, 1e-5), "LFO 2 au quart de son cycle : %f", o.value);

    // Sans RESTART : temps global
    s.seq.restart = false;
    o = Mod::seq (s, 0.3, 120.0, 1);
    CHECK (near (o.value, Mod::lfo (s.lfo[1], 0.3), 1e-6), "sans restart");
    s.seq.restart = true;

    // OFF
    s.seq.pattern[1] = 0;
    CHECK (Mod::seq (s, 0.3, 120.0, 1).value == 0.0f, "pas OFF muet");

    // Séquenceur éteint : source manuelle
    s.seq.on = false;
    s.seq.manual = 2;
    o = Mod::seq (s, 0.03125, 120.0, 1);
    CHECK (o.step == -1 && o.source == 2 && near (o.value, 1.0, 1e-5), "manuel");
    s.seq.on = true;

    // CHAOS dans un pas
    s.seq.pattern[0] = 7;
    o = Mod::seq (s, 0.1, 120.0, 55);
    CHECK (o.source == 7 && o.value == Mod::chaos (s.chaos[0], 0.1, 55), "chaos 1");
}

//==============================================================================
static void testCrossfade()
{
    auto s = seqSettings();
    s.lfo[0].shape = LfoShape::square;  s.lfo[0].periodBeats = 4.0;   // +1 pendant tout le pas
    s.lfo[1].shape = LfoShape::ramp;    s.lfo[1].periodBeats = 4.0;   // +1 au début, descend
    s.lfo[1].phase = 0.5f;                                            // -> 0 au début du pas
    const double bpm = 120.0;                                         // 1 noire = 0,5 s

    // Sans fondu : saut net à la frontière
    float before = Mod::seq (s, 0.25 - 1e-6, bpm, 1).value;
    float after = Mod::seq (s, 0.25 + 1e-6, bpm, 1).value;
    CHECK (std::abs (before - after) > 0.9f, "saut sans fondu : %f -> %f", before, after);

    // Fondu de 10 ms : continu, et terminé après 10 ms
    s.seq.xfadeMs = 10.0f;
    before = Mod::seq (s, 0.25 - 1e-6, bpm, 1).value;
    after = Mod::seq (s, 0.25 + 1e-6, bpm, 1).value;
    CHECK (std::abs (before - after) < 0.01f, "continu avec fondu : %f -> %f", before, after);
    const double tenMs = 0.010 / 0.5;
    CHECK (near (Mod::seq (s, 0.25 + tenMs * 1.01, bpm, 1).value, Mod::lfo (s.lfo[1], tenMs * 1.01), 1e-5), "fondu terminé");

    // Le tout premier pas d'une note n'a pas de fondu
    CHECK (near (Mod::seq (s, 0.0, bpm, 1).value, 1.0, 1e-6), "pas de fondu au départ");
}

static void testRandomPatterns()
{
    std::array<bool, numSources + 1> all {};
    all.fill (true);
    all[0] = false;

    for (int style = 0; style < RandomPattern::numStyles; ++style)
        for (int steps : { 1, 3, 8, 16, 32 })
        {
            Rng a ((uint64_t) (style * 100 + steps)), b ((uint64_t) (style * 100 + steps));
            std::array<int, maxSteps> p1 {}, p2 {};
            CHECK (RandomPattern::generate (a, steps, all, p1, style) == style, "style imposé");
            RandomPattern::generate (b, steps, all, p2, style);
            CHECK (p1 == p2, "même graine, même motif");

            bool valid = true, sound = false, varied = false;
            for (int i = 0; i < maxSteps; ++i)
                valid = valid && p1[(size_t) i] >= 0 && p1[(size_t) i] <= numSources;
            for (int i = 0; i < steps; ++i)
            {
                sound = sound || p1[(size_t) i] != 0;
                varied = varied || p1[(size_t) i] != p1[0];
            }
            CHECK (valid && sound, "motif valide (style %s, %d pas)", RandomPattern::styleName (style), steps);
            CHECK (steps == 1 || varied, "motif varié (style %s, %d pas)", RandomPattern::styleName (style), steps);
        }

    // Seules les sources disponibles sont tirées
    std::array<bool, numSources + 1> some {};
    some[2] = some[5] = some[8] = true;
    Rng rng (5);
    bool onlyAvailable = true;
    for (int run = 0; run < 200; ++run)
    {
        std::array<int, maxSteps> p {};
        RandomPattern::generate (rng, 16, some, p);
        for (int v : p)
            onlyAvailable = onlyAvailable && (v == 0 || some[(size_t) v]);
    }
    CHECK (onlyAvailable, "seulement les sources disponibles");

    std::array<int, RandomPattern::numStyles> seen {};
    std::array<int, maxSteps> p {};
    for (int i = 0; i < 500; ++i)
        ++seen[(size_t) RandomPattern::generate (rng, 16, all, p)];
    for (int sIdx = 0; sIdx < RandomPattern::numStyles; ++sIdx)
        CHECK (seen[(size_t) sIdx] > 50, "style %d tiré %d fois", sIdx, seen[(size_t) sIdx]);
}

static void testWavetableBuilder()
{
    // Une scie de 2048 points : chaque niveau ne garde que ses harmoniques
    std::vector<float> saw (2048);
    for (int i = 0; i < 2048; ++i) saw[(size_t) i] = 1.0f - 2.0f * (float) i / 2048.0f;
    auto t = Wavetable::build ({ saw }, "saw");
    CHECK (t != nullptr && t->numFrames == 1, "table construite");

    for (int level : { 0, 3, 6, 10 })
    {
        const int m = t->size[(size_t) level], harmonics = wt::harmonicsForLevel (level);
        std::vector<std::complex<double>> spec ((size_t) m);
        for (int i = 0; i < m; ++i) spec[(size_t) i] = { t->frame (level, 0)[i], 0.0 };
        wt::fft (spec, false);
        double inBand = 0.0, outBand = 0.0;
        for (int h = 1; h < m / 2; ++h)
            (h <= harmonics ? inBand : outBand) += std::norm (spec[(size_t) h]);
        CHECK (outBand < 1e-12 * inBand, "niveau %d : rien au-dessus de %d harmoniques", level, harmonics);
        CHECK (std::abs (spec[0].real()) < 1e-6 * m, "niveau %d : pas de continu", level);
        CHECK (t->frame (level, 0)[m] == t->frame (level, 0)[0], "niveau %d : échantillon de garde", level);
    }

    float peak = 0.0f;
    for (float v : t->data[0]) peak = std::max (peak, std::abs (v));
    CHECK (near (peak, 1.0, 1e-6), "normalisée : %f", peak);

    // Deux frames : au milieu, on lit la moyenne
    std::vector<float> a (2048), b (2048);
    for (int i = 0; i < 2048; ++i)
    {
        a[(size_t) i] = (float) std::sin (twoPi * i / 2048.0);
        b[(size_t) i] = (float) std::sin (2.0 * twoPi * i / 2048.0);
    }
    auto two = Wavetable::build ({ a, b }, "two", false);
    const double p = 0.1;
    const double expected = 0.5 * (std::sin (twoPi * p) + std::sin (2.0 * twoPi * p));
    CHECK (near (two->read (0.5, 0.0, p), expected, 2e-3), "interpolation entre frames : %f / %f", two->read (0.5, 0.0, p), expected);
    CHECK (near (two->read (0.0, 0.0, 0.25), 1.0, 2e-3), "lecture au quart");

    // Cycles de tailles quelconques (rééchantillonnés), trop de frames (ramenées à 256)
    std::vector<std::vector<float>> many (300, std::vector<float> (600));
    for (auto& c : many) for (size_t i = 0; i < c.size(); ++i) c[i] = (float) std::sin (twoPi * (double) i / 600.0);
    auto big = Wavetable::build (many, "big");
    CHECK (big != nullptr && big->numFrames == 256, "256 frames au plus : %d", big ? big->numFrames : -1);
    CHECK (near (big->read (10.0, 0.0, 0.25), 1.0, 5e-3), "cycle de 600 points rééchantillonné");

    CHECK (Wavetable::build ({}, "vide") == nullptr, "table vide refusée");

    // Tables intégrées
    for (int i = 0; i < BuiltinTables::count(); ++i)
    {
        const auto* bt = BuiltinTables::get (i);
        bool finite = true;
        for (auto& level : bt->data) for (float v : level) finite = finite && std::isfinite (v);
        CHECK (bt->numFrames >= 1 && finite, "table intégrée %s", bt->name.c_str());
    }
}

static std::vector<uint8_t> makeWav (int format, int channels, int bits, const std::vector<float>& samples, const char* clm = nullptr,
                                     bool extensible = false, bool junkFirst = false)
{
    std::vector<uint8_t> out;
    auto put32 = [&out] (uint32_t v) { for (int i = 0; i < 4; ++i) out.push_back ((uint8_t) (v >> (8 * i))); };
    auto put16 = [&out] (uint32_t v) { out.push_back ((uint8_t) v); out.push_back ((uint8_t) (v >> 8)); };
    auto tag = [&out] (const char* t) { out.insert (out.end(), t, t + 4); };

    tag ("RIFF"); put32 (0); tag ("WAVE");
    if (junkFirst) { tag ("JUNK"); put32 (5); for (int i = 0; i < 6; ++i) out.push_back (0); }
    tag ("fmt "); put32 (extensible ? 40 : 16);
    put16 (extensible ? 0xFFFE : (uint32_t) format); put16 ((uint32_t) channels); put32 (48000);
    const int bytes = bits / 8;
    put32 (48000u * (uint32_t) (bytes * channels)); put16 ((uint32_t) (bytes * channels)); put16 ((uint32_t) bits);
    if (extensible) { put16 (22); put16 ((uint32_t) bits); put32 (0); put16 ((uint32_t) format); for (int i = 0; i < 14; ++i) out.push_back (0); }
    if (clm) { const std::string c = clm; tag ("clm "); put32 ((uint32_t) c.size()); out.insert (out.end(), c.begin(), c.end()); if (c.size() & 1) out.push_back (0); }
    tag ("data"); put32 ((uint32_t) (samples.size() * (size_t) bytes));
    for (float v : samples)
    {
        if (format == 3 && bits == 32) { uint32_t u; std::memcpy (&u, &v, 4); put32 (u); }
        else if (format == 3)          { double d = v; uint64_t u; std::memcpy (&u, &d, 8); put32 ((uint32_t) u); put32 ((uint32_t) (u >> 32)); }
        else if (bits == 16)           put16 ((uint32_t) (uint16_t) (int16_t) std::lround (v * 32767.0f));
        else if (bits == 24)           { const int32_t s = (int32_t) std::lround (v * 8388607.0f); out.push_back ((uint8_t) s); out.push_back ((uint8_t) (s >> 8)); out.push_back ((uint8_t) (s >> 16)); }
        else if (bits == 8)            out.push_back ((uint8_t) std::lround (v * 127.0f + 128.0f));
        else                           put32 ((uint32_t) (int32_t) std::lround ((double) v * 2147483647.0));
    }
    const uint32_t riff = (uint32_t) out.size() - 8;
    std::memcpy (out.data() + 4, &riff, 4);
    return out;
}

static std::vector<float> ramp (int n, int cycle)
{
    std::vector<float> v ((size_t) n);
    for (int i = 0; i < n; ++i) v[(size_t) i] = 0.9f * (float) std::sin (twoPi * (double) (i % cycle) / cycle);
    return v;
}

static void testWavParser()
{
    // Format Serum : flottant 32 bits, bloc clm
    {
        std::vector<std::vector<float>> cycles (3, std::vector<float> (2048));
        for (int f = 0; f < 3; ++f) for (int i = 0; i < 2048; ++i) cycles[(size_t) f][(size_t) i] = (float) std::sin ((f + 1) * twoPi * i / 2048.0);
        const auto bytes = WavetableFile::writeSerum (cycles, 2048);
        const auto w = WavetableFile::parse (bytes.data(), bytes.size());
        CHECK (w.error.empty() && w.serumHeader && w.frameSize == 2048 && w.cycles.size() == 3, "Serum 32 bits : %zu frames", w.cycles.size());
        CHECK (near (w.cycles[2][100], cycles[2][100], 1e-6), "valeurs exactes");
        const auto w16 = WavetableFile::parse (WavetableFile::writeSerum (cycles, 2048, 16).data(), WavetableFile::writeSerum (cycles, 2048, 16).size());
        CHECK (w16.cycles.size() == 3 && near (w16.cycles[1][300], cycles[1][300], 1e-4), "Serum 16 bits");
    }

    // clm à 1024, avec un bloc JUNK avant (comme les tables de Serum 2)
    {
        const auto bytes = makeWav (3, 1, 32, ramp (5120, 1024), "<!>1024 11000000 wavetable (www.xferrecords.com)", false, true);
        const auto w = WavetableFile::parse (bytes.data(), bytes.size());
        CHECK (w.frameSize == 1024 && w.cycles.size() == 5, "clm 1024 : %d / %zu", w.frameSize, w.cycles.size());
    }

    // WAV ordinaires
    struct Case { int format, channels, bits; bool ext; };
    for (auto c : { Case { 1, 1, 16, false }, Case { 1, 2, 16, false }, Case { 1, 1, 24, false }, Case { 1, 1, 32, false },
                    Case { 1, 1, 8, false }, Case { 3, 1, 64, false }, Case { 1, 2, 24, true }, Case { 3, 1, 32, true } })
    {
        std::vector<float> samples;
        for (float v : ramp (4096, 2048)) for (int ch = 0; ch < c.channels; ++ch) samples.push_back (v);
        const auto bytes = makeWav (c.format, c.channels, c.bits, samples, nullptr, c.ext);
        const auto w = WavetableFile::parse (bytes.data(), bytes.size());
        CHECK (w.error.empty() && w.frameSize == 2048 && w.cycles.size() == 2 && ! w.serumHeader,
               "format %d, %d canaux, %d bits%s : %s", c.format, c.channels, c.bits, c.ext ? " (ext)" : "", w.error.c_str());
        if (! w.cycles.empty())
            CHECK (near (w.cycles[0][512], 0.9, c.bits == 8 ? 0.02 : 1e-3), "valeur lue : %f", w.cycles[0][512]);
    }

    // Un seul cycle de taille quelconque
    {
        const auto bytes = makeWav (1, 1, 16, ramp (600, 600));
        const auto w = WavetableFile::parse (bytes.data(), bytes.size());
        CHECK (w.cycles.size() == 1 && w.frameSize == 600, "cycle unique de 600");
    }

    // Fichiers abîmés : erreur propre, jamais de plantage
    {
        auto good = makeWav (3, 1, 32, ramp (4096, 2048), "<!>2048 01000000");
        CHECK (! WavetableFile::parse (good.data(), 10).error.empty(), "trop court");
        CHECK (! WavetableFile::parse (nullptr, 0).error.empty(), "vide");
        const std::string text = "ceci n'est pas un wav du tout, vraiment pas";
        CHECK (! WavetableFile::parse ((const uint8_t*) text.data(), text.size()).error.empty(), "pas un wav");

        auto truncated = good;
        truncated.resize (good.size() - 5000);
        const auto w = WavetableFile::parse (truncated.data(), truncated.size());
        CHECK (w.cycles.size() <= 1, "données tronquées : %zu frames", w.cycles.size());

        auto badFmt = makeWav (2, 1, 16, ramp (4096, 2048));   // ADPCM
        CHECK (! WavetableFile::parse (badFmt.data(), badFmt.size()).error.empty(), "format inconnu refusé");

        auto hugeChunk = good;
        hugeChunk[16] = 0xFF; hugeChunk[17] = 0xFF; hugeChunk[18] = 0xFF; hugeChunk[19] = 0x7F;   // taille de fmt délirante
        WavetableFile::parse (hugeChunk.data(), hugeChunk.size());

        auto badClm = makeWav (3, 1, 32, ramp (4096, 2048), "<!>99999999999999999 junk");
        CHECK (WavetableFile::parse (badClm.data(), badClm.size()).frameSize == 2048, "clm absurde ignoré");

        // Mutations au hasard
        Rng rng (77);
        int survived = 0;
        for (int run = 0; run < 3000; ++run)
        {
            auto bytes = good;
            const int edits = rng.range (1, 12);
            for (int k = 0; k < edits; ++k)
                bytes[(size_t) rng.range (0, (int) bytes.size() - 1)] = (uint8_t) rng.range (0, 255);
            if (rng.chance (0.3)) bytes.resize ((size_t) rng.range (0, (int) bytes.size()));
            const auto w2 = WavetableFile::parse (bytes.data(), bytes.size());
            bool finite = true;
            for (auto& c : w2.cycles) for (float v : c) finite = finite && std::isfinite (v) && std::abs (v) <= 8.0f;
            if (! w2.cycles.empty() && finite)
            {
                auto t = Wavetable::build (w2.cycles, "fuzz");
                finite = finite && t != nullptr;
            }
            survived += finite ? 1 : 0;
        }
        CHECK (survived == 3000, "fichiers mutés : %d / 3000 sans valeur aberrante", survived);
    }
}

static void testDecimator()
{
    const auto& h = HalfbandDecimator::coefficients();
    auto response = [&h] (double f)   // f en fraction de la fréquence d'entrée (2 fe)
    {
        double re = 0.0, im = 0.0;
        for (int n = 0; n < HalfbandDecimator::taps; ++n)
        {
            re += h[(size_t) n] * std::cos (twoPi * f * n);
            im -= h[(size_t) n] * std::sin (twoPi * f * n);
        }
        return 20.0 * std::log10 (std::sqrt (re * re + im * im) + 1e-30);
    };

    double passRipple = 0.0, stop = -1e9;
    for (double f = 0.0; f <= 0.22; f += 0.002) passRipple = std::max (passRipple, std::abs (response (f)));
    for (double f = 0.28; f <= 0.5; f += 0.002) stop = std::max (stop, response (f));
    CHECK (passRipple < 0.05, "bande passante plate jusqu'à 0,44 fe : %.4f dB", passRipple);
    CHECK (stop < -80.0, "coupure au-delà de 0,56 fe : %.1f dB", stop);

    for (int k = 2; k < HalfbandDecimator::centre; k += 2)
        CHECK (h[(size_t) (HalfbandDecimator::centre - k)] == 0.0f || std::abs (h[(size_t) (HalfbandDecimator::centre - k)]) < 1e-9f, "coefficient pair nul");
}

//==============================================================================
static void testDestRanges()
{
    for (int d = 1; d < numDests; ++d)
        for (float n : { 0.0f, 0.3f, 0.8f, 1.0f })
        {
            const float real = DestRange::toReal ((Dest) d, n);
            CHECK (std::isfinite (real) && near (DestRange::toNorm ((Dest) d, real), n, 1e-4), "aller-retour dest %d à %.1f", d, n);
        }
    CHECK (near (DestRange::toReal (Dest::volume, 0.8f), 1.0, 1e-6), "volume 0,8 = 0 dB");
}

static void testWavetablePitch()
{
    for (int os : { 1, 2, 4 })
        for (double sr : { 44100.0, 48000.0, 96000.0 })
        {
            auto s = cleanSine();
            s.oversampling = os;
            auto x = play (s, 69, (int) (sr * 0.4), sr);
            const double f = measureFreq (x, (int) (sr * 0.05), (int) (sr * 0.4), sr);
            CHECK (near (f, 440.0, 0.3), "WT 440 Hz (x%d, %.0f Hz) : %.3f", os, sr, f);
        }

    struct Case { int oct; float semi, fine; double expected; const char* name; };
    for (auto c : { Case { 1, 0.0f, 0.0f, 880.0, "octave +1" }, Case { 0, 7.0f, 0.0f, 440.0 * std::pow (2.0, 7.0 / 12.0), "semi +7" },
                    Case { 0, 0.5f, 0.0f, 440.0 * std::pow (2.0, 0.5 / 12.0), "semi +0,5 (continu)" },
                    Case { -1, 0.0f, 50.0f, 220.0 * std::pow (2.0, 0.5 / 12.0), "octave -1 + fine +50" } })
    {
        auto s = cleanSine();
        s.wtOctave = c.oct;
        setReal (s, Dest::wtSemi, c.semi);
        setReal (s, Dest::wtFine, c.fine);
        auto x = play (s, 69, 24000);
        const double f = measureFreq (x, 4800, 24000, 48000.0);
        CHECK (near (f, c.expected, c.expected * 2e-4), "%s : %.3f / %.3f", c.name, f, c.expected);
    }

    // Repliement : scie aiguë (CLASSIC à 50 %), x1, x2, x4
    for (int os : { 1, 2, 4 })
    {
        auto s = cleanSine();
        s.table = BuiltinTables::get (0);
        setReal (s, Dest::wtPos, 0.5f);
        s.oversampling = os;
        auto x = play (s, 100, 48000 + 8192);
        const double db = inharmonicDb (x, 8192, 440.0 * std::pow (2.0, 31.0 / 12.0), 48000.0);
        CHECK (db < (os == 1 ? -45.0 : -60.0), "repliement scie x%d : %.1f dB", os, db);
    }
}

static void testUnison()
{
    std::vector<float> r;
    auto s = cleanSine();
    auto mono = play (s, 57, 24000, 48000.0, &r);
    float diff = 0.0f;
    for (size_t i = 0; i < r.size(); ++i) diff = std::max (diff, std::abs (r[i] - mono[i]));
    CHECK (diff < 1e-6f, "unison 1 : mono");

    s.unison = 5;
    setReal (s, Dest::wtDetune, 0.6f);
    setReal (s, Dest::wtSpread, 1.0f);
    s.wtRandomPhase = true;
    auto l = play (s, 57, 48000, 48000.0, &r);
    double corr = 0.0, el = 0.0, er = 0.0;
    for (size_t i = 4800; i < l.size(); ++i) { corr += l[i] * r[i]; el += l[i] * l[i]; er += r[i] * r[i]; }
    CHECK (corr / std::sqrt (el * er) < 0.9, "unison écarté : corrélation L/R %.2f", corr / std::sqrt (el * er));
    // Le désaccord fait battre : l'enveloppe RMS varie dans le temps
    double lo = 1e9, hi = 0.0;
    for (int k = 0; k < 20; ++k) { const double v = rms (l, 4800 + k * 2000, 4800 + k * 2000 + 1000); lo = std::min (lo, v); hi = std::max (hi, v); }
    CHECK (hi > 1.3 * lo, "unison désaccordé : battements (%.3f / %.3f)", lo, hi);
    float peak = 0.0f;
    for (float v : l) peak = std::max (peak, std::abs (v));
    CHECK (peak < 2.0f, "unison borné : %.2f", peak);
}

static void testWtWarp()
{
    std::array<double, (int) WtWarp::count> hf {};
    for (int w = 0; w < (int) WtWarp::count; ++w)
    {
        auto s = cleanSine();
        s.wtWarp = (WtWarp) w;
        setReal (s, Dest::wtWarp, 0.6f);
        auto x = play (s, 45, 24000);
        hf[(size_t) w] = brightness (x);
        const int period = periodByAutocorrelation (x, 4000, 4000, 300, 600);
        CHECK (std::abs (period - 436) <= 2, "warp %d : la période ne change pas (%d)", w, period);
    }
    for (int w = 1; w < (int) WtWarp::count; ++w)
        CHECK (hf[(size_t) w] > 1.15 * hf[0], "warp %d enrichit le son (%.4f / %.4f)", w, hf[(size_t) w], hf[0]);
}

static void testPhaseDistortion()
{
    // PD AMT 0 : cosinus pur, à la bonne hauteur
    {
        auto x = play (onlyPd (PdWave::saw, 0.0f), 69, 24000);
        CHECK (near (measureFreq (x, 2400, 24000, 48000.0), 440.0, 0.3), "PD à 440 Hz");
        CHECK (toneLevel (x, 4800, 16384, 880.0, 48000.0) < 0.002, "PD AMT 0 : pas d'harmonique 2");
    }
    // Chaque forme : bornée, plus riche quand PD AMT monte
    for (int w = 0; w < (int) PdWave::count; ++w)
    {
        auto low = play (onlyPd ((PdWave) w, 0.1f), 45, 24000);
        auto high = play (onlyPd ((PdWave) w, 0.9f), 45, 24000);
        auto hfOf = [] (const std::vector<float>& x) { return brightness (x); };
        float peak = 0.0f;
        for (float v : high) peak = std::max (peak, std::abs (v));
        CHECK (peak < 1.5f && rms (high, 2400, 24000) > 0.05, "forme PD %d bornée (%.2f)", w, peak);
        CHECK (hfOf (high) > 1.2 * hfOf (low), "forme PD %d : PD AMT enrichit (%.4f / %.4f)", w, hfOf (high), hfOf (low));
    }
    // CRUSH, FOLD, SHAPE changent le son
    for (Dest d : { Dest::pdCrush, Dest::pdFold, Dest::pdShape })
    {
        auto s = onlyPd (PdWave::saw, 0.5f);
        auto a = play (s, 45, 9600);
        setReal (s, d, 0.9f);
        auto b = play (s, 45, 9600);
        double diff = 0.0;
        for (size_t i = 2400; i < a.size(); ++i) diff += std::abs (a[i] - b[i]);
        CHECK (diff / 7200.0 > 0.02, "PD %d change le son (%.3f)", (int) d, diff / 7200.0);
    }
    // Les fonctions de phase restent dans [0, 1] et la lecture est continue
    for (int w = 0; w < (int) PdWave::count; ++w)
        for (double d : { 0.0, 0.5, 1.0 })
        {
            bool finite = true;
            for (int i = 0; i < 1000; ++i) finite = finite && std::isfinite (PhaseDistortion::read ((PdWave) w, i / 1000.0, d, 0.3));
            CHECK (finite, "PD %d fini à %.1f", w, d);
        }
}

static void testPlaits()
{
    // Chaque modèle sonne (les modèles percussifs sur une note), sans valeur aberrante
    for (int m = 0; m < PlaitsModels::count; ++m)
    {
        auto s = cleanSine();
        setReal (s, Dest::wtLevel, 0.0f);
        s.algoOn = true;
        s.algoModel = m;
        setReal (s, Dest::algoLevel, 1.0f);
        auto x = play (s, 48, 24000);
        bool finite = true;
        float peak = 0.0f;
        for (float v : x) { finite = finite && std::isfinite (v); peak = std::max (peak, std::abs (v)); }
        CHECK (finite && peak < 2.0f && rms (x, 0, 24000) > 0.005, "Plaits %s : rms %.4f, crête %.2f", PlaitsModels::name (m), rms (x, 0, 24000), peak);
    }

    // Hauteur juste (VIRTUAL ANALOG, harmonics 0) à 44,1, 48 et 96 kHz
    for (double sr : { 44100.0, 48000.0, 96000.0 })
    {
        auto s = cleanSine();
        setReal (s, Dest::wtLevel, 0.0f);
        s.algoOn = true;
        s.algoModel = 9;      // WAVESHAPING : un seul oscillateur
        setReal (s, Dest::algoHarmonics, 0.0f);
        setReal (s, Dest::algoTimbre, 0.0f);
        setReal (s, Dest::algoMorph, 0.0f);
        setReal (s, Dest::algoLevel, 1.0f);
        auto x = play (s, 45, (int) (sr * 0.5), sr);
        const int expected = (int) std::lround (sr / 110.0);
        const int period = periodByAutocorrelation (x, (int) (sr * 0.1), (int) (sr * 0.1), expected - 40, expected + 40);
        CHECK (std::abs (period - expected) <= 2, "Plaits 110 Hz à %.0f Hz : période %d / %d", sr, period, expected);
    }

    // LPG : le son retombe même note tenue
    {
        auto s = cleanSine();
        setReal (s, Dest::wtLevel, 0.0f);
        s.algoOn = true;
        s.algoModel = 8;
        s.algoLpg = true;
        setReal (s, Dest::algoDecay, 0.2f);
        setReal (s, Dest::algoLevel, 1.0f);
        auto x = play (s, 48, 48000);
        CHECK (rms (x, 200, 2400) > 8.0 * rms (x, 36000, 48000), "LPG : %.4f puis %.4f", rms (x, 200, 2400), rms (x, 36000, 48000));
    }

    // OFF : rien
    {
        auto s = cleanSine();
        setReal (s, Dest::wtLevel, 0.0f);
        s.algoOn = false;
        setReal (s, Dest::algoLevel, 1.0f);
        CHECK (rms (play (s, 48, 4800), 0, 4800) < 1e-6, "ALGO éteint");
    }
}

static void testSubNoise()
{
    auto s = cleanSine();
    setReal (s, Dest::wtLevel, 0.0f);
    setReal (s, Dest::subLevel, 1.0f);
    for (int w = 0; w < (int) SubWave::count; ++w)
    {
        s.subWave = (SubWave) w;
        s.subOctave = -1;
        auto x = play (s, 69, 24000);
        const int period = periodByAutocorrelation (x, 4000, 4000, 180, 250);
        CHECK (std::abs (period - 218) <= 2, "SUB %d à -1 octave : période %d", w, period);
    }

    setReal (s, Dest::subLevel, 0.0f);
    setReal (s, Dest::noiseLevel, 1.0f);
    std::array<double, (int) NoiseType::count> hfRatio {};
    for (int n = 0; n < (int) NoiseType::count; ++n)
    {
        s.noiseType = (NoiseType) n;
        auto x = play (s, 60, 24000);
        double d = 0.0;
        for (size_t i = 1; i < x.size(); ++i) d += std::abs (x[i] - x[i - 1]);
        hfRatio[(size_t) n] = d / (rms (x, 0, 24000) * 24000.0 + 1e-12);
        CHECK (rms (x, 2400, 24000) > 0.02, "bruit %d présent", n);
    }
    CHECK (hfRatio[0] > hfRatio[1] && hfRatio[1] > hfRatio[2], "blanc > rose > brun en aigus (%.2f %.2f %.2f)", hfRatio[0], hfRatio[1], hfRatio[2]);
}

static void testFmMatrix()
{
    // PD -> WT (comme sur la capture d'AB3) : PD coupé dans le mix, il module quand même
    auto make = [] (float depth, FmMode mode)
    {
        auto s = cleanSine();
        setReal (s, Dest::pdLevel, 0.0f);
        s.pdWave = PdWave::saw;
        setReal (s, Dest::pdAmount, 0.0f);
        setReal (s, Dest::pdSemi, 7.0f);
        s.fm[1][0] = 1.0f;
        s.fmMode = mode;
        setReal (s, Dest::fmDepth, depth);
        return s;
    };
    const double side = 440.0 + 440.0 * std::pow (2.0, 7.0 / 12.0);
    auto plain = play (make (0.0f, FmMode::phase), 69, 24000);
    auto fm = play (make (0.1f, FmMode::phase), 69, 24000);
    CHECK (toneLevel (plain, 4800, 16384, side, 48000.0) < 1e-3, "DEPTH 0 : pas de bande latérale");
    CHECK (toneLevel (fm, 4800, 16384, side, 48000.0) > 0.05, "PD -> WT : bande à fc + fm (%.3f)", toneLevel (fm, 4800, 16384, side, 48000.0));
    auto lin = play (make (0.1f, FmMode::linear), 69, 24000);
    double diff = 0.0;
    for (size_t i = 4800; i < lin.size(); ++i) diff += std::abs (lin[i] - fm[i]);
    CHECK (diff / 19200.0 > 0.01, "mode LINEAR différent de PHASE");

    // Toutes les cases, feedback compris : borné
    for (int row = 0; row < numFmRows; ++row)
        for (int col = 0; col < numFmCols; ++col)
        {
            auto s = cleanSine();
            setReal (s, Dest::pdLevel, 0.5f);
            s.fm[(size_t) row][(size_t) col] = 1.0f;
            setReal (s, Dest::fmDepth, 1.0f);
            auto x = play (s, 57, 9600);
            float peak = 0.0f;
            bool finite = true;
            for (float v : x) { finite = finite && std::isfinite (v); peak = std::max (peak, std::abs (v)); }
            CHECK (finite && peak < 2.0f, "case FM %d -> %d bornée (%.2f)", row, col, peak);
        }

    // ALGO dans la matrice : WT -> ALGO (FM par la hauteur) et ALGO -> WT
    {
        auto algoOnly = [] (float depth)
        {
            auto s = cleanSine();
            setReal (s, Dest::wtLevel, 0.0f);
            s.algoOn = true;
            s.algoModel = 9;
            setReal (s, Dest::algoHarmonics, 0.0f); setReal (s, Dest::algoTimbre, 0.0f); setReal (s, Dest::algoMorph, 0.0f);
            setReal (s, Dest::algoLevel, 1.0f);
            setReal (s, Dest::wtSemi, 7.0f);
            s.fm[0][2] = 1.0f;
            setReal (s, Dest::fmDepth, depth);
            return s;
        };
        auto unmod = play (algoOnly (0.0f), 57, 24000), fmd = play (algoOnly (0.5f), 57, 24000);
        bool finite = true;
        for (float v : fmd) finite = finite && std::isfinite (v);
        CHECK (finite && brightness (fmd) > 1.4 * brightness (unmod), "WT -> ALGO : FM audible (%.4f / %.4f)", brightness (unmod), brightness (fmd));
        // Feedback de l'ALGO et toutes les cases ALGO : bornés
        for (int row = 0; row < numFmRows; ++row)
        {
            auto s = algoOnly (1.0f);
            s.fm[0][2] = 0.0f;
            s.fm[(size_t) row][2] = 1.0f;
            s.fm[2][0] = 1.0f; s.fm[2][1] = 1.0f;
            setReal (s, Dest::wtLevel, 0.5f); setReal (s, Dest::pdLevel, 0.5f);
            auto x = play (s, 45, 9600);
            float peak = 0.0f; bool ok = true;
            for (float v : x) { ok = ok && std::isfinite (v); peak = std::max (peak, std::abs (v)); }
            CHECK (ok && peak < 2.0f, "cases ALGO, ligne %d : bornées (%.2f)", row, peak);
        }
        auto toWt = [] (float depth)
        {
            auto s = cleanSine();
            s.algoOn = true; s.algoModel = 9;
            setReal (s, Dest::algoLevel, 0.0f);
            setReal (s, Dest::algoSemi, 7.0f);
            s.fm[2][0] = 1.0f;
            setReal (s, Dest::fmDepth, depth);
            return s;
        };
        auto a = play (toWt (0.0f), 57, 24000), b = play (toWt (0.1f), 57, 24000);
        CHECK (brightness (b) > 1.5 * brightness (a), "ALGO -> WT à niveau 0 : module quand même (%.4f / %.4f)", brightness (a), brightness (b));
    }

    // Repliement en FM sur une scie aiguë (x2)
    {
        auto s = make (0.03f, FmMode::phase);
        s.table = BuiltinTables::get (0);
        setReal (s, Dest::wtPos, 0.5f);
        setReal (s, Dest::pdSemi, 0.0f);
        auto x = play (s, 100, 48000 + 8192);
        const double db = inharmonicDb (x, 8192, 440.0 * std::pow (2.0, 31.0 / 12.0), 48000.0);
        CHECK (db < -55.0, "repliement scie + FM : %.1f dB", db);
    }
}

static void testBite()
{
    for (Dest d : { Dest::biteFold, Dest::biteCrush, Dest::biteSoft })
    {
        auto s = cleanSine();
        setReal (s, d, 1.0f);
        auto x = play (s, 45, 9600);
        float peak = 0.0f;
        for (float v : x) peak = std::max (peak, std::abs (v));
        CHECK (peak < 2.0f && rms (x, 2400, 9600) > 0.05, "Bite %d borné (%.2f)", (int) d, peak);
        CHECK (toneLevel (x, 2400, 4096, 330.0, 48000.0) > 0.005, "Bite %d ajoute des harmoniques", (int) d);
    }
    // ASYM : harmoniques paires
    {
        auto s = cleanSine();
        setReal (s, Dest::biteSoft, 0.5f);
        auto sym = play (s, 45, 9600);
        s.biteAsym = true;
        auto asym = play (s, 45, 9600);
        CHECK (toneLevel (asym, 4800, 4096, 220.0, 48000.0) > 5.0 * toneLevel (sym, 4800, 4096, 220.0, 48000.0) + 0.005,
               "ASYM : 2e harmonique %.4f / %.4f", toneLevel (asym, 4800, 4096, 220.0, 48000.0), toneLevel (sym, 4800, 4096, 220.0, 48000.0));
    }
    // POST FILTER : distorsion après le filtre -> plus d'aigus qu'avant le filtre
    {
        auto s = cleanSine();
        s.table = BuiltinTables::get (0);
        setReal (s, Dest::wtPos, 0.5f);
        setReal (s, Dest::cutoff, 400.0f);
        setReal (s, Dest::biteSoft, 1.0f);
        auto pre = play (s, 45, 9600);
        s.bitePost = true;
        auto post = play (s, 45, 9600);
        CHECK (toneLevel (post, 2400, 4096, 1650.0, 48000.0) > 2.0 * toneLevel (pre, 2400, 4096, 1650.0, 48000.0),
               "POST FILTER plus brillant");
    }
}

static void testFilters()
{
    auto saw = [] (FilterModel m, FilterType t, bool s24, float cutoff, float reso)
    {
        auto s = cleanSine();
        s.table = BuiltinTables::get (0);
        setReal (s, Dest::wtPos, 0.5f);
        s.filterModel = m;
        s.filterType = t;
        s.slope24 = s24;
        setReal (s, Dest::cutoff, cutoff);
        setReal (s, Dest::resonance, reso);
        return s;
    };
    for (FilterModel m : { FilterModel::ladder, FilterModel::svf })
    {
        const char* name = m == FilterModel::ladder ? "LADDER" : "SVF";
        auto open = play (saw (m, FilterType::lowpass, false, 20000.0f, 0.0f), 45, 9600);
        auto lp12 = play (saw (m, FilterType::lowpass, false, 300.0f, 0.0f), 45, 9600);
        auto lp24 = play (saw (m, FilterType::lowpass, true, 300.0f, 0.0f), 45, 9600);
        const double h = 3000.0;
        CHECK (toneLevel (lp12, 2400, 4096, h, 48000.0) < 0.1 * toneLevel (open, 2400, 4096, h, 48000.0), "%s LP 12 coupe", name);
        CHECK (toneLevel (lp24, 2400, 4096, h, 48000.0) < 0.3 * toneLevel (lp12, 2400, 4096, h, 48000.0), "%s 24 dB plus raide", name);
        CHECK (toneLevel (lp12, 2400, 4096, 110.0, 48000.0) > 0.5 * toneLevel (open, 2400, 4096, 110.0, 48000.0), "%s LP garde le grave", name);

        auto hp = play (saw (m, FilterType::highpass, true, 3000.0f, 0.0f), 45, 9600);
        CHECK (toneLevel (hp, 2400, 4096, 110.0, 48000.0) < 0.05 * toneLevel (open, 2400, 4096, 110.0, 48000.0), "%s HP coupe le grave", name);
        auto bp = play (saw (m, FilterType::bandpass, false, 1100.0f, 0.3f), 45, 9600);
        CHECK (toneLevel (bp, 2400, 4096, 1100.0, 48000.0) / toneLevel (open, 2400, 4096, 1100.0, 48000.0)
               > 3.0 * toneLevel (bp, 2400, 4096, 110.0, 48000.0) / toneLevel (open, 2400, 4096, 110.0, 48000.0), "%s BP", name);
        auto notch = play (saw (m, FilterType::notch, false, 1100.0f, 0.0f), 45, 9600);
        CHECK (toneLevel (notch, 2400, 4096, 1100.0, 48000.0) < 0.5 * toneLevel (open, 2400, 4096, 1100.0, 48000.0), "%s NOTCH", name);

        // Résonance : pic près de la coupure
        auto res = play (saw (m, FilterType::lowpass, false, 990.0f, 0.9f), 45, 9600);
        auto flat = play (saw (m, FilterType::lowpass, false, 990.0f, 0.0f), 45, 9600);
        CHECK (toneLevel (res, 2400, 4096, 990.0, 48000.0) > 2.0 * toneLevel (flat, 2400, 4096, 990.0, 48000.0), "%s résonance", name);
        float peak = 0.0f;
        for (float v : play (saw (m, FilterType::lowpass, true, 990.0f, 1.0f), 45, 24000)) peak = std::max (peak, std::abs (v));
        CHECK (peak < 4.0f, "%s résonance max bornée (%.2f)", name, peak);
    }

    // ENV AMT : l'enveloppe du filtre ouvre puis referme
    {
        auto s = saw (FilterModel::ladder, FilterType::lowpass, true, 150.0f, 0.0f);
        setReal (s, Dest::filterEnv, 0.8f);
        s.fltA = 0.0005f; s.fltD = 0.05f; s.fltS = 0.0f;
        auto x = play (s, 69, 24000);
        CHECK (rms (x, 100, 1200) > 4.0 * rms (x, 18000, 24000), "ENV AMT : %.4f / %.4f", rms (x, 100, 1200), rms (x, 18000, 24000));
    }
}

static void testModulation()
{
    const double sr = 48000.0;
    // MAIN LFO carré synchro -> WT SEMI : +12 une demi-période sur deux
    {
        auto s = cleanSine();
        s.mainLfo.sync = true;
        s.mainLfo.periodBeats = 1.0;
        s.mainLfo.shape = MainLfoShape::square;
        s.mainLfo.retrig = true;
        s.mainLfo.dest[0] = Dest::wtSemi;
        s.mainLfo.amount[0] = 6.0f / 96.0f;      // ±6 -> 12 d'écart
        auto x = play (s, 57, (int) (sr * 0.5));
        const double a = measureFreq (x, 2000, 11000, sr), b = measureFreq (x, 13000, 23000, sr);
        CHECK (near (a, 220.0 * std::pow (2.0, 6.0 / 12.0), 1.0) && near (b, 220.0 * std::pow (2.0, -6.0 / 12.0), 1.0), "MAIN LFO -> SEMI : %.1f / %.1f", a, b);
    }
    // MAIN LFO libre en Hz
    {
        auto s = cleanSine();
        s.mainLfo.sync = false;
        s.mainLfo.rateHz = 4.0f;
        s.mainLfo.shape = MainLfoShape::square;
        s.mainLfo.retrig = true;
        s.mainLfo.dest[1] = Dest::volume;
        s.mainLfo.amount[1] = 0.8f;
        auto x = play (s, 57, 24000);
        CHECK (rms (x, 1000, 5000) > 3.0 * rms (x, 7000, 11000), "MAIN LFO 4 Hz sur le volume");
    }
    // ENV 3, vélocité, molette dans les routages
    for (int src : { (int) srcEnv3, (int) srcVelocity, (int) srcModWheel })
    {
        auto s = cleanSine();
        s.slotSource[0] = src;
        s.slotDest[0] = Dest::wtSemi;
        s.slotAmount[0] = 12.0f / 96.0f;
        s.modA = 0.0005f; s.modD = 5.0f; s.modS = 1.0f;
        Engine e;
        e.setSettings (s);
        e.prepare (sr);
        e.setModWheel (1.0f);
        e.noteOn (57, 1.0f);
        auto x = render (e, 24000);
        CHECK (near (measureFreq (x, 4800, 24000, sr), 440.0, 1.0), "source %d -> SEMI +12", src);
    }
    // SEQ -> CUTOFF : la tête de lecture avance
    {
        auto s = cleanSine();
        s.seq.on = true; s.seq.steps = 16; s.seq.stepBeats = 0.25; s.seq.pattern.fill (1);
        Engine e;
        e.setSettings (s);
        e.prepare (sr);
        e.noteOn (60, 1.0f);
        render (e, (int) (sr * 0.4));
        CHECK (e.getPlayheadStep() == 3, "tête de lecture : %d", e.getPlayheadStep());
    }
}

static void testVoiceModes()
{
    const double sr = 48000.0;
    // MONO + glide
    {
        auto s = cleanSine();
        s.voiceMode = VoiceMode::mono;
        s.glideMs = 100.0f;
        Engine e;
        e.setSettings (s);
        e.prepare (sr);
        e.noteOn (57, 1.0f);
        render (e, 4800);
        e.noteOn (69, 1.0f);
        auto x = render (e, 24000);
        CHECK (measureFreq (x, 0, 1200, sr) > 225.0 && measureFreq (x, 0, 1200, sr) < 400.0, "glide en cours");
        CHECK (near (measureFreq (x, 14400, 24000, sr), 440.0, 1.0), "glide arrivé");
        CHECK (e.activeVoiceCount() == 1, "mono");
        e.noteOff (69);
        x = render (e, 24000);
        CHECK (near (measureFreq (x, 14400, 24000, sr), 220.0, 1.0), "retour à la note tenue");
    }
    // LEGATO : pas de nouvelle attaque quand une note est tenue ; MONO : l'enveloppe repart
    {
        auto levelAfterSecondNote = [sr] (VoiceMode mode)
        {
            auto s = cleanSine();
            s.voiceMode = mode;
            s.ampA = 0.01f; s.ampD = 0.05f; s.ampS = 0.2f;
            Engine e;
            e.setSettings (s);
            e.prepare (sr);
            e.noteOn (57, 1.0f);
            render (e, 24000);          // au palier (0,2)
            e.noteOn (60, 1.0f);
            auto x = render (e, 960);   // 20 ms : l'attaque est passée
            return rms (x, 480, 960);
        };
        const double legato = levelAfterSecondNote (VoiceMode::legato), mono = levelAfterSecondNote (VoiceMode::mono);
        CHECK (legato < 0.15, "legato : reste au palier (%.3f)", legato);
        CHECK (mono > 1.5 * legato, "mono : nouvelle attaque (%.3f / %.3f)", mono, legato);
    }
    // POLY : 16 voix
    {
        Engine e;
        e.setSettings (cleanSine());
        e.prepare (sr);
        for (int n = 0; n < 20; ++n) e.noteOn (40 + n, 1.0f);
        render (e, 256);
        CHECK (e.activeVoiceCount() == 16, "16 voix : %d", e.activeVoiceCount());
    }
    // BEND
    {
        auto s = cleanSine();
        s.bendRange = 12.0f;
        Engine e;
        e.setSettings (s);
        e.prepare (sr);
        e.setPitchWheel (1.0f);
        e.noteOn (57, 1.0f);
        auto x = render (e, 24000);
        CHECK (near (measureFreq (x, 4800, 24000, sr), 440.0, 1.0), "BEND +12");
    }
}

static void testFuzz()
{
    Rng rng (1234);
    for (double sr : { 44100.0, 48000.0, 96000.0 })
        for (int run = 0; run < 8; ++run)
        {
            Settings s;
            for (auto& v : s.base) v = rng.uniformf();
            s.wtOn = rng.chance (0.8); s.pdOn = rng.chance (0.8); s.algoOn = rng.chance (0.6);
            s.table = BuiltinTables::get (rng.range (0, BuiltinTables::count() - 1));
            s.wtWarp = (WtWarp) rng.range (0, (int) WtWarp::count - 1);
            s.wtOctave = rng.range (-3, 3); s.pdOctave = rng.range (-3, 3); s.algoOctave = rng.range (-3, 3);
            s.unison = rng.range (1, maxUnison);
            s.wtPhase = rng.uniformf(); s.wtRandomPhase = rng.chance (0.5);
            s.pdWave = (PdWave) rng.range (0, (int) PdWave::count - 1);
            s.algoModel = rng.range (0, PlaitsModels::count - 1); s.algoLpg = rng.chance (0.3);
            s.subWave = (SubWave) rng.range (0, 3); s.subOctave = rng.range (-2, 0);
            s.noiseType = (NoiseType) rng.range (0, 3);
            for (auto& row : s.fm) for (auto& c : row) c = rng.chance (0.4) ? rng.uniformf() : 0.0f;
            s.fmMode = (FmMode) rng.range (0, 1);
            s.bitePost = rng.chance (0.5); s.biteAsym = rng.chance (0.5);
            s.filterModel = (FilterModel) rng.range (0, 1); s.filterType = (FilterType) rng.range (0, 3); s.slope24 = rng.chance (0.5);
            s.keyTrack = rng.uniformf();
            s.voiceMode = (VoiceMode) rng.range (0, 2); s.glideMs = 300.0f * rng.uniformf(); s.bendRange = 12.0f * rng.uniformf();
            s.oversampling = 1 << rng.range (0, 2);
            s.mainLfo.sync = rng.chance (0.5); s.mainLfo.rateHz = 20.0f * rng.uniformf(); s.mainLfo.periodBeats = 0.25 * rng.range (1, 8);
            s.mainLfo.shape = (MainLfoShape) rng.range (0, (int) MainLfoShape::count - 1); s.mainLfo.retrig = rng.chance (0.5);
            for (int k = 0; k < 2; ++k) { s.mainLfo.dest[(size_t) k] = (Dest) rng.range (0, numDests - 1); s.mainLfo.amount[(size_t) k] = rng.uniformf() * 2 - 1; }
            for (auto& l : s.lfo) { l.periodBeats = 0.0625 * std::pow (2.0, rng.range (0, 8)); l.shape = (LfoShape) rng.range (0, (int) LfoShape::count - 1); }
            s.seq.on = rng.chance (0.7);
            std::array<bool, numSources + 1> all {};
            all.fill (true);
            RandomPattern::generate (rng, s.seq.steps, all, s.seq.pattern);
            for (int k = 0; k < numSlots; ++k)
            {
                s.slotSource[(size_t) k] = rng.range (0, numModSources - 1);
                s.slotDest[(size_t) k] = rng.chance (0.5) ? (Dest) rng.range (0, numDests - 1) : Dest::off;
                s.slotAmount[(size_t) k] = rng.uniformf() * 2 - 1;
            }

            Engine e;
            e.setSettings (s);
            e.prepare (sr);
            Engine::Transport t { 60.0 + 180.0 * rng.uniform(), rng.chance (0.5), 16.0 * rng.uniform() };
            bool finite = true;
            float peak = 0.0f;
            std::vector<float> l (512), r (512);
            for (int block = 0; block < (int) (sr / 512.0 * 0.6); ++block)
            {
                if (block % 15 == 0) e.noteOn (rng.range (12, 110), rng.uniformf());
                if (block % 15 == 9) e.noteOff (rng.range (12, 110));
                if (block == 20) { e.setPitchWheel (rng.uniformf() * 2 - 1); e.setModWheel (rng.uniformf()); }
                e.render (l.data(), r.data(), 512, t);
                t.ppq += 512.0 * t.bpm / 60.0 / sr;
                for (int i = 0; i < 512; ++i)
                {
                    finite = finite && std::isfinite (l[(size_t) i]) && std::isfinite (r[(size_t) i]);
                    peak = std::max ({ peak, std::abs (l[(size_t) i]), std::abs (r[(size_t) i]) });
                }
            }
            CHECK (finite, "valeurs finies (sr %.0f, essai %d)", sr, run);
            CHECK (peak < 16.0f, "crête raisonnable (sr %.0f, essai %d) : %.2f", sr, run, peak);
        }
}

//==============================================================================
struct ArpEvent { bool on; int note; int at; };

static std::vector<ArpEvent> runArp (Arpeggiator& arp, int samples, double bpm = 120.0, bool host = false, double ppq = 0.0)
{
    std::vector<ArpEvent> events;
    int done = 0;
    while (done < samples)
    {
        const int n = std::min (256, samples - done);
        const int base = done;
        arp.process (n, 48000.0, bpm, host, ppq + base * bpm / 60.0 / 48000.0,
                     [&events, base] (bool on, int note, float, int at) { events.push_back ({ on, note, base + at }); });
        done += n;
    }
    return events;
}

static std::vector<int> onsets (const std::vector<ArpEvent>& ev)
{
    std::vector<int> notes;
    for (auto& e : ev) if (e.on) notes.push_back (e.note);
    return notes;
}

static void testArp()
{
    auto make = [] (ArpMode mode, int octaves = 1)
    {
        ArpSettings s;
        s.on = true; s.mode = mode; s.octaves = octaves; s.stepBeats = 0.25; s.gate = 0.5f;
        return s;
    };
    auto chord = [] (Arpeggiator& a) { a.noteOn (64, 1.0f); a.noteOn (60, 1.0f); a.noteOn (67, 1.0f); };

    // UP sur un accord, 1/16 à 120 BPM = 6000 échantillons
    {
        Arpeggiator a; a.reset(); a.setSettings (make (ArpMode::up)); chord (a);
        auto ev = runArp (a, 6000 * 6 + 10);
        auto n = onsets (ev);
        CHECK ((n == std::vector<int> { 60, 64, 67, 60, 64, 67, 60 }), "UP : %d notes", (int) n.size());
        int firstOff = -1;
        for (auto& e : ev) if (! e.on) { firstOff = e.at; break; }
        CHECK (std::abs (firstOff - 3000) <= 1, "GATE 50 %% : fin à %d", firstOff);
        int second = -1, count = 0;
        for (auto& e : ev) if (e.on && ++count == 2) second = e.at;
        CHECK (std::abs (second - 6000) <= 1, "pas suivant à %d", second);
    }
    // DOWN, UP/DOWN, DOWN/UP, ORDER, 2 octaves
    {
        Arpeggiator a; a.reset(); a.setSettings (make (ArpMode::down)); chord (a);
        CHECK ((onsets (runArp (a, 6000 * 3 + 10)) == std::vector<int> { 67, 64, 60, 67 }), "DOWN");
    }
    {
        Arpeggiator a; a.reset(); a.setSettings (make (ArpMode::upDown)); chord (a);
        CHECK ((onsets (runArp (a, 6000 * 5 + 10)) == std::vector<int> { 60, 64, 67, 64, 60, 64 }), "UP/DOWN");
    }
    {
        Arpeggiator a; a.reset(); a.setSettings (make (ArpMode::downUp)); chord (a);
        CHECK ((onsets (runArp (a, 6000 * 4 + 10)) == std::vector<int> { 67, 64, 60, 64, 67 }), "DOWN/UP");
    }
    {
        Arpeggiator a; a.reset(); a.setSettings (make (ArpMode::order)); chord (a);
        CHECK ((onsets (runArp (a, 6000 * 2 + 10)) == std::vector<int> { 64, 60, 67 }), "AS PLAYED");
    }
    {
        Arpeggiator a; a.reset(); a.setSettings (make (ArpMode::up, 2)); a.noteOn (60, 1.0f); a.noteOn (64, 1.0f);
        CHECK ((onsets (runArp (a, 6000 * 4 + 10)) == std::vector<int> { 60, 64, 72, 76, 60 }), "2 octaves");
    }
    {
        Arpeggiator a; a.reset(); a.setSettings (make (ArpMode::chord)); chord (a);
        auto n = onsets (runArp (a, 10));
        CHECK (n.size() == 3, "CHORD : 3 notes ensemble");
    }
    {
        Arpeggiator a; a.reset(); a.setSettings (make (ArpMode::random)); chord (a);
        auto n = onsets (runArp (a, 6000 * 40));
        bool valid = true;
        std::array<int, 3> seen {};
        for (int v : n) { valid = valid && (v == 60 || v == 64 || v == 67); seen[(size_t) (v == 60 ? 0 : v == 64 ? 1 : 2)]++; }
        CHECK (valid && seen[0] > 3 && seen[1] > 3 && seen[2] > 3, "RANDOM");
    }
    // Relâché : tout s'arrête ; LATCH : continue
    {
        Arpeggiator a; a.reset(); a.setSettings (make (ArpMode::up)); chord (a);
        runArp (a, 7000);
        a.noteOff (60); a.noteOff (64); a.noteOff (67);
        auto ev = runArp (a, 20000);
        CHECK (onsets (ev).empty() && a.playingCount() == 0, "relâché : silence");
        auto s = make (ArpMode::up); s.latch = true;
        Arpeggiator b; b.reset(); b.setSettings (s); chord (b);
        b.noteOff (60); b.noteOff (64); b.noteOff (67);
        CHECK (onsets (runArp (b, 6000 * 3 + 10)).size() == 4, "LATCH : continue");
        b.noteOn (50, 1.0f);   // nouvelle frappe : remplace l'accord
        auto m = onsets (runArp (b, 6000 * 3));
        bool only50 = true;
        for (int v : m) only50 = only50 && v == 50;
        CHECK (only50 && ! m.empty(), "LATCH : nouvelle frappe remplace");
    }
    // Synchro hôte : la grille suit la position du morceau
    {
        Arpeggiator a; a.reset(); a.setSettings (make (ArpMode::up)); a.noteOn (60, 1.0f);
        auto ev = runArp (a, 6000, 120.0, true, 0.2);   // 0,2 noire : le pas suivant (0,25) tombe 0,05 noire plus tard = 1200 échantillons
        int on = -1, k = 0;
        for (auto& e : ev) if (e.on && ++k == 2) on = e.at;
        CHECK (std::abs (on - 1200) <= 1, "calé sur l'hôte : %d", on);
    }
    // Swing : le pas pair dure plus longtemps
    {
        auto s = make (ArpMode::up); s.swing = 0.5;
        Arpeggiator a; a.reset(); a.setSettings (s); a.noteOn (60, 1.0f);
        auto ev = runArp (a, 12000);
        int second = -1, k = 0;
        for (auto& e : ev) if (e.on && ++k == 2) second = e.at;
        CHECK (std::abs (second - 9000) <= 1, "swing : %d", second);
    }
    // Éteint : les notes en cours s'arrêtent
    {
        Arpeggiator a; a.reset(); a.setSettings (make (ArpMode::up)); chord (a);
        runArp (a, 100);
        auto s = make (ArpMode::up); s.on = false;
        a.setSettings (s);
        auto ev = runArp (a, 10);
        CHECK (! ev.empty() && ! ev[0].on, "éteint : note off envoyé");
    }
}

static void testFx()
{
    const double sr = 48000.0;
    auto impulse = [] (int n) { std::vector<float> v ((size_t) n, 0.0f); v[0] = 1.0f; return v; };

    // Tout éteint : intact
    {
        Fx fx; fx.prepare (sr); fx.setSettings ({});
        auto l = impulse (1000), r = impulse (1000);
        fx.process (l.data(), r.data(), 1000, 120.0);
        CHECK (l[0] == 1.0f && l[1] == 0.0f && r[0] == 1.0f, "FX éteints : intact");
    }
    // DELAY : écho à 1/4 de noire à 120 BPM = 0,125 s, qui décroît ; ping-pong alterne
    {
        FxSettings s; s.delayOn = true; s.delayBeats = 0.25; s.delayFeedback = 0.5f; s.delayMix = 1.0f; s.delayTone = 1.0f; s.delayPingPong = true;
        Fx fx; fx.prepare (sr); fx.setSettings (s);
        const int n = 30000;
        auto l = impulse (n), r = impulse (n);
        fx.process (l.data(), r.data(), n, 120.0);
        auto peakAround = [] (const std::vector<float>& x, int at) { float p = 0.0f; for (int i = at - 50; i < at + 50; ++i) p = std::max (p, std::abs (x[(size_t) i])); return p; };
        CHECK (peakAround (l, 6000) > 0.2f, "1er écho à gauche (%.3f)", peakAround (l, 6000));
        CHECK (peakAround (r, 12000) > 0.08f && peakAround (r, 12000) < peakAround (l, 6000), "2e écho à droite, plus faible");
        CHECK (peakAround (l, 3000) < 1e-3f, "rien entre les échos");
    }
    // DELAY FREE : 100 ms quel que soit le tempo
    for (double bpm : { 90.0, 174.0 })
    {
        FxSettings s; s.delayOn = true; s.delaySync = false; s.delayMs = 100.0; s.delayFeedback = 0.0f; s.delayMix = 1.0f; s.delayTone = 1.0f; s.delayPingPong = false;
        Fx fx; fx.prepare (sr); fx.setSettings (s);
        const int n = 12000;
        auto l = impulse (n), r = impulse (n);
        fx.process (l.data(), r.data(), n, bpm);
        int at = 0; float best = 0.0f;
        for (int i = 100; i < n; ++i) if (std::abs (l[(size_t) i]) > best) { best = std::abs (l[(size_t) i]); at = i; }
        CHECK (std::abs (at - 4800) < 30, "DELAY FREE 100 ms à %.0f BPM : écho à %d", bpm, at);
    }

    // REVERB : queue qui décroît, finie
    {
        FxSettings s; s.reverbOn = true; s.reverbMix = 1.0f; s.reverbDecay = 0.4f; s.reverbPredelay = 0.0f;
        Fx fx; fx.prepare (sr); fx.setSettings (s);
        const int n = 48000 * 3;
        auto l = impulse (n), r = impulse (n);
        fx.process (l.data(), r.data(), n, 120.0);
        bool finite = true;
        for (float v : l) finite = finite && std::isfinite (v);
        const double early = rms (l, 2000, 12000), late = rms (l, 96000, 144000);
        CHECK (finite && early > 1e-3 && late < 0.2 * early, "reverb : %.5f puis %.5f", early, late);
        double corr = 0.0, el = 0.0, er = 0.0;
        for (int i = 2000; i < 24000; ++i) { corr += l[(size_t) i] * r[(size_t) i]; el += l[(size_t) i] * l[(size_t) i]; er += r[(size_t) i] * r[(size_t) i]; }
        CHECK (corr / std::sqrt (el * er) < 0.7, "reverb stéréo (corrélation %.2f)", corr / std::sqrt (el * er));
    }
    // Reverb longue + feedback max du delay : pas d'emballement
    {
        FxSettings s; s.reverbOn = true; s.reverbMix = 0.5f; s.reverbDecay = 1.0f; s.reverbSize = 1.0f;
        s.delayOn = true; s.delayFeedback = 0.95f; s.delayMix = 0.5f; s.chorusOn = true; s.chorusDepth = 1.0f; s.chorusMix = 0.5f;
        Fx fx; fx.prepare (sr); fx.setSettings (s);
        std::vector<float> l (48000 * 10), r (48000 * 10);
        Rng rng (3);
        for (int i = 0; i < 48000; ++i) { l[(size_t) i] = rng.uniformf() * 2 - 1; r[(size_t) i] = rng.uniformf() * 2 - 1; }
        fx.process (l.data(), r.data(), (int) l.size(), 174.0);
        float peak = 0.0f;
        for (float v : l) peak = std::max (peak, std::abs (v));
        CHECK (peak < 20.0f, "FX au maximum : borné (%.2f)", peak);
    }
    // CHORUS : le signal est modifié et reste borné
    {
        FxSettings s; s.chorusOn = true; s.chorusMix = 0.5f;
        Fx fx; fx.prepare (sr); fx.setSettings (s);
        std::vector<float> l (48000), r (48000);
        for (int i = 0; i < 48000; ++i) l[(size_t) i] = r[(size_t) i] = std::sin ((float) (twoPi * 440.0 * i / sr));
        fx.process (l.data(), r.data(), 48000, 120.0);
        double diff = 0.0;
        for (int i = 4800; i < 48000; ++i) diff += std::abs (l[(size_t) i] - r[(size_t) i]);
        CHECK (diff / 43200.0 > 0.01, "chorus stéréo");
    }
}

int main (int argc, char** argv)
{
    testStepPositions();
    testLfoShapes();
    testChaos();
    testSequencerSources();
    testCrossfade();
    testRandomPatterns();
    testDestRanges();
    testWavetableBuilder();
    testWavParser();
    testDecimator();
    testWavetablePitch();
    testUnison();
    testWtWarp();
    testPhaseDistortion();
    testPlaits();
    testSubNoise();
    testFmMatrix();
    testBite();
    testFilters();
    testModulation();
    testVoiceModes();
    testArp();
    testFx();
    testFuzz();
    (void) argc; (void) argv;

    std::printf ("%d vérifications, %d échecs\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
