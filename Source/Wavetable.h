#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

/**
    Wavetables de Sequences (sans JUCE, testable seul).

    Une table = 1 à 256 frames d'un cycle. À la construction, chaque frame passe par une FFT et on
    fabrique 11 versions à bande limitée (mipmaps) : 1024, 512, … 1 harmoniques. À la lecture, on
    choisit la version qui ne dépasse pas la fréquence de Nyquist et on fond avec la suivante,
    ce qui évite le repliement même sur des tables très riches.

    Le lecteur WAV accepte le format des tables Serum (bloc « clm » qui donne la taille de frame,
    2048 en général, parfois 1024) et des WAV ordinaires (PCM 8/16/24/32 bits, flottant 32/64 bits,
    mono ou stéréo, « WAVE_FORMAT_EXTENSIBLE »).
*/
namespace seqs
{

namespace wt
{
    constexpr int numLevels = 11;          // 1024, 512, … 1 harmoniques
    constexpr int maxFrames = 256;
    constexpr int baseSize = 2048;         // taille des frames d'origine après rééchantillonnage
    constexpr double twoPiD = 6.283185307179586;

    inline int harmonicsForLevel (int level)   { return 1024 >> level; }
    inline int sizeForLevel (int level)        { return std::min (2048, std::max (256, 4 * harmonicsForLevel (level))); }

    /** FFT complexe en place (taille = puissance de 2). inverse : signe +, sans division. */
    inline void fft (std::vector<std::complex<double>>& a, bool inverse)
    {
        const size_t n = a.size();
        for (size_t i = 1, j = 0; i < n; ++i)
        {
            size_t bit = n >> 1;
            for (; j & bit; bit >>= 1)
                j ^= bit;
            j ^= bit;
            if (i < j)
                std::swap (a[i], a[j]);
        }

        for (size_t len = 2; len <= n; len <<= 1)
        {
            const double angle = twoPiD / (double) len * (inverse ? 1.0 : -1.0);
            const std::complex<double> wl (std::cos (angle), std::sin (angle));
            for (size_t i = 0; i < n; i += len)
            {
                std::complex<double> w (1.0, 0.0);
                for (size_t k = 0; k < len / 2; ++k)
                {
                    const auto u = a[i + k];
                    const auto v = a[i + k + len / 2] * w;
                    a[i + k] = u + v;
                    a[i + k + len / 2] = u - v;
                    w *= wl;
                }
            }
        }
    }

    inline bool isPowerOfTwo (size_t n)        { return n >= 2 && (n & (n - 1)) == 0; }

    /** Rééchantillonne un cycle à la taille voulue (interpolation linéaire, en boucle). */
    inline std::vector<float> resampleCycle (const std::vector<float>& in, int size)
    {
        std::vector<float> out ((size_t) size);
        const size_t n = in.size();
        for (int i = 0; i < size; ++i)
        {
            const double pos = (double) i * (double) n / (double) size;
            const size_t i0 = (size_t) pos;
            const double f = pos - (double) i0;
            const float a = in[i0 % n], b = in[(i0 + 1) % n];
            out[(size_t) i] = (float) (a + (b - a) * f);
        }
        return out;
    }
}

//==============================================================================
/** Une table prête à jouer (mipmaps). Immuable une fois construite. */
struct Wavetable
{
    std::string name;
    int numFrames = 0;
    std::array<int, wt::numLevels> size {};
    /** data[level] : numFrames blocs de (size + 1) échantillons (le dernier = le premier, pour interpoler sans modulo). */
    std::array<std::vector<float>, wt::numLevels> data;

    const float* frame (int level, int f) const noexcept
    {
        return data[(size_t) level].data() + (size_t) f * (size_t) (size[(size_t) level] + 1);
    }

