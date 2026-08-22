// SWAY 2 — does the rebuild actually fix what was broken?
//
// The v1 engine failed on two structural counts (F4 and F5 in the audit), and
// both are things you can measure rather than argue about.

#include "TestHarness.h"
#include "Tests.h"
#include "sway/Source/dsp/SwayEngine.h"

using sway::SwayEngine;

static constexpr double kSr = 48000.0;
static constexpr int    kBs = 512;

namespace
{
    juce::dsp::ProcessSpec spec() { return { kSr, (juce::uint32) kBs, 2 }; }

    // Magnitude of a probe tone, optionally on top of a loud low-frequency bed.
    double probeDb (SwayEngine& e, double probeHz, double probeAmp,
                    double bedHz = 0.0, double bedAmp = 0.0, int blocks = 60)
    {
        juce::AudioBuffer<float> b (2, kBs);
        double re = 0.0, im = 0.0;
        const long long total = (long long) kBs * blocks;
        const long long skip  = total / 2;
        long long tt = 0;

        for (int k = 0; k < blocks; ++k)
        {
            for (int n = 0; n < kBs; ++n)
            {
                const double t = (double) (tt + n) / kSr;
                float s = (float) (probeAmp * std::sin (2.0 * juce::MathConstants<double>::pi * probeHz * t));
                if (bedAmp > 0.0)
                    s += (float) (bedAmp * std::sin (2.0 * juce::MathConstants<double>::pi * bedHz * t));
                b.setSample (0, n, s);
                b.setSample (1, n, s);
            }
            e.process (b);
            for (int n = 0; n < kBs; ++n)
            {
                const long long g = tt + n;
                if (g < skip) continue;
                const double ph = 2.0 * juce::MathConstants<double>::pi * probeHz * (double) g / kSr;
                re += b.getSample (0, n) * std::cos (ph);
                im += b.getSample (0, n) * std::sin (ph);
            }
            tt += kBs;
        }
        const double n = (double) (total - skip);
        return 20.0 * std::log10 (2.0 * std::sqrt (re * re + im * im) / n / probeAmp + 1e-12);
    }


    // Wow and flutter are PITCH modulation, so block RMS says nothing about
    // them — a frequency-modulated sine has constant amplitude. Measure the
    // thing itself: track the interval between interpolated zero crossings and
    // report how far the pitch wanders, in cents. That is also the unit a tape
    // spec is written in, so the numbers can be checked against real machines.
    double wanderCents (float move, float color, float mix, float rate = 0.5f)
    {
        SwayEngine e;
        e.prepare (spec());
        e.setMode (SwayEngine::Mode::Tape);
        e.setMove01 (move); e.setRate01 (rate); e.setColor01 (color); e.setMix01 (mix);

        const double hz = 1000.0;
        const int blocks = 700;                    // ~7.5 s, several wow cycles
        juce::AudioBuffer<float> b (2, kBs);
        std::vector<float> out;
        out.reserve ((size_t) blocks * kBs);
        for (int k = 0; k < blocks; ++k)
        {
            for (int n = 0; n < kBs; ++n)
            {
                const float sv = 0.3f * (float) std::sin (2.0 * juce::MathConstants<double>::pi
                                                          * hz * (double) (k * kBs + n) / kSr);
                b.setSample (0, n, sv); b.setSample (1, n, sv);
            }
            e.process (b);
            if (k > blocks / 8)
                for (int n = 0; n < kBs; ++n) out.push_back (b.getSample (0, n));
        }

        std::vector<double> crossings;
        for (size_t n = 1; n < out.size(); ++n)
            if (out[n - 1] <= 0.0f && out[n] > 0.0f)
                crossings.push_back ((double) (n - 1) + (double) (-out[n - 1]) / (out[n] - out[n - 1]));

        if (crossings.size() < 32) return 0.0;

        const double nominal = kSr / hz;
        std::vector<float> cents;
        for (size_t i = 1; i < crossings.size(); ++i)
        {
            const double period = crossings[i] - crossings[i - 1];
            if (period > nominal * 0.5 && period < nominal * 2.0)
                cents.push_back ((float) (1200.0 * std::log2 (nominal / period)));
        }
        return (double) t::stdev (cents);
    }
}

