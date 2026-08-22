#pragma once
#include "Spec.h"
#include "Filters.h"
#include "Delay.h"
#include "Mod.h"
#include <array>
#include <vector>
#include <cmath>

namespace fofo
{

// ─────────────────────────────────────────────────────────────────────────────
// Space.
//
// The previous reverbs were a Dattorro tank and nothing else. A tank is a good
// late-field generator, but on its own it makes a *tail*, not a *place* — the
// first 50 ms of a real room is a handful of discrete reflections off nearby
// surfaces, and that pattern is most of what tells a listener how big the room
// is and where the source sits in it. Adding early reflections is the cheapest
// large improvement available to this catalogue.
//
// Two pieces here:
//
//   EarlyReflections  a tapped delay bank from a shoebox image-source pattern
//   Fdn8              a Jot-style feedback delay network for the late field,
//                     with per-band decay so the lows can be made to die
//                     faster than the mids — which is the difference between
//                     a reverb that sits in a mix and one that swamps it
// ─────────────────────────────────────────────────────────────────────────────

// ─── Early reflections ───────────────────────────────────────────────────────
class EarlyReflections
{
public:
    static constexpr int kTaps = 16;

    void prepare (const Spec& spec)
    {
        sr = spec.sampleRate;
        const float maxMs = 140.0f;
        line.prepare (sr, maxMs * 0.001f * (float) sr + 8.0f);
        for (auto& f : damp) { f.prepare (sr); f.set (Svf::Type::Lowpass, 8000.0f, 0.7f); }
        reset();
    }

    void reset()
    {
        line.reset();
        for (auto& f : damp) f.reset();
    }

    // 0 = a small booth, 1 = a large hall.
    void setSize01 (float v) noexcept { size01 = juce::jlimit (0.0f, 1.0f, v); }
    void setDampHz (float hz) noexcept
    {
        for (auto& f : damp) f.set (Svf::Type::Lowpass, hz, 0.7f);
    }
    void setWidth01 (float v) noexcept { width = juce::jlimit (0.0f, 1.0f, v); }

    // in: mono send. Writes stereo early field.
    inline void process (float x, float& outL, float& outR) noexcept
    {
        line.push (x);

        // Room scale: times stretch with size, and the whole pattern gets
        // quieter and later as the surfaces move away.
        const float scale = juce::jmap (size01, 0.35f, 2.6f);
        const float msToSamp = 0.001f * (float) sr;

        float l = 0.0f, r = 0.0f;
        for (int i = 0; i < kTaps; ++i)
        {
            const float d = juce::jmax (2.0f, kTapMs[i] * scale * msToSamp);
            const float g = kTapGain[i];
            // The two channels read the same bank at slightly different times,
            // which is what makes an early field feel like a room rather than
            // a widened mono signal.
            l += line.read (d)          * g * panL (i);
            r += line.read (d * 1.037f) * g * panR (i);
        }

        l = damp[0].process (l);
        r = damp[1].process (r);

        const float mid  = 0.5f * (l + r);
        const float side = 0.5f * (l - r) * width;
        outL = mid + side;
        outR = mid - side;
    }

private:
    // A shoebox image-source pattern: first-order reflections off six
    // surfaces, then a few second-order ones. Times in ms at scale 1.0,
    // gains falling roughly as 1/distance with alternating polarity from the
    // pressure reversal at each bounce.
    static constexpr float kTapMs[kTaps] = {
         7.3f, 11.9f, 15.1f, 19.7f, 24.3f, 28.9f, 33.1f, 37.7f,
        41.3f, 46.1f, 50.7f, 55.3f, 59.9f, 64.7f, 69.1f, 73.9f
    };
    static constexpr float kTapGain[kTaps] = {
         0.84f, -0.71f,  0.63f, -0.55f,  0.48f, -0.42f,  0.37f, -0.32f,
         0.28f, -0.25f,  0.22f, -0.19f,  0.16f, -0.14f,  0.12f, -0.10f
    };

    static float panL (int i) noexcept { return (i % 2 == 0) ? 0.86f : 0.51f; }
    static float panR (int i) noexcept { return (i % 2 == 0) ? 0.51f : 0.86f; }

