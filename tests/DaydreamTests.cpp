// DAYDREAM 2 — does the one knob still tell a story, on better parts?

#include "TestHarness.h"
#include "Tests.h"
#include "daydream/Source/dsp/DaydreamEngine.h"
#include "fofo/Mod.h"

using daydream::DaydreamEngine;

static constexpr double kSr = 48000.0;
static constexpr int    kBs = 512;

namespace
{
    juce::dsp::ProcessSpec spec() { return { kSr, (juce::uint32) kBs, 2 }; }

    // Let the smoothed macro settle before measuring — the knob is
    // deliberately slewed so a sweep reads as a journey.
    void settle (DaydreamEngine& e, int blocks = 120)
    {
        juce::AudioBuffer<float> b (2, kBs);
        for (int k = 0; k < blocks; ++k) { b.clear(); e.process (b); }
    }

    // Output level for a NOISE probe. A tone is the wrong probe here: from
    // DREAM 0.32 to 0.70 the transport's wow is modulating pitch, and the RMS
    // of a pitch-modulated tone measured over a short window depends on where
    // the wobble happened to be — which made a perfectly smooth knob look like
    // it had 3 dB steps scattered through exactly that region. Noise RMS is
    // indifferent to pitch modulation, so it measures gain and nothing else.
    //
    // The noise is low-passed at 4 kHz so it has roughly the spectrum of
    // program material. Since DAYDREAM 3 the tape machine is in the main
    // path, and tape gap loss closes down to 7 kHz across the memory zone:
    // WHITE noise carries most of its energy above that, so a white probe
    // reads the gap loss as a 6 dB level drop that no real track would show.
    double noiseLevelDb (DaydreamEngine& e, float amp)
    {
        fofo::Rng rng;
        rng.seed (0xC0FFEEu);
        fofo::Svf shape;
        shape.prepare (kSr);
        shape.set (fofo::Svf::Type::Lowpass, 4000.0f, 0.7f);
        juce::AudioBuffer<float> b (2, kBs);
        double acc = 0.0; int cnt = 0;
        const int blocks = 140;
        for (int k = 0; k < blocks; ++k)
        {
            for (int n = 0; n < kBs; ++n)
            {
                const float s = amp * 2.0f * shape.process (rng.bipolar());
                b.setSample (0, n, s); b.setSample (1, n, s);
            }
            e.process (b);
            if (k > blocks / 2) { acc += t::rms (b.getReadPointer (0), kBs); ++cnt; }
        }
        return 20.0 * std::log10 (acc / cnt + 1e-12);
    }

    // How long the wash rings on after the input stops.
    double tailSeconds (DaydreamEngine& e, double maxSec = 20.0)
    {
        juce::AudioBuffer<float> b (2, kBs);
        for (int k = 0; k < 120; ++k)
        {
            for (int n = 0; n < kBs; ++n)
            {
                const float s = 0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi
                                                         * 440.0 * (double) (k * kBs + n) / kSr);
                b.setSample (0, n, s); b.setSample (1, n, s);
            }
            e.process (b);
        }
        const double peak = t::rms (b.getReadPointer (0), kBs);
        if (peak < 1e-9) return 0.0;
        const double target = peak * std::pow (10.0, -40.0 / 20.0);
        const int maxBlocks = (int) (maxSec * kSr / kBs);
        for (int k = 0; k < maxBlocks; ++k)
        {
            b.clear();
            e.process (b);
            if (t::rms (b.getReadPointer (0), kBs) < target) return (double) (k * kBs) / kSr;
        }
        return maxSec;
    }
}

