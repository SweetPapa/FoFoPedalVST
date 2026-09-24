// IMPACT — does each shipping preset actually do something?
//
// Every other suite checks that a pedal is correct: stable, aligned, flat
// where it should be flat. None of them caught the thing that mattered most
// to the person playing through it — that most presets barely changed the
// sound. That is what this suite measures.
//
// The probe is a Karplus-Strong strummed guitar at two levels (-6 and -18 dBFS
// peak). For each preset it reports:
//
//   change  mean |dB| difference between the input's and the output's
//           third-octave spectrograms, after matching overall level. It hears
//           EQ, distortion, reverb filling the gaps, tremolo and pitch movement,
//           and ignores latency and plain gain. Reference effects are printed
//           first so the numbers have a scale.
//   level   output vs input power — a pedal that gets quieter as it works
//           sounds like it is doing less, so this is guarded too.
//   side    side/mid energy of the output for a mono input — how wide it is.
//
// and then holds every pedal to three floors: its weakest preset changes the
// sound at least as much as a 3 dB treble shelf, its default preset is
// plainly audible, and nothing gets more than 5 dB quieter. A voicing change
// that slides a pedal back toward timid fails CI instead of shipping.
//
// FOFO_IMPACT_ONLY=1 runs only this suite: the fast loop while voicing.

#include "TestHarness.h"
#include "Tests.h"
#include "fofo/Presets.h"
#include "fofo/Mod.h"

#include "sway/Source/dsp/SwayEngine.h"
#include "backporch/Source/dsp/BackporchEngine.h"
#include "double/Source/dsp/DoubleEngine.h"
#include "daydream/Source/dsp/DaydreamEngine.h"
#include "dreamripper/Source/dsp/DreamRipperEngine.h"
#include "fofopedal/Source/dsp/FofoEngine.h"
#include "fofopedal/Source/dsp/Controls.h"

#include "sway/Source/presets/PresetBank.h"
#include "backporch/Source/presets/PresetBank.h"
#include "double/Source/presets/PresetBank.h"
#include "daydream/Source/presets/PresetBank.h"
#include "dreamripper/Source/presets/PresetBank.h"

#include <functional>
#include <array>
#include <cstring>

static constexpr double kSr = 48000.0;
static constexpr int    kBs = 512;

