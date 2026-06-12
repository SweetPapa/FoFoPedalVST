#pragma once
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <cmath>

namespace spt
{

// FableVerb — a Dattorro-style "figure-of-eight" plate reverb core with a
// modulated tank, shared by all Sweet Papa plugins. This replaces
// juce::Reverb (freeverb), whose short parallel combs are what made the old
// tails sound small and metallic.
//
// Topology (Dattorro, "Effect Design Part 1", JAES 1997):
//
//   in → bandwidth LPF → 4 series input-diffusion allpasses ─┐
//        ┌────────────────────────────────────────────────────┤
//        ▼                                                    ▼
//   [modulated AP 672] → [delay 4453] → [damp LPF] → ×decay → [AP 1800] → [delay 3720] ─┐
//        ▲                                                                              │×decay
//        └×decay ─ [delay 3163] ← [AP 2656] ← ×decay ← [damp LPF] ← [delay 4217] ← [modulated AP 908] ◄┘
//
// The two tank allpasses are delay-modulated by slow, slightly-detuned sine
// LFOs — this smears the tank's modes over time, which is what kills the
// static metallic ring of an unmodulated structure. Output is tapped at
// seven points per channel (Dattorro's tap table) for high echo density.
//
// All magic numbers below are in samples at Dattorro's 29761 Hz reference
// rate; they're rescaled at prepare() for the actual sample rate and again
// per-voicing by sizeScale.
class FableVerb
{
public:
    enum class Voicing { Plate = 0, Hall, Room };

    void prepare (double sampleRate, juce::uint32 /*maxBlockSize*/)
    {
        sr = sampleRate;
        srScale = (float) (sampleRate / 29761.0);

        // Allocate every line at its largest possible size (max sizeScale +
        // max LFO excursion) so size changes never reallocate.
        auto alloc = [this] (DelayBuf& d, float baseLen, float headroom = 1.0f)
        {
            const int n = (int) std::ceil (baseLen * srScale * kMaxSizeScale + headroom + 4.0f);
            d.buf.assign ((size_t) juce::nextPowerOfTwo (n), 0.0f);
            d.mask = (int) d.buf.size() - 1;
            d.w = 0;
        };

        const float excHeadroom = kMaxExcursion * srScale;

        alloc (inAP[0], 142.0f);  alloc (inAP[1], 107.0f);
        alloc (inAP[2], 379.0f);  alloc (inAP[3], 277.0f);

        alloc (modAP[0], 672.0f, excHeadroom);
        alloc (modAP[1], 908.0f, excHeadroom);
        alloc (tankDelay[0], 4453.0f);
        alloc (tankDelay[1], 4217.0f);
        alloc (tankAP[0], 1800.0f);
        alloc (tankAP[1], 2656.0f);
        alloc (tankDelay[2], 3720.0f);
        alloc (tankDelay[3], 3163.0f);

        lfoPhase[0] = 0.0f;
        lfoPhase[1] = 0.25f; // quadrature start

        reset();
        updateDerived();
    }

    void reset()
    {
        for (auto* d : { &inAP[0], &inAP[1], &inAP[2], &inAP[3],
                         &modAP[0], &modAP[1],
                         &tankAP[0], &tankAP[1],
                         &tankDelay[0], &tankDelay[1], &tankDelay[2], &tankDelay[3] })
        {
            std::fill (d->buf.begin(), d->buf.end(), 0.0f);
            d->w = 0;
        }
        bandwidthState = 0.0f;
        damp[0] = damp[1] = 0.0f;
        dcState[0] = dcState[1] = 0.0f;
        fb[0] = fb[1] = 0.0f;
    }

    void setVoicing (Voicing v) noexcept { if (v != voicing) { voicing = v; dirty = true; } }

