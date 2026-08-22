#pragma once
#include <cstdio>
#include <cmath>
#include <string>
#include <vector>
#include <juce_audio_basics/juce_audio_basics.h>

// A deliberately tiny assertion harness — no external framework, so no extra
// dependency and no extra license to audit. Exits non-zero on any failure so
// CI can gate on it.

namespace t
{

inline int  gPass = 0;
inline int  gFail = 0;
inline std::string gSection;

inline void section (const char* name)
{
    gSection = name;
    std::printf ("\n\033[1m%s\033[0m\n", name);
}

inline void ok (bool cond, const std::string& what, const std::string& detail = {})
{
    if (cond)
    {
        ++gPass;
        std::printf ("  \033[32mPASS\033[0m  %s", what.c_str());
        if (! detail.empty()) std::printf ("   \033[2m(%s)\033[0m", detail.c_str());
        std::printf ("\n");
    }
    else
    {
        ++gFail;
        std::printf ("  \033[31mFAIL\033[0m  %s", what.c_str());
        if (! detail.empty()) std::printf ("   \033[31m%s\033[0m", detail.c_str());
        std::printf ("\n");
    }
}

inline std::string fmt (const char* f, double a)
{
    char b[128]; std::snprintf (b, sizeof b, f, a); return b;
}
inline std::string fmt2 (const char* f, double a, double b_)
{
    char b[192]; std::snprintf (b, sizeof b, f, a, b_); return b;
}
inline std::string fmtI (const char* f, int a)
{
    char b[128]; std::snprintf (b, sizeof b, f, a); return b;
}

inline void near (double actual, double expected, double tol, const std::string& what)
{
    ok (std::abs (actual - expected) <= tol, what,
        fmt2 ("got %.4f, want %.4f", actual, expected) + fmt (" +-%.4f", tol));
}

inline int report()
{
    std::printf ("\n\033[1m%d passed, %d failed\033[0m\n\n", gPass, gFail);
    return gFail == 0 ? 0 : 1;
}

// ─── signal helpers ──────────────────────────────────────────────────────────

inline double rms (const float* x, int n)
{
    double s = 0.0;
    for (int i = 0; i < n; ++i) s += (double) x[i] * x[i];
    return std::sqrt (s / (double) n);
}

inline double peak (const float* x, int n)
{
    double p = 0.0;
    for (int i = 0; i < n; ++i) p = std::max (p, (double) std::abs (x[i]));
    return p;
}

// Steady-state magnitude at `freq` by quadrature detection, discarding the
// first half of the run so filter and delay transients are gone.
template <typename RunBlock>
inline double magnitudeAt (RunBlock&& runBlock, double freq, double sr,
                           int blockSize, int blocks, int channel = 0)
{
    juce::AudioBuffer<float> b (2, blockSize);
    double re = 0.0, im = 0.0;
    const long long total = (long long) blockSize * blocks;
    const long long skip  = total / 2;
    long long tt = 0;

    for (int k = 0; k < blocks; ++k)
    {
        for (int n = 0; n < blockSize; ++n)
        {
            const float s = (float) std::sin (2.0 * juce::MathConstants<double>::pi * freq * (double) (tt + n) / sr);
            b.setSample (0, n, s);
            b.setSample (1, n, s);
        }
        runBlock (b, blockSize);
        for (int n = 0; n < blockSize; ++n)
        {
            const long long g = tt + n;
            if (g < skip) continue;
            const double ph = 2.0 * juce::MathConstants<double>::pi * freq * (double) g / sr;
            re += b.getSample (channel, n) * std::cos (ph);
            im += b.getSample (channel, n) * std::sin (ph);
        }
        tt += blockSize;
    }

    const double n = (double) (total - skip);
    return 2.0 * std::sqrt (re * re + im * im) / n;
}

template <typename RunBlock>
inline double magnitudeDb (RunBlock&& runBlock, double freq, double sr,
                           int blockSize, int blocks, int channel = 0)
{
    return 20.0 * std::log10 (magnitudeAt (runBlock, freq, sr, blockSize, blocks, channel) + 1e-12);
}

inline double stdev (const std::vector<float>& v)
{
    if (v.empty()) return 0.0;
    double m = 0.0;
    for (float x : v) m += x;
    m /= (double) v.size();
    double s = 0.0;
    for (float x : v) s += (x - m) * (x - m);
    return std::sqrt (s / (double) v.size());
}

} // namespace t
