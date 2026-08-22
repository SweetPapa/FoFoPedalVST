#pragma once
#include "Spec.h"
#include "Filters.h"
#include "Saturation.h"
#include "Oversampler.h"
#include "Mod.h"
#include <vector>

namespace fofo
{

// ─────────────────────────────────────────────────────────────────────────────
// Tape.
//
// The previous SWAY had a mode called "Tape" that was a modulated delay line
// and nothing else — no saturation, no head bump, no gap loss, no dropouts, no
// hiss. It was vibrato with a tape label on it, and it sounded identical
// whether you played soft or hard, which is the one thing tape never does.
//
// A tape machine's character comes from a chain of separate physical effects,
// and leaving any of them out is audible:
//
//   record head   pre-emphasis, then magnetic saturation with hysteresis
//   transport     wow (reel/capstan eccentricity, sub-2 Hz) and flutter
//                 (idler and scrape, 5–20 Hz)
//   the tape      oxide imperfections → brief dropouts; bias noise → hiss
//   replay head   head bump (a low resonance from gap geometry) and gap loss
//                 (HF falls off, and falls off further as level rises because
//                 the tape partially erases its own high frequencies)
//
// These are split into two nodes because saturation needs oversampling at
// block granularity while the transport and heads need per-sample modulation.
// ─────────────────────────────────────────────────────────────────────────────

// ─── Record head: pre-emphasis → hysteresis → de-emphasis, oversampled ───────
class TapeSaturator : public Node
{
public:
    TapeSaturator()
        : os ([this] (float x, int ch) { return shape (x, ch); }, 2 /* 4x */)
    {}

    // 0 = barely magnetised, 1 = pinned. Also opens the hysteresis loop.
    void setDrive (float d01) noexcept { drive01 = juce::jlimit (0.0f, 1.0f, d01); }

    // How much the highs are pushed into the curve before being pulled back
    // out. Higher = highs saturate first, which is what makes tape compress
    // cymbals and pick attack rather than the whole spectrum evenly.
    void setEmphasisDb (float db) noexcept
    {
        if (juce::approximatelyEqual (db, emphasisDb)) return;
        emphasisDb = db;
        updateEmphasis();
    }

    void prepare (const Spec& spec) override
    {
        spec_ = spec;
        os.prepare (spec);

        const int nCh = juce::jmax (1, spec.numChannels);
        hyst.assign ((size_t) nCh, {});
        dc.assign ((size_t) nCh, {});
        preEmph.assign ((size_t) nCh, {});
        postEmph.assign ((size_t) nCh, {});

        for (auto& d : dc) d.prepare (spec.sampleRate, 18.0f);
        for (auto& f : preEmph)  f.prepare (spec.sampleRate);
        for (auto& f : postEmph) f.prepare (spec.sampleRate);
        updateEmphasis();
    }

    void reset() override
    {
        os.reset();
        for (auto& h : hyst) h.reset();
        for (auto& d : dc)   d.reset();
        for (auto& f : preEmph)  f.reset();
        for (auto& f : postEmph) f.reset();
    }

    void process (juce::AudioBuffer<float>& buffer, int numSamples) noexcept override
    {
        if (bypassed) return;
        const int nCh = juce::jmin (buffer.getNumChannels(), (int) hyst.size());
        if (nCh == 0 || numSamples == 0) return;

        // Emphasis filters are linear, so they sit outside the oversampled
        // region — only the nonlinearity needs the extra rate.
        for (int ch = 0; ch < nCh; ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            for (int n = 0; n < numSamples; ++n) d[n] = preEmph[(size_t) ch].process (d[n]);
        }

        os.process (buffer, numSamples);

        for (int ch = 0; ch < nCh; ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            for (int n = 0; n < numSamples; ++n) d[n] = postEmph[(size_t) ch].process (d[n]);
        }
    }