    // size01 scales the tank delay lengths (bigger room) — decay is set
    // separately so callers can macro them together or not.
    void setSize01  (float v) noexcept { v = juce::jlimit (0.0f, 1.0f, v); if (std::abs (v - size01)  > 1.0e-4f) { size01  = v; dirty = true; } }
    void setDecay01 (float v) noexcept { v = juce::jlimit (0.0f, 1.0f, v); if (std::abs (v - decay01) > 1.0e-4f) { decay01 = v; dirty = true; } }
    void setDamp01  (float v) noexcept { v = juce::jlimit (0.0f, 1.0f, v); if (std::abs (v - damp01)  > 1.0e-4f) { damp01  = v; dirty = true; } }
    void setWidth01 (float v) noexcept { width01 = juce::jlimit (0.0f, 1.0f, v); }

    // Fully wet stereo render. in/out may alias.
    void processBlock (const float* inL, const float* inR,
                       float* outL, float* outR, int numSamples) noexcept
    {
        if (dirty) updateDerived();

        for (int n = 0; n < numSamples; ++n)
        {
            // ── input: mono sum → bandwidth LPF → 4 diffusers ──────────────
            float x = 0.5f * (inL[n] + inR[n]);
            bandwidthState += bandwidthCoef * (x - bandwidthState);
            x = bandwidthState;

            x = allpass (inAP[0], x, inApLen[0], inApG[0]);
            x = allpass (inAP[1], x, inApLen[1], inApG[1]);
            x = allpass (inAP[2], x, inApLen[2], inApG[2]);
            x = allpass (inAP[3], x, inApLen[3], inApG[3]);

            // ── tank LFOs (slightly detuned pair → modes never align) ──────
            const float exc0 = excursion * std::sin (juce::MathConstants<float>::twoPi * lfoPhase[0]);
            const float exc1 = excursion * std::sin (juce::MathConstants<float>::twoPi * lfoPhase[1]);
            lfoPhase[0] += lfoInc[0]; if (lfoPhase[0] >= 1.0f) lfoPhase[0] -= 1.0f;
            lfoPhase[1] += lfoInc[1]; if (lfoPhase[1] >= 1.0f) lfoPhase[1] -= 1.0f;

            // ── tank, two cross-coupled branches ───────────────────────────
            float a = x + decayGain * fb[1];
            float b = x + decayGain * fb[0];

            a = allpassMod (modAP[0], a, modApLen[0] + exc0, kDecayDiff1);
            a = delayWrite (tankDelay[0], a, tankLen[0]);
            damp[0] += dampCoef * (a - damp[0]);
            a = damp[0];
            // gentle DC/low-rumble control inside the loop so long decays
            // can't accumulate low-end sludge
            dcState[0] += dcCoef * (a - dcState[0]);
            a -= dcState[0];
            a *= decayGain;
            a = allpass (tankAP[0], a, tankApLen[0], kDecayDiff2);
            a = delayWrite (tankDelay[2], a, tankLen[2]);
            fb[0] = a;

            b = allpassMod (modAP[1], b, modApLen[1] + exc1, kDecayDiff1);
            b = delayWrite (tankDelay[1], b, tankLen[1]);
            damp[1] += dampCoef * (b - damp[1]);
            b = damp[1];
            dcState[1] += dcCoef * (b - dcState[1]);
            b -= dcState[1];
            b *= decayGain;
            b = allpass (tankAP[1], b, tankApLen[1], kDecayDiff2);
            b = delayWrite (tankDelay[3], b, tankLen[3]);
            fb[1] = b;

            // ── output taps (Dattorro's table, scaled) ─────────────────────
            float yl =  tap (tankDelay[1], tapL[0])
                      + tap (tankDelay[1], tapL[1])
                      - tap (tankAP[1],    tapL[2])
                      + tap (tankDelay[3], tapL[3])
                      - tap (tankDelay[0], tapL[4])
                      - tap (tankAP[0],    tapL[5])
                      - tap (tankDelay[2], tapL[6]);

            float yr =  tap (tankDelay[0], tapR[0])
                      + tap (tankDelay[0], tapR[1])
                      - tap (tankAP[0],    tapR[2])
                      + tap (tankDelay[2], tapR[3])
                      - tap (tankDelay[1], tapR[4])
                      - tap (tankAP[1],    tapR[5])
                      - tap (tankDelay[3], tapR[6]);

            yl *= 0.6f;
            yr *= 0.6f;

            // ── width (mid/side) ───────────────────────────────────────────
            const float mid  = 0.5f * (yl + yr);
            const float side = 0.5f * (yl - yr) * width01;
            outL[n] = mid + side;
            outR[n] = mid - side;
        }
    }

private:
    struct DelayBuf
    {
        std::vector<float> buf;
        int mask { 0 };
        int w { 0 };
    };

