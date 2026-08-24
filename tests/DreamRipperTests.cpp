// DREAMRIPPER — the claims this pedal makes, measured.
//
// A distortion is the easiest thing in a plugin catalogue to ship broken and
// the hardest to argue about by ear, because every defect it can have sounds
// like "a distortion". So each of these tests pins one specific claim from the
// engine header to a number:
//
//   • the mode switch is safe in a host (latency cannot move);
//   • MIX 0 is genuinely the dry signal, not a comb (the F1/F9 family);
//   • RIP changes texture, not level, and the four modes match each other;
//   • TIGHT removes low end BEFORE the gain, which is measurable as the low
//     end no longer eating a probe tone through intermodulation;
//   • SCOOP moves mids in both directions from centre;
//   • CAB moves the whole speaker, cliff and all;
//   • the gate shuts between notes without chopping the attack off the front;
//   • the cascade does not alias, which is the entire reason it is oversampled;
//   • every shipping preset makes finite, audible, bounded sound.

#include "TestHarness.h"
#include "Tests.h"
#include "dreamripper/Source/dsp/DreamRipperEngine.h"
#include "dreamripper/Source/presets/PresetBank.h"

#include <set>
#include <string>
#include <vector>

using drip::DreamRipperEngine;
using Mode = DreamRipperEngine::Mode;

static constexpr double kSr = 48000.0;
static constexpr int    kBs = 512;

namespace
{
    juce::dsp::ProcessSpec spec() { return { kSr, (juce::uint32) kBs, 2 }; }

    struct Knobs
    {
        float rip = 0.55f, tight = 0.45f, scoop = 0.5f, cab = 0.5f,
              level = 0.5f, gate = 0.0f, mix = 1.0f;
        Mode  mode = Mode::Grunge;
    };

    void apply (DreamRipperEngine& e, const Knobs& k)
    {
        e.setRip01 (k.rip); e.setTight01 (k.tight); e.setScoop01 (k.scoop);
        e.setCab01 (k.cab); e.setLevel01 (k.level); e.setGate01 (k.gate);
        e.setMix01 (k.mix); e.setMode (k.mode);
    }

