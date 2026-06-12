#pragma once
#include <cmath>
#include <cstdint>

namespace spt
{

// ── Random-walk drift source ─────────────────────────────────────────────────
// White noise → heavy one-pole lowpass (~0.2–0.5 Hz) → slow wander in ±1.
// The "nothing in the box sits perfectly still" ingredient: modulate delay
// times ±0.1%, pitch ±3 cents, cutoffs ±3% with one of these and a static
// patch starts breathing. Deterministic per-instance seed so two instances
// never walk in lockstep.
struct DriftWalk
{
    uint32_t rng { 0x9E3779B9u };
    float z { 0.0f };
    float a { 0.0001f };

    void prepare (float cutoffHz, double sampleRate, uint32_t seed = 0x9E3779B9u) noexcept
    {
        a = 1.0f - std::exp (-2.0f * 3.14159265f * cutoffHz / (float) sampleRate);
        rng = seed | 1u;
        z = 0.0f;
    }

    inline float next() noexcept
    {
        rng = rng * 1664525u + 1013904223u;
        const float white = (float) ((rng >> 9) & 0x7FFFFF) / (float) 0x3FFFFF - 1.0f; // ±1
        z += a * (white - z);
        // The LP shrinks the variance massively; rescale so output wanders
        // through a useful chunk of ±1. Empirical ~sqrt(a) compensation.
        return z * (0.5f / std::sqrt (a + 1.0e-9f)) * 0.05f;
    }
};

// ── Sine LFO with organic drift ──────────────────────────────────────────────
// Pure-sine modulation is the "machine gun" tell; adding a filtered-noise
// component to both rate and output is most of the Juno/Dimension-D magic.
struct DriftLFO
{
    float phase { 0.0f };
    float baseInc { 0.0f };
    DriftWalk rateDrift, ampDrift;
    float rateDriftAmt { 0.05f };  // ±5% rate wander
    float ampDriftAmt  { 0.25f };  // additive output wander

    void prepare (float rateHz, double sampleRate, uint32_t seed) noexcept
    {
        baseInc = rateHz / (float) sampleRate;
        rateDrift.prepare (0.35f, sampleRate, seed);
        ampDrift .prepare (0.70f, sampleRate, seed ^ 0x5DEECE66u);
    }

    void setRateHz (float rateHz, double sampleRate) noexcept
    {
        baseInc = rateHz / (float) sampleRate;
    }

    void resetPhase (float p) noexcept { phase = p; }

    // returns roughly ±1
    inline float next() noexcept
    {
        const float s = std::sin (6.28318530718f * phase);
        phase += baseInc * (1.0f + rateDriftAmt * rateDrift.next());
        if (phase >= 1.0f) phase -= 1.0f;
        if (phase <  0.0f) phase += 1.0f;
        return s + ampDriftAmt * ampDrift.next();
    }
};

} // namespace spt