namespace
{
juce::dsp::ProcessSpec spec() { return { kSr, (juce::uint32) kBs, 2 }; }

// Karplus-Strong strummed guitar: open chords, a strum per second, 10 s.
std::vector<float> guitar (float peakLin)
{
    const int N = (int) (10.0 * kSr);
    std::vector<float> out ((size_t) N, 0.0f);
    fofo::Rng rng; rng.seed (1234u);
    const float chords[4][6] = {
        { 82.4f, 123.5f, 164.8f, 207.7f, 246.9f, 329.6f },   // E
        { 110.0f, 164.8f, 220.0f, 277.2f, 329.6f, 440.0f },  // A
        { 98.0f, 123.5f, 146.8f, 196.0f, 246.9f, 392.0f },   // G
        { 146.8f, 220.0f, 293.7f, 370.0f, 440.0f, 587.3f },  // D
    };
    for (int s = 0; s < 9; ++s)
    {
        const int start = (int) (s * 1.05 * kSr);
        for (int k = 0; k < 6; ++k)
        {
            const int onset = start + k * (int) (0.012 * kSr);
            const int L = (int) (kSr / chords[s % 4][k]);
            std::vector<float> line ((size_t) L);
            for (auto& v : line) v = rng.bipolar();
            int idx = 0; float prev = 0.0f;
            for (int n = onset; n < N && n < onset + (int) (2.5 * kSr); ++n)
            {
                const float cur = line[(size_t) idx];
                const float nv = 0.996f * 0.5f * (cur + prev);
                prev = cur;
                line[(size_t) idx] = nv;
                idx = (idx + 1) % L;
                out[(size_t) n] += cur;
            }
        }
    }
    float pk = 0.0f;
    for (float v : out) pk = std::max (pk, std::abs (v));
    for (auto& v : out) v *= peakLin / pk;
    return out;
}

struct Result { double levelDb, changeDb, widthDb; };

// Band-energy spectrogram: 2048 Hann / hop 512, 28 third-octave bands 50 Hz..16 kHz.
std::vector<std::array<double, 28>> bands (const std::vector<float>& x)
{
    juce::dsp::FFT fft (11);
    const int N = 2048, H = 512;
    std::vector<float> win ((size_t) N);
    for (int i = 0; i < N; ++i) win[(size_t) i] = 0.5f - 0.5f * std::cos (2.0f * juce::MathConstants<float>::pi * i / N);
    std::vector<std::array<double, 28>> out;
    std::vector<float> buf ((size_t) N * 2);
    for (int s = 0; s + N <= (int) x.size(); s += H)
    {
        std::fill (buf.begin(), buf.end(), 0.0f);
        for (int i = 0; i < N; ++i) buf[(size_t) i] = x[(size_t) (s + i)] * win[(size_t) i];
        fft.performFrequencyOnlyForwardTransform (buf.data());
        std::array<double, 28> b {};
        for (int k = 0; k < 28; ++k)
        {
            const double lo = 50.0 * std::pow (2.0, k / 3.0), hi = lo * std::pow (2.0, 1.0 / 3.0);
            int i0 = (int) (lo * N / kSr), i1 = std::max (i0 + 1, (int) (hi * N / kSr));
            double e = 0; for (int i = i0; i < i1 && i < N / 2; ++i) e += (double) buf[(size_t) i] * buf[(size_t) i];
            b[(size_t) k] = e;
        }
        out.push_back (b);
    }
    return out;
}

// change = mean |dB| difference between input and output spectrograms after
// matching overall level, with a floor 60 dB under the loudest cell so
// inaudible cells can't dominate. ~0.5 = nothing, 1-2 = subtle, 3-5 = clearly
// there, 6+ = transformed.
Result measure (const std::vector<float>& in, int latency,
                const std::function<void (juce::AudioBuffer<float>&)>& proc)
{
    const int N = (int) in.size();
    std::vector<float> oL ((size_t) N), oR ((size_t) N);
    juce::AudioBuffer<float> b (2, kBs);
    for (int s = 0; s < N; s += kBs)
    {
        const int n = std::min (kBs, N - s);
        b.setSize (2, n, false, false, true);
        for (int i = 0; i < n; ++i) { b.setSample (0, i, in[(size_t) (s + i)]); b.setSample (1, i, in[(size_t) (s + i)]); }
        proc (b);
        for (int i = 0; i < n; ++i) { oL[(size_t) (s + i)] = b.getSample (0, i); oR[(size_t) (s + i)] = b.getSample (1, i); }
    }
    std::vector<float> mid ((size_t) (N - latency)), x ((size_t) (N - latency));
    double ii = 0, mm = 0, ss = 0;
    for (int n = latency; n < N; ++n)
    {
        const float m = 0.5f * (oL[(size_t) n] + oR[(size_t) n]), sd = 0.5f * (oL[(size_t) n] - oR[(size_t) n]);
        mid[(size_t) (n - latency)] = m; x[(size_t) (n - latency)] = in[(size_t) (n - latency)];
        ii += (double) in[(size_t) (n - latency)] * in[(size_t) (n - latency)];
        mm += (double) oL[(size_t) n] * oL[(size_t) n] + (double) oR[(size_t) n] * oR[(size_t) n];
        ss += (double) sd * sd;
    }
    auto A = bands (x), B = bands (mid);
    double ea = 0, eb = 0, mx = 0;
    for (size_t f = 0; f < A.size(); ++f) for (int k = 0; k < 28; ++k) { ea += A[f][(size_t) k]; eb += B[f][(size_t) k]; mx = std::max (mx, A[f][(size_t) k]); }
    const double g = ea / eb, floorE = mx * 1e-6;
    double acc = 0; int cnt = 0;
    for (size_t f = 0; f < A.size(); ++f) for (int k = 0; k < 28; ++k)
    {
        acc += std::abs (10.0 * std::log10 ((A[f][(size_t) k] + floorE) / (B[f][(size_t) k] * g + floorE)));
        ++cnt;
    }
    return { 10.0 * std::log10 (0.5 * mm / ii), acc / cnt, 10.0 * std::log10 (ss / (0.25 * mm) + 1e-12) };
}

float pv (const fofo::FactoryPreset& p, const char* id, float def)
{
    for (auto& v : p.values) if (std::strcmp (v.paramId, id) == 0) return v.value;
    return def;
}

void row (const char* pedal, const juce::String& name, Result hot, Result nom)
{
    std::printf ("  %-12s %-20s | -6dBFS pk: change %5.2f  level %+5.1f  side %+6.1f | -18dBFS pk: change %5.2f  level %+5.1f\n",
                 pedal, name.toRawUTF8(), hot.changeDb, hot.levelDb, hot.widthDb, nom.changeDb, nom.levelDb);
}
// The floors. A preset may be subtle, but it may not be inaudible, and a
// pedal may not get quieter as it works — the two things v2 got wrong.
constexpr double kEveryPresetFloor  = 1.0;   // a +3 dB treble shelf
constexpr double kDefaultFloor      = 1.75;  // plainly there, first time it loads
constexpr double kQuietestAllowedDb = -5.0;

struct Judged { juce::String name; double change, level; bool isDefault; };

void judge (const char* pedal, const std::vector<Judged>& rows)
{
    const Judged* weakest = nullptr;
    const Judged* quietest = nullptr;
    const Judged* dflt = nullptr;
    for (auto& r : rows)
    {
        if (weakest  == nullptr || r.change < weakest->change) weakest  = &r;
        if (quietest == nullptr || r.level  < quietest->level) quietest = &r;
        if (r.isDefault) dflt = &r;
    }
    const juce::String P (pedal);
    t::ok (weakest->change >= kEveryPresetFloor,
           (P + ": every preset changes the sound at least as much as a 3 dB shelf").toStdString(),
           (weakest->name + t::fmt (" is weakest at %.2f", weakest->change)).toStdString());
    t::ok (dflt != nullptr && dflt->change >= kDefaultFloor,
           (P + ": the default preset is plainly audible").toStdString(),
           dflt ? (dflt->name + t::fmt (" at %.2f", dflt->change)).toStdString() : std::string ("no default"));
    t::ok (quietest->level > kQuietestAllowedDb,
           (P + ": no preset gets more than 5 dB quieter").toStdString(),
           (quietest->name + t::fmt (" is quietest at %+.1f dB", quietest->level)).toStdString());
}

// Measure a preset at both input levels, print its row, and keep the worse
// of the two readings for judging.
template <typename Run>
void measureInto (std::vector<Judged>& out, const char* pedal, const juce::String& name, bool isDefault,
                  const std::vector<float>& hotIn, const std::vector<float>& nomIn, Run&& run)
{
    const Result hot = run (hotIn), nom = run (nomIn);
    row (pedal, name, hot, nom);
    out.push_back ({ name, std::min (hot.changeDb, nom.changeDb), std::min (hot.levelDb, nom.levelDb), isDefault });
}
} // namespace