void runSwayTests()
{
    // ─────────────────────────────────────────────────────────────────────
    t::section ("SWAY/F4 — Tape MIX no longer combs");
    {
        // v1 crossfaded a ~10 ms modulated delay against undelayed dry, so at
        // any partial MIX the response was a comb with nulls every ~100 Hz.
        // v2 has no dry path in Tape at all.
        //
        // A comb and an EQ curve are both "not flat", so flatness is the wrong
        // test. What separates them is how FAST the response varies with
        // frequency: a 10 ms comb swings tens of dB between neighbouring
        // 25 Hz steps, while a head bump and a gap-loss rolloff are smooth.
        for (float mix : { 0.25f, 0.5f, 0.75f })
        {
            SwayEngine e;
            e.prepare (spec());
            e.setMode (SwayEngine::Mode::Tape);
            e.setMove01 (0.0f);          // no modulation: isolate the recombine
            e.setRate01 (0.35f);
            e.setColor01 (0.3f);
            e.setMix01 (mix);

            double prev = 1e9, worstStep = 0.0;
            for (double f = 1000.0; f <= 2000.0; f += 25.0)
            {
                e.reset();
                const double db = probeDb (e, f, 0.05, 0.0, 0.0, 40);
                if (prev < 1e8) worstStep = std::max (worstStep, std::abs (db - prev));
                prev = db;
            }
            t::ok (worstStep < 1.5,
                   "MIX " + t::fmt ("%.2f", mix) + ": response varies smoothly, no comb",
                   t::fmt ("largest step between 25 Hz-spaced probes %.2f dB", worstStep));
        }
    }

    // ─────────────────────────────────────────────────────────────────────
    t::section ("SWAY/F5 — there is actually tape in the tape mode");
    {
        // Self-erasure: tape loses its own highs as level rises. Probe with a
        // quiet 8 kHz tone, once alone and once riding a loud 150 Hz bed. On
        // v1 — a bare delay line — these were identical by construction.
        SwayEngine e;
        e.prepare (spec());
        e.setMode (SwayEngine::Mode::Tape);
        e.setMove01 (0.0f);
        e.setColor01 (0.7f);
        e.setMix01 (1.0f);

        e.reset();
        const double alone = probeDb (e, 8000.0, 0.03);
        e.reset();
        const double withBed = probeDb (e, 8000.0, 0.03, 150.0, 0.85);

        t::ok (withBed < alone - 1.0,
               "an 8 kHz probe loses level when a loud low bed is present (self-erasure)",
               t::fmt2 ("%.2f dB alone vs %.2f dB with bed", alone, withBed));

        // Head bump: a low resonance that grows as the deck gets more worn.
        auto lowDb = [&] (float color)
        {
            SwayEngine s;
            s.prepare (spec());
            s.setMode (SwayEngine::Mode::Tape);
            s.setMove01 (0.0f); s.setColor01 (color); s.setMix01 (1.0f);
            return probeDb (s, 70.0, 0.05);
        };
        const double clean = lowDb (0.0f), worn = lowDb (1.0f);
        t::ok (worn > clean + 1.0, "the head bump grows as COLOR wears the machine in",
               t::fmt2 ("70 Hz: %.2f dB pristine vs %.2f dB worn", clean, worn));

        // Gap loss: highs close in as the machine wears.
        auto highDb = [&] (float color)
        {
            SwayEngine s;
            s.prepare (spec());
            s.setMode (SwayEngine::Mode::Tape);
            s.setMove01 (0.0f); s.setColor01 (color); s.setMix01 (1.0f);
            return probeDb (s, 12000.0, 0.05);
        };
        t::ok (highDb (1.0f) < highDb (0.0f) - 6.0, "and the highs roll off with it",
               t::fmt2 ("12 kHz: %.2f dB pristine vs %.2f dB worn", highDb (0.0f), highDb (1.0f)));

        // Saturation is program-dependent: a hot input compresses.
        SwayEngine s2;
        s2.prepare (spec());
        s2.setMode (SwayEngine::Mode::Tape);
        s2.setMove01 (0.0f); s2.setColor01 (1.0f); s2.setMix01 (1.0f);
        s2.reset(); const double quiet = probeDb (s2, 400.0, 0.05);
        s2.reset(); const double loud  = probeDb (s2, 400.0, 0.90);
        t::ok (loud < quiet - 1.5, "and a hot input is compressed relative to a quiet one",
               t::fmt2 ("400 Hz gain: %.2f dB quiet vs %.2f dB loud", quiet, loud));
    }

    // ─────────────────────────────────────────────────────────────────────
    t::section ("SWAY — MOVE actually moves, and MIX scales it");
    {
        // Wow and flutter are PITCH modulation, so block RMS says nothing
        // about them — a frequency-modulated sine has constant amplitude.
        // Measure the thing itself: track the period between interpolated
        // zero crossings and report how far the pitch wanders, in cents.
        // This is also the unit a tape spec is written in.
        const double still = wanderCents (0.0f, 0.2f, 1.0f);
        const double full  = wanderCents (1.0f, 0.2f, 1.0f);
        const double half  = wanderCents (1.0f, 0.2f, 0.5f);

        t::ok (full > 3.0, "MOVE at 1 wanders the pitch by a musically real amount",
               t::fmt ("%.2f cents RMS", full));
        t::ok (full > still * 4.0, "and MOVE at 0 holds pitch steady",
               t::fmt2 ("%.2f cents at MOVE 1 vs %.2f at MOVE 0", full, still));
        t::ok (half < full * 0.75 && half > still * 2.0,
               "MIX scales the movement rather than blending it away",
               t::fmt2 ("MIX 0.5 gives %.2f cents vs %.2f at full", half, full));

        // Pin the voicing. Before calibration every COLOR setting landed
        // between 36 and 43 cents — seasick, and COLOR barely changed the
        // amount, so its "serviced deck to dying cassette" axis did nothing.
        const double serviced = wanderCents (1.0f, 0.0f, 1.0f);
        const double dying    = wanderCents (1.0f, 1.0f, 1.0f);
        const double dflt     = wanderCents (0.45f, 0.5f, 1.0f);

        t::ok (serviced > 2.0 && serviced < 4.5,
               "COLOR 0 at full MOVE is a serviced deck (~3 cents)", t::fmt ("%.2f cents", serviced));
        t::ok (dying > 15.0 && dying < 26.0,
               "COLOR 1 at full MOVE is a cassette on its way out (~20 cents)", t::fmt ("%.2f cents", dying));
        t::ok (dflt > 3.0 && dflt < 6.5,
               "and the shipped default is audible without being seasick", t::fmt ("%.2f cents", dflt));
        t::ok (dying > serviced * 4.0, "COLOR genuinely swings the amount of wear",
               t::fmt2 ("%.2f cents vs %.2f", dying, serviced));
    }

    // ─────────────────────────────────────────────────────────────────────
    t::section ("SWAY — latency is the same in every mode");
    {
        // A host compensates once and does not re-query on a parameter change,
        // so a mode-dependent latency would shift the track when the mode
        // changed. Tape genuinely has transport delay; the others are padded.
        SwayEngine e;
        e.prepare (spec());
        const int lat = e.getLatencySamples();
        t::ok (lat > 0, "Tape's transport delay is reported rather than hidden",
               t::fmtI ("%d samples", lat));

        for (auto m : { SwayEngine::Mode::Tape, SwayEngine::Mode::Ensemble, SwayEngine::Mode::Pump })
        {
            e.setMode (m);
            t::ok (e.getLatencySamples() == lat, "every mode reports the same latency");
        }

        // And the padding is real: an impulse should emerge at the same offset
        // in Pump as the reported figure.
        e.setMode (SwayEngine::Mode::Pump);
        e.setMove01 (0.0f); e.setMix01 (0.0f); e.setColor01 (0.0f);
        e.reset();
        juce::AudioBuffer<float> b (2, kBs);
        b.clear();
        b.setSample (0, 0, 1.0f); b.setSample (1, 0, 1.0f);
        e.process (b);
        int firstNonZero = -1;
        for (int n = 0; n < kBs; ++n)
            if (std::abs (b.getSample (0, n)) > 0.05f) { firstNonZero = n; break; }
        t::ok (firstNonZero >= lat - 2 && firstNonZero <= lat + 2,
               "Pump's output is padded to match, so switching modes doesn't shift the track",
               t::fmtI ("impulse arrived at sample %d", firstNonZero));
    }

    // ─────────────────────────────────────────────────────────────────────
    t::section ("SWAY — Ensemble keeps a clean dry path");
    {
        SwayEngine e;
        e.prepare (spec());
        e.setMode (SwayEngine::Mode::Ensemble);
        e.setMove01 (0.8f); e.setRate01 (0.4f); e.setColor01 (0.6f); e.setMix01 (0.0f);

        juce::AudioBuffer<float> b (2, kBs);
        double worstErr = 0.0, outPeak = 0.0;
        for (int k = 0; k < 20; ++k)
        {
            std::vector<float> in ((size_t) kBs);
            for (int n = 0; n < kBs; ++n)
            {
                in[(size_t) n] = 1.4f * (float) std::sin (2.0 * juce::MathConstants<double>::pi
                                                          * 220.0 * (double) (k * kBs + n) / kSr);
                b.setSample (0, n, in[(size_t) n]); b.setSample (1, n, in[(size_t) n]);
            }
            e.process (b);
            // The whole output is padded by the reported latency, so compare
            // against the input delayed by the same amount.
            for (int n = e.getLatencySamples(); n < kBs; ++n)
            {
                const float want = in[(size_t) (n - e.getLatencySamples())];
                worstErr = std::max (worstErr, (double) std::abs (b.getSample (0, n) - want));
                outPeak  = std::max (outPeak, (double) std::abs (b.getSample (0, n)));
            }
        }
        t::ok (worstErr < 2.0e-3, "MIX 0 passes a 1.4-peak input through unchanged",
               t::fmt ("max |out-in| = %.3e", worstErr));
        t::ok (outPeak > 1.35, "and nothing clips it", t::fmt ("peak %.3f", outPeak));
    }

    // ─────────────────────────────────────────────────────────────────────
    t::section ("SWAY — Pump breathes rather than just chopping");
    {
        auto hfSwing = [&] (float color)
        {
            SwayEngine e;
            e.prepare (spec());
            e.setMode (SwayEngine::Mode::Pump);
            e.setMove01 (0.9f); e.setRate01 (0.55f); e.setColor01 (color); e.setMix01 (1.0f);

            // Feed high-frequency content and watch how much of it survives
            // across the cycle beyond what the gain alone would explain.
            juce::AudioBuffer<float> b (2, kBs);
            std::vector<float> hi, lo;
            for (int k = 0; k < 200; ++k)
            {
                for (int n = 0; n < kBs; ++n)
                {
                    const double t = (double) (k * kBs + n) / kSr;
                    const float s = 0.3f * (float) (std::sin (2.0 * juce::MathConstants<double>::pi * 200.0 * t)
                                                  + std::sin (2.0 * juce::MathConstants<double>::pi * 9000.0 * t));
                    b.setSample (0, n, s); b.setSample (1, n, s);
                }
                e.process (b);
                if (k > 60)
                {
                    // crude HF/LF split by first difference
                    double h = 0.0, l = 0.0;
                    const auto* d = b.getReadPointer (0);
                    for (int n = 1; n < kBs; ++n) { const double df = d[n] - d[n - 1]; h += df * df; l += (double) d[n] * d[n]; }
                    hi.push_back ((float) std::sqrt (h / kBs));
                    lo.push_back ((float) std::sqrt (l / kBs));
                }
            }
            // How much the HF/LF ratio itself moves — gain alone would keep it
            // constant, so anything here is the filter breathing.
            std::vector<float> ratio;
            for (size_t i = 0; i < hi.size(); ++i) ratio.push_back (hi[i] / (lo[i] + 1e-9f));
            return t::stdev (ratio);
        };

        const double flat  = hfSwing (0.0f);
        const double moved = hfSwing (1.0f);
        t::ok (moved > flat * 2.0,
               "at COLOR 1 the tone moves with the cycle, not just the level",
               t::fmt2 ("HF/LF ratio stdev %.5f vs %.5f at COLOR 0", moved, flat));
    }

    // ─────────────────────────────────────────────────────────────────────
    t::section ("SWAY — stable across the parameter space");
    {
        // Every mode, corners of the knob space, hot input. Nothing may go
        // non-finite or run away.
        bool allFinite = true;
        double worstPeak = 0.0;
        const char* worstAt = "";

        for (auto m : { SwayEngine::Mode::Tape, SwayEngine::Mode::Ensemble, SwayEngine::Mode::Pump })
        for (float mv : { 0.0f, 1.0f })
        for (float rt : { 0.0f, 1.0f })
        for (float cl : { 0.0f, 1.0f })
        for (float mx : { 0.0f, 1.0f })
        {
            SwayEngine e;
            e.prepare (spec());
            e.setMode (m); e.setMove01 (mv); e.setRate01 (rt); e.setColor01 (cl); e.setMix01 (mx);

            juce::AudioBuffer<float> b (2, kBs);
            for (int k = 0; k < 40; ++k)
            {
                for (int n = 0; n < kBs; ++n)
                {
                    const double t = (double) (k * kBs + n) / kSr;
                    const float s = 0.95f * (float) (std::sin (2.0 * juce::MathConstants<double>::pi * 110.0 * t)
                                                   + 0.5 * std::sin (2.0 * juce::MathConstants<double>::pi * 3700.0 * t));
                    b.setSample (0, n, s); b.setSample (1, n, s);
                }
                e.process (b);
                for (int ch = 0; ch < 2; ++ch)
                    for (int n = 0; n < kBs; ++n)
                    {
                        const float v = b.getSample (ch, n);
                        if (! std::isfinite (v)) allFinite = false;
                        if (std::abs (v) > worstPeak) { worstPeak = std::abs (v); worstAt = "corner"; }
                    }
            }
        }
        juce::ignoreUnused (worstAt);
        t::ok (allFinite, "no mode produces a non-finite sample at any knob corner");
        t::ok (worstPeak < 6.0, "and nothing runs away on a 1.4-peak input",
               t::fmt ("worst output peak %.3f", worstPeak));
    }
}