    double sr { 44100.0 };
    float  size01 { 0.5f };
    float  width { 1.0f };
    DelayLine line;
    Svf       damp[2];
};

// ─── Feedback delay network, 8 lines ─────────────────────────────────────────
//
// Eight mutually-prime delay lines mixed through a Hadamard matrix every pass.
// Compared with a Dattorro figure-8 this builds echo density faster and stays
// smoother on long decays, and — the reason it is here — it gives somewhere
// natural to put per-band decay.
//
// Each line's feedback path carries a gain derived from the target RT60 plus a
// two-shelf filter, so low, mid and high frequencies can decay at genuinely
// different rates. A single damping lowpass, which is what the old tank had,
// can only make highs die sooner; it cannot stop low end from piling up, and
// low end piling up is exactly what makes a reverb swamp a track.
class Fdn8
{
public:
    static constexpr int kLines = 8;

    void prepare (const Spec& spec)
    {
        sr = spec.sampleRate;

        for (int i = 0; i < kLines; ++i)
        {
            const float maxSamp = (float) (kBaseLen[i] * kMaxSize * sr / 44100.0) + 64.0f;
            line[i].prepare (sr, maxSamp);
            lowShelf[i].prepare (sr);
            highShelf[i].prepare (sr);
            mod[i].setSeed (0xB0B0u + (uint32_t) i * 7919u);
            mod[i].setRateHz (0.13f + 0.031f * (float) i);
            mod[i].prepare (sr / (double) kControlBlock);
        }
        modCounter = 0;
        updateDerived();
        reset();
    }

    void reset()
    {
        for (auto& l : line) l.reset();
        for (auto& f : lowShelf)  f.reset();
        for (auto& f : highShelf) f.reset();
        for (auto& m : mod) m.reset();
        modCounter = 0;
    }

    void setSize01 (float v) noexcept    { if (! juce::approximatelyEqual (v, size01))  { size01  = juce::jlimit (0.0f, 1.0f, v); dirty = true; } }
    void setDecaySeconds (float s) noexcept { if (! juce::approximatelyEqual (s, rt60)) { rt60 = juce::jmax (0.05f, s); dirty = true; } }

    // Decay multipliers relative to the mid band. <1 = that band dies sooner.
    void setLowDecayRatio  (float r) noexcept { if (! juce::approximatelyEqual (r, lowRatio))  { lowRatio  = juce::jlimit (0.1f, 2.0f, r); dirty = true; } }
    void setHighDecayRatio (float r) noexcept { if (! juce::approximatelyEqual (r, highRatio)) { highRatio = juce::jlimit (0.1f, 2.0f, r); dirty = true; } }

    void setCrossovers (float lowHz, float highHz) noexcept
    {
        loXoverHz = lowHz; hiXoverHz = highHz; dirty = true;
    }

    void setModDepth (float samples) noexcept { modDepth = juce::jmax (0.0f, samples); }
    void setWidth01 (float v) noexcept { width = juce::jlimit (0.0f, 1.0f, v); }

