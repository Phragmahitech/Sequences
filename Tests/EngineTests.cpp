// Tests du moteur de Sequences (sans JUCE).
#include "SequencesEngine.h"

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
static const Wavetable* basicTable()    { return BuiltinTables::get (BuiltinTables::indexOf ("BASIC SHAPES")); }

static void setReal (Settings& s, Dest d, float v)   { s.base[(size_t) d] = DestRange::toNorm (d, v); }

/** Osc A = sinus pur, B coupé, filtre ouvert, rien d'autre. */
static Settings cleanSine()
{
    Settings s;
    s.osc[0].table = sineTable();
    s.osc[1].table = sineTable();
    setReal (s, Dest::aLevel, 1.0f);
    setReal (s, Dest::bLevel, 0.0f);
    setReal (s, Dest::cutoff, 20000.0f);
    setReal (s, Dest::resonance, 0.0f);
    setReal (s, Dest::volume, 1.0f);
    s.keyTrack = 0.0f;
    s.slope24 = false;
    s.ampA = 0.0005f; s.ampD = 1.0f; s.ampS = 1.0f; s.ampR = 0.01f;
    s.velocity = 0.0f;
    s.seq.on = false;
    s.seq.manual = 1;
    return s;
}

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

//==============================================================================
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

static void testRealSerumFiles (const std::vector<std::string>& paths)
{
    for (const auto& path : paths)
    {
        std::ifstream f (path, std::ios::binary);
        if (! f) continue;
        std::vector<uint8_t> bytes ((std::istreambuf_iterator<char> (f)), {});
        const auto w = WavetableFile::parse (bytes.data(), bytes.size());
        CHECK (w.error.empty() && w.serumHeader && ! w.cycles.empty(), "table Serum %s : %s", path.c_str(), w.error.c_str());
        auto t = Wavetable::build (w.cycles, "serum");
        CHECK (t != nullptr && t->numFrames == (int) std::min ((size_t) 256, w.cycles.size()), "construite : %s", path.c_str());

        // On la joue : son présent et fini
        Engine e;
        e.prepare (48000.0);
        auto s = cleanSine();
        s.osc[0].table = t.get();
        setReal (s, Dest::aPos, 0.5f);
        e.setSettings (s);
        e.noteOn (48, 1.0f);
        auto x = render (e, 9600);
        bool finite = true;
        for (float v : x) finite = finite && std::isfinite (v);
        CHECK (finite && rms (x, 2400, 9600) > 0.01, "jouée : %s (rms %.3f)", path.c_str(), rms (x, 2400, 9600));
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

static void testPitch()
{
    for (double sr : { 44100.0, 48000.0, 96000.0 })
    {
        Engine e;
        e.prepare (sr);
        e.setSettings (cleanSine());
        e.noteOn (69, 1.0f);
        auto x = render (e, (int) (sr * 0.5));
        const double f = measureFreq (x, (int) (sr * 0.05), (int) (sr * 0.5), sr);
        CHECK (near (f, 440.0, 0.3), "sinus 440 Hz à %.0f Hz : %.3f", sr, f);
    }

    struct Case { int oct; float coarse, fine, pitch; double expected; const char* name; };
    for (auto c : { Case { 1, 0.0f, 0.0f, 0.0f, 880.0, "octave +1" }, Case { -2, 0.0f, 0.0f, 0.0f, 110.0, "octave -2" },
                    Case { 0, 7.0f, 0.0f, 0.0f, 440.0 * std::pow (2.0, 7.0 / 12.0), "coarse +7" },
                    Case { 0, 0.5f, 0.0f, 0.0f, 440.0 * std::pow (2.0, 0.5 / 12.0), "coarse +0,5 (continu)" },
                    Case { 0, -12.0f, 50.0f, 0.0f, 220.0 * std::pow (2.0, 0.5 / 12.0), "coarse -12 + fine +50" },
                    Case { 0, 0.0f, 0.0f, -3.25f, 440.0 * std::pow (2.0, -3.25 / 12.0), "pitch -3,25" } })
    {
        Engine e;
        e.prepare (48000.0);
        auto s = cleanSine();
        s.osc[0].octave = c.oct;
        setReal (s, Dest::aCoarse, c.coarse);
        setReal (s, Dest::aFine, c.fine);
        setReal (s, Dest::pitch, c.pitch);
        e.setSettings (s);
        e.prepare (48000.0);
        e.noteOn (69, 1.0f);
        auto x = render (e, 24000);
        const double f = measureFreq (x, 4800, 24000, 48000.0);
        CHECK (near (f, c.expected, c.expected * 2e-4), "%s : %.3f / %.3f", c.name, f, c.expected);
    }
}

static void testCoarseGlides()
{
    // COARSE tourné de 0 à +12 entre deux blocs : la hauteur glisse (pas de saut d'un coup)
    const double sr = 48000.0;
    Engine e;
    e.prepare (sr);
    auto s = cleanSine();
    e.setSettings (s);
    e.noteOn (57, 1.0f);
    render (e, 4800);
    setReal (s, Dest::aCoarse, 12.0f);
    e.setSettings (s);
    auto x = render (e, 9600);
    const double early = measureFreq (x, 200, 900, sr);
    const double late = measureFreq (x, 6000, 9600, sr);
    CHECK (early > 235.0 && early < 420.0, "pendant le glissement : %.1f Hz", early);
    CHECK (near (late, 440.0, 0.5), "arrivé à +12 : %.2f", late);
}

static Settings modSettings (Dest d, float amount, int source)
{
    auto s = cleanSine();
    s.slotSource[0] = source;
    s.slotDest[0] = d;
    s.slotAmount[0] = amount;
    return s;
}

static void testModulationSlots()
{
    const double sr = 48000.0;

    // LFO 1 carré unipolaire -> COARSE A, +12/96 : +12 demi-tons pendant la première moitié
    {
        Engine e;
        e.prepare (sr);
        auto s = modSettings (Dest::aCoarse, 12.0f / 96.0f, srcLfo1);
        s.lfo[0].shape = LfoShape::square; s.lfo[0].periodBeats = 1.0; s.lfo[0].unipolar = true;
        e.setSettings (s);
        e.noteOn (57, 1.0f);
        auto x = render (e, (int) (sr * 0.2));
        CHECK (e.getLiveOffset (Dest::aCoarse) > 0.12f, "décalage visible par l'interface : %f", e.getLiveOffset (Dest::aCoarse));
        auto y = render (e, (int) (sr * 0.3));
        x.insert (x.end(), y.begin(), y.end());
        CHECK (near (measureFreq (x, 2000, 11000, sr), 440.0, 0.6), "+12 sur la première moitié");
        CHECK (near (measureFreq (x, 13000, 23000, sr), 220.0, 0.4), "0 sur la seconde");
    }

    // La modulation est bornée par la course du potard
    {
        Engine e;
        e.prepare (sr);
        auto s = modSettings (Dest::aCoarse, 1.0f, srcLfo1);
        setReal (s, Dest::aCoarse, 40.0f);
        s.lfo[0].shape = LfoShape::square; s.lfo[0].periodBeats = 4.0; s.lfo[0].unipolar = true;
        e.setSettings (s);
        e.prepare (sr);
        e.noteOn (33, 1.0f);   // 55 Hz, +40 -> +48 maxi
        auto x = render (e, 24000);
        const double expected = 55.0 * std::pow (2.0, 48.0 / 12.0);
        CHECK (near (measureFreq (x, 4800, 24000, sr), expected, 1.0), "plafonné à +48 : %.2f", measureFreq (x, 4800, 24000, sr));
    }

    // Deux routages vers le même potard s'additionnent ; un routage OFF ne fait rien
    {
        Engine e;
        e.prepare (sr);
        auto s = modSettings (Dest::pitch, 6.0f / 48.0f, srcVelocity);
        s.slotSource[1] = srcVelocity; s.slotDest[1] = Dest::pitch; s.slotAmount[1] = 6.0f / 48.0f;
        s.slotSource[2] = srcVelocity; s.slotDest[2] = Dest::off;   s.slotAmount[2] = 1.0f;
        e.setSettings (s);
        e.noteOn (57, 1.0f);   // vélocité 1 -> +12
        auto x = render (e, 24000);
        CHECK (near (measureFreq (x, 4800, 24000, sr), 440.0, 0.5), "vélocité -> pitch, deux routages");
    }

    // ENV 2 -> CUTOFF : le filtre se referme avec l'enveloppe
    {
        Engine e;
        e.prepare (sr);
        auto s = modSettings (Dest::cutoff, 0.5f, srcEnv2);
        s.osc[0].table = basicTable();
        setReal (s, Dest::aPos, 2.0f / 3.0f);   // scie
        setReal (s, Dest::cutoff, 200.0f);
        s.slope24 = true;
        s.modA = 0.0005f; s.modD = 0.05f; s.modS = 0.0f;
        e.setSettings (s);
        e.prepare (sr);
        e.noteOn (69, 1.0f);
        auto x = render (e, 24000);
        CHECK (rms (x, 100, 1000) > 4.0 * rms (x, 18000, 24000), "ENV 2 ouvre puis referme : %.4f / %.4f", rms (x, 100, 1000), rms (x, 18000, 24000));
    }

    // SEQ -> VOLUME -100 % avec un carré : coupe le son une moitié du temps, sans clic
    {
        Engine e;
        e.prepare (sr);
        auto s = modSettings (Dest::volume, -1.0f, srcSeq);
        s.seq.on = true; s.seq.steps = 2; s.seq.stepBeats = 0.25; s.seq.xfadeMs = 2.0f;
        s.seq.pattern = {}; s.seq.pattern[0] = 0; s.seq.pattern[1] = 1;
        s.lfo[0].shape = LfoShape::square; s.lfo[0].periodBeats = 4.0; s.lfo[0].unipolar = true;
        e.setSettings (s);
        e.noteOn (45, 1.0f);
        auto x = render (e, (int) (sr * 0.5));
        const int stepLen = (int) (sr * 0.125);
        CHECK (rms (x, 1000, stepLen - 200) > 0.2, "pas ouvert");
        CHECK (rms (x, stepLen + 900, 2 * stepLen - 200) < 0.005, "pas fermé : %.4f", rms (x, stepLen + 900, 2 * stepLen - 200));
        float maxJump = 0.0f;
        for (size_t i = 1; i < x.size(); ++i) maxJump = std::max (maxJump, std::abs (x[i] - x[i - 1]));
        CHECK (maxJump < 0.06f, "pas de clic : %.4f", maxJump);
    }

    // Tête de lecture du séquenceur, mode HOST
    {
        Engine e;
        e.prepare (sr);
        auto s = cleanSine();
        s.seq.on = true; s.seq.steps = 16; s.seq.stepBeats = 0.25; s.seq.hostSync = true;
        e.setSettings (s);
        e.noteOn (60, 1.0f);
        std::vector<float> l (64), r (64);
        e.render (l.data(), r.data(), 64, { 120.0, true, 1.0 });
        CHECK (e.getPlayheadStep() == 4, "HOST : pas 4 : %d", e.getPlayheadStep());
    }
}

static void testWarp()
{
    const double sr = 48000.0;

    // FM depuis B (B coupé mais il module quand même) : bandes latérales à fc ± fm
    {
        auto run = [&] (float warp)
        {
            Engine e;
            e.prepare (sr);
            auto s = cleanSine();
            s.osc[0].warp = Warp::fm;
            setReal (s, Dest::bCoarse, 7.0f);      // B = 659,26 Hz
            setReal (s, Dest::aWarp, warp);
            e.setSettings (s);
            e.prepare (sr);
            e.noteOn (69, 1.0f);                   // A = 440 Hz
            return render (e, 24000);
        };
        const auto plain = run (0.0f), fm = run (0.15f);
        const double side = 440.0 + 440.0 * std::pow (2.0, 7.0 / 12.0);
        CHECK (toneLevel (plain, 4800, 16384, side, sr) < 1e-3, "sans FM : pas de bande latérale");
        CHECK (toneLevel (fm, 4800, 16384, side, sr) > 0.05, "FM : bande à fc + fm (%.3f)", toneLevel (fm, 4800, 16384, side, sr));
        CHECK (toneLevel (fm, 4800, 16384, 440.0, sr) < toneLevel (plain, 4800, 16384, 440.0, sr), "FM : la porteuse perd de l'énergie");
    }

    // FM depuis A sur B : A à 0 dans le mix, B module par A
    {
        Engine e;
        e.prepare (sr);
        auto s = cleanSine();
        setReal (s, Dest::aLevel, 0.0f);
        setReal (s, Dest::bLevel, 1.0f);
        setReal (s, Dest::aCoarse, 7.0f);
        s.osc[1].warp = Warp::fm;
        setReal (s, Dest::bWarp, 0.15f);
        e.setSettings (s);
        e.prepare (sr);
        e.noteOn (69, 1.0f);
        auto x = render (e, 24000);
        CHECK (toneLevel (x, 4800, 16384, 440.0 + 440.0 * std::pow (2.0, 7.0 / 12.0), sr) > 0.05, "FM depuis A sur B");
    }

    // RM : A 440 Hz x B 110 Hz -> 330 et 550 Hz, presque plus de 440
    {
        Engine e;
        e.prepare (sr);
        auto s = cleanSine();
        s.osc[0].warp = Warp::rm;
        s.osc[1].octave = -2;
        setReal (s, Dest::aWarp, 1.0f);
        e.setSettings (s);
        e.prepare (sr);
        e.noteOn (69, 1.0f);
        auto x = render (e, 24000);
        const double side = toneLevel (x, 4800, 16384, 330.0, sr), carrier = toneLevel (x, 4800, 16384, 440.0, sr);
        CHECK (side > 0.15 && carrier < 0.01, "RM : 330 Hz %.3f, 440 Hz %.4f", side, carrier);
    }

    // AM : la porteuse reste, des bandes apparaissent
    {
        Engine e;
        e.prepare (sr);
        auto s = cleanSine();
        s.osc[0].warp = Warp::am;
        s.osc[1].octave = -2;
        setReal (s, Dest::aWarp, 1.0f);
        e.setSettings (s);
        e.prepare (sr);
        e.noteOn (69, 1.0f);
        auto x = render (e, 24000);
        CHECK (toneLevel (x, 4800, 16384, 440.0, sr) > 0.15 && toneLevel (x, 4800, 16384, 550.0, sr) > 0.05, "AM");
    }

    // SYNC : la période reste celle de l'oscillateur, le timbre change
    {
        Engine e;
        e.prepare (sr);
        auto s = cleanSine();
        s.osc[0].warp = Warp::sync;
        setReal (s, Dest::aWarp, 0.4f);
        e.setSettings (s);
        e.prepare (sr);
        e.noteOn (45, 1.0f);   // 110 Hz -> 436,4 échantillons
        auto x = render (e, 24000);
        const int period = periodByAutocorrelation (x, 4000, 4000, 300, 600);
        CHECK (std::abs (period - 436) <= 2, "SYNC : période %d", period);
        CHECK (toneLevel (x, 4800, 16384, 110.0 * 3.0, sr) > 0.02, "SYNC : harmoniques");
    }
}

static void testAliasing()
{
    // Scie à 2637 Hz (note 100) : l'énergie hors des harmoniques doit être infime
    const double sr = 48000.0;
    for (Warp w : { Warp::off, Warp::fm })
    {
        Engine e;
        e.prepare (sr);
        auto s = cleanSine();
        s.osc[0].table = basicTable();
        setReal (s, Dest::aPos, 2.0f / 3.0f);
        s.osc[0].warp = w;
        s.osc[1].octave = 0;
        setReal (s, Dest::aWarp, w == Warp::fm ? 0.04f : 0.0f);
        e.setSettings (s);
        e.prepare (sr);
        e.noteOn (100, 1.0f);
        auto x = render (e, 48000 + 8192);

        const int n = 32768;
        std::vector<std::complex<double>> spec ((size_t) n);
        for (int i = 0; i < n; ++i)
        {
            const double win = 0.35875 - 0.48829 * std::cos (twoPi * i / n) + 0.14128 * std::cos (2 * twoPi * i / n) - 0.01168 * std::cos (3 * twoPi * i / n);
            spec[(size_t) i] = { x[(size_t) (8192 + i)] * win, 0.0 };
        }
        wt::fft (spec, false);
        const double f0 = 440.0 * std::pow (2.0, (100 - 69) / 12.0);
        double harmonic = 0.0, other = 0.0;
        for (int b = 1; b < n / 2; ++b)
        {
            const double f = b * sr / n;
            const double nearest = std::round (f / f0) * f0;
            (std::abs (f - nearest) < 12.0 ? harmonic : other) += std::norm (spec[(size_t) b]);
        }
        const double db = 10.0 * std::log10 (other / harmonic);
        CHECK (db < -60.0, "repliement (%s) : %.1f dB", w == Warp::off ? "scie" : "scie + FM", db);
    }
}

static void testMonoGlide()
{
    const double sr = 48000.0;
    Engine e;
    e.prepare (sr);
    auto s = cleanSine();
    s.mono = true;
    s.glideMs = 100.0f;
    e.setSettings (s);
    e.noteOn (57, 1.0f);
    render (e, 4800);
    e.noteOn (69, 1.0f);
    auto x = render (e, 24000);
    const double early = measureFreq (x, 0, 1200, sr);
    CHECK (early > 225.0 && early < 400.0, "pendant le glide : %.1f", early);
    CHECK (near (measureFreq (x, 14400, 24000, sr), 440.0, 1.0), "fin du glide");
    CHECK (e.activeVoiceCount() == 1, "mono : une voix");
    e.noteOff (69);
    x = render (e, 24000);
    CHECK (near (measureFreq (x, 14400, 24000, sr), 220.0, 1.0), "retour à la note tenue");
    e.noteOff (57);
    render (e, 24000);
    CHECK (e.activeVoiceCount() == 0, "relâché");

    auto s2 = cleanSine();
    s2.mono = true;
    s2.seq.on = true; s2.seq.steps = 16; s2.seq.stepBeats = 0.25; s2.seq.pattern.fill (1);
    e.setSettings (s2);
    e.noteOn (60, 1.0f);
    render (e, (int) (sr * 0.40));
    const int before = e.getPlayheadStep();
    e.noteOn (62, 1.0f);
    render (e, 64);
    CHECK (before == 3 && e.getPlayheadStep() == 3, "legato garde la position : %d -> %d", before, e.getPlayheadStep());
}

static void testPolyphony()
{
    Engine e;
    e.prepare (48000.0);
    e.setSettings (cleanSine());
    for (int n = 0; n < 8; ++n) e.noteOn (48 + n, 1.0f);
    render (e, 256);
    CHECK (e.activeVoiceCount() == 8, "8 voix");
    e.noteOn (70, 1.0f);
    render (e, 256);
    CHECK (e.activeVoiceCount() == 8, "vol de voix");
    for (int n = 0; n < 128; ++n) e.noteOff (n);
    render (e, 48000);
    CHECK (e.activeVoiceCount() == 0, "tout relâché");
    e.noteOn (60, 1.0f); e.noteOn (64, 1.0f); e.noteOn (67, 1.0f);
    std::vector<float> r;
    auto l = render (e, 4800, {}, &r);
    CHECK (rms (l, 2400, 4800) > 0.3 && rms (r, 2400, 4800) > 0.3, "accord stéréo");
}

static void testFilterAndDrive()
{
    const double sr = 48000.0;
    {
        Engine e;
        e.prepare (sr);
        auto s = cleanSine();
        s.osc[0].table = basicTable();
        setReal (s, Dest::aPos, 2.0f / 3.0f);
        setReal (s, Dest::cutoff, 200.0f);
        s.slope24 = true;
        e.setSettings (s);
        e.prepare (sr);
        e.noteOn (93, 1.0f);
        auto x = render (e, 9600);
        CHECK (rms (x, 2400, 9600) < 0.02, "passe-bas 24 dB : %.4f", rms (x, 2400, 9600));
    }
    {
        Engine e;
        e.prepare (sr);
        auto s = cleanSine();
        s.filterType = FilterType::highpass;
        setReal (s, Dest::cutoff, 4000.0f);
        e.setSettings (s);
        e.prepare (sr);
        e.noteOn (33, 1.0f);
        auto x = render (e, 9600);
        CHECK (rms (x, 2400, 9600) < 0.01, "passe-haut : %.4f", rms (x, 2400, 9600));
    }
    for (int type = 0; type < 3; ++type)
    {
        Engine e;
        e.prepare (sr);
        auto s = cleanSine();
        s.driveType = (DriveType) type;
        setReal (s, Dest::drive, 1.0f);
        e.setSettings (s);
        e.prepare (sr);
        e.noteOn (45, 1.0f);
        auto x = render (e, 9600);
        float peak = 0.0f;
        for (float v : x) peak = std::max (peak, std::abs (v));
        CHECK (peak < 2.0f && rms (x, 2400, 9600) > 0.1, "drive %d : crête %.3f", type, peak);
    }
}

static void testFuzz()
{
    Rng rng (1234);
    for (double sr : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        for (int run = 0; run < 10; ++run)
        {
            Engine e;
            e.prepare (sr);
            Settings s;
            s.mono = rng.chance (0.3);
            s.glideMs = 1000.0f * rng.uniformf();
            s.velocity = rng.uniformf();
            for (auto& v : s.base) v = rng.uniformf();
            for (auto& o : s.osc)
            {
                o.table = BuiltinTables::get (rng.range (0, BuiltinTables::count() - 1));
                o.octave = rng.range (-4, 4);
                o.warp = (Warp) rng.range (0, (int) Warp::count - 1);
            }
            s.filterType = (FilterType) rng.range (0, 2); s.slope24 = rng.chance (0.5); s.keyTrack = rng.uniformf();
            s.ampA = 0.002f * rng.uniformf(); s.ampD = rng.uniformf(); s.ampS = rng.uniformf(); s.ampR = 0.2f * rng.uniformf();
            s.modA = 0.5f * rng.uniformf(); s.modD = rng.uniformf(); s.modS = rng.uniformf(); s.modR = rng.uniformf();
            s.driveType = (DriveType) rng.range (0, 2); s.drivePost = rng.chance (0.5);
            for (auto& l : s.lfo)
            {
                l.periodBeats = 0.0625 * std::pow (2.0, rng.range (0, 9));
                l.shape = (LfoShape) rng.range (0, (int) LfoShape::count - 1);
                l.amount = rng.uniformf(); l.steps = rng.chance (0.5) ? 0 : rng.range (2, 32);
                l.phase = rng.uniformf(); l.unipolar = rng.chance (0.3);
            }
            for (auto& c : s.chaos) { c.periodBeats = 0.0625 * std::pow (2.0, rng.range (0, 6)); c.amount = rng.uniformf(); c.smooth = rng.uniformf(); }
            s.seq.on = rng.chance (0.8); s.seq.steps = rng.range (1, 32); s.seq.stepBeats = 0.0625 * rng.range (1, 16);
            s.seq.swing = 0.75 * rng.uniform(); s.seq.hostSync = rng.chance (0.5); s.seq.restart = rng.chance (0.5);
            s.seq.xfadeMs = 20.0f * rng.uniformf(); s.seq.manual = rng.range (1, numSources);
            std::array<bool, numSources + 1> all {};
            all.fill (true);
            RandomPattern::generate (rng, s.seq.steps, all, s.seq.pattern);
            for (int k = 0; k < numSlots; ++k)
            {
                s.slotSource[(size_t) k] = rng.range (0, numModSources - 1);
                s.slotDest[(size_t) k] = (Dest) rng.range (0, numDests - 1);
                s.slotAmount[(size_t) k] = rng.uniformf() * 2 - 1;
            }

            e.setSettings (s);
            Engine::Transport t { 60.0 + 180.0 * rng.uniform(), rng.chance (0.5), 16.0 * rng.uniform() };
            bool finite = true;
            float peak = 0.0f;
            std::vector<float> l (512), r (512);
            for (int block = 0; block < (int) (sr / 512.0); ++block)
            {
                if (block % 20 == 0) e.noteOn (rng.range (12, 120), rng.uniformf());
                if (block % 20 == 13) e.noteOff (rng.range (12, 120));
                if (block == 30) e.setPitchBend (rng.uniformf() * 4 - 2);
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
}

static void testDestRanges()
{
    for (int d = 1; d < numDests; ++d)
        for (float n : { 0.0f, 0.3f, 0.8f, 1.0f })
        {
            const float real = DestRange::toReal ((Dest) d, n);
            CHECK (std::isfinite (real) && near (DestRange::toNorm ((Dest) d, real), n, 1e-4), "aller-retour dest %d à %.1f", d, n);
        }
    CHECK (near (DestRange::toReal (Dest::volume, 0.8f), 1.0, 1e-6), "volume 0,8 = 0 dB");
    CHECK (near (DestRange::toReal (Dest::cutoff, 1.0f), 20000.0, 0.1), "cutoff max");
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
    testRealSerumFiles (std::vector<std::string> (argv + 1, argv + argc));
    testDecimator();
    testPitch();
    testCoarseGlides();
    testModulationSlots();
    testWarp();
    testAliasing();
    testMonoGlide();
    testPolyphony();
    testFilterAndDrive();
    testFuzz();

    std::printf ("%d vérifications, %d échecs\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