    // Value written `delay` samples ago (call before writing this sample's slot;
    // delay >= 1).
    static inline float tapRead (const DelayBuf& d, int delay) noexcept
    {
        return d.buf[(size_t) ((d.w - delay) & d.mask)];
    }

    inline float tap (const DelayBuf& d, int delay) const noexcept { return tapRead (d, delay); }

    // Plain delay line: write x, return the sample written `len` ago.
    static inline float delayWrite (DelayBuf& d, float x, int len) noexcept
    {
        const float out = tapRead (d, len);
        d.buf[(size_t) d.w] = x;
        d.w = (d.w + 1) & d.mask;
        return out;
    }

    // One-multiply allpass, integer delay:  w = x + g·wD ; y = wD − g·w
    static inline float allpass (DelayBuf& d, float x, int len, float g) noexcept
    {
        const float wD = tapRead (d, len);
        const float w  = x + g * wD;
        d.buf[(size_t) d.w] = w;
        d.w = (d.w + 1) & d.mask;
        return wD - g * w;
    }

    // Allpass with fractional (modulated) delay — linear interp read.
    static inline float allpassMod (DelayBuf& d, float x, float len, float g) noexcept
    {
        const int   i0 = (int) len;
        const float fr = len - (float) i0;
        const float wD = tapRead (d, i0) * (1.0f - fr) + tapRead (d, i0 + 1) * fr;
        const float w  = x + g * wD;
        d.buf[(size_t) d.w] = w;
        d.w = (d.w + 1) & d.mask;
        return wD - g * w;
    }