    /** Lecture : frame (0..numFrames-1, fractionnaire), niveau (fractionnaire, 0 = le plus riche), phase 0..1. */
    float read (double framePos, double levelPos, double phase) const noexcept
    {
        const int f0 = std::clamp ((int) framePos, 0, numFrames - 1);
        const int f1 = std::min (f0 + 1, numFrames - 1);
        const float ft = (float) std::clamp (framePos - (double) f0, 0.0, 1.0);

        const int l0 = std::clamp ((int) levelPos, 0, wt::numLevels - 1);
        const int l1 = std::min (l0 + 1, wt::numLevels - 1);
        const float lt = (float) std::clamp (levelPos - (double) l0, 0.0, 1.0);

        auto lookup = [this, phase] (int level, int f)
        {
            const int n = size[(size_t) level];
            const double x = phase * (double) n;
            int i = (int) x;
            const float t = (float) (x - (double) i);
            i = std::clamp (i, 0, n - 1);
            const float* d = frame (level, f);
            return d[i] + (d[i + 1] - d[i]) * t;
        };

        auto atLevel = [&] (int level)
        {
            const float a = lookup (level, f0);
            return f1 == f0 || ft <= 0.0f ? a : a + (lookup (level, f1) - a) * ft;
        };

        const float a = atLevel (l0);
        return lt <= 0.0f || l1 == l0 ? a : a + (atLevel (l1) - a) * lt;
    }

    //==========================================================================
    /** Construit une table depuis des cycles (chaque cycle de n'importe quelle taille).
        normalise : la crête de toute la table est ramenée à 1. */
    static std::unique_ptr<Wavetable> build (std::vector<std::vector<float>> cycles, std::string tableName, bool normalise = true)
    {
        cycles.erase (std::remove_if (cycles.begin(), cycles.end(), [] (const std::vector<float>& c) { return c.size() < 2; }), cycles.end());
        if (cycles.empty())
            return nullptr;

        // Plus de 256 frames : on en garde 256 réparties sur toute la table
        if ((int) cycles.size() > wt::maxFrames)
        {
            std::vector<std::vector<float>> kept;
            for (int i = 0; i < wt::maxFrames; ++i)
                kept.push_back (std::move (cycles[(size_t) ((double) i * (double) (cycles.size() - 1) / (wt::maxFrames - 1) + 0.5)]));
            cycles = std::move (kept);
        }

        auto table = std::make_unique<Wavetable>();
        table->name = std::move (tableName);
        table->numFrames = (int) cycles.size();

        for (int l = 0; l < wt::numLevels; ++l)
        {
            table->size[(size_t) l] = wt::sizeForLevel (l);
            table->data[(size_t) l].assign ((size_t) table->numFrames * (size_t) (table->size[(size_t) l] + 1), 0.0f);
        }

        std::vector<std::complex<double>> spectrum, buffer;

        for (int f = 0; f < table->numFrames; ++f)
        {
            auto cycle = std::move (cycles[(size_t) f]);
            if (! wt::isPowerOfTwo (cycle.size()) || cycle.size() > 8192)
                cycle = wt::resampleCycle (cycle, wt::baseSize);

            const size_t n = cycle.size();
            spectrum.assign (n, {});
            for (size_t i = 0; i < n; ++i)
                spectrum[i] = { std::isfinite (cycle[i]) ? (double) cycle[i] : 0.0, 0.0 };
            wt::fft (spectrum, false);

            const int available = (int) n / 2 - 1;

            for (int l = 0; l < wt::numLevels; ++l)
            {
                const int m = table->size[(size_t) l];
                const int harmonics = std::min ({ wt::harmonicsForLevel (l), available, m / 2 - 1 });
                buffer.assign ((size_t) m, {});

                // DC retiré (bin 0) ; harmoniques 1..H recopiées avec leur symétrique
                for (int h = 1; h <= harmonics; ++h)
                {
                    const auto c = spectrum[(size_t) h] / (double) n;
                    buffer[(size_t) h] = c;
                    buffer[(size_t) (m - h)] = std::conj (c);
                }
                wt::fft (buffer, true);

                float* d = table->data[(size_t) l].data() + (size_t) f * (size_t) (m + 1);
                for (int i = 0; i < m; ++i)
                    d[i] = (float) buffer[(size_t) i].real();
                d[m] = d[0];
            }
        }

        if (normalise)
        {
            float peak = 0.0f;
            for (float v : table->data[0])
                peak = std::max (peak, std::abs (v));
            if (peak > 1.0e-9f)
                for (auto& level : table->data)
                    for (float& v : level)
                        v /= peak;
        }

        return table;
    }
};

//==============================================================================
/** Lecture d'un fichier WAV en table (format Serum compris). */
struct WavetableFile
{
    std::vector<std::vector<float>> cycles;
    int frameSize = 0;
    bool serumHeader = false;     // un bloc « clm » donnait la taille de frame
    std::string error;