void runImpactTests()
{
    const auto hotIn = guitar (0.5f);     // -6 dBFS peak
    const auto nomIn = guitar (0.125f);   // -18 dBFS peak

    auto both = [&] (auto&& run) { return std::pair<Result, Result> { run (hotIn), run (nomIn) }; };

    t::section ("IMPACT — reference effects, for scale");

    // A reference effect is a per-sample function of (input, sample index,
    // one float of state), fresh for every run.
    auto anchor = [&] (const char* nm, std::function<float (float, int, float&)> fx)
    {
        auto r = both ([&] (const std::vector<float>& in)
        {
            int n = 0; float z = 0.0f;
            return measure (in, 0, [&] (auto& b)
            {
                for (int i = 0; i < b.getNumSamples(); ++i)
                {
                    const float y = fx (b.getSample (0, i), n++, z);
                    b.setSample (0, i, y); b.setSample (1, i, y);
                }
            });
        });
        row ("REFERENCE", nm, r.first, r.second);
    };
    const float w5 = 2.0f * juce::MathConstants<float>::pi * 5.0f / (float) kSr;
    anchor ("nothing",            [] (float x, int, float&) { return x; });
    anchor ("+3 dB treble shelf", [] (float x, int, float& z) { const float hp = x - z; z += 0.2f * (x - z); return x + 0.41f * hp; });
    anchor ("tanh x4 (crunch)",   [] (float x, int, float&) { return std::tanh (4.0f * x); });
    anchor ("tanh x30 (heavy)",   [] (float x, int, float&) { return std::tanh (30.0f * x); });
    anchor ("tremolo 50% 5 Hz",   [w5] (float x, int n, float&) { return x * (0.75f + 0.25f * std::sin (w5 * (float) n)); });
    anchor ("tremolo 100% 5 Hz",  [w5] (float x, int n, float&) { return x * (0.5f + 0.5f * std::sin (w5 * (float) n)); });

    t::section ("IMPACT — every shipping preset");

    // Walk a bank, measuring each preset with `run (preset, input)`.
    auto bank = [&] (const char* pedal, const auto& presets, int defaultIndex, auto&& run)
    {
        std::vector<Judged> rows;
        for (int i = 0; i < (int) presets.size(); ++i)
        {
            const auto& p = presets[(size_t) i];
            measureInto (rows, pedal, juce::String (p.name), i == defaultIndex, hotIn, nomIn,
                         [&] (const std::vector<float>& in) { return run (p, in); });
        }
        judge (pedal, rows);
    };

    bank ("SWAY", sway::getFactoryPresets(), sway::kDefaultPresetIndex,
          [] (const fofo::FactoryPreset& p, const std::vector<float>& in)
    {
        sway::SwayEngine e; e.prepare (spec());
        e.setMove01 (pv (p, "move", 45) * 0.01f); e.setRate01 (pv (p, "rate", 35) * 0.01f);
        e.setColor01 (pv (p, "color", 50) * 0.01f); e.setMix01 (pv (p, "mix", 100) * 0.01f);
        e.setMode ((sway::SwayEngine::Mode) (int) pv (p, "mode", 0));
        return measure (in, e.getLatencySamples(), [&] (auto& b) { e.process (b); });
    });

    bank ("BACKPORCH", bkpr::getFactoryPresets(), bkpr::kDefaultPresetIndex,
          [] (const fofo::FactoryPreset& p, const std::vector<float>& in)
    {
        bkpr::BackporchEngine e; e.prepare (spec());
        e.setSpace01 (pv (p, "space", 45) * 0.01f); e.setTone01 (pv (p, "tone", 45) * 0.01f);
        e.setDuck01 (pv (p, "duck", 35) * 0.01f); e.setMix01 (pv (p, "mix", 40) * 0.01f);
        e.setMode ((bkpr::BackporchEngine::Mode) (int) pv (p, "mode", 1));
        return measure (in, e.getLatencySamples(), [&] (auto& b) { e.process (b); });
    });

    bank ("DOUBLE", dbl::getFactoryPresets(), dbl::kDefaultPresetIndex,
          [] (const fofo::FactoryPreset& p, const std::vector<float>& in)
    {
        dbl::DoubleEngine e; e.prepare (spec());
        e.setThick01 (pv (p, "thick", 50) * 0.01f); e.setWide01 (pv (p, "wide", 70) * 0.01f);
        e.setHuman01 (pv (p, "human", 50) * 0.01f); e.setMix01 (pv (p, "mix", 60) * 0.01f);
        e.setMode ((dbl::DoubleEngine::Mode) (int) pv (p, "mode", 0));
        return measure (in, e.getLatencySamples(), [&] (auto& b) { e.process (b); });
    });

    bank ("DAYDREAM", daydream::getFactoryPresets(), daydream::kDefaultPresetIndex,
          [] (const fofo::FactoryPreset& p, const std::vector<float>& in)
    {
        daydream::DaydreamEngine e; e.setDream01 (pv (p, "dream", 35) * 0.01f); e.prepare (spec());
        return measure (in, e.getLatencySamples(), [&] (auto& b) { e.process (b); });
    });

    bank ("DREAMRIPPER", drip::getFactoryPresets(), drip::kDefaultPresetIndex,
          [] (const fofo::FactoryPreset& p, const std::vector<float>& in)
    {
        drip::DreamRipperEngine e;
        e.setRip01 (pv (p, "rip", 50) * 0.01f); e.setTight01 (pv (p, "tight", 50) * 0.01f);
        e.setScoop01 (pv (p, "scoop", 50) * 0.01f); e.setCab01 (pv (p, "cab", 50) * 0.01f);
        e.setLevel01 (pv (p, "level", 50) * 0.01f); e.setGate01 (pv (p, "gate", 0) * 0.01f);
        e.setMix01 (pv (p, "mix", 100) * 0.01f);
        e.setMode ((drip::DreamRipperEngine::Mode) (int) pv (p, "mode", 0));
        e.prepare (spec());
        return measure (in, e.getLatencySamples(), [&] (auto& b) { e.process (b); });
    });

    // FOFOPEDAL's characters go through applyControls — the same front-panel
    // mapping the plugin runs every block. Its default is character 0.
    {
        using namespace fofopedal;
        std::vector<Judged> rows;
        for (size_t i = 0; i < CharacterBank::kFactoryPresets.size(); ++i)
        {
            const auto& c = CharacterBank::kFactoryPresets[i];
            measureInto (rows, "FOFOPEDAL", juce::String (c.name), i == 0, hotIn, nomIn,
                         [&] (const std::vector<float>& in)
            {
                FofoEngine e; e.prepare (spec());
                applyControls (e, c);
                return measure (in, e.getLatencySamples(), [&] (auto& b) { applyControls (e, c); e.process (b); });
            });
        }
        judge ("FOFOPEDAL", rows);
    }
}
