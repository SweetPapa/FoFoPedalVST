#pragma once
#include "Spec.h"
#include "Delay.h"
#include <cmath>

namespace fofo
{

// ─────────────────────────────────────────────────────────────────────────────
// Delay-line pitch shifting (the "rotating head" / dual-tap construction).
//
// Two taps sweep a delay range at a rate offset from the write rate, and a
// Hann crossfade hands over between them. A tap whose delay changes by
// (1 − ratio) per sample has a read pointer advancing at `ratio`, which is the
// shift.
//
// Two things about this are easy to get wrong, and the previous shifter got
// both of them:
//
// 1. INTERPOLATION. It read with linear interpolation. Linear interpolation
//    between two samples is a lowpass whose cutoff depends on the fractional
//    position, so a continuously moving read pointer — which is the entire
//    mechanism here — imposes high-frequency loss that is itself amplitude-
//    modulated. Audible as veiled and faintly grainy, and DOUBLE summed four
//    of them. This reads with cubic Hermite via DelayLine.
//
// 2. THE SWEEP PERIOD IS NOT FREE. My own first attempt fixed the grain length
//    at 32 ms and swept the delay across it. That cannot produce a small
//    shift, and the arithmetic says why: a tap that resets every L samples
//    advances its read pointer by ratio·L and then jumps back by (ratio−1)·L,
//    so its *average* rate is exactly 1 no matter what ratio is. The shift
//    only exists inside a sweep, as a read excursion of (ratio−1)·L samples —
//    which at 14 cents with L = 1536 is 12.5 samples, or 0.11 of a cycle at
//    440 Hz. Measured, that version left a 14-cent setting sitting at 0.0
//    cents while a 1200-cent setting (excursion 14 cycles) worked fine.
//
//    The sweep period is therefore L / |ratio − 1|, not a free parameter. What
//    IS free is the delay RANGE, and it trades off the two audible artefacts:
//    a wide range separates the two taps further (more comb and smear while
//    they overlap) but crosses over far less often. At 14 cents a 20 ms range
//    hands over about once every 2.5 seconds.
//
// For the micro-detune this catalogue uses, this is the right tool and a phase
// vocoder would be overkill. For large shifts a phase vocoder (Signalsmith
// Stretch, MIT) is the next step up and drops in behind this interface.
// ─────────────────────────────────────────────────────────────────────────────
class PitchShifter
{
public:
    // rangeMs: how far the taps sweep. 15-25 ms suits dry detune and doubling;
    // 60-100 ms suits a shimmer feeding a reverb, where smear hides in the
    // tail. See the note above on what this trades off.
    void prepare (double sampleRate, float rangeMs)
    {
        sr = sampleRate;
        rangeSamp = juce::jmax (64.0f, rangeMs * 0.001f * (float) sr);
        line.prepare (sr, kMinDelay + rangeSamp + 64.0f);
        reset();
    }

    void reset()
    {
        line.reset();
        phase[0] = 0.0f;
        phase[1] = 0.5f;
    }

    // 2.0 = +1 octave, 0.5 = −1 octave, 2^(cents/1200) for detune.
    void setRatio (float r) noexcept { ratio = juce::jlimit (0.25f, 4.0f, r); }

    float getRatio() const noexcept { return ratio; }

    // How often the two taps hand over, in seconds. Useful for choosing a
    // range: too short and the crossfade itself becomes the sound.
    double crossfadePeriodSeconds() const noexcept
    {
        const float d = std::abs (ratio - 1.0f);
        return d < 1.0e-6f ? 0.0 : (double) rangeSamp / (double) d / sr;
    }

    inline float process (float x) noexcept
    {
        line.push (x);

        // Delay must change by (1 − ratio) per sample for the read pointer to
        // advance at `ratio`, so the phase rate follows from the range.
        const float rate = (1.0f - ratio) / rangeSamp;

        float out = 0.0f;
        for (int g = 0; g < 2; ++g)
        {
            const float p = phase[g];

            // Hann. Two of these at 50% offset sum to exactly one, so the
            // crossfade adds no amplitude ripple, and each tap is silent at
            // the moment it wraps — which is what hides the discontinuity.
            const float w = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * p);

            out += line.read (kMinDelay + p * rangeSamp) * w;

            float np = p + rate;
            while (np >= 1.0f) np -= 1.0f;
            while (np <  0.0f) np += 1.0f;
            phase[g] = np;
        }

        return out;
    }

private:
    static constexpr float kMinDelay = 4.0f;

    double sr { 44100.0 };
    float  rangeSamp { 1024.0f };
    float  ratio { 1.0f };

    DelayLine line;
    float phase[2] { 0.0f, 0.5f };
};

// Cents → ratio.
inline float centsToRatio (float cents) noexcept
{
    return std::pow (2.0f, cents / 1200.0f);
}

} // namespace fofo
