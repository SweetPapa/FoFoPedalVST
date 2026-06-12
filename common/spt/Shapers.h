#pragma once
#include <cmath>
#include <algorithm>

namespace spt
{

// ── numerically stable ln(cosh(x)) ──────────────────────────────────────────
// ln cosh x = |x| + ln(1 + e^(-2|x|)) - ln 2   (exact, no overflow for big x)
inline float logCosh (float x) noexcept
{
    const float ax = std::abs (x);
    return ax + std::log1p (std::exp (-2.0f * ax)) - 0.6931471805599453f;
}

// ── First-order ADAA asymmetric tanh ────────────────────────────────────────
// Shape:           f(x)  = tanh(x + b) - tanh(b)
// Antiderivative:  F(x)  = ln cosh(x + b) - x·tanh(b)
// (Parker/Zavalishin/La Cour-Harbo, DAFx-16.) ADAA at 2x oversampling rivals
// plain 8x for alias suppression at a fraction of the cost. Introduces a
// half-sample delay — fine inside an already-latency-reported OS block.
//
// One instance per channel; bias may be modulated per-sample (cheap, and
// "moving asymmetry" is most of the tube feel).
struct ADAATanh
{
    float x1 { 0.0f };
    float F1 { 0.0f };

    void reset() noexcept { x1 = 0.0f; F1 = 0.0f; }

    inline float process (float x, float bias) noexcept
    {
        const float tb = std::tanh (bias);
        const float F  = logCosh (x + bias) - x * tb;
        const float dx = x - x1;

        float y;
        if (std::abs (dx) > 1.0e-4f)
            y = (F - F1) / dx;
        else
            y = std::tanh (0.5f * (x + x1) + bias) - tb;

        x1 = x;
        F1 = F;
        return y;
    }
};

// ── Static shapers (run inside an oversampled region) ───────────────────────

// Asymmetric tanh, DC-compensated.
inline float tubeShape (float x, float bias) noexcept
{
    return std::tanh (x + bias) - std::tanh (bias);
}

// Triode-ish soft cubic with hard book-ends (Doidic-style), asymmetric gains.
inline float crunchShape (float x, float posGain = 1.2f, float negGain = 0.9f) noexcept
{
    x *= (x >= 0.0f ? posGain : negGain);
    if (x >  1.0f) return  2.0f / 3.0f;
    if (x < -1.0f) return -2.0f / 3.0f;
    return x - (x * x * x) / 3.0f;
}

// Output-bus cubic soft clip: unity gain at 0, ceiling at ±1.
inline float softClipCubic (float x) noexcept
{
    if (x >  1.0f) return  1.0f;
    if (x < -1.0f) return -1.0f;
    return 1.5f * x - 0.5f * x * x * x;
}

// ── "Poor man's hysteresis" tape stage ──────────────────────────────────────
// y[n] = tanh(g·x[n] − k·y[n−1]) ; the one-sample feedback widens the transfer
// into a loop: output depends on history → program-dependent even/odd blend
// and a subtle HF squash. Stable for k < 1. One per channel.
struct TapeHysteresis
{
    float y1 { 0.0f };
    void reset() noexcept { y1 = 0.0f; }

    inline float process (float x, float gain, float k) noexcept
    {
        const float y = std::tanh (gain * x - k * y1);
        y1 = y;
        return y;
    }
};

// ── One-pole DC blocker ──────────────────────────────────────────────────────
// y[n] = x[n] − x[n−1] + R·y[n−1].  R ≈ 0.997 → ~20 Hz at 44.1k. Place after
// asymmetric stages and inside any saturating feedback loop.
struct DCBlocker
{
    float R { 0.997f };
    float x1 { 0.0f }, y1 { 0.0f };

    void setCutoff (float hz, double sampleRate) noexcept
    {
        R = 1.0f - 2.0f * 3.14159265f * hz / (float) sampleRate;
    }
    void reset() noexcept { x1 = 0.0f; y1 = 0.0f; }

    inline float process (float x) noexcept
    {
        const float y = x - x1 + R * y1;
        x1 = x; y1 = y;
        return y;
    }
};

// ── One-pole helpers ─────────────────────────────────────────────────────────
struct OnePoleLP
{
    float z { 0.0f }, a { 1.0f };
    void setCutoff (float hz, double sampleRate) noexcept
    {
        a = 1.0f - std::exp (-2.0f * 3.14159265f * hz / (float) sampleRate);
    }
    void reset() noexcept { z = 0.0f; }
    inline float process (float x) noexcept { z += a * (x - z); return z; }
};

// Tilt EQ as a single one-pole split: out = gLow·lp + gHigh·(x − lp).
// One knob, can't sound bad. tiltDb > 0 brightens.
struct TiltEQ
{
    OnePoleLP lp;
    float gLow { 1.0f }, gHigh { 1.0f };

    void prepare (float pivotHz, double sampleRate) noexcept { lp.setCutoff (pivotHz, sampleRate); }
    void setTiltDb (float tiltDb) noexcept
    {
        gLow  = std::pow (10.0f, -tiltDb / 20.0f);
        gHigh = std::pow (10.0f,  tiltDb / 20.0f);
    }
    void reset() noexcept { lp.reset(); }
    inline float process (float x) noexcept
    {
        const float lo = lp.process (x);
        return gLow * lo + gHigh * (x - lo);
    }
};

// ── Envelope follower (peak, asymmetric one-pole) ───────────────────────────
struct EnvFollower
{
    float env { 0.0f };
    float aAtk { 0.0f }, aRel { 0.0f };

    void prepare (float attackMs, float releaseMs, double sampleRate) noexcept
    {
        aAtk = std::exp (-1.0f / (0.001f * attackMs  * (float) sampleRate));
        aRel = std::exp (-1.0f / (0.001f * releaseMs * (float) sampleRate));
    }
    void reset() noexcept { env = 0.0f; }
    inline float process (float x) noexcept
    {
        const float ax = std::abs (x);
        const float a  = ax > env ? aAtk : aRel;
        env = a * env + (1.0f - a) * ax;
        return env;
    }
};

} // namespace spt
