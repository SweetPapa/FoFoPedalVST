#pragma once
#include <juce_dsp/juce_dsp.h>
#include "spt/DriftLFO.h"
#include "spt/Shapers.h"

namespace fofopedal
{

// Modulation block, v2. Three algorithms with a single set of common controls:
//   Chorus  — true tri-chorus: three voices at staggered base delays
//             (5/7/9 ms) with INDEPENDENT, slightly-detuned, noise-drifted
//             LFOs at -120°/0°/+120°, each voice darkened (BBD-style LP) and
//             lightly saturated. The detune+drift is what makes it sound
//             like hardware instead of a sine on a delay line.
//   Phaser  — 4/6-stage all-pass cascade with feedback; exponential sweep
//             (linear sweeps spend most of their time sounding parked).
//   TremVib — one knob (SHAPE) crossfades amplitude tremolo → pitch vibrato,
//             with organic rate drift.
//
// Single LFO rate (0.05–8 Hz) drives whichever algorithm is active.
class Mod
{
public:
    enum class Algo { Chorus = 0, Phaser = 1, TremVib = 2, NumAlgos = 3 };

    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();

    void setAlgo     (Algo  a) noexcept { algo = a; }
    void setRate01   (float v) noexcept { rate01     = juce::jlimit (0.0f, 1.0f, v); }
    void setDepth01  (float v) noexcept { depth01    = juce::jlimit (0.0f, 1.0f, v); }
    void setShape01  (float v) noexcept { shape01    = juce::jlimit (0.0f, 1.0f, v); }
    void setFeedback01 (float v) noexcept { feedback01 = juce::jlimit (0.0f, 1.0f, v); }
    void setMix01    (float v) noexcept { mix01      = juce::jlimit (0.0f, 1.0f, v); }
    void setBypassed (bool  b) noexcept { bypassed = b; }

    void process (juce::AudioBuffer<float>& buffer) noexcept;

private:
    juce::dsp::ProcessSpec spec {};
    Algo  algo       { Algo::Chorus };
    float rate01     { 0.30f };
    float depth01    { 0.50f };
    float shape01    { 0.50f };
    float feedback01 { 0.20f };
    float mix01      { 0.40f };
    bool  bypassed   { false };

    // ── Tri-chorus state ────────────────────────────────────────────────
    // Three delay lines at staggered base delays, each with its own
    // detuned + drifted LFO. Stereo output: voice 0 → L, voice 1 → centre,
    // voice 2 → R.
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> chorusD[3];
    spt::DriftLFO  chorusLfo[3];
    spt::OnePoleLP chorusBBD[3]; // per-voice darkening

    // ── Phaser state ────────────────────────────────────────────────────
    static constexpr int kPhaserStages = 6;
    struct PhaserChan
    {
        float ap1[kPhaserStages] {}; // first-order all-pass state (x[n-1])
        float ap2[kPhaserStages] {}; // y[n-1]
        float fb { 0.0f };           // feedback memory
    };
    std::vector<PhaserChan> phaser;
    spt::DriftLFO phaserLfo;

    // ── Trem/Vib state ──────────────────────────────────────────────────
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> vibLine;
    spt::DriftLFO tremLfo;

    juce::AudioBuffer<float> dryBuffer;
};

} // namespace fofopedal