    /** Lit un WAV en mémoire. Ne fait aucune confiance aux tailles annoncées (fichier quelconque). */
    static WavetableFile parse (const uint8_t* bytes, size_t length)
    {
        WavetableFile out;
        auto fail = [&out] (const char* message) { out.cycles.clear(); out.error = message; return out; };

        auto u16 = [bytes] (size_t p) { return (uint32_t) bytes[p] | ((uint32_t) bytes[p + 1] << 8); };
        auto u32 = [bytes] (size_t p) { return (uint32_t) bytes[p] | ((uint32_t) bytes[p + 1] << 8) | ((uint32_t) bytes[p + 2] << 16) | ((uint32_t) bytes[p + 3] << 24); };

        if (bytes == nullptr || length < 12 || std::memcmp (bytes, "RIFF", 4) != 0 || std::memcmp (bytes + 8, "WAVE", 4) != 0)
            return fail ("NOT A WAV FILE");

        int format = 0, channels = 0, bits = 0, blockAlign = 0;
        const uint8_t* data = nullptr;
        size_t dataLength = 0;
        int clmFrame = 0;

        size_t pos = 12;
        while (pos + 8 <= length)
        {
            const uint32_t chunkSize = u32 (pos + 4);
            const size_t body = pos + 8;
            const size_t available = length - body;
            const size_t size = std::min ((size_t) chunkSize, available);

            if (std::memcmp (bytes + pos, "fmt ", 4) == 0 && size >= 16)
            {
                format = (int) u16 (body);
                channels = (int) u16 (body + 2);
                blockAlign = (int) u16 (body + 12);
                bits = (int) u16 (body + 14);
                if (format == 0xFFFE && size >= 26)   // WAVE_FORMAT_EXTENSIBLE : le vrai format est dans le GUID
                    format = (int) u16 (body + 24);
            }
            else if (std::memcmp (bytes + pos, "data", 4) == 0)
            {
                data = bytes + body;
                dataLength = size;
            }
            else if (std::memcmp (bytes + pos, "clm ", 4) == 0 && size >= 4)
            {
                // « <!>2048 01000000 wavetable (www.xferrecords.com) »
                const std::string text ((const char*) bytes + body, std::min (size, (size_t) 64));
                if (text.rfind ("<!>", 0) == 0)
                {
                    int v = 0;
                    for (size_t i = 3; i < text.size() && text[i] >= '0' && text[i] <= '9' && v < 100000; ++i)
                        v = v * 10 + (text[i] - '0');
                    if (v >= 16 && v <= 65536)
                        clmFrame = v;
                }
            }

            if (chunkSize > available)
                break;
            pos = body + chunkSize + (chunkSize & 1u);
        }

        if (data == nullptr || channels < 1 || channels > 16)
            return fail ("NO AUDIO IN THIS FILE");

        const bool isFloat = format == 3;
        if (! ((format == 1 && (bits == 8 || bits == 16 || bits == 24 || bits == 32)) || (isFloat && (bits == 32 || bits == 64))))
            return fail ("UNSUPPORTED WAV FORMAT");

        const int bytesPerSample = bits / 8;
        if (blockAlign < bytesPerSample * channels)
            blockAlign = bytesPerSample * channels;

        const size_t numSamples = dataLength / (size_t) blockAlign;
        if (numSamples < 2)
            return fail ("FILE TOO SHORT");

        // Mono : moyenne des canaux
        std::vector<float> mono (numSamples);
        for (size_t i = 0; i < numSamples; ++i)
        {
            double sum = 0.0;
            for (int c = 0; c < channels; ++c)
            {
                const uint8_t* p = data + i * (size_t) blockAlign + (size_t) (c * bytesPerSample);
                double v = 0.0;
                if (isFloat && bits == 32)      { float f; std::memcpy (&f, p, 4); v = f; }
                else if (isFloat)               { double d; std::memcpy (&d, p, 8); v = d; }
                else if (bits == 8)             v = ((double) p[0] - 128.0) / 128.0;
                else if (bits == 16)            v = (double) (int16_t) (p[0] | (p[1] << 8)) / 32768.0;
                else if (bits == 24)            { int32_t s = (p[0] << 8) | (p[1] << 16) | (p[2] << 24); v = (double) (s >> 8) / 8388608.0; }
                else                            { int32_t s; std::memcpy (&s, p, 4); v = (double) s / 2147483648.0; }
                sum += std::isfinite (v) ? v : 0.0;
            }
            mono[i] = (float) std::clamp (sum / channels, -8.0, 8.0);
        }

        // Taille de frame : bloc clm de Serum, sinon 2048 si ça tombe juste, sinon un seul cycle
        int frame = clmFrame;
        out.serumHeader = clmFrame > 0;
        if (frame == 0)
        {
            if (numSamples % 2048 == 0)          frame = 2048;
            else if (numSamples <= 8192)         frame = (int) numSamples;
            else                                 frame = 2048;
        }

        const size_t numFrames = std::max ((size_t) 1, numSamples / (size_t) frame);
        if ((size_t) frame > numSamples)
            frame = (int) numSamples;

        out.frameSize = frame;
        for (size_t f = 0; f < numFrames && (f + 1) * (size_t) frame <= numSamples; ++f)
            out.cycles.emplace_back (mono.begin() + (long) (f * (size_t) frame), mono.begin() + (long) ((f + 1) * (size_t) frame));

        if (out.cycles.empty())
            return fail ("FILE TOO SHORT");
        return out;
    }

