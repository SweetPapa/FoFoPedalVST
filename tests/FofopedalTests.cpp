// FOFOPEDAL — the three blocks rebuilt on the kernel, and the whole chain.

#include "TestHarness.h"
#include "Tests.h"
#include "fofopedal/Source/dsp/Space.h"
#include "fofopedal/Source/dsp/Pitch.h"
#include "fofopedal/Source/dsp/Mod.h"
#include "fofopedal/Source/dsp/FofoEngine.h"
#include "fofo/Pitch.h"

using namespace fofopedal;

static constexpr double kSr = 48000.0;
static constexpr int    kBs = 512;

namespace
{
    juce::dsp::ProcessSpec spec() { return { kSr, (juce::uint32) kBs, 2 }; }

    template <typename Block>
    std::vector<float> impulseOf (Block& b, int numSamples)
    {
        std::vector<float> ir;
        juce::AudioBuffer<float> buf (2, kBs);
        bool first = true;
        while ((int) ir.size() < numSamples)
        {
            buf.clear();
            if (first) { buf.setSample (0, 0, 1.0f); buf.setSample (1, 0, 1.0f); first = false; }
            b.process (buf);
            for (int n = 0; n < kBs && (int) ir.size() < numSamples; ++n)
                ir.push_back (buf.getSample (0, n));
        }
        return ir;
    }

    template <typename Block>
    double bandTail (Block& b, double hz, double maxSec = 20.0)
    {
        juce::AudioBuffer<float> buf (2, kBs);
        for (int k = 0; k < 140; ++k)
        {
            for (int n = 0; n < kBs; ++n)
            {
                const float s = 0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi
                                                         * hz * (double) (k * kBs + n) / kSr);
                buf.setSample (0, n, s); buf.setSample (1, n, s);
            }
            b.process (buf);
        }
        const double peak = t::rms (buf.getReadPointer (0), kBs);
        if (peak < 1e-9) return 0.0;
        const double target = peak * std::pow (10.0, -30.0 / 20.0);
        for (int k = 0; k < (int) (maxSec * kSr / kBs); ++k)
        {
            buf.clear(); b.process (buf);
            if (t::rms (buf.getReadPointer (0), kBs) < target) return (double) (k * kBs) / kSr;
        }
        return maxSec;
    }
}

