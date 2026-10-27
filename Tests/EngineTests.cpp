// Tests du moteur de Sequences (sans JUCE).
#include "SequencesEngine.h"

#include <cstdio>
#include <functional>
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
    for (auto& q : s.seq) q.on = false;
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

/** Fréquence par passages à zéro montants (interpolés), sur [from, to). */
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

//==============================================================================
static void testStepPositions()
{
    // 16 doubles-croches droites
    CHECK (stepAt (0.0, 16, 0.25, 0.0).step == 0, "pas 0");
    CHECK (stepAt (0.25, 16, 0.25, 0.0).step == 1, "pas 1");
    CHECK (stepAt (3.99, 16, 0.25, 0.0).step == 15, "pas 15");
    CHECK (stepAt (4.0, 16, 0.25, 0.0).step == 0, "boucle");
    CHECK (std::abs (stepAt (0.125, 16, 0.25, 0.0).frac - 0.5) < 1e-9, "milieu du pas");

    // 3 pas : la boucle dure 3 doubles-croches
    CHECK (stepAt (0.75, 3, 0.25, 0.0).step == 0, "3 pas");
    CHECK (stepAt (1.0, 3, 0.25, 0.0).step == 1, "3 pas, suivant");

    // Swing 50 % : le pas pair dure 0,375, l'impair 0,125
    CHECK (stepAt (0.3, 16, 0.25, 0.5).step == 0, "swing pair");
    CHECK (stepAt (0.38, 16, 0.25, 0.5).step == 1, "swing impair");
    CHECK (stepAt (0.5, 16, 0.25, 0.5).step == 2, "swing : la paire dure toujours une croche");
    CHECK (std::abs (stepAt (0.0, 16, 0.25, 0.5).lengthBeats - 0.375) < 1e-9, "durée du pas pair");

    // Triolet de doubles-croches (1/16 × 2/3)
    CHECK (stepAt (1.0 / 6.0 * 5.0 + 1e-9, 32, 0.25 * 2.0 / 3.0, 0.0).step == 5, "triolet");

    // Glide : la valeur va vers celle du pas suivant
    SeqSettings s;
    s.steps = 2;
    s.value[0] = 0.0f; s.value[1] = 1.0f;
    s.glide[0] = true;
    StepPos p; p.step = 0; p.frac = 0.25;
    CHECK (std::abs (rawSeqValue (s, p) - 0.25f) < 1e-6f, "glide à 25 %%");
    s.glide[0] = false;
    CHECK (rawSeqValue (s, p) == 0.0f, "sans glide");
    p.step = 1; p.frac = 0.5; s.glide[1] = true;
    CHECK (std::abs (rawSeqValue (s, p) - 0.5f) < 1e-6f, "glide du dernier pas vers le premier");
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
        CHECK (std::abs (f - 440.0) < 0.5, "sinus 440 Hz à %.0f Hz : %.3f", sr, f);
    }

    // FM sans indice = sinus pur de la porteuse
    {
        Engine e;
        e.prepare (48000.0);
        auto s = cleanSine();
        s.oscLevel = 0.0f;
        s.fmLevel = 1.0f;
        s.fmIndex = 0.0f;
        e.setSettings (s);
        e.noteOn (57, 1.0f);
        auto x = render (e, 24000);
        CHECK (std::abs (measureFreq (x, 2400, 24000, 48000.0) - 220.0) < 0.3, "porteuse FM 220 Hz");
    }

    // Transposition et octave de l'oscillateur
    {
        Engine e;
        e.prepare (48000.0);
        auto s = cleanSine();
        s.transpose = 12;
        s.oscOctave = -1;
        e.setSettings (s);
        e.noteOn (69, 1.0f);
        auto x = render (e, 24000);
        CHECK (std::abs (measureFreq (x, 2400, 24000, 48000.0) - 440.0) < 0.5, "transpose +12, octave -1");
    }
}

static void testSequencerPitch (bool hostSync)
{
    const double sr = 48000.0;
    Engine e;
    e.prepare (sr);
    auto s = cleanSine();
    auto& q = s.seq[0];
    q.on = true;
    q.steps = 2;
    q.stepBeats = 0.25;            // 1/16 à 120 BPM = 0,125 s
    q.hostSync = hostSync;
    q.value[0] = 0.0f;
    q.value[1] = 0.5f;             // × 24 demi-tons -> +12
    q.dest[0] = Dest::pitch;
    q.amount[0] = 1.0f;
    e.setSettings (s);

    Engine::Transport t;
    t.bpm = 120.0;
    t.playing = hostSync;
    t.ppq = 0.0;
    e.noteOn (57, 1.0f);           // 220 Hz
    auto x = render (e, (int) (sr * 0.5), t);

    const int stepLen = (int) (sr * 0.125);
    const double f0 = measureFreq (x, 600, stepLen - 600, sr);
    const double f1 = measureFreq (x, stepLen + 600, 2 * stepLen - 600, sr);
    const double f2 = measureFreq (x, 2 * stepLen + 600, 3 * stepLen - 600, sr);
    CHECK (std::abs (f0 - 220.0) < 1.0, "pas 0 = 220 Hz (%s) : %.2f", hostSync ? "HOST" : "NOTE", f0);
    CHECK (std::abs (f1 - 440.0) < 1.5, "pas 1 = +12 (%s) : %.2f", hostSync ? "HOST" : "NOTE", f1);
    CHECK (std::abs (f2 - 220.0) < 1.0, "retour au pas 0 (%s) : %.2f", hostSync ? "HOST" : "NOTE", f2);
}

