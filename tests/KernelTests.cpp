#include "TestHarness.h"
#include "Tests.h"
#include "fofo/Fofo.h"

using namespace fofo;

static constexpr double kSr = 48000.0;
static constexpr int    kBs = 512;

// A wet node that is a pure N-sample delay and declares it. Stands in for
// anything with latency — an oversampler, a lookahead, an FFT frame.
class LatencyNode : public Node
{
public:
    explicit LatencyNode (int samples) : lat (samples) {}

    void prepare (const Spec& s) override
    {
        lines.resize ((size_t) s.numChannels);
        for (auto& l : lines) l.prepare (s.sampleRate, (float) lat + 8.0f);
    }
    void reset() override { for (auto& l : lines) l.reset(); }
    void process (juce::AudioBuffer<float>& b, int n) noexcept override
    {
        if (lat <= 0) return;
        const int nCh = juce::jmin (b.getNumChannels(), (int) lines.size());
        for (int ch = 0; ch < nCh; ++ch)
        {
            auto* d = b.getWritePointer (ch);
            for (int i = 0; i < n; ++i) d[i] = lines[(size_t) ch].processSample (d[i], (float) lat);
        }
    }
    int latencySamples() const noexcept override { return lat; }

private:
    int lat;
    std::vector<DelayLine> lines;
};

