#pragma once
#include <cmath>
#include <vector>
#include <array>

namespace spt
{

// Dual-grain delay-line pitch shifter (one channel). Two Hann-windowed read
// taps scheduled 50% out of phase — as one fades out the other fades in, and
// the two windows sum to ~unity.
//
// The window size is the core tradeoff: small = warble (audible splice AM),
// large = smear. Defaults:
//   • dry detune / harmony use: 30–40 ms
//   • shimmer feeding a reverb tank: 60–100 ms (smear hides in the tail,
//     warble doesn't)
// A small per-restart randomization of the grain start (±10%) breaks up the
// splice-rate periodicity that otherwise reads as comb/AM texture.
class GrainShifter
{
public:
    void prepare (double sampleRate, float windowMs, uint32_t seed = 0x1234567u)
    {
        grainSize = std::max (256, (int) std::lround (windowMs * 0.001 * sampleRate));
        int want = grainSize * 4;
        bufSize = 1; while (bufSize < want) bufSize <<= 1;
        buf.assign ((size_t) bufSize, 0.0f);
        w = 0;
        rng = seed | 1u;
        g[0] = { 0.0f, 0.0f };
        g[1] = { 0.0f, 0.5f };
        phaseInc = 1.0f / (float) grainSize;
    }

    void reset()
    {
        std::fill (buf.begin(), buf.end(), 0.0f);
        w = 0;
        g[0] = { 0.0f, 0.0f };
        g[1] = { 0.0f, 0.5f };
    }

    // ratio: 2.0 = +1 octave, 0.5 = −1 octave, 2^(cents/1200) for detune.
    inline float process (float x, float ratio) noexcept
    {
        buf[(size_t) w] = x;

        float out = 0.0f;
        for (auto& gr : g)
        {
            // Hann window over grain lifetime.
            const float win = 0.5f - 0.5f * std::cos (6.28318530718f * gr.phase);

            int   i0 = (int) gr.readPos;
            const float fr = gr.readPos - (float) i0;
            i0 &= (bufSize - 1);
            const int i1 = (i0 + 1) & (bufSize - 1);
            out += (buf[(size_t) i0] * (1.0f - fr) + buf[(size_t) i1] * fr) * win;

            gr.readPos += ratio;
            if (gr.readPos >= (float) bufSize) gr.readPos -= (float) bufSize;
            if (gr.readPos <  0.0f)            gr.readPos += (float) bufSize;

            gr.phase += phaseInc;
            if (gr.phase >= 1.0f)
            {
                gr.phase -= 1.0f;
                // Restart just behind the write head, with ±10% jitter so the
                // splice rate never settles into a fixed comb.
                rng = rng * 1664525u + 1013904223u;
                const float jit = ((float) ((rng >> 9) & 0xFFFF) / 65535.0f - 0.5f) * 0.2f;
                int restart = w - grainSize - (int) ((float) grainSize * jit);
                restart &= (bufSize - 1);
                gr.readPos = (float) restart;
            }
        }

        w = (w + 1) & (bufSize - 1);
        return out;
    }

private:
    struct Grain { float readPos; float phase; };

    std::vector<float> buf;
    std::array<Grain, 2> g {};
    int bufSize { 0 };
    int grainSize { 0 };
    int w { 0 };
    float phaseInc { 0.0f };
    uint32_t rng { 1 };
};

} // namespace spt