    int   latencySamples() const noexcept override { return os.latencySamples(); }
    float latencyExact()   const noexcept override { return os.latencyExact(); }

private:
    void updateEmphasis()
    {
        for (auto& f : preEmph)  f.set (Svf::Type::HighShelf, 3000.0f, 0.6f,  emphasisDb);
        for (auto& f : postEmph) f.set (Svf::Type::HighShelf, 3000.0f, 0.6f, -emphasisDb);
    }

    inline float shape (float x, int ch) noexcept
    {
        const size_t i = (size_t) juce::jlimit (0, (int) hyst.size() - 1, ch);

        // Loop width grows with drive: harder magnetisation, more history.
        const float g = 1.0f + 5.0f * drive01;
        const float k = 0.08f + 0.34f * drive01;

        float y = hyst[i].process (x, g, k);

        // Hysteresis is asymmetric about zero once it has history, so it
        // generates DC. Remove it inside the loop rather than letting it
        // accumulate into the rest of the chain.
        y = dc[i].process (y);

        // Loudness compensation — drive should change texture, not level.
        return y * (1.0f / (1.0f + 1.9f * drive01));
    }

    Oversampled os;
    Spec  spec_ {};
    float drive01 { 0.3f };
    float emphasisDb { 5.0f };

    std::vector<TapeHysteresis> hyst;
    std::vector<DcBlocker>      dc;
    std::vector<Svf>            preEmph, postEmph;
};

// ─── Transport + tape + replay head, per sample ──────────────────────────────
//
// Delay times come from outside (a ModMatrix destination per channel) so wow
// and flutter share the pedal's one modulation system rather than growing a
// private LFO the way every previous pedal did.
class TapeTransport : public Node
{
public:
    // Called once per sample, before the delays for that sample are read —
    // this is where the owner advances its modulation matrix, so the node
    // stays usable with any modulation source rather than owning private LFOs
    // the way every previous pedal did.
    void setSampleTick (std::function<void()> fn) { tickFn = std::move (fn); }

    // Delay offset in samples for this channel, for the current sample.
    void setDelayProvider (std::function<float (int channel)> fn) { delayFor = std::move (fn); }

    void setCentreDelayMs (float ms) noexcept { centreMs = juce::jmax (0.5f, ms); }
    void setHeadBump (float hz, float db) noexcept { bumpHz = hz; bumpDb = db; dirty = true; }
    void setGapLossHz (float hz) noexcept { gapHz = juce::jlimit (1000.0f, 20000.0f, hz); }

    // How much louder passages lose their own highs (tape self-erasure).
    void setSelfErasure (float amount01) noexcept { erasure = juce::jlimit (0.0f, 1.0f, amount01); }

    void setHissDb (float db) noexcept { hissGain = db <= -120.0f ? 0.0f : juce::Decibels::decibelsToGain (db); }

    // Probability-ish knob: 0 = pristine tape, 1 = well-used.
    void setDropoutAmount (float a01) noexcept { dropAmount = juce::jlimit (0.0f, 1.0f, a01); }

    void prepare (const Spec& spec) override
    {
        spec_ = spec;
        const int nCh = juce::jmax (1, spec.numChannels);

        maxDelaySamp = (float) (0.045 * spec.sampleRate);
        lines.assign ((size_t) nCh, {});
        for (auto& l : lines) l.prepare (spec.sampleRate, maxDelaySamp + 8.0f);

        bump.assign ((size_t) nCh, {});
        gap .assign ((size_t) nCh, {});
        for (auto& f : bump) f.prepare (spec.sampleRate);
        for (auto& f : gap)  f.prepare (spec.sampleRate);

        level.assign ((size_t) nCh, {});
        for (auto& e : level) { e.prepare (spec.sampleRate); e.setCutoff (8.0f); }

        rng.seed (0x7A9E1u);
        dropLeft = 0; dropLen = 0; dropDepth = 0.0f;
        dirty = true;
        updateFilters();
    }

    void reset() override
    {
        for (auto& l : lines) l.reset();
        for (auto& f : bump)  f.reset();
        for (auto& f : gap)   f.reset();
        for (auto& e : level) e.reset();
        dropLeft = 0;
    }

