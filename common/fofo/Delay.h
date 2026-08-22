#pragma once
#include "Spec.h"
#include <vector>
#include <cmath>

namespace fofo
{

// ─── Fractional delay line, cubic Hermite read ───────────────────────────────
//
// The old toolkit read its modulated taps with linear interpolation. Linear
// interpolation between two samples is a lowpass whose cutoff depends on the
// fractional position, so a read pointer that moves — which is every read
// pointer worth having: chorus, doubler, tape, pitch shifter — imposes
// high-frequency loss that is itself amplitude-modulated. That is audible as a
// veiled, faintly grainy top end, and it was on every pitched sound the
// catalogue shipped.
//
// Cubic Hermite (Catmull-Rom) uses four points and is dramatically flatter: at
// a half-sample offset it loses ~0.07 dB at 6 kHz / 48 kHz where linear loses
// ~0.69 dB, and the gap widens sharply toward Nyquist.
//
// Delay convention: read(0) returns the sample just pushed. The four-point
// kernel needs one sample of context on each side of the read position, so the
// minimum usable delay is 2 — for anything smaller, use AlignDelay, which is
// built for exactly that case.
//
// Single channel — hold one per channel. Power-of-two sized so the wrap is a
// mask rather than a modulo.
class DelayLine
{
public:
    static constexpr float kMinDelay = 2.0f;

    void prepare (double sampleRate, float maxDelaySamples)
    {
        sr = sampleRate;
        // +4 for the Hermite kernel's reach, +2 for safety at max delay.
        const int want = (int) std::ceil (juce::jmax (kMinDelay, maxDelaySamples)) + 6;
        int n = 1;
        while (n < want) n <<= 1;
        buf.assign ((size_t) n, 0.0f);
        mask = n - 1;
        maxDelay = (float) (n - 5);
        w = 0;
    }

    void reset()
    {
        std::fill (buf.begin(), buf.end(), 0.0f);
        w = 0;
    }

    float maxDelaySamples() const noexcept { return maxDelay; }

    inline void push (float x) noexcept
    {
        buf[(size_t) w] = x;
        w = (w + 1) & mask;
    }

    // Sample from `delay` pushes ago. delay is clamped to [2, maxDelay].
    inline float read (float delay) const noexcept
    {
        delay = juce::jlimit (kMinDelay, maxDelay, delay);

        // The newest sample lives at w-1, so that is the zero-delay position.
        const float readPos = (float) (w - 1) - delay;
        const int   i       = (int) std::floor (readPos);
        const float t       = readPos - (float) i;

        const float xm1 = buf[(size_t) ((i - 1) & mask)];
        const float x0  = buf[(size_t) ( i      & mask)];
        const float x1  = buf[(size_t) ((i + 1) & mask)];
        const float x2  = buf[(size_t) ((i + 2) & mask)];

        // Catmull-Rom coefficients.
        const float c0 = x0;
        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);

        return ((c3 * t + c2) * t + c1) * t + c0;
    }

    inline float processSample (float x, float delay) noexcept
    {
        push (x);
        return read (delay);
    }

private:
    std::vector<float> buf;
    double sr { 44100.0 };
    int   mask { 0 };
    int   w { 0 };
    float maxDelay { 0.0f };
};

// ─── Fixed fractional delay for latency alignment ────────────────────────────
//
// This is what Parallel uses to hold the dry path back so it lines up with a
// wet branch that has been through an oversampler. It is a separate class from
// DelayLine for two reasons, both of which matter:
//
//   • Oversampler latency is small — often under two samples — and fractional.
//     DelayLine's four-point kernel cannot reach below a delay of 2.
//   • Alignment must not change the dry signal's magnitude at all. An
//     interpolating read imposes a small high-frequency loss; a first-order
//     Thiran allpass gives the fractional delay with *exactly* flat magnitude,
//     which is the whole job here. Its slow settling doesn't matter because
//     the delay is set once and never modulated.
//
// A delay of zero is a bit-exact passthrough — an identity path has to
// actually be an identity, or the "is the empty graph transparent?" test is
// meaningless.
class AlignDelay
{
public:
    void prepare (const Spec& spec, float maxSamples = 64.0f)
    {
        const int want = (int) std::ceil (juce::jmax (4.0f, maxSamples)) + 4;
        int n = 1;
        while (n < want) n <<= 1;

        chans.assign ((size_t) juce::jmax (1, spec.numChannels), {});
        for (auto& c : chans)
        {
            c.buf.assign ((size_t) n, 0.0f);
            c.mask = n - 1;
            c.w = 0;
            c.y1 = 0.0f;
            c.x1 = 0.0f;
        }
        maxDelay = (float) (n - 3);
        setDelay (delaySamples);
    }

    void reset()
    {
        for (auto& c : chans)
        {
            std::fill (c.buf.begin(), c.buf.end(), 0.0f);
            c.w = 0; c.y1 = 0.0f; c.x1 = 0.0f;
        }
    }

    void setDelay (float samples) noexcept
    {
        delaySamples = juce::jlimit (0.0f, maxDelay, samples);

        if (delaySamples <= 0.0f) { intDelay = 0; thiranA = 0.0f; frac = 0.0f; return; }

        intDelay = (int) std::floor (delaySamples);
        frac     = delaySamples - (float) intDelay;

        // Thiran is best-behaved for a fractional part around 1, so borrow a
        // whole sample from the integer part when the fraction is small.
        if (frac < 0.5f && intDelay >= 1) { intDelay -= 1; frac += 1.0f; }

        thiranA = (1.0f - frac) / (1.0f + frac);
    }

    float getDelay() const noexcept { return delaySamples; }

    void process (juce::AudioBuffer<float>& buffer, int numSamples) noexcept
    {
        if (delaySamples <= 0.0f) return;   // bit-exact identity

        const int nCh = juce::jmin (buffer.getNumChannels(), (int) chans.size());
        for (int ch = 0; ch < nCh; ++ch)
        {
            auto& c = chans[(size_t) ch];
            auto* d = buffer.getWritePointer (ch);

            for (int n = 0; n < numSamples; ++n)
            {
                // integer part
                c.buf[(size_t) c.w] = d[n];
                c.w = (c.w + 1) & c.mask;
                const float xi = c.buf[(size_t) ((c.w - 1 - intDelay) & c.mask)];

                // fractional part — first-order Thiran allpass, magnitude-flat
                const float y = thiranA * (xi - c.y1) + c.x1;
                c.x1 = xi;
                c.y1 = y;
                d[n] = y;
            }
        }
    }

private:
    struct Ch
    {
        std::vector<float> buf;
        int   mask { 0 }, w { 0 };
        float y1 { 0.0f }, x1 { 0.0f };
    };

    std::vector<Ch> chans;
    float delaySamples { 0.0f };
    float maxDelay { 0.0f };
    int   intDelay { 0 };
    float frac { 0.0f };
    float thiranA { 0.0f };
};

} // namespace fofo
