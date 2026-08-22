// DOUBLE 2 — is it a doubler now, or still a chorus?

#include "TestHarness.h"
#include <algorithm>
#include "Tests.h"
#include "double/Source/dsp/DoubleEngine.h"
#include "fofo/Pitch.h"

using dbl::DoubleEngine;

static constexpr double kSr = 48000.0;
static constexpr int    kBs = 512;

namespace
{
    juce::dsp::ProcessSpec spec() { return { kSr, (juce::uint32) kBs, 2 }; }

    // Pitch of the output over time, in cents relative to nominal, sampled
    // once per zero crossing. Pitch modulation is invisible to block RMS — a
    // frequency-modulated sine has constant amplitude — so this is the only
    // honest way to see whether the voices wander.
    std::vector<float> pitchTrace (DoubleEngine& e, double hz, int blocks)
    {
        juce::AudioBuffer<float> b (2, kBs);
        std::vector<float> out;
        out.reserve ((size_t) blocks * kBs);
        for (int k = 0; k < blocks; ++k)
        {
            for (int n = 0; n < kBs; ++n)
            {
                const float s = 0.3f * (float) std::sin (2.0 * juce::MathConstants<double>::pi
                                                         * hz * (double) (k * kBs + n) / kSr);
                b.setSample (0, n, s); b.setSample (1, n, s);
            }
            e.process (b);
            if (k > blocks / 8)
                for (int n = 0; n < kBs; ++n) out.push_back (b.getSample (0, n));
        }

        std::vector<double> cross;
        for (size_t n = 1; n < out.size(); ++n)
            if (out[n - 1] <= 0.0f && out[n] > 0.0f)
                cross.push_back ((double) (n - 1) + (double) (-out[n - 1]) / (out[n] - out[n - 1]));

        const double nominal = kSr / hz;
        std::vector<float> cents;
        for (size_t i = 1; i < cross.size(); ++i)
        {
            const double p = cross[i] - cross[i - 1];
            if (p > nominal * 0.5 && p < nominal * 2.0)
                cents.push_back ((float) (1200.0 * std::log2 (nominal / p)));
        }
        return cents;
    }

    // Heavily smoothed, so only sub-Hz wander survives and the fixed beating
    // between detuned voices is removed.
    double slowWander (const std::vector<float>& v, int win)
    {
        if ((int) v.size() <= win) return 0.0;
        std::vector<float> sm;
        for (size_t i = (size_t) win; i < v.size(); ++i)
        {
            double acc = 0.0;
            for (int j = 0; j < win; ++j) acc += v[i - (size_t) j];
            sm.push_back ((float) (acc / win));
        }
        return t::stdev (sm);
    }
}