    void process (juce::AudioBuffer<float>& buffer, int numSamples) noexcept override
    {
        if (bypassed) return;
        const int nCh = juce::jmin (buffer.getNumChannels(), (int) lines.size());
        if (nCh == 0 || numSamples == 0) return;

        if (dirty) updateFilters();

        const float centreSamp = centreMs * 0.001f * (float) spec_.sampleRate;

        for (int n = 0; n < numSamples; ++n)
        {
            if (tickFn) tickFn();

            // ── oxide dropouts ────────────────────────────────────────────
            // Rare, brief, and they take the highs with them — that combined
            // signature is what reads as "tape" rather than "volume automation".
            if (dropLeft <= 0)
            {
                // ~0.6 events/sec at full amount.
                const float p = dropAmount * 0.6f / (float) spec_.sampleRate;
                if (dropAmount > 0.0f && rng.unipolar() < p)
                {
                    dropLen   = (int) ((0.006f + 0.045f * rng.unipolar()) * (float) spec_.sampleRate);
                    dropLeft  = dropLen;
                    dropDepth = (0.12f + 0.55f * rng.unipolar()) * dropAmount;
                }
            }

            float dropGain = 1.0f, dropDark = 1.0f;
            if (dropLeft > 0)
            {
                const float t = 1.0f - (float) dropLeft / (float) juce::jmax (1, dropLen);
                const float shapeV = std::sin (juce::MathConstants<float>::pi * t); // 0 → 1 → 0
                dropGain = 1.0f - dropDepth * shapeV;
                dropDark = 1.0f - 0.55f * dropDepth * shapeV;
                --dropLeft;
            }

            for (int ch = 0; ch < nCh; ++ch)
            {
                auto* d = buffer.getWritePointer (ch);
                float x = d[n];

                // ── transport: wow + flutter as a moving delay ─────────────
                const float mod = delayFor ? delayFor (ch) : 0.0f;
                const float delaySamp = juce::jlimit (2.0f, maxDelaySamp, centreSamp + mod);
                x = lines[(size_t) ch].processSample (x, delaySamp);

                // ── replay head: low resonance from the gap geometry ──────
                x = bump[(size_t) ch].process (x);

                // ── gap loss, pushed down further by the tape's own level ─
                const float env = level[(size_t) ch].process (std::abs (x));
                const float selfErase = 1.0f - erasure * juce::jmin (1.0f, env * 2.2f);
                x = gap[(size_t) ch].processModulated (x, gapHz * selfErase * dropDark);

                // ── dropout gain, then bias noise ─────────────────────────
                x *= dropGain;
                if (hissGain > 0.0f) x += hissGain * rng.bipolar();

                d[n] = x;
            }
        }
    }

    // The centre of the transport delay is a fixed offset, so it is honest
    // latency and the host can compensate it. The wow and flutter then read as
    // movement around zero rather than as a chunk of delay.
    int latencySamples() const noexcept override
    {
        return (int) std::round (centreMs * 0.001f * spec_.sampleRate);
    }

private:
    void updateFilters()
    {
        for (auto& f : bump) f.set (Svf::Type::Bell, bumpHz, 0.9f, bumpDb);
        for (auto& f : gap)  f.set (Svf::Type::Lowpass, gapHz, 0.65f);
        dirty = false;
    }

    std::function<void()>      tickFn;
    std::function<float (int)> delayFor;

    Spec  spec_ {};
    float centreMs { 6.0f };
    float maxDelaySamp { 1000.0f };
    float bumpHz { 70.0f }, bumpDb { 2.0f };
    float gapHz { 14000.0f };
    float erasure { 0.35f };
    float hissGain { 0.0f };
    float dropAmount { 0.0f };
    bool  dirty { true };

    std::vector<DelayLine> lines;
    std::vector<Svf>       bump, gap;
    std::vector<OnePole>   level;

    Rng rng;
    int dropLeft { 0 }, dropLen { 0 };
    float dropDepth { 0.0f };
};

} // namespace fofo
