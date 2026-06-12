#pragma once
#include <juce_dsp/juce_dsp.h>
#include "spt/Shapers.h"

namespace fofopedal
{

// Delay block. Three algorithms share a Lagrange3rd-interpolated delay line
// and a feedback-path tone shaper (HPF + LPF) so repeats degrade musically.
//
//   Digital — Clean. Cubic (Lagrange) interpolation. The workhorse.
//   BBD     — Darker repeats: stronger LPF on feedback, gentle compression
//             via soft-clip in the feedback loop, slow read-pointer drift.
//   Tape    — Wow + flutter modulation on the read pointer (two LFOs: slow
//             ~0.4 Hz wow and faster ~5 Hz flutter), head-bump EQ on
//             feedback, soft tape sat in the feedback loop.
//
// Time is settable in milliseconds (host-sync handled by the processor).
// Ping-pong cross-feeds L↔R feedback. Self-oscillation possible at high
// feedback but soft-clipped against runaway.
class Delay
{
public:
    enum class Algo { Digital = 0, BBD = 1, Tape = 2, NumAlgos = 3 };

    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();

    void setAlgo       (Algo  a) noexcept { algo = a; }
    void setTimeMs     (float ms) noexcept { timeTargetMs = juce::jlimit (1.0f, 2000.0f, ms); }
    void setFeedback01 (float v)  noexcept { feedback01 = juce::jlimit (0.0f, 1.0f, v); }
    void setMix01      (float v)  noexcept { mix01      = juce::jlimit (0.0f, 1.0f, v); }
    void setHfCut01    (float v)  noexcept { hfCut01    = juce::jlimit (0.0f, 1.0f, v); markFilterDirty(); }
    void setPingPong   (bool  b)  noexcept { pingPong = b; }
    void setBypassed   (bool  b)  noexcept { bypassed = b; }

    void process (juce::AudioBuffer<float>& buffer) noexcept;

private:
    void rebuildFeedbackFilters();
    void markFilterDirty() noexcept { filterDirty = true; }

    juce::dsp::ProcessSpec spec {};
    Algo  algo         { Algo::Digital };
    float timeTargetMs { 350.0f };
    float feedback01   { 0.30f };
    float mix01        { 0.30f };
    float hfCut01      { 0.50f };
    bool  pingPong     { false };
    bool  bypassed     { false };

    // Per-sample smoothed delay (in samples) so TIME changes don't pitch-shift
    // the tail abruptly — a slow ramp gives a more musical analogue feel.
    juce::SmoothedValue<float> delaySamplesSmoothed { 0.0f };

    std::vector<juce::dsp::DelayLine<float,
        juce::dsp::DelayLineInterpolationTypes::Lagrange3rd>> lines;
    int maxDelaySamples { 0 };

    // Feedback-path filters: HPF tightens lows, LPF darkens highs, head-bump
    // peak (Tape only) fattens each repeat, DC blocker keeps the saturating
    // loop from latching up at self-oscillation feedback settings.
    std::vector<juce::dsp::IIR::Filter<float>> fbHpf;
    std::vector<juce::dsp::IIR::Filter<float>> fbLpf;
    std::vector<juce::dsp::IIR::Filter<float>> fbBump;
    std::vector<spt::DCBlocker>                fbDC;
    spt::EnvFollower duckEnv; // fixed 25% duck of the wet tap vs block input
    bool  filterDirty  { true };

    // Modulation phases for BBD/Tape — single phase shared across channels;
    // a small inter-channel offset gives gentle stereo wobble.
    float wowPhase     { 0.0f };
    float flutterPhase { 0.0f };
    float bbdDriftPhase { 0.0f };
};

} // namespace fofopedal