void runDaydreamTests()
{
    // ─────────────────────────────────────────────────────────────────────
    t::section ("DAYDREAM — the knob is a journey, and it is monotonic");
    {
        // Each stage has to arrive without a step, and the wash has to grow
        // steadily rather than jumping when a stage crosses over.
        double prev = -1e9, worstJump = 0.0, worstAt = 0.0;
        std::vector<double> tails;
        for (double k = 0.0; k <= 1.0001; k += 0.02)
        {
            DaydreamEngine e;
            e.prepare (spec());
            e.setDream01 ((float) k);
            settle (e);
            const double lvl = noiseLevelDb (e, 0.25f);
            if (prev > -1e8)
            {
                const double j = std::abs (lvl - prev);
                if (j > worstJump) { worstJump = j; worstAt = k; }
            }
            prev = lvl;
        }
        // Measured in 0.02 steps: a genuine discontinuity in the macro
        // mapping would show as a jump that does not shrink with step size.
        t::ok (worstJump < 1.5, "no level step anywhere along the sweep",
               t::fmt2 ("largest step %.2f dB at DREAM %.2f", worstJump, worstAt));

        for (double k : { 0.1, 0.5, 0.9 })
        {
            DaydreamEngine e;
            e.prepare (spec());
            e.setDream01 ((float) k);
            settle (e);
            tails.push_back (tailSeconds (e));
        }
        t::ok (tails[1] > tails[0] * 1.5 && tails[2] > tails[1] * 1.5,
               "the space grows monotonically across the three stages",
               t::fmt2 ("tail %.2f s / %.2f s", tails[0], tails[1]) + t::fmt (" / %.2f s", tails[2]));
        t::ok (tails[2] > 4.0, "and full dream is a genuinely long wash",
               t::fmt ("%.2f s", tails[2]));
    }

    // ─────────────────────────────────────────────────────────────────────
    t::section ("DAYDREAM — bottom of the knob is effectively clean");
    {
        DaydreamEngine e;
        e.prepare (spec());
        e.setDream01 (0.0f);
        settle (e);

        // The tape machine sits in the main path, so the output is the input
        // one transport-delay later. The reported figure is an integer (the
        // oversampler's is rounded up) and the machine's filters add their own
        // phase, so the true alignment at 440 Hz sits a few samples from it —
        // align on the best lag in a small window around it.
        const int lat = e.getLatencySamples();
        auto inAt = [] (long long g)
        {
            return g < 0 ? 0.0f : 0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi
                                                            * 440.0 * (double) g / kSr);
        };

        constexpr int kLags = 9;
        juce::AudioBuffer<float> b (2, kBs);
        double errAt[kLags] {}, outPeak = 0.0;
        for (int k = 0; k < 40; ++k)
        {
            for (int n = 0; n < kBs; ++n)
            {
                const float x = inAt ((long long) k * kBs + n);
                b.setSample (0, n, x); b.setSample (1, n, x);
            }
            e.process (b);
            if (k > 20)
                for (int n = 0; n < kBs; ++n)
                {
                    for (int l = 0; l < kLags; ++l)
                        errAt[l] = std::max (errAt[l], (double) std::abs (b.getSample (0, n)
                                                        - inAt ((long long) k * kBs + n - lat - (l - kLags / 2))));
                    outPeak = std::max (outPeak, (double) std::abs (b.getSample (0, n)));
                }
        }
        const double err = *std::min_element (errAt, errAt + kLags);
        t::ok (err < 0.02, "DREAM 0 is within a whisker of the input",
               t::fmt ("max |out-in| = %.4f", err));
        t::ok (outPeak > 0.45 && outPeak < 0.56, "at essentially unity gain",
               t::fmt ("peak %.3f vs input 0.5", outPeak));
    }

    // ─────────────────────────────────────────────────────────────────────
    t::section ("DAYDREAM — the wash ducks under playing");
    {
        // The trick that keeps high settings usable: the wash gets out of the
        // way while you play and swells back in the gaps.
        DaydreamEngine e;
        e.prepare (spec());
        e.setDream01 (1.0f);
        settle (e);

        juce::AudioBuffer<float> b (2, kBs);
        // Sustained loud playing.
        double playing = 0.0; int pc = 0;
        for (int k = 0; k < 200; ++k)
        {
            for (int n = 0; n < kBs; ++n)
            {
                const float s = 0.7f * (float) std::sin (2.0 * juce::MathConstants<double>::pi
                                                         * 330.0 * (double) (k * kBs + n) / kSr);
                b.setSample (0, n, s); b.setSample (1, n, s);
            }
            e.process (b);
            if (k > 150) { playing += t::rms (b.getReadPointer (0), kBs); ++pc; }
        }
        // Then the gap: the wash should swell rather than simply decay away.
        double firstGap = 0.0, laterGap = 0.0;
        for (int k = 0; k < 40; ++k)
        {
            b.clear();
            e.process (b);
            if (k < 4)  firstGap = std::max (firstGap, t::rms (b.getReadPointer (0), kBs));
            if (k >= 8 && k < 20) laterGap = std::max (laterGap, t::rms (b.getReadPointer (0), kBs));
        }
        t::ok (laterGap > firstGap * 1.05, "the wash swells back once the playing stops",
               t::fmt2 ("%.4f just after vs %.4f a moment later", firstGap, laterGap));
        juce::ignoreUnused (playing, pc);
    }

    // ─────────────────────────────────────────────────────────────────────
    t::section ("DAYDREAM — the wash never turns to mud");
    {
        // Per-band decay in the tank means the low end dies faster than the
        // mids. Without that, a 14-second decay is a swamp.
        // Measured at DREAM 0.5, below where the shimmer comes in. Above
        // that, an octave-up shimmer fills the tail with harmonics of
        // whatever you played, so a "110 Hz tail" measurement is really
        // measuring 220 and 440 Hz and says nothing about the low band.
        auto bandTail = [&] (double hz)
        {
            DaydreamEngine e;
            e.prepare (spec());
            e.setDream01 (0.5f);
            settle (e);
            juce::AudioBuffer<float> b (2, kBs);
            for (int k = 0; k < 120; ++k)
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
            const double target = peak * std::pow (10.0, -30.0 / 20.0);
            for (int k = 0; k < (int) (25.0 * kSr / kBs); ++k)
            {
                b.clear(); e.process (b);
                if (t::rms (b.getReadPointer (0), kBs) < target) return (double) (k * kBs) / kSr;
            }
            return 25.0;
        };
        const double low = bandTail (110.0), mid = bandTail (1000.0);
        t::ok (low < mid * 0.9, "low end decays faster than the mids",
               t::fmt2 ("110 Hz T30 %.2f s vs 1 kHz %.2f s", low, mid));
    }

    // ─────────────────────────────────────────────────────────────────────
    t::section ("DAYDREAM — stable across the knob, including the shimmer loop");
    {
        // The shimmer sits inside the tank's feedback. A pitch shifter in a
        // feedback path is the classic way to build an oscillator by accident,
        // so this one is band-limited and soft-clipped inside the loop.
        bool finite = true;
        double worst = 0.0;
        for (double k = 0.0; k <= 1.0001; k += 0.1)
        {
            DaydreamEngine e;
            e.prepare (spec());
            e.setDream01 ((float) k);
            juce::AudioBuffer<float> b (2, kBs);
            // 30 seconds at full tilt, so a slow runaway has time to show.
            for (int blk = 0; blk < (int) (30.0 * kSr / kBs); ++blk)
            {
                for (int n = 0; n < kBs; ++n)
                {
                    const double t = (double) (blk * kBs + n) / kSr;
                    const float s = (blk < 200)
                        ? 0.9f * (float) (std::sin (2.0 * juce::MathConstants<double>::pi * 220.0 * t)
                                        + 0.5 * std::sin (2.0 * juce::MathConstants<double>::pi * 3300.0 * t))
                        : 0.0f;
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
        t::ok (finite, "no setting produces a non-finite sample over 30 s");
        t::ok (worst < 6.0, "and the shimmer feedback loop never runs away",
               t::fmt ("worst peak %.2f", worst));
    }
}
