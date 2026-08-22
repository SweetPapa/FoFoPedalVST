#pragma once
#include "Spec.h"
#include "Filters.h"
#include <vector>
#include <memory>
#include <cmath>
#include <cstdint>

namespace fofo
{

// ─── Deterministic per-instance RNG ──────────────────────────────────────────
// Seeded explicitly so two instances of the same plugin never walk in
// lockstep, and so a test can reproduce a run exactly.
struct Rng
{
    uint32_t s { 0x9E3779B9u };

    void seed (uint32_t v) noexcept { s = v | 1u; }

    inline uint32_t nextBits() noexcept { s = s * 1664525u + 1013904223u; return s; }

    // Uniform in [-1, 1).
    inline float bipolar() noexcept
    {
        return (float) (nextBits() >> 8) * (1.0f / 8388608.0f) - 1.0f;
    }

    // Uniform in [0, 1).
    inline float unipolar() noexcept
    {
        return (float) (nextBits() >> 8) * (1.0f / 16777216.0f);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Modulation sources
//
// Every source is prepared with the CONTROL rate, not the sample rate, and is
// ticked only by ModMatrix. That is deliberate: the single worst modulation
// bug in the previous code was a random walk whose coefficient was derived
// from the sample rate but which was then advanced once per audio block,
// dividing its effective corner by the block size and freezing it into a
// constant. Sources cannot be ticked by hand here, so that cannot recur.
//
// All sources return roughly [-1, 1].
// ─────────────────────────────────────────────────────────────────────────────
class ModSource
{
public:
    virtual ~ModSource() = default;
    virtual void  prepare (double controlRate) = 0;
    virtual void  reset() = 0;
    virtual float tick() noexcept = 0;

    // Sources that follow the audio (envelope followers) get fed here; the
    // rest ignore it.
    virtual void feedAudio (float /*monoSample*/) noexcept {}

    float lastValue() const noexcept { return last; }

protected:
    float last { 0.0f };
};

// ─── LFO ─────────────────────────────────────────────────────────────────────
// Real shapes, and an optional drift on both rate and output. Note the drift
// defaults to ZERO here. The old DriftLFO added 25% filtered noise to every
// LFO's output unconditionally, which on a delay time is not "organic" — it is
// random pitch jitter riding a sine. Drift is now something you ask for.
class Lfo : public ModSource
{
public:
    enum class Shape { Sine = 0, Triangle, SawUp, SawDown, Square, SampleHold, SmoothRandom };

    Lfo (Shape s = Shape::Sine, float hz = 1.0f, uint32_t seed = 0x1234567u)
        : shape (s), rateHz (hz), seedValue (seed) {}

    void prepare (double controlRate) override
    {
        cr = controlRate;
        rng.seed (seedValue);
        smoothLp.prepare (cr);
        smoothLp.setCutoff (juce::jmax (0.05f, rateHz * 2.0f));
        reset();
    }

    void reset() override
    {
        phase = (double) startPhase;
        shTarget = rng.bipolar();
        shPrev = shTarget;
        smoothLp.reset();
        last = 0.0f;
    }

    void setShape (Shape s) noexcept { shape = s; }
    void setRateHz (float hz) noexcept
    {
        rateHz = juce::jmax (0.0f, hz);
        if (cr > 0.0) smoothLp.setCutoff (juce::jmax (0.05f, rateHz * 2.0f));
    }
    void setStartPhase (float p01) noexcept { startPhase = p01; }
    void setRateDrift (float amt) noexcept { rateDriftAmt = juce::jlimit (0.0f, 1.0f, amt); }
    void setDepthDrift (float amt) noexcept { depthDriftAmt = juce::jlimit (0.0f, 1.0f, amt); }

    float phase01() const noexcept { return (float) phase; }

    float tick() noexcept override
    {
        if (cr <= 0.0) return 0.0f;

        float out = 0.0f;
        switch (shape)
        {
            case Shape::Sine:
                out = (float) std::sin (juce::MathConstants<double>::twoPi * phase);
                break;
            case Shape::Triangle:
                out = 4.0f * std::abs ((float) phase - 0.5f) - 1.0f;
                break;
            case Shape::SawUp:
                out = 2.0f * (float) phase - 1.0f;
                break;
            case Shape::SawDown:
                out = 1.0f - 2.0f * (float) phase;
                break;
            case Shape::Square:
                out = phase < 0.5 ? 1.0f : -1.0f;
                break;
            case Shape::SampleHold:
                out = shTarget;
                break;
            case Shape::SmoothRandom:
                out = smoothLp.process (shTarget);
                break;
        }

        if (depthDriftAmt > 0.0f)
            out *= (1.0f - depthDriftAmt * 0.5f * (1.0f + rng.bipolar() * 0.02f));

        // Advance.
        double inc = (double) rateHz / cr;
        if (rateDriftAmt > 0.0f)
            inc *= (1.0 + (double) (rateDriftAmt * 0.05f * rng.bipolar()));

        phase += inc;
        while (phase >= 1.0)
        {
            phase -= 1.0;
            shPrev   = shTarget;
            shTarget = rng.bipolar();   // new value each cycle for S&H / smooth
        }
        while (phase < 0.0) phase += 1.0;

        last = out;
        return out;
    }

private:
    Shape shape { Shape::Sine };
    float rateHz { 1.0f };
    double phase { 0.0 };
    float startPhase { 0.0f };
    float rateDriftAmt { 0.0f }, depthDriftAmt { 0.0f };
    uint32_t seedValue { 0x1234567u };

    double  cr { 0.0 };
    Rng     rng;
    OnePole smoothLp;
    float   shTarget { 0.0f }, shPrev { 0.0f };
};

// ─── Random-walk drift ───────────────────────────────────────────────────────
// White noise through a heavy lowpass — the slow wander that keeps a static
// patch from sitting perfectly still. Output is normalised so it actually
// covers a useful part of [-1, 1] whatever the corner frequency is; the old
// version's empirical fudge left it an order of magnitude too small at some
// rates.
class Drift : public ModSource
{
public:
    Drift (float rateHz = 0.3f, uint32_t seed = 0xA11CE5u)
        : cutoffHz (rateHz), seedValue (seed) {}

    void prepare (double controlRate) override
    {
        cr = controlRate;
        rng.seed (seedValue);
        lp.prepare (cr);
        lp.setCutoff (cutoffHz);

        // A one-pole fed white noise of unit variance settles at output
        // variance a/(2−a). Normalising by its reciprocal square root keeps
        // the walk's amplitude independent of the chosen corner.
        const float a = lp.a;
        norm = std::sqrt ((2.0f - a) / juce::jmax (1.0e-9f, a));
        reset();
    }

    void reset() override { lp.reset(); last = 0.0f; }

    void setRateHz (float hz) noexcept
    {
        cutoffHz = juce::jmax (0.001f, hz);
        if (cr > 0.0)
        {
            lp.setCutoff (cutoffHz);
            const float a = lp.a;
            norm = std::sqrt ((2.0f - a) / juce::jmax (1.0e-9f, a));
        }
    }

    // For array members, which cannot be constructed with arguments. Call
    // before prepare(); two drifts sharing a seed walk in lockstep, which
    // defeats the point of having more than one.
    void setSeed (uint32_t s) noexcept { seedValue = s; rng.seed (s); }

    float tick() noexcept override
    {
        if (cr <= 0.0) return 0.0f;
        // Uniform white has variance 1/3; scale to unit variance first.
        const float white = rng.bipolar() * 1.7320508f;
        last = juce::jlimit (-1.0f, 1.0f, lp.process (white) * norm);
        return last;
    }

private:
    float cutoffHz { 0.3f };
    uint32_t seedValue { 0xA11CE5u };
    double  cr { 0.0 };
    Rng     rng;
    OnePole lp;
    float   norm { 1.0f };
};

// ─── Envelope follower as a modulation source ────────────────────────────────
// Fed the mono sum of whatever the caller considers the key signal. Stereo
// linkage is therefore structural: there is one follower, not one per channel.
class EnvSource : public ModSource
{
public:
    EnvSource (float attackMs = 5.0f, float releaseMs = 150.0f)
        : atkMs (attackMs), relMs (releaseMs) {}

    void prepare (double controlRate) override
    {
        cr = controlRate;
        recalc();
        reset();
    }

    void reset() override { env = 0.0f; peak = 0.0f; last = 0.0f; }

    void setTimes (float attackMs, float releaseMs) noexcept
    {
        atkMs = attackMs; relMs = releaseMs; recalc();
    }
    void setGain (float g) noexcept { gain = g; }

    // Called per audio sample — accumulates the peak seen since the last tick
    // so a fast transient between control ticks is not missed.
    void feedAudio (float monoSample) noexcept override
    {
        peak = juce::jmax (peak, std::abs (monoSample));
    }

    float tick() noexcept override
    {
        const float x = peak;
        peak = 0.0f;
        const float a = (x > env) ? aAtk : aRel;
        env = a * env + (1.0f - a) * x;
        last = juce::jlimit (0.0f, 1.0f, env * gain);
        return last;
    }

private:
    void recalc() noexcept
    {
        if (cr <= 0.0) return;
        aAtk = std::exp (-1.0f / juce::jmax (1.0e-6f, 0.001f * atkMs * (float) cr));
        aRel = std::exp (-1.0f / juce::jmax (1.0e-6f, 0.001f * relMs * (float) cr));
    }

    float atkMs { 5.0f }, relMs { 150.0f };
    float aAtk { 0.0f }, aRel { 0.0f };
    float env { 0.0f }, peak { 0.0f }, gain { 1.0f };
    double cr { 0.0 };
};

// ─── A plain value source (macro knob) ───────────────────────────────────────
class Macro : public ModSource
{
public:
    void prepare (double) override {}
    void reset() override { last = value; }
    void setValue (float v) noexcept { value = juce::jlimit (-1.0f, 1.0f, v); }
    float tick() noexcept override { last = value; return last; }
private:
    float value { 0.0f };
};

// ─────────────────────────────────────────────────────────────────────────────
// ModMatrix — sources → sparse routing → named destinations
//
// A destination holds a base value (from a parameter) plus the sum of every
// route pointing at it. Targets are recomputed once per control block and
// ramped linearly per sample, so nothing steps and nothing zippers.
//
// Usage:
//     auto rate  = matrix.addSource (std::make_unique<Lfo> (Lfo::Shape::Sine, 0.4f));
//     auto cutoff = matrix.addDest ("cutoff", 800.0f, 200.0f, 8000.0f);
//     matrix.connect (rate, cutoff, 600.0f);
//     matrix.prepare (spec);
//     ...
//     for (int n = 0; n < nS; ++n) {
//         matrix.tick (monoInput[n]);
//         svf.processModulated (x[n], matrix.get (cutoff));
//     }
// ─────────────────────────────────────────────────────────────────────────────
class ModMatrix
{
public:
    enum class Curve { Linear = 0, Squared, Cubed, Exponential };

    using SourceId = int;
    using DestId   = int;

    SourceId addSource (std::unique_ptr<ModSource> s)
    {
        sources.push_back (std::move (s));
        return (SourceId) (sources.size() - 1);
    }

    DestId addDest (const char* name, float base, float minValue, float maxValue)
    {
        Dest d;
        d.name = name;
        d.base = base;
        d.lo   = minValue;
        d.hi   = maxValue;
        d.cur  = juce::jlimit (minValue, maxValue, base);
        d.target = d.cur;
        dests.push_back (d);
        return (DestId) (dests.size() - 1);
    }

    // Returns a route id. Wire routes once at prepare() time and change their
    // depth with setRouteDepth() from then on — rebuilding the route list every
    // block would touch the heap on the audio thread.
    int connect (SourceId src, DestId dst, float depth, Curve curve = Curve::Linear)
    {
        if (src < 0 || src >= (int) sources.size()) return -1;
        if (dst < 0 || dst >= (int) dests.size())   return -1;
        routes.push_back ({ src, dst, depth, curve });
        return (int) routes.size() - 1;
    }

    void setRouteDepth (int routeId, float depth) noexcept
    {
        if (routeId >= 0 && routeId < (int) routes.size())
            routes[(size_t) routeId].depth = depth;
    }

    void clearRoutes() { routes.clear(); }

    void prepare (const Spec& spec)
    {
        for (auto& s : sources) s->prepare (spec.controlRate());
        counter = 0;
        for (auto& d : dests)
        {
            d.cur = juce::jlimit (d.lo, d.hi, d.base);
            d.target = d.cur;
            d.inc = 0.0f;
        }
        computeTargets();
        snapAll();
    }

    void reset()
    {
        for (auto& s : sources) s->reset();
        counter = 0;
        computeTargets();
        snapAll();
    }

    ModSource* source (SourceId id) noexcept
    {
        return (id >= 0 && id < (int) sources.size()) ? sources[(size_t) id].get() : nullptr;
    }

    // Set a destination's unmodulated value (i.e. where the knob is).
    void setBase (DestId id, float v) noexcept
    {
        if (id < 0 || id >= (int) dests.size()) return;
        dests[(size_t) id].base = v;
    }

    float getBase (DestId id) const noexcept
    {
        return (id >= 0 && id < (int) dests.size()) ? dests[(size_t) id].base : 0.0f;
    }

    // Advance one audio sample. `keySample` feeds any audio-following sources.
    inline void tick (float keySample = 0.0f) noexcept
    {
        for (auto& s : sources) s->feedAudio (keySample);

        if (counter == 0)
        {
            for (auto& s : sources) s->tick();
            computeTargets();
            const float k = 1.0f / (float) kControlBlock;
            for (auto& d : dests) d.inc = (d.target - d.cur) * k;
        }

        for (auto& d : dests) d.cur += d.inc;

        if (++counter >= kControlBlock) counter = 0;
    }

    inline float get (DestId id) const noexcept
    {
        return dests[(size_t) id].cur;
    }

    int numSources() const noexcept { return (int) sources.size(); }
    int numDests()   const noexcept { return (int) dests.size(); }
    int numRoutes()  const noexcept { return (int) routes.size(); }

private:
    struct Dest
    {
        const char* name { "" };
        float base { 0.0f }, lo { 0.0f }, hi { 1.0f };
        float target { 0.0f }, cur { 0.0f }, inc { 0.0f };
    };

    struct Route
    {
        int   src { 0 }, dst { 0 };
        float depth { 0.0f };
        Curve curve { Curve::Linear };
    };

    static inline float shape (float v, Curve c) noexcept
    {
        switch (c)
        {
            case Curve::Squared:     return v * std::abs (v);        // keeps sign
            case Curve::Cubed:       return v * v * v;
            case Curve::Exponential: return (std::exp (std::abs (v) * 1.09861229f) - 1.0f) * 0.5f * (v < 0.0f ? -1.0f : 1.0f);
            case Curve::Linear:
            default:                 return v;
        }
    }

    void computeTargets() noexcept
    {
        for (auto& d : dests) d.target = d.base;

        for (const auto& r : routes)
            dests[(size_t) r.dst].target += r.depth * shape (sources[(size_t) r.src]->lastValue(), r.curve);

        for (auto& d : dests) d.target = juce::jlimit (d.lo, d.hi, d.target);
    }

    void snapAll() noexcept
    {
        for (auto& d : dests) { d.cur = d.target; d.inc = 0.0f; }
    }

    std::vector<std::unique_ptr<ModSource>> sources;
    std::vector<Dest>  dests;
    std::vector<Route> routes;
    int counter { 0 };
};

} // namespace fofo