    /** Écrit un WAV au format Serum (flottant 32 bits mono, bloc clm). Sert aux tests et à l'export. */
    static std::vector<uint8_t> writeSerum (const std::vector<std::vector<float>>& cycles, int frameSize, int bits = 32)
    {
        std::vector<uint8_t> out;
        auto put32 = [&out] (uint32_t v) { for (int i = 0; i < 4; ++i) out.push_back ((uint8_t) (v >> (8 * i))); };
        auto put16 = [&out] (uint32_t v) { out.push_back ((uint8_t) v); out.push_back ((uint8_t) (v >> 8)); };
        auto tag = [&out] (const char* t) { out.insert (out.end(), t, t + 4); };

        const std::string clm = "<!>" + std::to_string (frameSize) + " 01000000 wavetable (www.xferrecords.com)";
        const uint32_t clmSize = (uint32_t) clm.size();
        const int bytesPerSample = bits / 8;
        const uint32_t dataSize = (uint32_t) (cycles.size() * (size_t) frameSize * (size_t) bytesPerSample);

        tag ("RIFF"); put32 (4 + 24 + 8 + clmSize + (clmSize & 1u) + 8 + dataSize); tag ("WAVE");
        tag ("fmt "); put32 (16); put16 (bits == 32 ? 3 : 1); put16 (1); put32 (44100); put32 (44100u * (uint32_t) bytesPerSample);
        put16 ((uint32_t) bytesPerSample); put16 ((uint32_t) bits);
        tag ("clm "); put32 (clmSize); out.insert (out.end(), clm.begin(), clm.end()); if (clmSize & 1u) out.push_back (0);
        tag ("data"); put32 (dataSize);
        for (const auto& c : cycles)
            for (int i = 0; i < frameSize; ++i)
            {
                const float v = i < (int) c.size() ? c[(size_t) i] : 0.0f;
                if (bits == 32) { uint32_t u; std::memcpy (&u, &v, 4); put32 (u); }
                else            put16 ((uint32_t) (uint16_t) (int16_t) std::lround (std::clamp (v, -1.0f, 1.0f) * 32767.0f));
            }
        return out;
    }
};

//==============================================================================
/** Tables intégrées, calculées une seule fois et partagées par toutes les instances. */
struct BuiltinTables
{
    static const std::vector<std::string>& names()
    {
        static const std::vector<std::string> n { "CLASSIC", "BASIC SHAPES", "PWM", "RESO SAW", "RESO TRI", "RESO TRAP",
                                                  "SAW-PULSE", "HARMONICS", "DBL SINE", "SINE" };
        return n;
    }

    static int count()                                { return (int) names().size(); }

