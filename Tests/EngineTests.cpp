// Tests du moteur de Sequences (sans JUCE).
#include "SequencesEngine.h"

#include <cstdio>
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

//==============================================================================
static Settings cleanSine()
{
    Settings s;
    s.fmLevel = 0.0f;
    s.oscLevel = 1.0f;
    s.oscWave = Wave::sine;
    s.noise = 0.0f;
    s.filterType = FilterType::lowpass;
    s.slope24 = false;
    s.cutoff = 20000.0f;
    s.resonance = 0.0f;
    s.filterEnv = 0.0f;
    s.keyTrack = 0.0f;
    s.ampA = 0.0005f; s.ampD = 1.0f; s.ampS = 1.0f; s.ampR = 0.01f;
    s.velocity = 0.0f;
    s.drive = 0.0f;
    s.fmEnv = 0.0f;
    s.seq.on = false;
    s.seq.manual = 1;
    for (auto& d : s.slotDest) d = Dest::off;
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

/** Période dominante par autocorrélation (en échantillons), cherchée entre lo et hi. */
static int periodByAutocorrelation (const std::vector<float>& x, int from, int len, int lo, int hi)
{
    int best = lo;
    double bestScore = -1e30;
    for (int lag = lo; lag <= hi; ++lag)
    {
        double s = 0.0, e1 = 0.0, e2 = 0.0;
        for (int i = from; i < from + len; ++i)
        {
            s += (double) x[(size_t) i] * x[(size_t) (i + lag)];
            e1 += (double) x[(size_t) i] * x[(size_t) i];
            e2 += (double) x[(size_t) (i + lag)] * x[(size_t) (i + lag)];
        }
        const double score = s / std::sqrt (e1 * e2 + 1e-12);
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

static void testPitchThroughSequencer (bool host)
{
    const double sr = 48000.0;
    Engine e;
    e.prepare (sr);
    auto s = cleanSine();
    s.seq.on = true;
    s.seq.steps = 2;
    s.seq.stepBeats = 0.25;
    s.seq.xfadeMs = 0.0f;
    s.seq.hostSync = host;
    s.lfo[0].shape = LfoShape::square;  s.lfo[0].periodBeats = 4.0; s.lfo[0].unipolar = true;   // 1 pendant le pas
    s.seq.pattern = {};
    s.seq.pattern[0] = 0;   // OFF : note jouée
    s.seq.pattern[1] = 1;   // +1 -> +12 demi-tons avec 50 %
    s.slotSource[0] = 0;
    s.slotDest[0] = Dest::pitch;
    s.slotAmount[0] = 0.5f;
    e.setSettings (s);

    Engine::Transport t { 120.0, host, 0.0 };
    e.noteOn (57, 1.0f);
    auto x = render (e, (int) (sr * 0.5), t);
    const int stepLen = (int) (sr * 0.125);
    const double f0 = measureFreq (x, 800, stepLen - 400, sr);
    const double f1 = measureFreq (x, stepLen + 800, 2 * stepLen - 400, sr);
    const double f2 = measureFreq (x, 2 * stepLen + 800, 3 * stepLen - 400, sr);
    CHECK (near (f0, 220.0, 1.0), "pas OFF : 220 Hz (%s) %.2f", host ? "HOST" : "NOTE", f0);
    CHECK (near (f1, 440.0, 1.5), "pas LFO : 440 Hz (%s) %.2f", host ? "HOST" : "NOTE", f1);
    CHECK (near (f2, 220.0, 1.0), "retour (%s) %.2f", host ? "HOST" : "NOTE", f2);
    CHECK (e.getPlayheadStep() >= 0, "tête de lecture");
}

static void testPitchIsContinuous()
{
    // Un LFO lent sur PITCH glisse sans s'arrêter aux demi-tons
    const double sr = 48000.0;
    Engine e;
    e.prepare (sr);
    auto s = cleanSine();
    s.seq.on = false;
    s.seq.manual = 1;
    s.lfo[0].shape = LfoShape::saw;
    s.lfo[0].periodBeats = 8.0;
    s.slotDest[0] = Dest::pitch;
    s.slotAmount[0] = 1.0f;
    e.setSettings (s);
    e.noteOn (69, 1.0f);
    auto x = render (e, 48000);
    const double fa = measureFreq (x, 2000, 7000, sr), fb = measureFreq (x, 40000, 47000, sr);
    const double semis = 12.0 * std::log2 (fb / fa);
    CHECK (semis > 2.0 && std::abs (semis - std::round (semis)) > 0.05, "pitch continu : %.3f demi-tons", semis);
}

static void testAmpGate()
{
    const double sr = 48000.0;
    Engine e;
    e.prepare (sr);
    auto s = cleanSine();
    s.seq.on = true;
    s.seq.steps = 2;
    s.seq.stepBeats = 0.25;
    s.seq.xfadeMs = 2.0f;
    s.lfo[0].shape = LfoShape::square; s.lfo[0].periodBeats = 4.0; s.lfo[0].unipolar = true;
    s.seq.pattern = {};
    s.seq.pattern[0] = 1;
    s.seq.pattern[1] = 0;
    s.slotDest[2] = Dest::amp;
    s.slotAmount[2] = 1.0f;
    e.setSettings (s);
    e.noteOn (45, 1.0f);
    auto x = render (e, (int) (sr * 0.5));
    const int stepLen = (int) (sr * 0.125);
    CHECK (rms (x, 1000, stepLen - 200) > 0.2, "pas ouvert : %.3f", rms (x, 1000, stepLen - 200));
    CHECK (rms (x, stepLen + 800, 2 * stepLen - 200) < 0.005, "pas fermé : %.4f", rms (x, stepLen + 800, 2 * stepLen - 200));

    float maxJump = 0.0f;
    for (size_t i = 1; i < x.size(); ++i)
        maxJump = std::max (maxJump, std::abs (x[i] - x[i - 1]));
    CHECK (maxJump < 0.06f, "pas de clic : %.4f", maxJump);

    // Profondeur négative : inversé
    s.slotAmount[2] = -1.0f;
    e.reset();
    e.setSettings (s);
    e.noteOn (45, 1.0f);
    x = render (e, (int) (sr * 0.5));
    CHECK (rms (x, 1000, stepLen - 200) < 0.005, "inversé : fermé");
    CHECK (rms (x, stepLen + 800, 2 * stepLen - 200) > 0.2, "inversé : ouvert");
}

static void testDirectSlot()
{
    // Un routage peut lire une source de la banque sans passer par le séquenceur
    const double sr = 48000.0;
    Engine e;
    e.prepare (sr);
    auto s = cleanSine();
    s.seq.on = true;
    s.seq.pattern = {};                // le séquenceur ne joue que des OFF
    s.lfo[3].shape = LfoShape::square; s.lfo[3].periodBeats = 4.0; s.lfo[3].unipolar = true;
    s.slotSource[1] = 4;               // LFO 4 directement
    s.slotDest[1] = Dest::pitch;
    s.slotAmount[1] = 0.5f;
    e.setSettings (s);
    e.noteOn (57, 1.0f);
    auto x = render (e, 24000);
    CHECK (near (measureFreq (x, 2000, 20000, sr), 440.0, 1.0), "routage direct LFO 4");
}

static void testSyncOscillator()
{
    const double sr = 48000.0;

    // Sans sync : scie juste
    {
        Engine e;
        e.prepare (sr);
        auto s = cleanSine();
        s.oscWave = Wave::saw;
        e.setSettings (s);
        e.noteOn (57, 1.0f);
        auto x = render (e, 24000);
        CHECK (near (measureFreq (x, 2400, 24000, sr), 220.0, 0.5), "scie 220 Hz : %.2f", measureFreq (x, 2400, 24000, sr));
    }

    // Avec sync : la période reste celle du maître, le timbre change
    for (Wave w : { Wave::saw, Wave::square, Wave::triangle, Wave::sine })
    {
        Engine e;
        e.prepare (sr);
        auto s = cleanSine();
        s.oscWave = w;
        s.oscSync = 0.55f;
        e.setSettings (s);
        e.noteOn (45, 1.0f);   // 110 Hz -> période 436,4 échantillons
        auto x = render (e, 24000);
        const int period = periodByAutocorrelation (x, 4000, 4000, 300, 600);
        CHECK (std::abs (period - 436) <= 2, "sync : période du maître (forme %d) : %d", (int) w, period);

        float peak = 0.0f;
        for (float v : x) peak = std::max (peak, std::abs (v));
        CHECK (peak < 1.5f && rms (x, 2000, 24000) > 0.2, "sync borné (forme %d) : %.3f", (int) w, peak);
    }

    // BLEP : une scie aiguë a nettement moins de repliement qu'une scie naïve
    {
        SyncOscillator osc;
        const double dt = 3520.0 / sr;
        std::vector<double> y (8192);
        for (auto& v : y) v = osc.process (Wave::saw, dt, dt, 0.5, false);
        // Énergie de l'erreur par rapport à la scie idéale band-limitée (série de Fourier)
        double err = 0.0, errNaive = 0.0;
        for (int i = 100; i < 8100; ++i)
        {
            const double t = (double) i * dt;   // la sortie i correspond à la phase i * dt
            double ideal = 0.0;
            for (int k = 1; k * 3520.0 < sr / 2; ++k)
                ideal -= 2.0 / (pi * k) * std::sin (twoPi * k * t);
            double p = t - std::floor (t);
            err += (y[(size_t) i] - ideal) * (y[(size_t) i] - ideal);
            errNaive += (2.0 * p - 1.0 - ideal) * (2.0 * p - 1.0 - ideal);
        }
        CHECK (err < 0.5 * errNaive, "polyBLEP : erreur %.2f contre %.2f en naïf", err, errNaive);
    }
}

static void testModWaves()
{
    for (int w = 0; w < (int) ModWave::count; ++w)
        for (double timbre : { 0.0, 0.5, 1.0 })
        {
            bool ok = true;
            double lo = 1e9, hi = -1e9;
            for (int i = 0; i < 1000; ++i)
            {
                const double v = detail::modWave ((ModWave) w, i / 1000.0, timbre);
                ok = ok && std::isfinite (v) && std::abs (v) <= 1.5;
                lo = std::min (lo, v); hi = std::max (hi, v);
            }
            CHECK (ok && hi - lo > 0.5, "forme %d, timbre %.1f : %.2f..%.2f", w, timbre, lo, hi);
        }

    // RESO : le timbre déplace la résonance (plus de passages à zéro)
    auto crossings = [] (double timbre)
    {
        int n = 0;
        double prev = detail::modWave (ModWave::reso1, 0.0, timbre);
        for (int i = 1; i < 2000; ++i)
        {
            const double v = detail::modWave (ModWave::reso1, i / 2000.0, timbre);
            n += (prev < 0.0) != (v < 0.0) ? 1 : 0;
            prev = v;
        }
        return n;
    };
    CHECK (crossings (1.0) > 3 * crossings (0.1), "timbre RESO : %d contre %d", crossings (1.0), crossings (0.1));

    // Le son FM change avec la forme de la modulante
    const double sr = 48000.0;
    double previous = -1.0;
    bool allDifferent = true;
    for (int w = 0; w < (int) ModWave::count; ++w)
    {
        Engine e;
        e.prepare (sr);
        auto s = cleanSine();
        s.oscLevel = 0.0f;
        s.fmLevel = 1.0f;
        s.fmIndex = 2.0f;
        s.modWave = (ModWave) w;
        e.setSettings (s);
        e.noteOn (57, 1.0f);
        auto x = render (e, 9600);
        double hf = 0.0;
        for (size_t i = 2401; i < x.size(); ++i) hf += std::abs (x[i] - x[i - 1]);
        allDifferent = allDifferent && std::abs (hf - previous) > 1e-3;
        previous = hf;
    }
    CHECK (allDifferent, "chaque forme de modulante sonne différemment");
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
    const double late = measureFreq (x, 14400, 24000, sr);
    CHECK (early > 225.0 && early < 400.0, "pendant le glide : %.1f", early);
    CHECK (near (late, 440.0, 1.0), "fin du glide : %.2f", late);
    CHECK (e.activeVoiceCount() == 1, "mono : une seule voix");

    e.noteOff (69);
    x = render (e, 24000);
    CHECK (near (measureFreq (x, 14400, 24000, sr), 220.0, 1.0), "retour à la note tenue");
    e.noteOff (57);
    render (e, 24000);
    CHECK (e.activeVoiceCount() == 0, "mono : relâché");

    // Legato : le séquenceur ne repart pas
    auto s2 = cleanSine();
    s2.mono = true;
    s2.seq.on = true;
    s2.seq.steps = 16;
    s2.seq.stepBeats = 0.25;
    s2.seq.pattern.fill (1);
    e.setSettings (s2);
    e.noteOn (60, 1.0f);
    render (e, (int) (sr * 0.40));
    const int before = e.getPlayheadStep();
    e.noteOn (62, 1.0f);
    render (e, 64);
    CHECK (before == 3 && e.getPlayheadStep() == 3, "legato garde la position : %d -> %d", before, e.getPlayheadStep());
    e.allNotesOff (true);
    e.noteOn (64, 1.0f);
    render (e, 64);
    CHECK (e.getPlayheadStep() == 0, "nouvelle note : pas 0");
}

static void testPolyphony()
{
    Engine e;
    e.prepare (48000.0);
    e.setSettings (cleanSine());
    for (int n = 0; n < 8; ++n)
        e.noteOn (48 + n, 1.0f);
    render (e, 256);
    CHECK (e.activeVoiceCount() == 8, "8 voix : %d", e.activeVoiceCount());
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
    e.allNotesOff (true);
    CHECK (e.activeVoiceCount() == 0, "coupure immédiate");
}

static void testFilterAndDrive()
{
    const double sr = 48000.0;
    {
        Engine e;
        e.prepare (sr);
        auto s = cleanSine();
        s.oscWave = Wave::saw;
        s.cutoff = 200.0f;
        s.slope24 = true;
        e.setSettings (s);
        e.noteOn (93, 1.0f);
        auto x = render (e, 9600);
        CHECK (rms (x, 2400, 9600) < 0.02, "passe-bas 24 dB : %.4f", rms (x, 2400, 9600));
    }
    {
        Engine e;
        e.prepare (sr);
        auto s = cleanSine();
        s.filterType = FilterType::highpass;
        s.cutoff = 4000.0f;
        e.setSettings (s);
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
        s.drive = 1.0f;
        e.setSettings (s);
        e.noteOn (45, 1.0f);
        auto x = render (e, 9600);
        float peak = 0.0f;
        for (float v : x) peak = std::max (peak, std::abs (v));
        CHECK (peak < 2.0f && rms (x, 2400, 9600) > 0.1, "drive %d : crête %.3f", type, peak);
    }
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

static void testFuzz()
{
    Rng rng (1234);
    for (double sr : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
    {
        for (int run = 0; run < 10; ++run)
        {
            Engine e;
            e.prepare (sr);
            Settings s;
            s.mono = rng.chance (0.3);
            s.glideMs = 1000.0f * rng.uniformf();
            s.velocity = rng.uniformf();
            s.transpose = rng.range (-24, 24);
            s.fmLevel = rng.uniformf(); s.fmRatio = 0.25f + 15.75f * rng.uniformf();
            s.fmIndex = 10.0f * rng.uniformf(); s.fmFeedback = rng.uniformf(); s.fmEnv = rng.uniformf() * 2 - 1;
            s.modWave = (ModWave) rng.range (0, (int) ModWave::count - 1); s.timbre = rng.uniformf();
            s.oscLevel = rng.uniformf(); s.oscWave = (Wave) rng.range (0, 3); s.oscOctave = rng.range (-2, 2);
            s.oscDetune = 200.0f * rng.uniformf() - 100.0f; s.oscPw = 0.05f + 0.9f * rng.uniformf(); s.oscSync = rng.uniformf();
            s.noise = rng.uniformf();
            s.filterType = (FilterType) rng.range (0, 2); s.slope24 = rng.chance (0.5);
            s.cutoff = 20.0f * std::pow (1000.0f, rng.uniformf()); s.resonance = rng.uniformf();
            s.filterEnv = rng.uniformf() * 2 - 1; s.keyTrack = rng.uniformf();
            s.ampA = 0.002f * rng.uniformf(); s.ampD = rng.uniformf(); s.ampS = rng.uniformf(); s.ampR = 0.2f * rng.uniformf();
            s.modA = 0.5f * rng.uniformf(); s.modD = rng.uniformf(); s.modS = rng.uniformf(); s.modR = rng.uniformf();
            s.driveType = (DriveType) rng.range (0, 2); s.drive = rng.uniformf(); s.drivePost = rng.chance (0.5);

            for (auto& l : s.lfo)
            {
                l.periodBeats = 0.0625 * std::pow (2.0, rng.range (0, 9));
                l.shape = (LfoShape) rng.range (0, (int) LfoShape::count - 1);
                l.amount = rng.uniformf();
                l.steps = rng.chance (0.5) ? 0 : rng.range (2, 32);
                l.phase = rng.uniformf();
                l.unipolar = rng.chance (0.3);
            }
            for (auto& c : s.chaos)
            {
                c.periodBeats = 0.0625 * std::pow (2.0, rng.range (0, 6));
                c.amount = rng.uniformf();
                c.smooth = rng.uniformf();
            }
            s.seq.on = rng.chance (0.8);
            s.seq.steps = rng.range (1, 32);
            s.seq.stepBeats = 0.0625 * rng.range (1, 16);
            s.seq.swing = 0.75 * rng.uniform();
            s.seq.hostSync = rng.chance (0.5);
            s.seq.restart = rng.chance (0.5);
            s.seq.xfadeMs = 20.0f * rng.uniformf();
            s.seq.manual = rng.range (1, numSources);
            std::array<bool, numSources + 1> all {};
            all.fill (true);
            RandomPattern::generate (rng, s.seq.steps, all, s.seq.pattern);
            for (int k = 0; k < numSlots; ++k)
            {
                s.slotSource[(size_t) k] = rng.range (0, numSources);
                s.slotDest[(size_t) k] = (Dest) rng.range (0, (int) Dest::count - 1);
                s.slotAmount[(size_t) k] = rng.uniformf() * 2 - 1;
            }

            e.setSettings (s);
            Engine::Transport t { 60.0 + 180.0 * rng.uniform(), rng.chance (0.5), 16.0 * rng.uniform() };

            bool finite = true;
            float peak = 0.0f;
            std::vector<float> l (512), r (512);
            for (int block = 0; block < (int) (sr / 512.0 * 1.2); ++block)
            {
                if (block % 20 == 0) e.noteOn (rng.range (24, 96), rng.uniformf());
                if (block % 20 == 13) e.noteOff (rng.range (24, 96));
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

int main()
{
    testStepPositions();
    testLfoShapes();
    testChaos();
    testSequencerSources();
    testCrossfade();
    testPitchThroughSequencer (false);
    testPitchThroughSequencer (true);
    testPitchIsContinuous();
    testAmpGate();
    testDirectSlot();
    testSyncOscillator();
    testModWaves();
    testMonoGlide();
    testPolyphony();
    testFilterAndDrive();
    testRandomPatterns();
    testFuzz();

    std::printf ("%d vérifications, %d échecs\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