    void updateDerived()
    {
        // ── per-voicing character ──────────────────────────────────────────
        float sizeLo, sizeHi;       // sizeScale range mapped from size01
        float diffScale;            // input diffusion strength
        float bwHz;                 // input bandwidth LPF
        float dampLoHz, dampHiHz;   // damping LPF range (damp01: lo→hi cut)
        float decayLo, decayHi;     // tank decay gain range
        float modRateHz, modDepth;  // tank modulation

        switch (voicing)
        {
            case Voicing::Hall:
                sizeLo = 0.90f; sizeHi = 1.55f;
                diffScale = 0.85f;          // slower build than plate
                bwHz = 7500.0f;
                dampLoHz = 9000.0f; dampHiHz = 2200.0f;
                decayLo = 0.55f; decayHi = 0.97f;
                modRateHz = 0.65f; modDepth = 11.0f;
                break;
            case Voicing::Room:
                sizeLo = 0.32f; sizeHi = 0.62f;
                diffScale = 1.0f;
                bwHz = 9500.0f;
                dampLoHz = 8000.0f; dampHiHz = 2800.0f;
                decayLo = 0.25f; decayHi = 0.72f;
                modRateHz = 0.9f; modDepth = 6.0f;
                break;
            case Voicing::Plate:
            default:
                sizeLo = 0.55f; sizeHi = 1.0f;
                diffScale = 1.0f;
                bwHz = 10500.0f;
                dampLoHz = 10000.0f; dampHiHz = 3000.0f;
                decayLo = 0.45f; decayHi = 0.94f;
                modRateHz = 0.80f; modDepth = 9.0f;
                break;
        }

        const float sizeScale = juce::jmap (size01, sizeLo, sizeHi);
        const float s = sizeScale * srScale;

        auto L = [s] (float base) { return juce::jmax (2, (int) std::round (base * s)); };

        // input diffusers scale only with sample rate (they shape the attack,
        // not the room size)
        inApLen[0] = juce::jmax (2, (int) std::round (142.0f * srScale));
        inApLen[1] = juce::jmax (2, (int) std::round (107.0f * srScale));
        inApLen[2] = juce::jmax (2, (int) std::round (379.0f * srScale));
        inApLen[3] = juce::jmax (2, (int) std::round (277.0f * srScale));
        inApG[0] = inApG[1] = 0.750f * diffScale;
        inApG[2] = inApG[3] = 0.625f * diffScale;

        modApLen[0]   = 672.0f  * s;
        modApLen[1]   = 908.0f  * s;
        tankApLen[0]  = L (1800.0f);
        tankApLen[1]  = L (2656.0f);
        tankLen[0]    = L (4453.0f);
        tankLen[1]    = L (4217.0f);
        tankLen[2]    = L (3720.0f);
        tankLen[3]    = L (3163.0f);

        // clamp all lengths to their buffers
        clampLen (modAP[0], modApLen[0], kMaxExcursion * srScale + 2.0f);
        clampLen (modAP[1], modApLen[1], kMaxExcursion * srScale + 2.0f);

        // output taps — same scale factor as the tank
        const int tl[7] = { L (266.0f), L (2974.0f), L (1913.0f), L (1996.0f), L (1990.0f), L (187.0f),  L (1066.0f) };
        const int tr[7] = { L (353.0f), L (3627.0f), L (1228.0f), L (2673.0f), L (2111.0f), L (335.0f),  L (121.0f)  };
        for (int i = 0; i < 7; ++i) { tapL[i] = tl[i]; tapR[i] = tr[i]; }

        decayGain = juce::jmap (decay01, decayLo, decayHi);

        const float dampHz = juce::jmap (damp01, dampLoHz, dampHiHz);
        dampCoef = onePoleCoef (dampHz);
        bandwidthCoef = onePoleCoef (bwHz);
        dcCoef = onePoleCoef (35.0f); // subtracted → acts as ~35 Hz HPF

        excursion = modDepth * srScale;
        lfoInc[0] = modRateHz / (float) sr;
        lfoInc[1] = modRateHz * 0.92f / (float) sr; // detuned sibling

        dirty = false;
    }

    static void clampLen (const DelayBuf& d, float& len, float headroom)
    {
        len = juce::jmin (len, (float) d.mask - headroom);
    }

    float onePoleCoef (float hz) const noexcept
    {
        return 1.0f - std::exp (-juce::MathConstants<float>::twoPi * hz / (float) sr);
    }

    static constexpr float kDecayDiff1   = 0.70f;
    static constexpr float kDecayDiff2   = 0.50f;
    static constexpr float kMaxSizeScale = 1.6f;
    static constexpr float kMaxExcursion = 16.0f; // samples @ 29.8k

    double sr { 44100.0 };
    float  srScale { 1.0f };

    Voicing voicing { Voicing::Plate };
    float size01 { 0.5f }, decay01 { 0.5f }, damp01 { 0.5f }, width01 { 1.0f };
    bool  dirty { true };

    DelayBuf inAP[4];
    DelayBuf modAP[2];
    DelayBuf tankAP[2];
    DelayBuf tankDelay[4];

    int   inApLen[4] {};
    float inApG[4] {};
    float modApLen[2] {};
    int   tankApLen[2] {};
    int   tankLen[4] {};
    int   tapL[7] {}, tapR[7] {};

    float bandwidthCoef { 1.0f }, dampCoef { 1.0f }, dcCoef { 0.01f };
    float decayGain { 0.7f };
    float excursion { 8.0f };
    float lfoInc[2] {};
    float lfoPhase[2] {};

    float bandwidthState { 0.0f };
    float damp[2] {};
    float dcState[2] {};
    float fb[2] {};
};

} // namespace spt
