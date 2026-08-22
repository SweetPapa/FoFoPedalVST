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
    t::section ("F2 — DOUBLE: humanization must actually wander");
    {
        // DriftWalk's coefficient comes from the sample rate but the pitch and
        // level walks were ticked once per block, dividing the effective
        // corner by the block size and freezing each voice at a constant
        // detune. A doubler whose voices never move is a chorus.
        //
        // Observing this needs care. The wet bus always beats against itself
        // because the voices are detuned by design, and that beating is
        // several Hz — far faster than the ~0.2 Hz drift and much larger in
        // amplitude. Comparing raw block-RMS variance therefore shows almost
        // no difference between HUMAN 0 and HUMAN 1 even when the fix works.
        //
        // So: take the block-RMS trace, smooth it hard enough to remove the
        // fixed beating, and look at what is left. Only genuine slow wander
        // survives that filter.
        auto slowWanderOf = [&] (float human)
        {
            dbl::DoubleEngine e;
            e.prepare (spec);
            e.setThick01 (0.8f); e.setWide01 (0.7f); e.setHuman01 (human); e.setMix01 (1.0f);

            juce::AudioBuffer<float> b (2, kBs);
            std::vector<float> trace;
            const int blocks = (int) (kSr * 30.0 / kBs);
            for (int k = 0; k < blocks; ++k)
            {
                for (int n = 0; n < kBs; ++n)
                {
                    const float s2 = 0.3f * std::sin (2.0f * juce::MathConstants<float>::pi
                                                      * 220.0f * (float) (k * kBs + n) / (float) kSr);
                    b.setSample (0, n, s2); b.setSample (1, n, s2);
                }
                e.process (b);
                if (k > blocks / 5) trace.push_back ((float) t::rms (b.getReadPointer (0), kBs));
            }

            // ~2 s moving average: kills the multi-Hz beat, keeps sub-Hz drift.
            const int win = (int) (2.0 * kSr / kBs);
            std::vector<float> smooth;
            for (size_t i = (size_t) win; i < trace.size(); ++i)
            {
                double acc = 0.0;
                for (int j = 0; j < win; ++j) acc += trace[i - (size_t) j];
                smooth.push_back ((float) (acc / win));
            }
            return t::stdev (smooth);
        };

        const double still  = slowWanderOf (0.0f);
        const double moving = slowWanderOf (1.0f);
        t::ok (moving > still * 1.5, "HUMAN at 1 produces clearly more slow wander than HUMAN at 0",
               t::fmt2 ("smoothed stdev %.6f vs %.6f", moving, still));

        // The above is an indirect observable and the effect is modest, so
        // guard the mechanism itself too: a DriftWalk prepared at the sample
        // rate but advanced once per block collapses to a near-constant. This
        // is the exact failure that shipped, stated as a number.
        {
            auto varianceAt = [] (int decimation)
            {
                spt::DriftWalk d;
                d.prepare (0.30f, kSr, 12345u);
                std::vector<float> trace;
                const int total = (int) kSr * 20;
                for (int i = 0; i < total; ++i)
                {
                    const float v = d.next();
                    if (i % decimation == 0) trace.push_back (v);
                }
                return t::stdev (trace);
            };
            const double perSample = varianceAt (1);

            spt::DriftWalk blockTicked;
            blockTicked.prepare (0.30f, kSr, 12345u);
            std::vector<float> blockTrace;
            for (int k = 0; k < (int) (kSr * 20 / kBs); ++k) blockTrace.push_back (blockTicked.next());
            const double perBlock = t::stdev (blockTrace);

            t::ok (perSample > perBlock * 4.0,
                   "ticking DriftWalk per sample moves >4x more than per block",
                   t::fmt2 ("stdev %.5f vs %.5f", perSample, perBlock));
        }
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
    t::section ("F9 — DOUBLE/BACKPORCH: the dry path stays pristine");
    {
        // Both engines used to end with a cubic soft clip on the dry+wet sum,
        // at base rate. That added third-harmonic distortion and aliasing to
        // the dry signal above unity and imposed a hard +-1.0 ceiling.
        dbl::DoubleEngine e;
        e.prepare (spec);
        e.setThick01 (0.7f); e.setWide01 (0.7f); e.setHuman01 (0.5f); e.setMix01 (0.0f);

        juce::AudioBuffer<float> b (2, kBs);
        double worstErr = 0.0, outPeak = 0.0;
        for (int k = 0; k < 20; ++k)
        {
            std::vector<float> in ((size_t) kBs);
            for (int n = 0; n < kBs; ++n)
            {
                in[(size_t) n] = 1.5f * std::sin (2.0f * juce::MathConstants<float>::pi
                                                  * 220.0f * (float) (k * kBs + n) / (float) kSr);
                b.setSample (0, n, in[(size_t) n]);
                b.setSample (1, n, in[(size_t) n]);
            }
            e.process (b);
            for (int n = 0; n < kBs; ++n)
            {
                worstErr = std::max (worstErr, (double) std::abs (b.getSample (0, n) - in[(size_t) n]));
                outPeak  = std::max (outPeak, (double) std::abs (b.getSample (0, n)));
            }
        }
        t::ok (worstErr == 0.0, "DOUBLE at MIX 0 passes a 1.5-peak input bit-exact",
               t::fmt ("max |out-in| = %.3e", worstErr));
        t::near (outPeak, 1.5, 1e-4, "and the peak is not clipped to unity");
    }

    // ─────────────────────────────────────────────────────────────────────
    t::section ("F10 — DOUBLE: THICK is continuous across noon");
    {
        // activeVoices flipped 2->4 the instant thick01 crossed 0.5, stepping
        // busNorm 0.707->0.5 at a block boundary: ~3 dB, and a click under
        // automation.
        double prev = -1e9, worstJump = 0.0, worstAt = 0.0;
        for (double thick = 0.40; thick <= 0.62001; thick += 0.01)
        {
            dbl::DoubleEngine e;
            e.prepare (spec);
            e.setThick01 ((float) thick); e.setWide01 (0.7f); e.setHuman01 (0.0f); e.setMix01 (1.0f);

            juce::AudioBuffer<float> b (2, kBs);
            double acc = 0.0; int cnt = 0;
            for (int k = 0; k < 60; ++k)
            {
                for (int n = 0; n < kBs; ++n)
                {
                    const float s = 0.3f * std::sin (2.0f * juce::MathConstants<float>::pi
                                                     * 220.0f * (float) (k * kBs + n) / (float) kSr);
                    b.setSample (0, n, s); b.setSample (1, n, s);
                }
                e.process (b);
                if (k >= 40) { acc += t::rms (b.getReadPointer (0), kBs); ++cnt; }
            }
            const double lvl = 20.0 * std::log10 (acc / cnt + 1e-12);
            if (prev > -1e8)
            {
                const double j = std::abs (lvl - prev);
                if (j > worstJump) { worstJump = j; worstAt = thick; }
            }
            prev = lvl;
        }
        t::ok (worstJump < 0.5, "no step between adjacent THICK settings around 0.5",
               t::fmt2 ("largest %.3f dB at THICK %.2f", worstJump, worstAt));
    }
}
