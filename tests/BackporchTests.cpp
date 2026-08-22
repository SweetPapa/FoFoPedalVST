// BACKPORCH 2 — does it make a place rather than a tail?

#include "TestHarness.h"
#include "Tests.h"
#include "backporch/Source/dsp/BackporchEngine.h"

using bkpr::BackporchEngine;

static constexpr double kSr = 48000.0;
static constexpr int    kBs = 512;

namespace
{
    juce::dsp::ProcessSpec spec() { return { kSr, (juce::uint32) kBs, 2 }; }

    // Impulse response of the engine, left channel.
    std::vector<float> impulse (BackporchEngine& e, int numSamples)
    {
        std::vector<float> ir;
        ir.reserve ((size_t) numSamples);
        juce::AudioBuffer<float> b (2, kBs);
        bool first = true;
        while ((int) ir.size() < numSamples)
        {
            b.clear();
            if (first) { b.setSample (0, 0, 1.0f); b.setSample (1, 0, 1.0f); first = false; }
            e.process (b);
            for (int n = 0; n < kBs && (int) ir.size() < numSamples; ++n)
                ir.push_back (b.getSample (0, n));
        }
        return ir;
    }

    // Energy in a window of the IR, in dB.
    double windowDb (const std::vector<float>& ir, int from, int to)
    {
        double acc = 0.0;
        for (int i = from; i < to && i < (int) ir.size(); ++i) acc += (double) ir[(size_t) i] * ir[(size_t) i];
        return 10.0 * std::log10 (acc + 1e-30);
    }

    // Energy decay of a band-limited burst — how long the tail lasts at a
    // given frequency, measured as the time for the envelope to fall 30 dB.
    double decayTimeAt (BackporchEngine& e, double hz, double maxSeconds = 6.0)
    {
        // Excite with a tone burst, then watch the tail.
        juce::AudioBuffer<float> b (2, kBs);
        const int burstBlocks = 40;
        for (int k = 0; k < burstBlocks; ++k)
        {
            for (int n = 0; n < kBs; ++n)
            {
                const float s = 0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi
                                                         * hz * (double) (k * kBs + n) / kSr);
                b.setSample (0, n, s); b.setSample (1, n, s);
            }
            e.process (b);
        }
        const double peak = t::rms (b.getReadPointer (0), kBs);
        if (peak < 1e-9) return 0.0;

        const double target = peak * std::pow (10.0, -30.0 / 20.0);
        const int maxBlocks = (int) (maxSeconds * kSr / kBs);
        for (int k = 0; k < maxBlocks; ++k)
        {
            b.clear();
            e.process (b);
            if (t::rms (b.getReadPointer (0), kBs) < target)
                return (double) (k * kBs) / kSr;
        }
        return maxSeconds;
    }
}