void runKernelTests()
{
    const Spec spec { kSr, kBs, 2 };

    // ─────────────────────────────────────────────────────────────────────
    t::section ("fofo::Chain — identity");
    {
        Chain c;
        c.prepare (spec);

        juce::AudioBuffer<float> b (2, kBs);
        std::vector<float> in ((size_t) kBs);
        for (int n = 0; n < kBs; ++n)
        {
            in[(size_t) n] = 1.7f * std::sin (0.031f * (float) n) + 0.4f * std::sin (0.31f * (float) n);
            b.setSample (0, n, in[(size_t) n]);
            b.setSample (1, n, in[(size_t) n]);
        }
        c.process (b, kBs);

        double worst = 0.0;
        for (int n = 0; n < kBs; ++n)
            worst = std::max (worst, (double) std::abs (b.getSample (0, n) - in[(size_t) n]));

        t::ok (worst == 0.0, "empty chain is bit-exact", t::fmt ("max |out-in| = %.3e", worst));
        t::ok (c.latencySamples() == 0, "empty chain reports zero latency");
    }

    // ─────────────────────────────────────────────────────────────────────
    t::section ("fofo::Parallel — latency compensation (the F1 class of bug)");
    {
        // A 5-sample wet latency at 48 kHz combs at ~4.8 kHz if uncompensated.
        for (int lat : { 3, 5, 11 })
        {
            Parallel p (std::make_unique<LatencyNode> (lat), MixRule::Blend, 0.5f);
            p.prepare (spec);

            double best = -1e9, worst = 1e9;
            for (double f : { 200.0, 1000.0, 2000.0, 3000.0, 4000.0, 4800.0, 6000.0, 8000.0, 11000.0 })
            {
                p.reset();
                const double db = t::magnitudeDb ([&] (juce::AudioBuffer<float>& b, int n) { p.process (b, n); },
                                                  f, kSr, kBs, 40);
                best  = std::max (best, db);
                worst = std::min (worst, db);
            }
            t::ok (best - worst < 0.5,
                   "50% blend around a " + std::to_string (lat) + "-sample wet branch is flat",
                   t::fmt ("ripple %.3f dB", best - worst));
            t::ok (p.latencySamples() == lat,
                   "Parallel reports the wet branch's latency (" + std::to_string (lat) + ")");
        }
    }

    // ─────────────────────────────────────────────────────────────────────
    t::section ("fofo::Parallel — mix rules");
    {
        auto runRule = [&] (MixRule rule, float mix, float dryIn)
        {
            // Wet branch doubles the signal, so wet = 2x dry and the rules are
            // distinguishable from the output level alone.
            Parallel p (makeFnNode ([] (juce::AudioBuffer<float>& b, int n)
                                    { for (int c = 0; c < b.getNumChannels(); ++c)
                                          juce::FloatVectorOperations::multiply (b.getWritePointer (c), 2.0f, n); }),
                        rule, mix);
            p.prepare (spec);
            juce::AudioBuffer<float> b (2, 64);
            for (int n = 0; n < 64; ++n) { b.setSample (0, n, dryIn); b.setSample (1, n, dryIn); }
            p.process (b, 64);
            return (double) b.getSample (0, 32);
        };

        t::near (runRule (MixRule::Blend, 0.0f, 1.0f), 1.0, 1e-6, "Blend at 0 = dry");
        t::near (runRule (MixRule::Blend, 1.0f, 1.0f), 2.0, 1e-6, "Blend at 1 = wet");
        t::near (runRule (MixRule::Blend, 0.5f, 1.0f), 1.5, 1e-6, "Blend at 0.5 = half");

        t::near (runRule (MixRule::Additive, 0.0f, 1.0f), 1.0, 1e-6, "Additive at 0 leaves dry untouched");
        t::near (runRule (MixRule::Additive, 1.0f, 1.0f), 3.0, 1e-6, "Additive at 1 = dry + wet");

        t::near (runRule (MixRule::Ducked, 0.70f, 1.0f), 3.0, 1e-6, "Ducked reaches full wet at 0.70 with dry still up");
        t::near (runRule (MixRule::Ducked, 1.00f, 1.0f), 2.0, 1e-6, "Ducked at 1 has dry fully down");
        t::near (runRule (MixRule::Ducked, 0.35f, 1.0f), 2.0, 1e-6, "Ducked at 0.35 = dry + half wet");
    }

    // ─────────────────────────────────────────────────────────────────────
    t::section ("fofo::Svf — responses");
    {
        auto sweep = [&] (Svf::Type type, float fc, float q, float db, double atHz)
        {
            Svf f; f.prepare (kSr); f.set (type, fc, q, db);
            return t::magnitudeDb ([&] (juce::AudioBuffer<float>& b, int n)
                                   { auto* d = b.getWritePointer (0);
                                     for (int i = 0; i < n; ++i) d[i] = f.process (d[i]); },
                                   atHz, kSr, kBs, 40);
        };

        // Butterworth Q: -3 dB at cutoff for both LP and HP.
        t::near (sweep (Svf::Type::Lowpass,  1000.0f, 0.70710678f, 0.0f, 1000.0), -3.0, 0.25, "LP is -3 dB at cutoff (Q=0.707)");
        t::near (sweep (Svf::Type::Highpass, 1000.0f, 0.70710678f, 0.0f, 1000.0), -3.0, 0.25, "HP is -3 dB at cutoff (Q=0.707)");
        // 12 dB/oct rolloff: an octave above cutoff the LP is ~-12 dB.
        t::near (sweep (Svf::Type::Lowpass,  1000.0f, 0.70710678f, 0.0f, 2000.0), -12.3, 0.6, "LP rolls off 12 dB/oct");
        // Resonance actually resonates — the thing the old toolkit could not do.
        t::near (sweep (Svf::Type::Lowpass,  1000.0f, 8.0f, 0.0f, 1000.0), 18.06, 0.5, "LP at Q=8 peaks +18 dB (real resonance)");
        // The classic SVF bandpass tap peaks at Q — that is the definition,
        // not a bug, and it is what makes it useful as a resonator.
        t::near (sweep (Svf::Type::Bandpass, 1000.0f, 2.0f, 0.0f, 1000.0), 6.02, 0.25, "BP tap peaks at Q (+6 dB at Q=2)");
        t::near (sweep (Svf::Type::BandpassUnity, 1000.0f, 2.0f, 0.0f, 1000.0), 0.0, 0.25, "BandpassUnity is 0 dB at centre");
        // Bell and shelves hit their nominal gain.
        t::near (sweep (Svf::Type::Bell, 1000.0f, 1.0f, 6.0f, 1000.0), 6.0, 0.3, "Bell hits +6 dB at centre");
        t::near (sweep (Svf::Type::LowShelf,  1000.0f, 0.70710678f, 6.0f, 60.0),   6.0, 0.4, "LowShelf hits +6 dB well below corner");
        t::near (sweep (Svf::Type::HighShelf, 1000.0f, 0.70710678f, 6.0f, 16000.0), 6.0, 0.4, "HighShelf hits +6 dB well above corner");
        // Allpass passes everything at unity.
        t::near (sweep (Svf::Type::Allpass, 1000.0f, 0.70710678f, 0.0f, 400.0), 0.0, 0.2, "Allpass is unity in magnitude");
    }

    // ─────────────────────────────────────────────────────────────────────
    t::section ("fofo::Svf — stability under audio-rate modulation");
    {
        // Sweep the cutoff at 800 Hz across four octaves with high resonance.
        // A biquad with recomputed coefficients misbehaves badly here; the TPT
        // form should stay bounded, which is why the kernel is built on it.
        Svf f; f.prepare (kSr); f.setType (Svf::Type::Lowpass); f.setResonance (6.0f);
        double worstPeak = 0.0;
        bool finite = true;
        for (int n = 0; n < (int) kSr * 2; ++n)
        {
            const float mod = std::sin (2.0f * juce::MathConstants<float>::pi * 800.0f * (float) n / (float) kSr);
            const float fc  = 1000.0f * std::pow (2.0f, mod * 2.0f);
            const float x   = 0.5f * std::sin (2.0f * juce::MathConstants<float>::pi * 220.0f * (float) n / (float) kSr);
            const float y   = f.processModulated (x, fc);
            if (! std::isfinite (y)) { finite = false; break; }
            worstPeak = std::max (worstPeak, (double) std::abs (y));
        }
        t::ok (finite, "stays finite while the cutoff is swept at 800 Hz, Q=6");
        t::ok (finite && worstPeak < 20.0, "stays bounded under that sweep", t::fmt ("peak %.2f", worstPeak));
    }

    // ─────────────────────────────────────────────────────────────────────
    t::section ("fofo::DelayLine — interpolation quality (the F8 class of bug)");
    {
        // Integer delays must be exact.
        DelayLine dl; dl.prepare (kSr, 256.0f);
        std::vector<float> in (2000);
        for (size_t i = 0; i < in.size(); ++i) in[i] = std::sin (0.1f * (float) i);
        double worst = 0.0;
        for (size_t i = 0; i < in.size(); ++i)
        {
            const float y = dl.processSample (in[i], 10.0f);
            if (i >= 10) worst = std::max (worst, (double) std::abs (y - in[i - 10]));
        }
        t::ok (worst < 1e-6, "integer delay is exact", t::fmt ("max err %.3e", worst));

        // Half-sample delay: measure HF loss at 6 kHz (Nyquist/4) and compare
        // against what linear interpolation would have cost.
        auto lossAt = [&] (double hz)
        {
            DelayLine d; d.prepare (kSr, 64.0f);
            return t::magnitudeDb ([&] (juce::AudioBuffer<float>& b, int n)
                                   { auto* p = b.getWritePointer (0);
                                     for (int i = 0; i < n; ++i) p[i] = d.processSample (p[i], 10.5f); },
                                   hz, kSr, kBs, 40);
        };
        // At a half-sample offset Catmull-Rom collapses to the 4-tap kernel
        // [-1/16, 9/16, 9/16, -1/16], so its response is known in closed form.
        // Checking against that verifies the interpolator is what it claims to
        // be, rather than merely landing under some threshold.
        auto hermiteTheory = [] (double f, double sr)
        {
            const double w = 2.0 * juce::MathConstants<double>::pi * f / sr;
            return 20.0 * std::log10 (std::abs (2.0 * 0.5625 * std::cos (w * 0.5)
                                              - 2.0 * 0.0625 * std::cos (w * 1.5)));
        };
        auto linearTheory = [] (double f, double sr)
        {
            return 20.0 * std::log10 (std::abs (std::cos (juce::MathConstants<double>::pi * f / sr)));
        };

        for (double f : { 6000.0, 16000.0 })
            t::near (lossAt (f), hermiteTheory (f, kSr), 0.05,
                     t::fmt ("half-sample read matches Catmull-Rom theory at %.0f Hz", f));

        t::ok (lossAt (16000.0) > linearTheory (16000.0, kSr) + 2.5,
               "and beats linear interpolation by >2.5 dB at 16 kHz",
               t::fmt2 ("Hermite %.3f dB vs linear %.3f dB", lossAt (16000.0), linearTheory (16000.0, kSr)));
    }

    // ─────────────────────────────────────────────────────────────────────
    t::section ("fofo::ModMatrix — control rate (the F2 class of bug)");
    {
        // The regression guard that matters: a Drift prepared through the
        // matrix must actually wander. If someone ever reintroduces a
        // block-rate tick, its variance collapses and this fails.
        ModMatrix m;
        auto drift = m.addSource (std::make_unique<Drift> (0.3f, 12345u));
        auto dest  = m.addDest ("detune", 0.0f, -50.0f, 50.0f);
        m.connect (drift, dest, 10.0f);
        m.prepare (spec);

        std::vector<float> trace;
        const int seconds = 20;
        for (int n = 0; n < (int) kSr * seconds; ++n)
        {
            m.tick (0.0f);
            if (n % 64 == 0) trace.push_back (m.get (dest));
        }
        const double sd = t::stdev (trace);
        t::ok (sd > 1.5, "a 0.3 Hz drift routed at depth 10 actually wanders",
               t::fmt ("stdev %.3f (want > 1.5)", sd));

        double lo = 1e9, hi = -1e9;
        for (float v : trace) { lo = std::min (lo, (double) v); hi = std::max (hi, (double) v); }
        t::ok (hi - lo > 6.0, "and covers a useful range", t::fmt2 ("span %.2f .. %.2f", lo, hi));
        t::ok (lo >= -50.0 && hi <= 50.0, "while staying inside the destination's bounds");
    }

    // ─────────────────────────────────────────────────────────────────────
    t::section ("fofo::ModMatrix — routing and smoothing");
    {
        ModMatrix m;
        auto lfo  = m.addSource (std::make_unique<Lfo> (Lfo::Shape::Sine, 2.0f));
        auto dest = m.addDest ("cutoff", 1000.0f, 0.0f, 5000.0f);
        m.connect (lfo, dest, 500.0f);
        m.prepare (spec);

        double lo = 1e9, hi = -1e9;
        float prev = m.get (dest);
        double worstJump = 0.0;
        for (int n = 0; n < (int) kSr; ++n)
        {
            m.tick (0.0f);
            const float v = m.get (dest);
            worstJump = std::max (worstJump, (double) std::abs (v - prev));
            prev = v;
            lo = std::min (lo, (double) v);
            hi = std::max (hi, (double) v);
        }
        t::near (lo, 500.0, 30.0, "routed LFO reaches base - depth");
        t::near (hi, 1500.0, 30.0, "routed LFO reaches base + depth");
        t::ok (worstJump < 2.0, "destination is ramped, not stepped",
               t::fmt ("largest per-sample step %.4f", worstJump));

        // Base changes must move the centre, not the depth.
        m.setBase (dest, 2000.0f);
        for (int n = 0; n < (int) kSr / 2; ++n) m.tick (0.0f);
        lo = 1e9; hi = -1e9;
        for (int n = 0; n < (int) kSr; ++n) { m.tick (0.0f); const float v = m.get (dest); lo = std::min (lo, (double) v); hi = std::max (hi, (double) v); }
        t::near (0.5 * (lo + hi), 2000.0, 40.0, "setBase moves the modulation centre");
    }

    // ─────────────────────────────────────────────────────────────────────
    t::section ("fofo::Lfo — rate accuracy and shapes");
    {
        for (double hz : { 0.5, 2.0, 7.0 })
        {
            ModMatrix m;
            auto id = m.addSource (std::make_unique<Lfo> (Lfo::Shape::Sine, (float) hz));
            auto d  = m.addDest ("x", 0.0f, -2.0f, 2.0f);
            m.connect (id, d, 1.0f);
            m.prepare (spec);

            int crossings = 0;
            float prev = m.get (d);
            const int n = (int) kSr * 4;
            for (int i = 0; i < n; ++i)
            {
                m.tick (0.0f);
                const float v = m.get (d);
                if (prev <= 0.0f && v > 0.0f) ++crossings;
                prev = v;
            }
            const double measured = crossings / 4.0;
            t::near (measured, hz, hz * 0.05 + 0.05, t::fmt ("LFO at %.1f Hz runs at the requested rate", hz));
        }

        // An LFO with no drift requested must be perfectly periodic — the old
        // DriftLFO added 25% noise to every output unconditionally.
        ModMatrix m;
        auto id = m.addSource (std::make_unique<Lfo> (Lfo::Shape::Sine, 1.0f));
        auto d  = m.addDest ("x", 0.0f, -2.0f, 2.0f);
        m.connect (id, d, 1.0f);
        m.prepare (spec);
        std::vector<float> cycle1, cycle2;
        for (int i = 0; i < (int) kSr; ++i) { m.tick (0.0f); cycle1.push_back (m.get (d)); }
        for (int i = 0; i < (int) kSr; ++i) { m.tick (0.0f); cycle2.push_back (m.get (d)); }
        double worst = 0.0;
        for (size_t i = 0; i < cycle1.size(); ++i)
            worst = std::max (worst, (double) std::abs (cycle1[i] - cycle2[i]));
        // The old DriftLFO added 25% filtered noise to every LFO output
        // unconditionally; that would show here as a difference of order 0.25.
        t::ok (worst < 0.01, "a drift-free LFO has no noise baked into it",
               t::fmt ("max cycle-to-cycle diff %.3e", worst));
    }

    // ─────────────────────────────────────────────────────────────────────
    t::section ("fofo::EnvSource — stereo-linked by construction");
    {
        ModMatrix m;
        auto env = m.addSource (std::make_unique<EnvSource> (5.0f, 100.0f));
        auto d   = m.addDest ("duck", 0.0f, 0.0f, 1.0f);
        m.connect (env, d, 1.0f);
        m.prepare (spec);

        for (int n = 0; n < (int) kSr / 2; ++n) m.tick (0.0f);
        t::ok (m.get (d) < 0.02, "idles near zero with no input", t::fmt ("%.4f", (double) m.get (d)));

        for (int n = 0; n < (int) kSr / 2; ++n)
            m.tick (0.8f * std::sin (2.0f * juce::MathConstants<float>::pi * 200.0f * (float) n / (float) kSr));
        t::ok (m.get (d) > 0.5, "follows a sustained 0.8-peak tone", t::fmt ("%.4f", (double) m.get (d)));

        for (int n = 0; n < (int) kSr; ++n) m.tick (0.0f);
        t::ok (m.get (d) < 0.02, "releases back to zero", t::fmt ("%.4f", (double) m.get (d)));
    }

    // ─────────────────────────────────────────────────────────────────────
    t::section ("fofo::Oversampled — latency is reported and compensated");
    {
        // A hard nonlinearity inside Parallel. If the wrapper lied about its
        // latency the recombine would comb, exactly as F1 did.
        auto shaper = [] (float x, int) { return std::tanh (2.0f * x); };
        auto osNode = std::make_unique<Oversampled> (shaper, 2);
        auto* raw = osNode.get();
        Parallel p (std::move (osNode), MixRule::Blend, 0.5f);
        p.prepare (spec);

        t::ok (raw->latencySamples() > 0, "4x oversampler reports non-zero latency",
               t::fmtI ("%d samples", raw->latencySamples())
                 + t::fmt (", exact %.3f", (double) raw->latencyExact()));
        t::ok (p.latencySamples() == raw->latencySamples(), "Parallel inherits it");

        // Drive it gently so tanh is near-linear and any dip is the recombine.
        auto runQuiet = [&] (juce::AudioBuffer<float>& b, int n)
        {
            for (int c = 0; c < b.getNumChannels(); ++c)
                juce::FloatVectorOperations::multiply (b.getWritePointer (c), 0.02f, n);
            p.process (b, n);
            for (int c = 0; c < b.getNumChannels(); ++c)
                juce::FloatVectorOperations::multiply (b.getWritePointer (c), 50.0f, n);
        };

        double best = -1e9, worst = 1e9;
        for (double f : { 200.0, 1000.0, 3000.0, 5000.0, 7000.0, 9000.0 })
        {
            p.reset();
            const double db = t::magnitudeDb (runQuiet, f, kSr, kBs, 40);
            best = std::max (best, db); worst = std::min (worst, db);
        }
        t::ok (best - worst < 0.5, "50% blend around an oversampled shaper is flat",
               t::fmt ("ripple %.3f dB", best - worst));
    }
}
