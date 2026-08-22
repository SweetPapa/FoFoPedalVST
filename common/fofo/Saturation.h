#pragma once
#include "Spec.h"
#include <cmath>

namespace fofo
{

// ─── numerically stable ln(cosh(x)) ──────────────────────────────────────────
// ln cosh x = |x| + ln(1 + e^(-2|x|)) - ln 2 — exact, and doesn't overflow for
// large x the way cosh() itself does.
inline float logCosh (float x) noexcept
{
    const float ax = std::abs (x);
    return ax + std::log1p (std::exp (-2.0f * ax)) - 0.6931471805599453f;
}

// ─── First-order antiderivative anti-aliasing tanh ───────────────────────────
//
//   f(x) = tanh(x + b) - tanh(b)          (asymmetric, DC-compensated)
//   F(x) = ln cosh(x + b) - x·tanh(b)
//   y[n] = (F(x[n]) - F(x[n-1])) / (x[n] - x[n-1])
//
// (Parker / Zavalishin / La Cour-Harbo, DAFx-16.) ADAA at 2x rivals naive 8x
// for alias suppression at a fraction of the cost. Introduces a half-sample
// delay, which is fine inside an oversampled region whose latency is reported.
//
// One instance per channel. The bias may be modulated per sample — moving
// asymmetry is most of what reads as "valve" or "tape" rather than "clipper".
struct AdaaTanh
{
    float x1 { 0.0f };
    float F1 { 0.0f };

    void reset() noexcept { x1 = 0.0f; F1 = 0.0f; }

    inline float process (float x, float bias = 0.0f) noexcept
    {
        const float tb = std::tanh (bias);
        const float F  = logCosh (x + bias) - x * tb;
        const float dx = x - x1;

        // Near dx = 0 the difference quotient is numerically useless; fall
        // back to the midpoint value of f itself.
        const float y = (std::abs (dx) > 1.0e-4f)
                            ? (F - F1) / dx
                            : std::tanh (0.5f * (x + x1) + bias) - tb;

        x1 = x;
        F1 = F;
        return y;
    }
};

// ─── Tape hysteresis ─────────────────────────────────────────────────────────
//
//   y[n] = tanh(g·x[n] − k·y[n−1])
//
// The one-sample feedback widens the transfer curve into a loop, so the output
// depends on where the magnetisation has been, not only where the input is
// now. That history dependence is what gives tape its program-dependent
// even/odd harmonic blend and its characteristic softening of fast transients.
// Stable for k < 1; k is the loop width.
//
// This is the cheap end of tape modelling — a Jiles-Atherton solve is more
// faithful — but it captures the audible behaviour and costs one tanh.
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

// ─── Output-bus cubic soft clip ──────────────────────────────────────────────
// Unity gain at zero, ceiling at ±1. Use it on a wet bus if you need a
// backstop — never across a dry path, and never without oversampling if the
// signal reaching it has real high-frequency content.
inline float softClipCubic (float x) noexcept
{
    if (x >  1.0f) return  1.0f;
    if (x < -1.0f) return -1.0f;
    return 1.5f * x - 0.5f * x * x * x;
}

} // namespace fofo