    inline void process (float inL, float inR, float& outL, float& outR) noexcept
    {
        if (dirty) updateDerived();

        // Slow, decorrelated modulation of the read points. Without it the
        // network's modes sit still and ring; with it they smear.
        if (modCounter == 0)
            for (int i = 0; i < kLines; ++i) modValue[i] = mod[i].tick() * modDepth;
        if (++modCounter >= kControlBlock) modCounter = 0;

        // Read the tank.
        float v[kLines];
        for (int i = 0; i < kLines; ++i)
            v[i] = line[i].read (juce::jmax (2.0f, lenSamp[i] + modValue[i]));

        // Per-band decay: one gain plus two shelves per line.
        for (int i = 0; i < kLines; ++i)
        {
            const float lo = lowShelf[i].process (v[i]);
            const float hi = highShelf[i].process (v[i]);
            v[i] = gainMid[i] * v[i] + gainLowRel[i] * lo + gainHighRel[i] * hi;
        }

        // Hadamard mixing — three butterfly stages, unitary after 1/sqrt(8).
        hadamard8 (v);

        // Inject and write back.
        const float dl = 0.5f * (inL + inR);
        const float ds = 0.5f * (inL - inR);
        for (int i = 0; i < kLines; ++i)
            line[i].push (v[i] + (i % 2 == 0 ? dl : ds) * kInGain[i]);

        // Two decorrelated output taps.
        float l = 0.0f, r = 0.0f;
        for (int i = 0; i < kLines; ++i)
        {
            l += v[i] * kOutL[i];
            r += v[i] * kOutR[i];
        }
        l *= 0.35f; r *= 0.35f;

        const float mid  = 0.5f * (l + r);
        const float side = 0.5f * (l - r) * width;
        outL = mid + side;
        outR = mid - side;
    }

private:
    static void hadamard8 (float* v) noexcept
    {
        for (int stride = 1; stride < kLines; stride <<= 1)
            for (int i = 0; i < kLines; i += stride << 1)
                for (int j = i; j < i + stride; ++j)
                {
                    const float a = v[j], b = v[j + stride];
                    v[j] = a + b;
                    v[j + stride] = a - b;
                }
        constexpr float norm = 0.35355339f; // 1/sqrt(8)
        for (int i = 0; i < kLines; ++i) v[i] *= norm;
    }

    void updateDerived()
    {
        dirty = false;
        const float scale = juce::jmap (size01, 0.30f, kMaxSize) * (float) (sr / 44100.0);

        for (int i = 0; i < kLines; ++i)
        {
            lenSamp[i] = juce::jmax (8.0f, (float) kBaseLen[i] * scale);

            // Jot: for a line of length L, the per-pass gain that yields the
            // target RT60 is 10^(-3L / (RT60·fs)).
            auto gFor = [&] (float seconds)
            {
                const float t = juce::jmax (0.05f, seconds);
                return std::pow (10.0f, -3.0f * lenSamp[i] / (t * (float) sr));
            };

            const float gm = gFor (rt60);
            const float gl = gFor (rt60 * lowRatio);
            const float gh = gFor (rt60 * highRatio);

            // out = gm·x + (gl−gm)·lowpassed + (gh−gm)·highpassed
            gainMid[i]     = gm;
            gainLowRel[i]  = gl - gm;
            gainHighRel[i] = gh - gm;

            lowShelf[i] .set (Svf::Type::Lowpass,  loXoverHz, 0.6f);
            highShelf[i].set (Svf::Type::Highpass, hiXoverHz, 0.6f);
        }
    }

    static constexpr float kMaxSize = 3.2f;

    // Mutually prime lengths at 44.1 kHz, spread over roughly 20–90 ms.
    static constexpr int kBaseLen[kLines] = { 1049, 1291, 1607, 1949, 2311, 2699, 3067, 3491 };
    static constexpr float kInGain[kLines] = { 1.0f, 0.9f, 1.0f, 0.9f, 1.0f, 0.9f, 1.0f, 0.9f };
    static constexpr float kOutL[kLines]  = {  1.0f,  0.7f, -0.9f,  0.5f,  0.8f, -0.6f,  0.4f, -0.7f };
    static constexpr float kOutR[kLines]  = { -0.7f,  1.0f,  0.5f, -0.9f, -0.6f,  0.8f, -0.7f,  0.4f };

    double sr { 44100.0 };
    float size01 { 0.5f };
    float rt60 { 1.5f };
    float lowRatio { 1.0f }, highRatio { 0.5f };
    float loXoverHz { 250.0f }, hiXoverHz { 3500.0f };
    float modDepth { 6.0f };
    float width { 1.0f };
    bool  dirty { true };

    DelayLine line[kLines];
    Svf       lowShelf[kLines], highShelf[kLines];
    Drift     mod[kLines];
    float     modValue[kLines] {};
    int       modCounter { 0 };

    float lenSamp[kLines] {};
    float gainMid[kLines] {}, gainLowRel[kLines] {}, gainHighRel[kLines] {};
};

} // namespace fofo