void runDoubleTests()
{
    // ─────────────────────────────────────────────────────────────────────
    t::section ("fofo::PitchShifter — interpolation quality (F8)");
    {
        // The old shifter read its tap with linear interpolation, whose
        // moving read pointer imposes amplitude-modulated HF loss. Measure
        // what survives a unity-ratio pass: it should be very close to
        // transparent, because at ratio 1 the only thing happening is the
        // interpolated read and the grain crossfade.
        auto throughputDb = [&] (double hz)
        {
            fofo::PitchShifter p;
            p.prepare (kSr, 20.0f);
            p.setRatio (1.0f);
            return t::magnitudeDb ([&] (juce::AudioBuffer<float>& b, int n)
                                   { auto* d = b.getWritePointer (0);
                                     for (int i = 0; i < n; ++i) d[i] = p.process (d[i]); },
                                   hz, kSr, kBs, 60);
        };
        for (double f : { 1000.0, 6000.0, 12000.0 })
            t::ok (throughputDb (f) > -1.2,
                   t::fmt ("at ratio 1 the shifter is near-transparent at %.0f Hz", f),
                   t::fmt ("%.2f dB", throughputDb (f)));

        // And it shifts to the right place. Judge that by finding where the
        // energy actually peaks, not by sampling one exact frequency: a
        // two-tap crossfading shifter always spreads some energy into
        // sidebands at the crossfade rate, so no such shifter puts unity
        // magnitude on the nominal frequency.
        auto peakHz = [&] (float cents, float rangeMs, double inHz, double searchLo, double searchHi)
        {
            fofo::PitchShifter p;
            p.prepare (kSr, rangeMs);
            p.setRatio (fofo::centsToRatio (cents));

            const int total = (int) kSr * 3;
            std::vector<float> out ((size_t) total);
            for (int i = 0; i < total; ++i)
                out[(size_t) i] = p.process (0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * inHz * i / kSr));

            double bestMag = 0.0, bestHz = 0.0;
            for (double hz = searchLo; hz <= searchHi; hz += 0.25)
            {
                double re = 0.0, im = 0.0;
                for (int i = total / 2; i < total; ++i)
                {
                    const double ph = 2.0 * juce::MathConstants<double>::pi * hz * i / kSr;
                    re += out[(size_t) i] * std::cos (ph);
                    im += out[(size_t) i] * std::sin (ph);
                }
                const double m = 2.0 * std::sqrt (re * re + im * im) / (total / 2);
                if (m > bestMag) { bestMag = m; bestHz = hz; }
            }
            return std::make_pair (bestHz, bestMag);
        };

        // Micro-detune is what this catalogue actually uses, and it is exactly
        // what the previous construction could not do at all.
        for (float c : { -25.0f, -14.0f, -7.0f, 7.0f, 14.0f, 25.0f })
        {
            const double want = 440.0 * (double) fofo::centsToRatio (c);
            const auto [got, mag] = peakHz (c, 20.0f, 440.0, want - 12.0, want + 12.0);
            const double errCents = 1200.0 * std::log2 (got / want);
            t::ok (std::abs (errCents) < 2.0 && mag > 0.15,
                   t::fmt ("%+.0f cents lands within 2 cents of target", c),
                   t::fmt2 ("peak at %.1f Hz, %.1f cents off", got, errCents));
        }

        // Large shifts need a proportionally longer sweep, because the
        // crossfade sidebands sit at ±(1/period) and a short range puts them
        // close enough to swamp the carrier. At 20 ms an octave up peaks
        // 40 Hz low — exactly the crossfade rate; at 80 ms it is within 10 Hz.
        for (float c : { -1200.0f, -700.0f, 700.0f, 1200.0f })
        {
            const double want = 440.0 * (double) fofo::centsToRatio (c);
            const auto [got, mag] = peakHz (c, 80.0f, 440.0, want * 0.9, want * 1.1);
            const double errCents = 1200.0 * std::log2 (got / want);
            t::ok (std::abs (errCents) < 45.0 && mag > 0.15,
                   t::fmt ("%+.0f cents lands close with an 80 ms sweep", c),
                   t::fmt2 ("peak at %.1f Hz, %.1f cents off", got, errCents));
        }

        // The documented relationship between range, ratio and crossfade rate.
        {
            fofo::PitchShifter p;
            p.prepare (kSr, 20.0f);
            p.setRatio (fofo::centsToRatio (14.0f));
            const double expect = (20.0 * 0.001 * kSr) / std::abs ((double) fofo::centsToRatio (14.0f) - 1.0) / kSr;
            t::near (p.crossfadePeriodSeconds(), expect, 0.01,
                     "crossfade period follows range / |ratio-1|");
            t::ok (p.crossfadePeriodSeconds() > 2.0,
                   "so micro-detune hands over only every few seconds",
                   t::fmt ("%.2f s between crossfades", p.crossfadePeriodSeconds()));
        }
    }

    // ─────────────────────────────────────────────────────────────────────
    t::section ("DOUBLE — the voices actually wander (F2, structurally)");
    {
        auto wanderAt = [&] (float humanAmt)
        {
            DoubleEngine e;
            e.prepare (spec());
            e.setThick01 (0.3f);   // pair 1 only, so the trace is readable
            e.setWide01 (0.0f);
            e.setHuman01 (humanAmt);
            e.setMix01 (1.0f);
            return slowWander (pitchTrace (e, 440.0, 900), 400);
        };

        const double still  = wanderAt (0.0f);
        const double moving = wanderAt (1.0f);
        t::ok (moving > still * 3.0, "HUMAN 1 wanders far more than HUMAN 0",
               t::fmt2 ("slow pitch stdev %.3f cents vs %.3f", moving, still));
        t::ok (moving > 0.5, "and by a musically real amount",
               t::fmt ("%.3f cents of slow wander", moving));
    }

    // ─────────────────────────────────────────────────────────────────────
    t::section ("DOUBLE — THICK, WIDE and the dry path");
    {
        // THICK spreads the detune.
        auto spreadAt = [&] (float thick)
        {
            DoubleEngine e;
            e.prepare (spec());
            e.setThick01 (thick); e.setWide01 (0.0f); e.setHuman01 (0.0f); e.setMix01 (1.0f);
            const auto tr = pitchTrace (e, 440.0, 300);
            return (double) t::stdev (tr);
        };
        t::ok (spreadAt (1.0f) > spreadAt (0.05f) * 1.8, "THICK widens the detune spread",
               t::fmt2 ("pitch spread %.2f cents at THICK 1 vs %.2f at 0.05", spreadAt (1.0f), spreadAt (0.05f)));

        // THICK is continuous across noon — no step where voices 3 and 4 arrive.
        double prev = -1e9, worstJump = 0.0;
        for (double thick = 0.40; thick <= 0.62001; thick += 0.01)
        {
            DoubleEngine e;
            e.prepare (spec());
            e.setThick01 ((float) thick); e.setWide01 (0.7f); e.setHuman01 (0.0f); e.setMix01 (1.0f);
            juce::AudioBuffer<float> b (2, kBs);
            double acc = 0.0; int cnt = 0;
            for (int k = 0; k < 60; ++k)
            {
                for (int n = 0; n < kBs; ++n)
                {
                    const float s = 0.3f * (float) std::sin (2.0 * juce::MathConstants<double>::pi
                                                             * 220.0 * (double) (k * kBs + n) / kSr);
                    b.setSample (0, n, s); b.setSample (1, n, s);
                }
                e.process (b);
                if (k >= 40) { acc += t::rms (b.getReadPointer (0), kBs); ++cnt; }
            }
            const double lvl = 20.0 * std::log10 (acc / cnt + 1e-12);
            if (prev > -1e8) worstJump = std::max (worstJump, std::abs (lvl - prev));
            prev = lvl;
        }
        t::ok (worstJump < 0.5, "THICK has no step where voices 3 and 4 arrive",
               t::fmt ("largest step %.3f dB", worstJump));

        // WIDE spreads, and folds back to mono-safe at zero.
        auto widthOf = [&] (float wide)
        {
            DoubleEngine e;
            e.prepare (spec());
            e.setThick01 (0.8f); e.setWide01 (wide); e.setHuman01 (0.3f); e.setMix01 (1.0f);
            juce::AudioBuffer<float> b (2, kBs);
            double side = 0.0, mid = 0.0;
            for (int k = 0; k < 80; ++k)
            {
                for (int n = 0; n < kBs; ++n)
                {
                    const float s = 0.3f * (float) std::sin (2.0 * juce::MathConstants<double>::pi
                                                             * 330.0 * (double) (k * kBs + n) / kSr);
                    b.setSample (0, n, s); b.setSample (1, n, s);
                }
                e.process (b);
                if (k > 40)
                    for (int n = 0; n < kBs; ++n)
                    {
                        side += std::abs (b.getSample (0, n) - b.getSample (1, n));
                        mid  += std::abs (b.getSample (0, n) + b.getSample (1, n));
                    }
            }
            return side / (mid + 1e-12);
        };
        t::ok (widthOf (1.0f) > widthOf (0.0f) * 3.0, "WIDE spreads the voices",
               t::fmt2 ("side/mid %.4f at WIDE 1 vs %.4f at 0", widthOf (1.0f), widthOf (0.0f)));
        t::ok (widthOf (0.0f) < 0.02, "and folds to centre for mono safety at zero",
               t::fmt ("side/mid %.4f", widthOf (0.0f)));

        // MIX is additive: dry untouched, and no clipper on the sum.
        DoubleEngine e;
        e.prepare (spec());
        e.setThick01 (0.7f); e.setWide01 (0.7f); e.setHuman01 (0.5f); e.setMix01 (0.0f);
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
    }

    // ─────────────────────────────────────────────────────────────────────
    t::section ("DOUBLE — the wet bus never adds mud");
    {
        DoubleEngine e;
        e.prepare (spec());
        e.setThick01 (0.8f); e.setWide01 (0.7f); e.setHuman01 (0.5f); e.setMix01 (1.0f);
        e.setMode (DoubleEngine::Mode::Vox);

        // Compare a 60 Hz probe with and without the doubles: the wet bus is
        // high-passed at 160 Hz in Vox mode, so it must add essentially
        // nothing down there.
        auto lowAdded = [&] (float mix)
        {
            DoubleEngine d;
            d.prepare (spec());
            d.setThick01 (0.8f); d.setWide01 (0.7f); d.setHuman01 (0.0f); d.setMix01 (mix);
            return t::magnitudeDb ([&] (juce::AudioBuffer<float>& b, int n) { juce::ignoreUnused (n); d.process (b); },
                                   60.0, kSr, kBs, 60);
        };
        t::ok (std::abs (lowAdded (1.0f) - lowAdded (0.0f)) < 1.0,
               "doubles add no low end at 60 Hz",
               t::fmt2 ("%.2f dB at MIX 1 vs %.2f dB at MIX 0", lowAdded (1.0f), lowAdded (0.0f)));
    }

    // ─────────────────────────────────────────────────────────────────────
    t::section ("DOUBLE — stable and bounded");
    {
        bool finite = true;
        double worst = 0.0;
        for (auto m : { DoubleEngine::Mode::Vox, DoubleEngine::Mode::Strings, DoubleEngine::Mode::Synth })
        for (float th : { 0.0f, 1.0f })
        for (float wd : { 0.0f, 1.0f })
        for (float hu : { 0.0f, 1.0f })
        for (float mx : { 0.0f, 1.0f })
        {
            DoubleEngine e;
            e.prepare (spec());
            e.setMode (m); e.setThick01 (th); e.setWide01 (wd); e.setHuman01 (hu); e.setMix01 (mx);
            juce::AudioBuffer<float> b (2, kBs);
            for (int k = 0; k < 40; ++k)
            {
                for (int n = 0; n < kBs; ++n)
                {
                    const double t = (double) (k * kBs + n) / kSr;
                    const float s = 0.9f * (float) (std::sin (2.0 * juce::MathConstants<double>::pi * 140.0 * t)
                                                  + 0.5 * std::sin (2.0 * juce::MathConstants<double>::pi * 4300.0 * t));
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
        t::ok (worst < 5.0, "and nothing runs away", t::fmt ("worst peak %.2f", worst));
    }
}