static void testNoteRetrigAndHost()
{
    const double sr = 48000.0;
    auto s = cleanSine();
    auto& q = s.seq[0];
    q.on = true;
    q.steps = 16;
    q.stepBeats = 0.25;
    q.dest[0] = Dest::pitch;
    q.amount[0] = 0.0f;   // ne change rien au son, on lit seulement la position

    // NOTE : une nouvelle note repart du pas 0
    {
        Engine e;
        e.prepare (sr);
        q.dest[0] = Dest::cutoff;
        q.amount[0] = 0.1f;
        e.setSettings (s);
        e.noteOn (60, 1.0f);
        render (e, (int) (sr * 0.40));                 // ~ 3,2 pas
        CHECK (e.getPlayheadStep (0) == 3, "position avant la seconde note : %d", e.getPlayheadStep (0));
        e.noteOff (60);
        e.noteOn (62, 1.0f);
        render (e, 256);
        CHECK (e.getPlayheadStep (0) == 0, "NOTE : repart au pas 0 : %d", e.getPlayheadStep (0));
    }

    // HOST : la position vient du morceau
    {
        Engine e;
        e.prepare (sr);
        q.hostSync = true;
        e.setSettings (s);
        e.noteOn (60, 1.0f);
        Engine::Transport t { 120.0, true, 1.0 };      // une noire après le début -> pas 4
        std::vector<float> l (64), r (64);
        e.render (l.data(), r.data(), 64, t);
        CHECK (e.getPlayheadStep (0) == 4, "HOST : pas 4 : %d", e.getPlayheadStep (0));
        t.ppq = 3.8;
        e.render (l.data(), r.data(), 64, t);
        CHECK (e.getPlayheadStep (0) == 15, "HOST : pas 15 : %d", e.getPlayheadStep (0));
    }

    // Séquenceur éteint : pas de tête de lecture
    {
        Engine e;
        e.prepare (sr);
        auto s2 = cleanSine();
        e.setSettings (s2);
        e.noteOn (60, 1.0f);
        render (e, 128);
        CHECK (e.getPlayheadStep (0) == -1, "séquenceur éteint");
    }
}

static void testAmpGate()
{
    const double sr = 48000.0;
    Engine e;
    e.prepare (sr);
    auto s = cleanSine();
    auto& q = s.seq[1];
    q.on = true;
    q.steps = 2;
    q.stepBeats = 0.25;
    q.value[0] = 1.0f;
    q.value[1] = 0.0f;
    q.dest[2] = Dest::amp;
    q.amount[2] = 1.0f;
    e.setSettings (s);
    e.noteOn (45, 1.0f);
    auto x = render (e, (int) (sr * 0.5));
    const int stepLen = (int) (sr * 0.125);
    CHECK (rms (x, 1000, stepLen - 200) > 0.2, "pas ouvert : %.3f", rms (x, 1000, stepLen - 200));
    CHECK (rms (x, stepLen + 600, 2 * stepLen - 200) < 0.005, "pas fermé : %.4f", rms (x, stepLen + 600, 2 * stepLen - 200));

    // Pas de clic : jamais de saut brusque entre deux échantillons
    float maxJump = 0.0f;
    for (size_t i = 1; i < x.size(); ++i)
        maxJump = std::max (maxJump, std::abs (x[i] - x[i - 1]));
    CHECK (maxJump < 0.06f, "saut maxi %.4f", maxJump);

    // Profondeur négative : le motif est inversé
    q.amount[2] = -1.0f;
    e.reset();
    e.setSettings (s);
    e.noteOn (45, 1.0f);
    x = render (e, (int) (sr * 0.5));
    CHECK (rms (x, 1000, stepLen - 200) < 0.005, "inversé : pas 0 fermé");
    CHECK (rms (x, stepLen + 600, 2 * stepLen - 200) > 0.2, "inversé : pas 1 ouvert");
}