void runFofopedalTests()
{
    // ─────────────────────────────────────────────────────────────────────
    t::section ("FOFOPEDAL/Space — a place, not just a tail");
    {
        Space sp;
        sp.prepare (spec());
        sp.setAlgo (Space::Algo::Room);
        sp.setSize01 (0.6f); sp.setMix01 (1.0f); sp.setPreDelayMs (10.0f); sp.setShimmer01 (0.0f);
        sp.reset();

        const auto ir = impulseOf (sp, (int) (0.5 * kSr));
        const int win = (int) (0.0008 * kSr);
        int arrivals = 0;
        for (int i = win; i < (int) (0.09 * kSr) - win; ++i)
        {
            const float v = std::abs (ir[(size_t) i]);
            if (v < 0.004f) continue;
            bool peak = true;
            for (int j = i - win; j <= i + win; ++j)
                if (j != i && std::abs (ir[(size_t) j]) > v) { peak = false; break; }
            if (peak) { ++arrivals; i += win; }
        }
        t::ok (arrivals >= 5, "Room has discrete early reflections",
               t::fmtI ("%d distinct arrivals in the first 90 ms", arrivals));

        // Per-band decay — the thing a single damping lowpass could never do.
        Space s2;
        s2.prepare (spec());
        s2.setAlgo (Space::Algo::Hall);
        s2.setSize01 (0.85f); s2.setMix01 (1.0f); s2.setShimmer01 (0.0f);
        s2.reset(); const double low = bandTail (s2, 120.0);
        s2.reset(); const double mid = bandTail (s2, 1000.0);
        t::ok (low < mid * 0.9, "Hall's low end decays faster than its mids",
               t::fmt2 ("120 Hz T30 %.2f s vs 1 kHz %.2f s", low, mid));

        // Plate must NOT have an early field: a plate goes dense immediately,
        // and giving it discrete reflections is how one ends up sounding like
        // a cheap room.
        auto earlyRatio = [&] (Space::Algo a)
        {
            Space s3;
            s3.prepare (spec());
            s3.setAlgo (a);
            s3.setSize01 (0.6f); s3.setMix01 (1.0f); s3.setPreDelayMs (10.0f); s3.setShimmer01 (0.0f);
            s3.reset();
            const auto r = impulseOf (s3, (int) (0.45 * kSr));
            auto energy = [&] (int from, int to)
            {
                double acc = 0.0;
                for (int i = from; i < to && i < (int) r.size(); ++i) acc += (double) r[(size_t) i] * r[(size_t) i];
                return 10.0 * std::log10 (acc + 1e-30);
            };
            return energy (0, (int) (0.09 * kSr)) - energy ((int) (0.09 * kSr), (int) (0.40 * kSr));
        };
        t::ok (earlyRatio (Space::Algo::Room) > earlyRatio (Space::Algo::Plate) + 3.0,
               "Room leads with its early field where Plate goes straight to dense",
               t::fmt2 ("early/late %.1f dB vs %.1f dB", earlyRatio (Space::Algo::Room), earlyRatio (Space::Algo::Plate)));
    }

    // ─────────────────────────────────────────────────────────────────────
    t::section ("FOFOPEDAL/Space — the shimmer loop stays bounded");
    {
        // A pitch shifter inside a feedback path is the classic way to build
        // an oscillator by accident. Run it at maximum for 30 seconds.
        Space sp;
        sp.prepare (spec());
        sp.setAlgo (Space::Algo::Shimmer);
        sp.setSize01 (1.0f); sp.setMix01 (1.0f); sp.setShimmer01 (1.0f);
        sp.reset();

        juce::AudioBuffer<float> b (2, kBs);
        bool finite = true;
        double worst = 0.0;
        for (int k = 0; k < (int) (30.0 * kSr / kBs); ++k)
        {
            for (int n = 0; n < kBs; ++n)
            {
                const double tt = (double) (k * kBs + n) / kSr;
                const float s = (k < 150) ? 0.8f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 220.0 * tt) : 0.0f;
                b.setSample (0, n, s); b.setSample (1, n, s);
            }
            sp.process (b);
            for (int ch = 0; ch < 2; ++ch)
                for (int n = 0; n < kBs; ++n)
                {
                    const float v = b.getSample (ch, n);
                    if (! std::isfinite (v)) finite = false;
                    worst = std::max (worst, (double) std::abs (v));
                }
        }
        t::ok (finite, "shimmer at maximum stays finite over 30 s");
        t::ok (worst < 5.0, "and never runs away", t::fmt ("worst peak %.2f", worst));
    }

    // ─────────────────────────────────────────────────────────────────────
    t::section ("FOFOPEDAL/Pitch — micro-detune actually detunes now (F8)");
    {
        // The old construction could not produce a small shift at all. Feed a
        // tone at full wet and look for the detuned partials either side.
        Pitch p;
        p.prepare (spec());
        p.setAlgo (Pitch::Algo::MicroDetune);
        p.setAmount01 (1.0f);          // ±18 cents
        p.setMix01 (1.0f);
        p.reset();

        const double inHz = 440.0;
        const int total = (int) kSr * 3;
        std::vector<float> out;
        out.reserve ((size_t) total);
        juce::AudioBuffer<float> b (2, kBs);
        for (int k = 0; k * kBs < total; ++k)
        {
            for (int n = 0; n < kBs; ++n)
            {
                const float s = 0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi
                                                         * inHz * (double) (k * kBs + n) / kSr);
                b.setSample (0, n, s); b.setSample (1, n, s);
            }
            p.process (b);
            for (int n = 0; n < kBs; ++n) out.push_back (b.getSample (0, n));
        }

        auto magAt = [&] (double hz)
        {
            double re = 0.0, im = 0.0;
            const int from = (int) out.size() / 2;
            for (int i = from; i < (int) out.size(); ++i)
            {
                const double ph = 2.0 * juce::MathConstants<double>::pi * hz * i / kSr;
                re += out[(size_t) i] * std::cos (ph);
                im += out[(size_t) i] * std::sin (ph);
            }
            return 2.0 * std::sqrt (re * re + im * im) / (double) ((int) out.size() - from);
        };

        const double atShift = magAt (inHz * (double) fofo::centsToRatio (-18.0f));
        const double atInput = magAt (inHz);
        t::ok (atShift > atInput * 0.5, "a detuned partial appears beside the input",
               t::fmt2 ("%.4f at -18 cents vs %.4f at the input", atShift, atInput));

        // And the octave algorithm lands an octave away.
        Pitch p2;
        p2.prepare (spec());
        p2.setAlgo (Pitch::Algo::OctaveHarm);
        p2.setShape01 (1.0f);          // up an octave
        p2.setMix01 (1.0f);
        p2.reset();
        std::vector<float> o2;
        for (int k = 0; k * kBs < total; ++k)
        {
            for (int n = 0; n < kBs; ++n)
            {
                const float s = 0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi
                                                         * inHz * (double) (k * kBs + n) / kSr);
                b.setSample (0, n, s); b.setSample (1, n, s);
            }
            p2.process (b);
            for (int n = 0; n < kBs; ++n) o2.push_back (b.getSample (0, n));
        }
        double bestMag = 0.0, bestHz = 0.0;
        for (double hz = 800.0; hz <= 960.0; hz += 0.5)
        {
            double re = 0.0, im = 0.0;
            const int from = (int) o2.size() / 2;
            for (int i = from; i < (int) o2.size(); ++i)
            {
                const double ph = 2.0 * juce::MathConstants<double>::pi * hz * i / kSr;
                re += o2[(size_t) i] * std::cos (ph);
                im += o2[(size_t) i] * std::sin (ph);
            }
            const double m = 2.0 * std::sqrt (re * re + im * im) / (double) ((int) o2.size() - from);
            if (m > bestMag) { bestMag = m; bestHz = hz; }
        }
        t::ok (std::abs (1200.0 * std::log2 (bestHz / 880.0)) < 45.0,
               "the octave-up algorithm lands an octave up",
               t::fmt ("strongest partial at %.1f Hz", bestHz));
    }

    // ─────────────────────────────────────────────────────────────────────
    t::section ("FOFOPEDAL/Mod — the phaser has real resonance now (F6)");
    {
        // v2's hand-rolled first-order allpass stages could sweep notches but
        // could not resonate. Measure the depth of the notches: with feedback
        // up, a resonant phaser cuts far harder than a plain one.
        // Judge by PEAK GAIN, not notch depth. Sweeping a coarse frequency
        // grid and taking max-minus-min mostly measures how close the grid
        // happened to land to a null, which is why a non-resonant phaser
        // scored 45 dB "depth" and a resonant one 21 dB. What actually
        // distinguishes them: a phaser without resonance is the sum of dry
        // and an allpass, so it can never exceed unity, while feedback
        // resonance pushes the peaks between the notches above 0 dB.
        auto peakGainDb = [&] (float feedback)
        {
            double best = -1e9;
            for (double f = 200.0; f <= 4000.0; f += 25.0)
            {
                Mod mm;
                mm.prepare (spec());
                mm.setAlgo (Mod::Algo::Phaser);
                mm.setRate01 (0.0f); mm.setDepth01 (0.0f); mm.setShape01 (0.5f);
                mm.setFeedback01 (feedback); mm.setMix01 (1.0f);
                mm.reset();
                best = std::max (best, t::magnitudeDb ([&] (juce::AudioBuffer<float>& b, int) { mm.process (b); },
                                                       f, kSr, kBs, 30));
            }
            return best;
        };

        const double flat = peakGainDb (0.0f);
        const double resonant = peakGainDb (1.0f);
        t::ok (resonant > flat + 2.0, "FEEDBACK lifts the peaks between the notches — the stages resonate",
               t::fmt2 ("peak gain %.1f dB at full feedback vs %.1f dB at none", resonant, flat));
        t::ok (flat < 0.5, "and with no feedback it stays at or below unity, as an allpass sum must",
               t::fmt ("%.2f dB", flat));
    }

    // ─────────────────────────────────────────────────────────────────────
    t::section ("FOFOPEDAL — the whole chain is stable and keeps its dry path");
    {
        FofoEngine e;
        e.prepare (spec());

        // Every algorithm combination at full tilt would be thousands of
        // cases; sweep each block through its algorithms with the rest hot.
        bool finite = true;
        double worst = 0.0;
        for (int d = 0; d < 3; ++d)
        for (int m = 0; m < 3; ++m)
        for (int pi = 0; pi < 3; ++pi)
        for (int sp = 0; sp < 4; ++sp)
        {
            FofoEngine en;
            en.prepare (spec());
            en.character().setAmount01 (0.8f);
            en.drive().setAlgo ((Drive::Algo) d);
            en.drive().setDrive01 (0.9f);
            en.drive().setMix01 (0.7f);
            en.mod().setAlgo ((Mod::Algo) m);
            en.mod().setDepth01 (1.0f); en.mod().setMix01 (0.6f); en.mod().setFeedback01 (0.9f);
            en.pitch().setAlgo ((Pitch::Algo) pi);
            en.pitch().setAmount01 (1.0f); en.pitch().setMix01 (0.6f);
            en.space().setAlgo ((Space::Algo) sp);
            en.space().setSize01 (1.0f); en.space().setMix01 (0.7f); en.space().setShimmer01 (0.8f);
            en.glue().setAmount01 (0.8f);

            juce::AudioBuffer<float> b (2, kBs);
            for (int k = 0; k < 90; ++k)
            {
                for (int n = 0; n < kBs; ++n)
                {
                    const double tt = (double) (k * kBs + n) / kSr;
                    const float s = 0.8f * (float) (std::sin (2.0 * juce::MathConstants<double>::pi * 150.0 * tt)
                                                  + 0.5 * std::sin (2.0 * juce::MathConstants<double>::pi * 3900.0 * tt));
                    b.setSample (0, n, s); b.setSample (1, n, s);
                }
                en.process (b);
                for (int ch = 0; ch < 2; ++ch)
                    for (int n = 0; n < kBs; ++n)
                    {
                        const float v = b.getSample (ch, n);
                        if (! std::isfinite (v)) finite = false;
                        worst = std::max (worst, (double) std::abs (v));
                    }
            }
        }
        t::ok (finite, "no combination of the four algorithm choices goes non-finite");
        t::ok (worst < 8.0, "and none of them runs away", t::fmt ("worst peak %.2f", worst));
    }
}