    static const Wavetable* get (int index)
    {
        static const std::vector<std::unique_ptr<Wavetable>> tables = []
        {
            std::vector<std::unique_ptr<Wavetable>> t;
            for (int i = 0; i < (int) names().size(); ++i)
                t.push_back (Wavetable::build (cycles (i), names()[(size_t) i]));
            return t;
        }();
        return tables[(size_t) std::clamp (index, 0, (int) tables.size() - 1)].get();
    }

    static int indexOf (const std::string& name)
    {
        const auto& n = names();
        const auto it = std::find (n.begin(), n.end(), name);
        return it == n.end() ? -1 : (int) (it - n.begin());
    }

    /** Cycles d'une table intégrée (2048 échantillons chacun). */
    static std::vector<std::vector<float>> cycles (int index)
    {
        static constexpr int n = wt::baseSize;   // static : utilisable dans les lambdas sans capture (MSVC)
        std::vector<std::vector<float>> frames;
        auto make = [&frames] (int count, auto&& fn)
        {
            for (int f = 0; f < count; ++f)
            {
                std::vector<float> c ((size_t) n);
                const double x = count > 1 ? (double) f / (double) (count - 1) : 0.0;
                for (int i = 0; i < n; ++i)
                    c[(size_t) i] = (float) fn ((double) i / (double) n, x);
                frames.push_back (std::move (c));
            }
        };

        const double tp = wt::twoPiD;
        auto sine = [tp] (double p) { return std::sin (tp * p); };
        auto tri = [] (double p) { return p < 0.25 ? 4.0 * p : p < 0.75 ? 2.0 - 4.0 * p : 4.0 * p - 4.0; };
        auto saw = [] (double p) { return 1.0 - 2.0 * p; };
        auto square = [] (double p) { return p < 0.5 ? 1.0 : -1.0; };

        switch (index)
        {
            case 0:   // CLASSIC : sinus -> triangle -> scie -> carré -> impulsion, en fondu continu (64 frames)
                make (64, [&] (double p, double x)
                {
                    const double pos = x * 4.0;
                    const int k = std::min (3, (int) pos);
                    const double t = pos - k;
                    auto shape = [&] (int s, double ph)
                    {
                        if (s == 0) return sine (ph);
                        if (s == 1) return tri (ph);
                        if (s == 2) return saw (ph);
                        if (s == 3) return square (ph);
                        return ph < 0.12 ? 1.0 : -1.0;
                    };
                    return shape (k, p) * (1.0 - t) + shape (k + 1, p) * t;
                });
                break;
            case 1:   // sinus -> triangle -> scie -> carré
                make (4, [&] (double p, double x)
                {
                    const int k = (int) std::lround (x * 3.0);
                    return k == 0 ? sine (p) : k == 1 ? tri (p) : k == 2 ? saw (p) : square (p);
                });
                break;
            case 2:   // largeur d'impulsion 50 % -> 2 %
                make (32, [] (double p, double x) { return p < 0.5 - 0.48 * x ? 1.0 : -1.0; });
                break;
            case 3: case 4: case 5:   // résonance façon Casio CZ, la position déplace la résonance
                make (64, [index, tp] (double p, double x)
                {
                    const double r = 1.0 + 15.0 * x;
                    double window = 1.0 - p;
                    if (index == 4) window = p < 0.5 ? 2.0 * p : 2.0 - 2.0 * p;
                    if (index == 5) window = p < 0.75 ? 1.0 : 4.0 - 4.0 * p;
                    return std::cos (tp * r * p) * window;
                });
                break;
            case 6:   // scie-impulsion, la position ouvre la partie scie
                make (32, [] (double p, double x)
                {
                    const double w = 0.1 + 0.8 * x;
                    return p < w ? 2.0 * p / w - 1.0 : -1.0;
                });
                break;
            case 7:   // additif : on ajoute les harmoniques une à une
                make (32, [tp] (double p, double x)
                {
                    const int count = 1 + (int) std::lround (x * 31.0);
                    double v = 0.0;
                    for (int h = 1; h <= count; ++h)
                        v += std::sin (tp * h * p) / h;
                    return v;
                });
                break;
            case 8:   // double sinus, la position monte le second
                make (16, [tp] (double p, double x) { return std::sin (tp * p) + x * std::sin (2.0 * tp * p); });
                break;
            case 9:
            default:
                make (1, [&] (double p, double) { return sine (p); });
                break;
        }
        return frames;
    }
};

} // namespace seqs