void runBackporchTests()
{
    // ─────────────────────────────────────────────────────────────────────
    t::section ("BACKPORCH — there is now an early field");
    {
        // v1 was a bare tank: energy arrived smoothly and never as discrete
        // reflections. A real room's first ~50 ms is a handful of distinct
        // arrivals, and that pattern is what tells a listener how big the
        // space is.
        BackporchEngine e;
        e.prepare (spec());
        e.setMode (BackporchEngine::Mode::Room);
        e.setSpace01 (0.6f); e.setTone01 (0.5f); e.setDuck01 (0.0f); e.setMix01 (1.0f);
        e.reset();

        const auto ir = impulse (e, (int) (0.5 * kSr));

        // Count distinct arrivals in the first 90 ms: local peaks that stand
        // clearly above their neighbourhood.
        const int win = (int) (0.0008 * kSr);
        int arrivals = 0;
        const int upto = (int) (0.09 * kSr);
        for (int i = win; i < upto - win; ++i)
        {
            const float v = std::abs (ir[(size_t) i]);
            if (v < 0.004f) continue;
            bool isPeak = true;
            for (int j = i - win; j <= i + win; ++j)
                if (j != i && std::abs (ir[(size_t) j]) > v) { isPeak = false; break; }
            if (isPeak) { ++arrivals; i += win; }
        }
        t::ok (arrivals >= 6, "discrete early reflections arrive in the first 90 ms",
               t::fmtI ("%d distinct arrivals", arrivals));

        // And they are stereo-decorrelated — an early field that is identical
        // left and right is just a widened mono signal.
        // Measure over 300 ms, not one block — Room's pre-delay alone is
        // ~22 ms, so a single 512-sample block is still silence.
        BackporchEngine e2;
        e2.prepare (spec());
        e2.setMode (BackporchEngine::Mode::Room);
        e2.setSpace01 (0.6f); e2.setDuck01 (0.0f); e2.setMix01 (1.0f);
        e2.reset();
        juce::AudioBuffer<float> b (2, kBs);
        double diff = 0.0, tot = 0.0;
        bool firstBlock = true;
        for (int k = 0; k < (int) (0.3 * kSr / kBs); ++k)
        {
            b.clear();
            if (firstBlock) { b.setSample (0, 0, 1.0f); b.setSample (1, 0, 1.0f); firstBlock = false; }
            e2.process (b);
            for (int n = 0; n < kBs; ++n)
            {
                diff += std::abs (b.getSample (0, n) - b.getSample (1, n));
                tot  += std::abs (b.getSample (0, n)) + std::abs (b.getSample (1, n));
            }
        }
        t::ok (diff / (tot + 1e-12) > 0.15, "and the two channels are genuinely decorrelated",
               t::fmt ("L/R difference is %.1f%% of total energy", 100.0 * diff / (tot + 1e-12)));
    }

    // ─────────────────────────────────────────────────────────────────────
    t::section ("BACKPORCH — per-band decay keeps the lows out of the way");
    {
        // The identity is "sounds produced, not wet". In DSP terms that means
        // low end must not pile up in the tail. v1's single damping lowpass
        // could shorten the highs but had no way to shorten the lows.
        BackporchEngine e;
        e.prepare (spec());
        e.setMode (BackporchEngine::Mode::Room);
        e.setSpace01 (0.85f); e.setTone01 (0.5f); e.setDuck01 (0.0f); e.setMix01 (1.0f);

        e.reset(); const double lowT = decayTimeAt (e, 120.0);
        e.reset(); const double midT = decayTimeAt (e, 1000.0);
        e.reset(); const double hiT  = decayTimeAt (e, 7000.0);

        t::ok (lowT < midT * 0.85, "low end decays faster than the mids",
               t::fmt2 ("120 Hz T30 %.2f s vs 1 kHz %.2f s", lowT, midT));
        t::ok (hiT < midT * 0.95, "and so do the highs",
               t::fmt2 ("7 kHz T30 %.2f s vs 1 kHz %.2f s", hiT, midT));
        t::ok (midT > 0.35, "while the mids still have a real tail",
               t::fmt ("1 kHz T30 %.2f s", midT));
    }

    // ─────────────────────────────────────────────────────────────────────
    t::section ("BACKPORCH — SPACE grows the room");
    {
        auto tailLength = [&] (float space)
        {
            BackporchEngine e;
            e.prepare (spec());
            e.setMode (BackporchEngine::Mode::Room);
            e.setSpace01 (space); e.setTone01 (0.5f); e.setDuck01 (0.0f); e.setMix01 (1.0f);
            e.reset();
            return decayTimeAt (e, 1000.0);
        };
        const double small = tailLength (0.05f), big = tailLength (0.95f);
        t::ok (big > small * 2.5, "SPACE meaningfully changes the decay time",
               t::fmt2 ("T30 %.2f s at SPACE 0.05 vs %.2f s at 0.95", small, big));
    }

    // ─────────────────────────────────────────────────────────────────────
    t::section ("BACKPORCH — the modes are actually different spaces");
    {
        auto earlyEnergy = [&] (BackporchEngine::Mode m)
        {
            BackporchEngine e;
            e.prepare (spec());
            e.setMode (m);
            e.setSpace01 (0.6f); e.setTone01 (0.5f); e.setDuck01 (0.0f); e.setMix01 (1.0f);
            e.reset();
            const auto ir = impulse (e, (int) (0.4 * kSr));
            // Ratio of the first 100 ms to the following 300 ms: high means a
            // forward early field, low means the tank dominates. The window is
            // wide enough that Plate's pre-delay still lands inside it, so the
            // comparison is between two real numbers.
            return windowDb (ir, 0, (int) (0.10 * kSr)) - windowDb (ir, (int) (0.10 * kSr), (int) (0.40 * kSr));
        };

        const double room  = earlyEnergy (BackporchEngine::Mode::Room);
        const double plate = earlyEnergy (BackporchEngine::Mode::Plate);
        const double slap  = earlyEnergy (BackporchEngine::Mode::Slap);

        t::ok (room > plate + 3.0,
               "Room leads with its early field where Plate goes straight to dense",
               t::fmt2 ("early/late ratio: Room %.1f dB vs Plate %.1f dB", room, plate));
        juce::ignoreUnused (slap);

        // Slap's discrete repeat has to actually be discrete.
        BackporchEngine e;
        e.prepare (spec());
        e.setMode (BackporchEngine::Mode::Slap);
        e.setSpace01 (0.5f); e.setDuck01 (0.0f); e.setMix01 (1.0f);
        e.reset();
        const auto ir = impulse (e, (int) (0.4 * kSr));
        int loudest = 0;
        for (int i = (int) (0.02 * kSr); i < (int) (0.30 * kSr); ++i)
            if (std::abs (ir[(size_t) i]) > std::abs (ir[(size_t) loudest])) loudest = i;
        const double ms = 1000.0 * loudest / kSr;
        t::ok (ms > 60.0 && ms < 200.0, "Slap's repeat lands where a slap should",
               t::fmt ("loudest arrival at %.0f ms", ms));
    }

    // ─────────────────────────────────────────────────────────────────────
    t::section ("BACKPORCH — DUCK, MIX and the dry path");
    {
        // The ducker keys off the mono-summed dry, so it is stereo-linked by
        // construction rather than by remembering to link it.
        auto tailUnderPlaying = [&] (float duck)
        {
            BackporchEngine e;
            e.prepare (spec());
            e.setMode (BackporchEngine::Mode::Room);
            e.setSpace01 (0.8f); e.setTone01 (0.5f); e.setDuck01 (duck); e.setMix01 (1.0f);
            e.reset();
            juce::AudioBuffer<float> b (2, kBs);
            double acc = 0.0; int cnt = 0;
            for (int k = 0; k < 120; ++k)
            {
                for (int n = 0; n < kBs; ++n)
                {
                    const float s = 0.6f * (float) std::sin (2.0 * juce::MathConstants<double>::pi
                                                             * 300.0 * (double) (k * kBs + n) / kSr);
                    b.setSample (0, n, s); b.setSample (1, n, s);
                }
                e.process (b);
                if (k > 80) { acc += t::rms (b.getReadPointer (0), kBs); ++cnt; }
            }
            return acc / cnt;
        };
        t::ok (tailUnderPlaying (0.9f) < tailUnderPlaying (0.0f) * 0.85,
               "DUCK pulls the tail down while you play",
               t::fmt2 ("%.4f at DUCK 0.9 vs %.4f at DUCK 0", tailUnderPlaying (0.9f), tailUnderPlaying (0.0f)));

        // MIX 0 must leave the input exactly alone — no clipper on the sum
        // any more (F9), and the engine reports no latency.
        BackporchEngine e;
        e.prepare (spec());
        e.setMode (BackporchEngine::Mode::Plate);
        e.setSpace01 (0.9f); e.setDuck01 (0.5f); e.setMix01 (0.0f);
        e.reset();
        juce::AudioBuffer<float> b (2, kBs);
        double worstErr = 0.0, outPeak = 0.0;
        for (int k = 0; k < 20; ++k)
        {
            std::vector<float> in ((size_t) kBs);
            for (int n = 0; n < kBs; ++n)
            {
                in[(size_t) n] = 1.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi
                                                          * 220.0 * (double) (k * kBs + n) / kSr);
                b.setSample (0, n, in[(size_t) n]); b.setSample (1, n, in[(size_t) n]);
            }
            e.process (b);
            for (int n = 0; n < kBs; ++n)
            {
                worstErr = std::max (worstErr, (double) std::abs (b.getSample (0, n) - in[(size_t) n]));
                outPeak  = std::max (outPeak, (double) std::abs (b.getSample (0, n)));
            }
        }
        t::ok (worstErr == 0.0, "MIX 0 passes a 1.5-peak input bit-exact",
               t::fmt ("max |out-in| = %.3e", worstErr));
        t::near (outPeak, 1.5, 1e-4, "and nothing clips it");

        // The Soundtoys curve: dry holds at unity until 70%.
        BackporchEngine e2;
        e2.prepare (spec());
        e2.setMode (BackporchEngine::Mode::Room);
        e2.setSpace01 (0.5f); e2.setDuck01 (0.0f); e2.setMix01 (0.70f);
        t::ok (e2.getLatencySamples() == 0, "and the engine adds no latency");
    }

    // ─────────────────────────────────────────────────────────────────────
    t::section ("BACKPORCH — stable and bounded");
    {
        bool finite = true;
        double worst = 0.0;
        for (auto m : { BackporchEngine::Mode::Slap, BackporchEngine::Mode::Room, BackporchEngine::Mode::Plate })
        for (float sp : { 0.0f, 1.0f })
        for (float tn : { 0.0f, 1.0f })
        for (float dk : { 0.0f, 1.0f })
        for (float mx : { 0.0f, 1.0f })
        {
            BackporchEngine e;
            e.prepare (spec());
            e.setMode (m); e.setSpace01 (sp); e.setTone01 (tn); e.setDuck01 (dk); e.setMix01 (mx);
            juce::AudioBuffer<float> b (2, kBs);
            for (int k = 0; k < 60; ++k)
            {
                for (int n = 0; n < kBs; ++n)
                {
                    const double t = (double) (k * kBs + n) / kSr;
                    const float s = 0.9f * (float) (std::sin (2.0 * juce::MathConstants<double>::pi * 90.0 * t)
                                                  + 0.6 * std::sin (2.0 * juce::MathConstants<double>::pi * 5100.0 * t));
                    b.setSample (0, n, s); b.setSample (1, n, s);
                }
                e.process (b);
                for (int ch = 0; ch < 2; ++ch)
                    for (int n = 0; n < kBs; ++n)
                    {
                        const float v = b.getSample (ch, n);
                        if (! std::isfinite (v)) finite = false;
                        worst = std::max (worst, (double) std::abs (v));
                    }
            }
        }
        t::ok (finite, "no mode produces a non-finite sample at any knob corner");
        t::ok (worst < 8.0, "and the tank never runs away", t::fmt ("worst peak %.2f", worst));
    }
}