static void testSmoothing()
{
    // Lissage à 100 % : la valeur met du temps à arriver
    const double sr = 48000.0;
    Engine e;
    e.prepare (sr);
    auto s = cleanSine();
    auto& q = s.seq[0];
    q.on = true;
    q.steps = 2;
    q.stepBeats = 0.5;
    q.value[0] = 1.0f; q.value[1] = 0.0f;
    q.dest[0] = Dest::amp;
    q.amount[0] = 1.0f;
    q.smooth = 1.0f;
    e.setSettings (s);
    e.noteOn (45, 1.0f);
    auto x = render (e, (int) (sr * 0.5));
    const int stepLen = (int) (sr * 0.25);
    // Juste après le changement de pas, le son est encore là ; à la fin du pas il a presque disparu
    CHECK (rms (x, stepLen + 100, stepLen + 1000) > 0.3, "lissé : encore présent");
    CHECK (rms (x, 2 * stepLen - 1500, 2 * stepLen - 50) < 0.1, "lissé : presque éteint en fin de pas");
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
    e.noteOn (69, 1.0f);                    // legato : glisse de 220 vers 440
    auto x = render (e, 24000);
    const double early = measureFreq (x, 0, 1200, sr);
    const double late = measureFreq (x, 14400, 24000, sr);
    CHECK (early > 225.0 && early < 400.0, "pendant le glide : %.1f", early);
    CHECK (std::abs (late - 440.0) < 1.0, "fin du glide : %.2f", late);
    CHECK (e.activeVoiceCount() == 1, "mono : une seule voix");

    e.noteOff (69);                         // revient sur la note encore tenue
    x = render (e, 24000);
    CHECK (std::abs (measureFreq (x, 14400, 24000, sr) - 220.0) < 1.0, "retour à la note tenue");
    e.noteOff (57);
    render (e, 24000);
    CHECK (e.activeVoiceCount() == 0, "mono : relâché");
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
    CHECK (e.activeVoiceCount() == 8, "vol de voix : %d", e.activeVoiceCount());
    e.noteOn (70, 1.0f);   // même note : même voix
    render (e, 256);
    CHECK (e.activeVoiceCount() == 8, "même note rejouée");

    for (int n = 0; n < 128; ++n) e.noteOff (n);
    render (e, 48000);
    CHECK (e.activeVoiceCount() == 0, "tout relâché : %d", e.activeVoiceCount());

    // Accord : les voix sonnent ensemble
    e.noteOn (60, 1.0f); e.noteOn (64, 1.0f); e.noteOn (67, 1.0f);
    std::vector<float> r;
    auto l = render (e, 4800, {}, &r);
    CHECK (rms (l, 2400, 4800) > 0.3 && rms (r, 2400, 4800) > 0.3, "accord stéréo");

    e.allNotesOff (true);
    CHECK (e.activeVoiceCount() == 0, "coupure immédiate");
}

