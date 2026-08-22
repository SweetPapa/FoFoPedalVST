#pragma once
#include "Spec.h"
#include <cmath>

namespace fofo
{

// ─── TPT / zero-delay-feedback state variable filter ─────────────────────────
//
// Andy Simper's topology-preserving SVF (Cytomic, "Solving the continuous SVF
// equations using trapezoidal integration"). One structure produces every
// response — lowpass, bandpass, highpass, notch, peak, allpass, bell, and both
// shelves — by mixing the same three internal signals with different weights:
//
//     out = m0·v0 + m1·v1 + m2·v2      (v0 = input, v1 = BP, v2 = LP)
//
// Two properties are why the whole kernel is built on this rather than on the
// one-pole filters the old toolkit shipped:
//
//   • It has real, controllable resonance. Nothing in the previous catalogue
//     had a resonant filter that could move, and resonance under modulation is
//     most of what a listener perceives as "character".
//   • It stays stable when the cutoff is modulated at audio rate. A biquad
//     with recomputed coefficients blows up or zipper-noises when you sweep it
//     quickly; this doesn't, because the state variables are integrator states
//     rather than a delay line of past outputs.
//
// setCutoff() is cheap enough to call per sample (one tan()). For hot paths,
// setG() takes a precomputed tan value.
class Svf
{
public:
    enum class Type
    {
        Lowpass = 0, Highpass, Bandpass, BandpassUnity, Notch, Peak, Allpass,
        Bell, LowShelf, HighShelf
    };

    void prepare (double sampleRate) noexcept
    {
        sr = sampleRate;
        reset();
        update();
    }

    void reset() noexcept { ic1eq = 0.0f; ic2eq = 0.0f; }

    void setType (Type t) noexcept        { if (t != type) { type = t; dirty = true; } }
    void setCutoff (float hz) noexcept    { hz = clampCutoff (hz); if (! juce::approximatelyEqual (hz, cutoffHz)) { cutoffHz = hz; dirty = true; } }
    void setResonance (float q) noexcept  { q = juce::jmax (0.025f, q); if (! juce::approximatelyEqual (q, Q)) { Q = q; dirty = true; } }
    void setGainDb (float db) noexcept    { if (! juce::approximatelyEqual (db, gainDb)) { gainDb = db; dirty = true; } }

    // Set all four at once — one dirty check instead of four.
    void set (Type t, float hz, float q, float db = 0.0f) noexcept
    {
        hz = clampCutoff (hz);
        q  = juce::jmax (0.025f, q);
        if (t == type && juce::approximatelyEqual (hz, cutoffHz)
            && juce::approximatelyEqual (q, Q) && juce::approximatelyEqual (db, gainDb)) return;
        type = t; cutoffHz = hz; Q = q; gainDb = db; dirty = true;
    }

    float getCutoff() const noexcept { return cutoffHz; }

    inline float process (float v0) noexcept
    {
        if (dirty) update();

        const float v3 = v0 - ic2eq;
        const float v1 = a1 * ic1eq + a2 * v3;
        const float v2 = ic2eq + a2 * ic1eq + a3 * v3;
        ic1eq = 2.0f * v1 - ic1eq;
        ic2eq = 2.0f * v2 - ic2eq;

        return m0 * v0 + m1 * v1 + m2 * v2;
    }

    // Snap the coefficients without a dirty check — for per-sample modulation
    // where the caller already knows the cutoff changed.
    inline float processModulated (float v0, float hz) noexcept
    {
        cutoffHz = clampCutoff (hz);
        update();
        return process (v0);
    }

private:
    float clampCutoff (float hz) const noexcept
    {
        // Keep g finite: tan() blows up as fc approaches Nyquist.
        const float nyq = (float) (sr * 0.5);
        return juce::jlimit (5.0f, nyq * 0.99f, hz);
    }

    void update() noexcept
    {
        dirty = false;
        if (sr <= 0.0) return;

        const float A = std::pow (10.0f, gainDb * 0.025f); // 10^(dB/40)
        float g = std::tan (juce::MathConstants<float>::pi * cutoffHz / (float) sr);
        float k = 1.0f / Q;

        switch (type)
        {
            case Type::Lowpass:   m0 = 0.0f; m1 = 0.0f;      m2 = 1.0f;  break;
            // The classic SVF bandpass tap peaks at Q, which is what you want
            // when resonance is the point. BandpassUnity normalises it to 0 dB
            // at centre, which is what you want in an EQ.
            case Type::Bandpass:      m0 = 0.0f; m1 = 1.0f; m2 = 0.0f; break;
            case Type::BandpassUnity: m0 = 0.0f; m1 = k;    m2 = 0.0f; break;
            case Type::Highpass:  m0 = 1.0f; m1 = -k;        m2 = -1.0f; break;
            case Type::Notch:     m0 = 1.0f; m1 = -k;        m2 = 0.0f;  break;
            case Type::Peak:      m0 = 1.0f; m1 = -k;        m2 = -2.0f; break;
            case Type::Allpass:   m0 = 1.0f; m1 = -2.0f * k; m2 = 0.0f;  break;

            case Type::Bell:
                k  = 1.0f / (Q * A);
                m0 = 1.0f; m1 = k * (A * A - 1.0f); m2 = 0.0f;
                break;

            case Type::LowShelf:
                g  = g / std::sqrt (A);
                m0 = 1.0f; m1 = k * (A - 1.0f); m2 = A * A - 1.0f;
                break;

            case Type::HighShelf:
                g  = g * std::sqrt (A);
                m0 = A * A; m1 = k * (1.0f - A) * A; m2 = 1.0f - A * A;
                break;
        }

        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    double sr { 44100.0 };
    Type  type     { Type::Lowpass };
    float cutoffHz { 1000.0f };
    float Q        { 0.70710678f };
    float gainDb   { 0.0f };
    bool  dirty    { true };

    float a1 { 0.0f }, a2 { 0.0f }, a3 { 0.0f };
    float m0 { 0.0f }, m1 { 0.0f }, m2 { 1.0f };
    float ic1eq { 0.0f }, ic2eq { 0.0f };
};

// ─── One-pole lowpass ────────────────────────────────────────────────────────
// Still the right tool for damping and envelope smoothing, where a 6 dB/oct
// slope and zero resonance are the point.
struct OnePole
{
    void prepare (double sampleRate) noexcept { sr = sampleRate; setCutoff (cutoffHz); reset(); }
    void reset() noexcept { z = 0.0f; }

    void setCutoff (float hz) noexcept
    {
        cutoffHz = hz;
        if (sr <= 0.0) return;
        a = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * juce::jmax (0.01f, hz) / (float) sr);
    }

    inline float process (float x) noexcept { z += a * (x - z); return z; }
    inline float lastValue() const noexcept { return z; }

    double sr { 44100.0 };
    float cutoffHz { 1000.0f };
    float a { 1.0f }, z { 0.0f };
};

// ─── DC blocker ──────────────────────────────────────────────────────────────
// y[n] = x[n] − x[n−1] + R·y[n−1]. Place after any asymmetric shaper and
// inside every saturating feedback loop.
struct DcBlocker
{
    void prepare (double sampleRate, float hz = 20.0f) noexcept
    {
        R = 1.0f - juce::MathConstants<float>::twoPi * hz / (float) sampleRate;
        reset();
    }
    void reset() noexcept { x1 = 0.0f; y1 = 0.0f; }

    inline float process (float x) noexcept
    {
        const float y = x - x1 + R * y1;
        x1 = x; y1 = y;
        return y;
    }

    float R { 0.997f }, x1 { 0.0f }, y1 { 0.0f };
};

} // namespace fofo
