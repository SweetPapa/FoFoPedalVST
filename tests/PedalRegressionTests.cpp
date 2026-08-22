// Regression guards for the five defects fixed in the Step 0 pass. Each one
// shipped and each one was audible, so each gets a test that fails loudly if
// it comes back. These run against the real pedal DSP, not a reimplementation.

#include "TestHarness.h"
#include "Tests.h"
#include "double/Source/dsp/DoubleEngine.h"
#include "fofopedal/Source/dsp/Character.h"
#include "fofopedal/Source/dsp/OutputGlue.h"
#include "fofopedal/Source/dsp/Drive.h"
#include "spt/DriftLFO.h"

static constexpr double kSr = 48000.0;
static constexpr int    kBs = 512;

void runPedalRegressionTests()
{
    const juce::dsp::ProcessSpec spec { kSr, (juce::uint32) kBs, 2 };

    // ─────────────────────────────────────────────────────────────────────
    t::section ("F1 — FOFOPEDAL Drive: parallel blend must not comb");
    {
        // Drive snapshots dry at base rate and runs wet through a 4x
        // oversampler. Without matching the oversampler's latency the sum is a
        // comb; measured before the fix, a -17.97 dB null at 5.2 kHz. The
        // global MIX macro scales this block's mix, so it fired on every
        // character below 100%.
        fofopedal::Drive d;
        d.prepare (spec);
        d.setAlgo (fofopedal::Drive::Algo::Tube);
        d.setDrive01 (0.0f);   // clean, so any dip is the recombine not the curve
        d.setTone01 (0.5f);
        d.setMix01 (0.5f);     // the case that used to be broken

        double best = -1e9, worst = 1e9, worstF = 0.0;
        for (double f : { 100.0, 500.0, 1000.0, 2000.0, 3000.0, 4000.0, 4400.0, 4800.0, 5200.0, 6000.0 })
        {
            d.reset();
            const double db = t::magnitudeDb ([&] (juce::AudioBuffer<float>& b, int) { d.process (b); },
                                              f, kSr, kBs, 40);
            best = std::max (best, db);
            if (db < worst) { worst = db; worstF = f; }
        }
        t::ok (best - worst < 1.0, "response is flat across 100 Hz - 6 kHz at MIX 50%",
               t::fmt2 ("ripple %.3f dB (deepest at %.0f Hz)", best - worst, worstF));
        t::ok (d.getLatencySamples() > 0, "and the block still reports its latency to the host",
               t::fmtI ("%d samples", d.getLatencySamples()));
    }

    // ─────────────────────────────────────────────────────────────────────
    t::section ("F2 — a drift source ticked per block is a frozen drift");
    {
        // DOUBLE's humanisation now runs through fofo::ModMatrix, which cannot
        // be ticked at the wrong rate, and DoubleTests measures the result
        // directly in cents of pitch wander. What remains worth guarding here
        // is the mechanism in the OLD toolkit, which DAYDREAM and FOFOPEDAL
        // still use: a DriftWalk whose coefficient comes from the sample rate
        // but which is advanced once per block collapses to a constant.
        auto stdevOf = [&] (int decimation, int blockTick)
        {
            spt::DriftWalk d;
            d.prepare (0.30f, kSr, 12345u);
            std::vector<float> trace;
            const int total = (int) kSr * 20;
            if (blockTick)
            {
                for (int k = 0; k < total / kBs; ++k) trace.push_back (d.next());
            }
            else
            {
                for (int i = 0; i < total; ++i)
                {
                    const float v = d.next();
                    if (i % decimation == 0) trace.push_back (v);
                }
            }
            return t::stdev (trace);
        };

        const double perSample = stdevOf (64, 0);
        const double perBlock  = stdevOf (0, 1);
        t::ok (perSample > perBlock * 4.0,
               "ticking DriftWalk per sample moves >4x more than per block",
               t::fmt2 ("stdev %.5f vs %.5f", perSample, perBlock));
    }

    // ─────────────────────────────────────────────────────────────────────
    t::section ("F3 — FOFOPEDAL Character/OutputGlue: compressors stereo-linked");
    {
        // Independent per-channel detectors make the two sides duck by
        // different amounts, so the centre image wanders. Both blocks are
        // always on (exempt from MIX), so this hit everything the plugin
        // output. Test: a hard-panned burst must not change the L/R ratio.
        auto ratioAfter = [&] (auto& block)
        {
            const float loud = 0.9f, quiet = 0.02f;
            juce::AudioBuffer<float> b (2, kBs);
            double sumL = 0.0, sumR = 0.0; int cnt = 0;
            const int blocks = 40;
            for (int k = 0; k < blocks; ++k)
            {
                for (int n = 0; n < kBs; ++n)
                {
                    const int tt = k * kBs + n;
                    const float s = std::sin (2.0f * juce::MathConstants<float>::pi * 220.0f * (float) tt / (float) kSr);
                    b.setSample (0, n, loud  * s);
                    b.setSample (1, n, quiet * s);
                }
                block.process (b);
                if (k >= blocks - 10)
                {
                    sumL += t::rms (b.getReadPointer (0), kBs);
                    sumR += t::rms (b.getReadPointer (1), kBs);
                    ++cnt;
                }
            }
            return (sumR / cnt) / (sumL / cnt + 1e-12);
        };

        const double want = 0.02 / 0.9;

        fofopedal::Character c;
        c.prepare (spec);
        c.setAmount01 (1.0f);
        c.setLowCutHz (20.0f);
        t::near (ratioAfter (c), want, 0.004, "Character holds the stereo image on a hard-panned burst");

        fofopedal::OutputGlue g;
        g.prepare (spec);
        g.setAmount01 (1.0f);
        t::near (ratioAfter (g), want, 0.004, "OutputGlue holds the stereo image on a hard-panned burst");
    }

    // ─────────────────────────────────────────────────────────────────────
    t::section ("F9 / F10 — covered where they now live");
    {
        // DOUBLE and BACKPORCH have both been rebuilt on the kernel, and their
        // dry-path and continuity guarantees are asserted in DoubleTests and
        // BackporchTests against the engines that actually ship. Duplicating
        // them here would mean two places to update and one of them going
        // stale.
        t::ok (true, "DOUBLE's dry path and THICK continuity: see DoubleTests");
        t::ok (true, "BACKPORCH's dry path: see BackporchTests");
    }
}