static void testFuzz()
{
    Rng rng (1234);
    for (double sr : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
    {
        for (int run = 0; run < 12; ++run)
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
            s.oscLevel = rng.uniformf(); s.oscWave = (Wave) rng.range (0, 3); s.oscOctave = rng.range (-2, 2);
            s.oscDetune = 200.0f * rng.uniformf() - 100.0f; s.oscPw = 0.05f + 0.9f * rng.uniformf();
            s.noise = rng.uniformf();
            s.filterType = (FilterType) rng.range (0, 2); s.slope24 = rng.chance (0.5);
            s.cutoff = 20.0f * std::pow (1000.0f, rng.uniformf()); s.resonance = rng.uniformf();
            s.filterEnv = rng.uniformf() * 2 - 1; s.keyTrack = rng.uniformf();
            s.ampA = 0.002f * rng.uniformf(); s.ampD = rng.uniformf(); s.ampS = rng.uniformf(); s.ampR = 0.2f * rng.uniformf();
            s.modA = 0.5f * rng.uniformf(); s.modD = rng.uniformf(); s.modS = rng.uniformf(); s.modR = rng.uniformf();
            s.driveType = (DriveType) rng.range (0, 2); s.drive = rng.uniformf(); s.drivePost = rng.chance (0.5);
            s.level = 1.0f;

            for (auto& q : s.seq)
            {
                q.on = rng.chance (0.8);
                q.steps = rng.range (1, 32);
                q.stepBeats = 0.0625 * rng.range (1, 16);
                q.swing = 0.75 * rng.uniform();
                q.smooth = rng.uniformf();
                q.hostSync = rng.chance (0.5);
                for (int k = 0; k < numSlots; ++k)
                {
                    q.dest[(size_t) k] = (Dest) rng.range (0, (int) Dest::count - 1);
                    q.amount[(size_t) k] = rng.uniformf() * 2 - 1;
                }
                RandomPattern::generate (rng, q.steps, q.value, q.glide);
            }

            e.setSettings (s);
            Engine::Transport t { 60.0 + 180.0 * rng.uniform(), rng.chance (0.5), 16.0 * rng.uniform() };

            bool finite = true;
            float peak = 0.0f;
            std::vector<float> l (512), r (512);
            for (int block = 0; block < (int) (sr / 512.0 * 1.5); ++block)
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

static void testRandomPatterns()
{
    for (int style = 0; style < RandomPattern::numStyles; ++style)
        for (int steps : { 1, 3, 8, 16, 32 })
        {
            Rng a (style * 100 + steps), b (style * 100 + steps);
            std::array<float, maxSteps> v1 {}, v2 {};
            std::array<bool, maxSteps> g1 {}, g2 {};
            CHECK (RandomPattern::generate (a, steps, v1, g1, style) == style, "style imposé");
            RandomPattern::generate (b, steps, v2, g2, style);
            CHECK (v1 == v2 && g1 == g2, "même graine = même motif (style %d)", style);

            bool inRange = true;
            float lo = 1.0f, hi = 0.0f;
            for (int i = 0; i < maxSteps; ++i)
            {
                inRange = inRange && v1[(size_t) i] >= 0.0f && v1[(size_t) i] <= 1.0f;
                if (i < std::max (steps, 4)) { lo = std::min (lo, v1[(size_t) i]); hi = std::max (hi, v1[(size_t) i]); }
            }
            CHECK (inRange, "valeurs entre 0 et 1 (style %d, %d pas)", style, steps);
            CHECK (hi - lo > 0.05f, "le motif bouge (style %s, %d pas) : %.3f", RandomPattern::styleName (style), steps, hi - lo);
        }

    // Deux tirages successifs sont différents
    Rng rng (7);
    std::array<float, maxSteps> v1 {}, v2 {};
    std::array<bool, maxSteps> g {};
    RandomPattern::generate (rng, 16, v1, g);
    RandomPattern::generate (rng, 16, v2, g);
    CHECK (v1 != v2, "deux tirages différents");

    // Tous les styles sortent au hasard
    std::array<int, RandomPattern::numStyles> seen {};
    for (int i = 0; i < 600; ++i)
        ++seen[(size_t) RandomPattern::generate (rng, 16, v1, g)];
    for (int sIdx = 0; sIdx < RandomPattern::numStyles; ++sIdx)
        CHECK (seen[(size_t) sIdx] > 40, "style %d tiré %d fois", sIdx, seen[(size_t) sIdx]);
}

static void testFilterAndDrive()
{
    const double sr = 48000.0;
    // Passe-bas fermé : une scie aiguë est très atténuée
    {
        Engine e;
        e.prepare (sr);
        auto s = cleanSine();
        s.oscWave = Wave::saw;
        s.cutoff = 200.0f;
        s.slope24 = true;
        e.setSettings (s);
        e.noteOn (93, 1.0f);   // 1760 Hz
        auto x = render (e, 9600);
        CHECK (rms (x, 2400, 9600) < 0.02, "passe-bas 24 dB : %.4f", rms (x, 2400, 9600));
    }
    // Passe-haut : une basse disparaît
    {
        Engine e;
        e.prepare (sr);
        auto s = cleanSine();
        s.filterType = FilterType::highpass;
        s.cutoff = 4000.0f;
        e.setSettings (s);
        e.noteOn (33, 1.0f);   // 55 Hz
        auto x = render (e, 9600);
        CHECK (rms (x, 2400, 9600) < 0.01, "passe-haut : %.4f", rms (x, 2400, 9600));
    }
    // Séquenceur sur la coupure : le niveau suit les pas
    {
        Engine e;
        e.prepare (sr);
        auto s = cleanSine();
        s.oscWave = Wave::saw;
        s.cutoff = 100.0f;
        s.slope24 = true;
        auto& q = s.seq[0];
        q.on = true; q.steps = 2; q.stepBeats = 0.25;
        q.value[0] = 0.0f; q.value[1] = 1.0f;
        q.dest[1] = Dest::cutoff; q.amount[1] = 1.0f;   // +6 octaves sur le pas 1
        e.setSettings (s);
        e.noteOn (81, 1.0f);
        auto x = render (e, 12000);
        const double closed = rms (x, 1000, 5800), open = rms (x, 6600, 11800);
        CHECK (open > 10.0 * closed, "coupure séquencée : fermé %.4f, ouvert %.4f", closed, open);
    }
    // Distorsions : bornées, et 0 = transparent
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

int main()
{
    testStepPositions();
    testPitch();
    testSequencerPitch (false);
    testSequencerPitch (true);
    testNoteRetrigAndHost();
    testAmpGate();
    testSmoothing();
    testMonoGlide();
    testPolyphony();
    testFilterAndDrive();
    testRandomPatterns();
    testFuzz();

    std::printf ("%d vérifications, %d échecs\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