    // Steady-state magnitude of `probeHz`, optionally with a loud low bed
    // running underneath it. Returned in dB relative to the probe's own
    // amplitude, so 0 dB means the pedal passed the probe at unity.
    double probeDb (const Knobs& k, double probeHz, double probeAmp,
                    double bedHz = 0.0, double bedAmp = 0.0, int blocks = 90)
    {
        DreamRipperEngine e;
        e.prepare (spec());
        apply (e, k);

        juce::AudioBuffer<float> b (2, kBs);
        double re = 0.0, im = 0.0;
        const long long total = (long long) kBs * blocks;
        const long long skip  = total / 2;
        long long tt = 0;

        for (int blk = 0; blk < blocks; ++blk)
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

    // Broadband output level, which is what "does the volume jump?" actually
    // means. Pink-ish noise rather than a sine, so the answer is not a
    // property of one frequency landing in one filter's peak.
    double outputRmsDb (const Knobs& k, int blocks = 40, float amp = 0.25f)
    {
        DreamRipperEngine e;
        e.prepare (spec());
        apply (e, k);

        fofo::Rng rng;
        rng.seed (0xB16Fu);
        fofo::OnePole tilt;
        tilt.prepare (kSr);
        tilt.setCutoff (1200.0f);

        juce::AudioBuffer<float> b (2, kBs);
        double sum = 0.0;
        long long count = 0;

        for (int blk = 0; blk < blocks; ++blk)
        {
            for (int n = 0; n < kBs; ++n)
            {
                const float s = amp * tilt.process (rng.bipolar()) * 2.2f;
                b.setSample (0, n, s);
                b.setSample (1, n, s);
            }
            e.process (b);
            if (blk < blocks / 4) continue;      // let the filters settle
            for (int n = 0; n < kBs; ++n)
            {
                const double v = b.getSample (0, n);
                sum += v * v;
                ++count;
            }
        }

        return 20.0 * std::log10 (std::sqrt (sum / (double) juce::jmax (1LL, count)) + 1e-12);
    }

    // Energy in a band, by brute-force quadrature over a set of bins. Used to
    // look for signal where none should exist.
    double bandDb (const std::vector<float>& x, double loHz, double hiHz, int bins)
    {
        double total = 0.0;
        for (int i = 0; i < bins; ++i)
        {
            const double f = loHz * std::pow (hiHz / loHz, (double) i / (double) juce::jmax (1, bins - 1));
            double re = 0.0, im = 0.0;
            for (size_t n = 0; n < x.size(); ++n)
            {
                const double ph = 2.0 * juce::MathConstants<double>::pi * f * (double) n / kSr;
                re += x[n] * std::cos (ph);
                im += x[n] * std::sin (ph);
            }
            const double mag = 2.0 * std::sqrt (re * re + im * im) / (double) x.size();
            total = juce::jmax (total, mag);
        }
        return 20.0 * std::log10 (total + 1e-12);
    }

    std::vector<float> renderSine (const Knobs& k, double hz, float amp, int blocks)
    {
        DreamRipperEngine e;
        e.prepare (spec());
        apply (e, k);

        juce::AudioBuffer<float> b (2, kBs);
        std::vector<float> out;
        out.reserve ((size_t) blocks * kBs);

        for (int blk = 0; blk < blocks; ++blk)
        {
            for (int n = 0; n < kBs; ++n)
            {
                const float s = amp * (float) std::sin (2.0 * juce::MathConstants<double>::pi
                                                        * hz * (double) (blk * kBs + n) / kSr);
                b.setSample (0, n, s);
                b.setSample (1, n, s);
            }
            e.process (b);
            if (blk >= blocks / 2)
                for (int n = 0; n < kBs; ++n) out.push_back (b.getSample (0, n));
        }
        return out;
    }

    const char* modeName (Mode m) { return DreamRipperEngine::voicingFor (m).name; }

    constexpr Mode kAllModes[4] = { Mode::Sludge, Mode::Grunge, Mode::Metal, Mode::Djent };
}

void runDreamRipperTests()
{

    // ─────────────────────────────────────────────────────────────────────
    t::section ("DREAMRIPPER — latency is constant, so a mode switch is safe");
    {
        // A host compensates for reported latency once and does not re-query
        // when a parameter moves. A mode that reported a different figure
        // would leave the track sitting early from the moment you changed it.
        int latencies[4] {};
        for (int i = 0; i < 4; ++i)
        {
            DreamRipperEngine e;
            e.prepare (spec());
            e.setMode (kAllModes[i]);
            juce::AudioBuffer<float> b (2, kBs);
            b.clear();
            e.process (b);
            latencies[i] = e.getLatencySamples();
        }

        t::ok (latencies[0] == latencies[1] && latencies[1] == latencies[2] && latencies[2] == latencies[3],
               "all four modes report the same latency",
               t::fmtI ("%d samples", latencies[0]));
        t::ok (latencies[0] >= 0 && latencies[0] < 64,
               "the reported latency is the oversampler's and nothing more",
               t::fmtI ("%d samples", latencies[0]));
    }

    // ─────────────────────────────────────────────────────────────────────
    t::section ("DREAMRIPPER — MIX 0 is the dry signal, not a comb");
    {
        // The wet branch runs through an oversampler, so a naive recombine
        // would sum the dry against a latency-shifted copy of itself and comb.
        // fofo::Parallel holds the dry back by the branch's own reported
        // latency, which is what this measures.
        Knobs k; k.mix = 0.0f; k.rip = 0.9f;
        double worst = 0.0;
        double at = 0.0;
        for (double f : { 110.0, 440.0, 1200.0, 3300.0, 5200.0, 8000.0 })
        {
            const double db = std::abs (probeDb (k, f, 0.2));
            if (db > worst) { worst = db; at = f; }
        }
        t::ok (worst < 0.35, "the dry path is flat at MIX 0",
               t::fmt2 ("worst %.3f dB at %.0f Hz", worst, at));
    }

    // ─────────────────────────────────────────────────────────────────────
    t::section ("DREAMRIPPER — RIP is texture, not volume");
    {
        for (auto m : kAllModes)
        {
            double lo = 1e9, hi = -1e9;
            for (float rip : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
            {
                Knobs k; k.mode = m; k.rip = rip;
                const double db = outputRmsDb (k);
                lo = juce::jmin (lo, db);
                hi = juce::jmax (hi, db);
            }
            t::ok (hi - lo < 4.0,
                   std::string (modeName (m)) + ": the level holds across the whole RIP sweep",
                   t::fmt ("%.2f dB of travel", hi - lo));
        }
    }

    t::section ("DREAMRIPPER — the four modes are level-matched");
    {
        double lo = 1e9, hi = -1e9;
        std::string detail;
        for (auto m : kAllModes)
        {
            Knobs k; k.mode = m;
            const double db = outputRmsDb (k);
            lo = juce::jmin (lo, db);
            hi = juce::jmax (hi, db);
            detail += std::string (modeName (m)) + " " + t::fmt ("%.1f", db) + "  ";
        }
        t::ok (hi - lo < 4.0, "no mode is a volume jump at matched settings", detail);
    }

    // ─────────────────────────────────────────────────────────────────────
    t::section ("DREAMRIPPER — TIGHT removes low end before the gain");
    {
        // This is the claim that separates a chug from a flub, and it is not an
        // EQ claim. Low end reaching a clipper intermodulates with everything
        // above it, so a loud 55 Hz bed drags a 1 kHz probe down with it. Take
        // the bed away BEFORE the gain and the probe survives. Measured as the
        // suppression the bed costs the probe, so the tone controls moving
        // underneath cannot flatter the result.
        auto suppression = [] (Mode m, float tight, float rip)
        {
            Knobs k; k.mode = m; k.rip = rip; k.tight = tight;
            return probeDb (k, 1000.0, 0.12) - probeDb (k, 1000.0, 0.12, 55.0, 0.55);
        };

        // Sludge is the loosest amplifier in the box, which makes it the one
        // where the knob has the most to do.
        const double loose = suppression (Mode::Sludge, 0.0f, 0.85f);
        const double tight = suppression (Mode::Sludge, 1.0f, 0.85f);

        t::ok (loose - tight > 10.0,
               "a loud low string stops eating the notes above it",
               t::fmt2 ("loose costs %.1f dB, tight costs %.1f dB", loose, tight));
        t::ok (tight < 2.0, "wound all the way up, the bed costs the probe almost nothing",
               t::fmt ("%.2f dB", tight));

        // And it is a real highpass, moving with the knob.
        Knobs lo; lo.mode = Mode::Metal; lo.rip = 0.85f; lo.tight = 0.0f;
        Knobs hi = lo; hi.tight = 1.0f;
        const double lowLoose = probeDb (lo, 80.0, 0.2);
        const double lowTight = probeDb (hi, 80.0, 0.2);
        t::ok (lowLoose - lowTight > 6.0, "80 Hz drops as TIGHT comes up",
               t::fmt2 ("%.1f dB → %.1f dB", lowLoose, lowTight));

        // The modes tighten by different amounts on purpose — that is most of
        // what makes Djent a different amplifier from Sludge rather than a
        // different tone control setting.
        double last = 1e9;
        bool monotonic = true;
        std::string detail;
        for (auto m : kAllModes)
        {
            Knobs k; k.mode = m; k.tight = 0.5f; k.rip = 0.7f;
            const double db = probeDb (k, 80.0, 0.2);
            detail += std::string (modeName (m)) + " " + t::fmt ("%.1f", db) + "  ";
            if (db > last + 0.5) monotonic = false;
            last = db;
        }
        t::ok (monotonic, "Sludge → Djent is a monotonic tightening at 80 Hz", detail);
    }

    // ─────────────────────────────────────────────────────────────────────
    t::section ("DREAMRIPPER — SCOOP moves the mids both ways from centre");
    {
        for (auto m : { Mode::Grunge, Mode::Metal })
        {
            const float midHz = DreamRipperEngine::voicingFor (m).midHz;

            Knobs push; push.mode = m; push.scoop = 0.0f;
            Knobs flat; flat.mode = m; flat.scoop = 0.5f;
            Knobs dig;  dig .mode = m; dig .scoop = 1.0f;

            const double p = probeDb (push, midHz, 0.15);
            const double f = probeDb (flat, midHz, 0.15);
            const double d = probeDb (dig,  midHz, 0.15);

            t::ok (p > f + 1.5 && f > d + 6.0,
                   std::string (modeName (m)) + ": pushed > flat > scooped at the mode's mid",
                   t::fmt2 ("push %.1f  flat %.1f", p, f) + t::fmt ("  dig %.1f", d));
        }
    }

    // ─────────────────────────────────────────────────────────────────────
    t::section ("DREAMRIPPER — CAB moves the speaker, cliff and all");
    {
        Knobs dark;   dark.cab = 0.0f;
        Knobs bright; bright.cab = 1.0f;

        const double darkTop   = probeDb (dark,   6000.0, 0.15);
        const double brightTop = probeDb (bright, 6000.0, 0.15);
        t::ok (brightTop - darkTop > 6.0, "the top opens up as CAB comes up",
               t::fmt2 ("dark %.1f dB → bright %.1f dB", darkTop, brightTop));

        // Even wide open it is still a speaker: there has to be a cliff.
        const double brightMid = probeDb (bright, 1000.0, 0.15);
        const double brightAir = probeDb (bright, 12000.0, 0.15);
        t::ok (brightMid - brightAir > 15.0, "there is always a cliff above the presence peak",
               t::fmt2 ("1 kHz %.1f dB, 12 kHz %.1f dB", brightMid, brightAir));
    }

    // ─────────────────────────────────────────────────────────────────────
    t::section ("DREAMRIPPER — the cascade is oversampled for a reason");
    {
        // A 3 kHz sine into a bounded nonlinearity can only produce harmonics
        // at 6, 9, 12 kHz and up. Anything appreciable BELOW the fundamental
        // is aliasing folded down, which is exactly what the oversampling is
        // there to prevent — and what the previous catalogue shipped on every
        // shaper that ran at base rate.
        Knobs k; k.mode = Mode::Djent; k.rip = 1.0f; k.cab = 1.0f;
        const auto out = renderSine (k, 3000.0, 0.4f, 24);

        const double fundamental = bandDb (out, 2990.0, 3010.0, 3);
        const double folded      = bandDb (out, 250.0, 2400.0, 40);

        t::ok (fundamental - folded > 35.0,
               "nothing folds down below the fundamental at full drive",
               t::fmt2 ("3 kHz %.1f dB, worst alias %.1f dB", fundamental, folded));
    }

    // ─────────────────────────────────────────────────────────────────────
    t::section ("DREAMRIPPER — the gate shuts up but does not chop");
    {
        // A note, then silence with a low noise floor underneath — the shape
        // of the problem a high-gain amp actually has.
        auto runBurst = [] (float gateKnob, std::vector<float>& out)
        {
            DreamRipperEngine e;
            e.prepare (spec());
            Knobs k; k.mode = Mode::Djent; k.rip = 0.85f; k.gate = gateKnob;
            apply (e, k);

            fofo::Rng rng; rng.seed (0x4EEDu);
            juce::AudioBuffer<float> b (2, kBs);
            out.clear();

            constexpr int kBlocks = 90;
            for (int blk = 0; blk < kBlocks; ++blk)
            {
                const bool playing = blk >= 10 && blk < 30;
                for (int n = 0; n < kBs; ++n)
                {
                    const double t = (double) (blk * kBs + n) / kSr;
                    float s = 0.0016f * rng.bipolar();          // the noise floor
                    if (playing)
                        s += 0.35f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 220.0 * t);
                    b.setSample (0, n, s);
                    b.setSample (1, n, s);
                }
                e.process (b);
                for (int n = 0; n < kBs; ++n) out.push_back (b.getSample (0, n));
            }
        };

        std::vector<float> open, gated;
        runBurst (0.0f, open);
        runBurst (0.62f, gated);

        auto windowDb = [] (const std::vector<float>& v, int firstBlock, int lastBlock)
        {
            const int a = firstBlock * kBs, b = lastBlock * kBs;
            return 20.0 * std::log10 (t::rms (v.data() + a, b - a) + 1e-12);
        };

        const double openFloor  = windowDb (open,  62, 88);
        const double gatedFloor = windowDb (gated, 62, 88);
        t::ok (gatedFloor < openFloor - 30.0, "the gate kills the noise between notes",
               t::fmt2 ("ungated %.1f dB → gated %.1f dB", openFloor, gatedFloor));

        // ...and the note itself has to survive intact. The gate opens in
        // under a millisecond, so the body of the note is untouched.
        const double openNote  = windowDb (open,  14, 28);
        const double gatedNote = windowDb (gated, 14, 28);
        t::ok (std::abs (gatedNote - openNote) < 0.6, "the note itself is untouched",
               t::fmt2 ("ungated %.1f dB, gated %.1f dB", openNote, gatedNote));

        // The very front of the note is where a slow gate is audible as a
        // missing pick attack.
        const double attackOpen  = windowDb (open,  10, 11);
        const double attackGated = windowDb (gated, 10, 11);
        t::ok (attackGated > attackOpen - 2.5, "the pick attack is not chopped off the front",
               t::fmt2 ("ungated %.1f dB, gated %.1f dB", attackOpen, attackGated));
    }

    // ─────────────────────────────────────────────────────────────────────
    t::section ("DREAMRIPPER — every shipping preset makes sound");
    {
        const auto& bank = drip::getFactoryPresets();
        bool allFinite = true, allAudible = true, allBounded = true;
        std::string notFinite, tooQuiet, tooLoud;
        double quietest = 1e9, loudest = -1e9;
        std::string quietestName, loudestName;

        for (const auto& preset : bank)
        {
            Knobs k;
            for (const auto& v : preset.values)
            {
                const juce::String id (v.paramId);
                if      (id == "rip")   k.rip   = v.value * 0.01f;
                else if (id == "tight") k.tight = v.value * 0.01f;
                else if (id == "scoop") k.scoop = v.value * 0.01f;
                else if (id == "cab")   k.cab   = v.value * 0.01f;
                else if (id == "level") k.level = v.value * 0.01f;
                else if (id == "gate")  k.gate  = v.value * 0.01f;
                else if (id == "mix")   k.mix   = v.value * 0.01f;
                else if (id == "mode")  k.mode  = (Mode) (int) v.value;
            }

            DreamRipperEngine e;
            e.prepare (spec());
            apply (e, k);

            fofo::Rng rng; rng.seed (0xC0DEu);
            juce::AudioBuffer<float> b (2, kBs);
            double energy = 0.0, worst = 0.0;
            bool finite = true;

            // Two band-limited sawtooth notes — a low A and an E above it.
            // A pure sine is the wrong probe here: half these presets are
            // built to remove a whole region of the spectrum, and a preset
            // that only looks silent because the test tone had nothing outside
            // the notch is a false alarm, not a defect.
            for (int blk = 0; blk < 24; ++blk)
            {
                for (int n = 0; n < kBs; ++n)
                {
                    const double t = (double) (blk * kBs + n) / kSr;
                    float s = 0.012f * rng.bipolar();
                    for (double f : { 110.0, 164.8 })
                        for (int h = 1; h <= 12; ++h)
                            s += (float) (0.14 / h * std::sin (2.0 * juce::MathConstants<double>::pi
                                                              * f * (double) h * t));
                    b.setSample (0, n, s);
                    b.setSample (1, n, s);
                }
                e.process (b);
                if (blk < 6) continue;
                for (int n = 0; n < kBs; ++n)
                {
                    const float v = b.getSample (0, n);
                    if (! std::isfinite (v)) finite = false;
                    energy += (double) v * v;
                    worst = juce::jmax (worst, (double) std::abs (v));
                }
            }

            const double rmsDb = 20.0 * std::log10 (std::sqrt (energy / (double) (18 * kBs)) + 1e-12);

            if (rmsDb < quietest) { quietest = rmsDb; quietestName = preset.name.toStdString(); }
            if (rmsDb > loudest)  { loudest  = rmsDb; loudestName  = preset.name.toStdString(); }

            if (! finite)      { allFinite  = false; if (notFinite.empty()) notFinite = preset.name.toStdString(); }
            if (rmsDb < -26.0) { allAudible = false; if (tooQuiet.empty())  tooQuiet  = preset.name.toStdString() + " at " + t::fmt ("%.1f dB", rmsDb); }
            if (worst > 2.5)   { allBounded = false; if (tooLoud.empty())   tooLoud   = preset.name.toStdString() + " peaks at " + t::fmt ("%.2f", worst); }
        }

        t::ok (bank.size() == 16, "the bank ships sixteen presets",
               std::to_string (bank.size()) + " presets");
        t::ok (allFinite,  "every preset stays finite", notFinite);
        t::ok (allAudible, "every preset is audible on a guitar-shaped signal", tooQuiet);
        t::ok (allBounded, "no preset runs away", tooLoud);

        // Stepping through a factory bank should be an audition, not a
        // volume ride. This is the assertion that keeps the makeup calibration
        // honest across the knobs the presets actually move.
        t::ok (loudest - quietest < 5.0,
               "the bank is level-matched end to end",
               t::fmt ("%.1f dB", loudest - quietest) + "  (" + quietestName
                 + " → " + loudestName + ")");
    }

    // ─────────────────────────────────────────────────────────────────────
    t::section ("DREAMRIPPER — the mode table is what the modes are");
    {
        std::set<std::string> names;
        bool sane = true;
        for (auto m : kAllModes)
        {
            const auto& v = DreamRipperEngine::voicingFor (m);
            names.insert (v.name);
            if (v.stages < 1 || v.stages > DreamRipperEngine::kMaxStages) sane = false;
            if (! (v.tightLoHz < v.tightHiHz))   sane = false;
            if (! (v.cabLpLoHz < v.cabLpHiHz))   sane = false;
            if (! (v.interLoHz < v.interHiHz))   sane = false;
        }
        t::ok (names.size() == 4, "the four modes are four distinct amplifiers");
        t::ok (sane, "every voicing's ranges point the right way");

        t::ok (DreamRipperEngine::voicingFor (Mode::Sludge).stages == 2
                 && DreamRipperEngine::voicingFor (Mode::Djent).stages == 3,
               "the high-gain modes cascade more stages than the fuzz does");
    }
}
