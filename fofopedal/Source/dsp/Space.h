#pragma once
#include <juce_dsp/juce_dsp.h>
#include "spt/FableVerb.h"
#include "spt/GrainShifter.h"
#include "spt/Shapers.h"

namespace fofopedal
{

// Space (reverb) block, v2 — built on spt::FableVerb (Dattorro figure-8
// tank with modulated allpasses) instead of juce::Reverb, so the four
// algorithms are genuinely different rooms, not four damping presets:
//
//   Plate   — fast diffusion, dense from the first millisecond, bright.
//   Hall    — long lines, slower build, darker damping, wide.
//   Room    — short tank, "you're in the studio" ambience.
//   Shimmer — Hall + granular +1-oct shifter in the feedback loop
//             (HP 250 / LP 6.5k / tanh inside the loop, capped — blooms
//             and holds instead of exploding).
//
// All algorithms get pre-delay (0–250 ms) and a high-pass on the send —
// the dry signal owns the low end. The tail is also gently ducked against
// the block's input (fixed 30%): it gets out of the way while you play and
// breathes back in the gaps. Baked in, not exposed — mix hygiene is the
// product, not a feature.
//
// SIZE is a macro: tank length, decay and damping move together per-algo.
class Space
{
public:
    enum class Algo { Plate = 0, Hall = 1, Room = 2, Shimmer = 3, NumAlgos = 4 };

    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();

    void setAlgo        (Algo  a) noexcept { algo = a; algoChanged = true; }
    void setSize01      (float v) noexcept { size01 = juce::jlimit (0.0f, 1.0f, v); paramsChanged = true; }
    void setMix01       (float v) noexcept { mix01  = juce::jlimit (0.0f, 1.0f, v); }
    void setPreDelayMs  (float ms) noexcept { preDelayMs = juce::jlimit (0.0f, 250.0f, ms); }
    void setSendHpHz    (float hz) noexcept { sendHpHz = juce::jlimit (40.0f, 400.0f, hz); paramsChanged = true; }
    void setShimmer01   (float v) noexcept { shimmer01 = juce::jlimit (0.0f, 1.0f, v); }
    void setBypassed    (bool  b) noexcept { bypassed = b; }

    void process (juce::AudioBuffer<float>& buffer) noexcept;

private:
    void updateAll();

    juce::dsp::ProcessSpec spec {};
    Algo  algo        { Algo::Hall };
    float size01      { 0.50f };
    float mix01       { 0.30f };
    float preDelayMs  { 25.0f };
    float sendHpHz    { 90.0f };
    float shimmer01   { 0.0f };
    bool  bypassed    { false };
    bool  algoChanged { true };
    bool  paramsChanged { true };

    std::vector<juce::dsp::DelayLine<float,
        juce::dsp::DelayLineInterpolationTypes::Lagrange3rd>> pre;
    int    preMaxSamp { 0 };
    std::vector<juce::dsp::IIR::Filter<float>> sendHpf;

    juce::AudioBuffer<float> drySnap;

    spt::FableVerb verb;

    spt::EnvFollower duckEnv; // keyed by block input; fixed gentle duck

    // Shimmer loop: previous block's tail, pitched +1 octave.
    spt::GrainShifter shifter[2];
    spt::OnePoleLP    shimLP[2];
    juce::dsp::IIR::Filter<float> shimHP[2];
    juce::AudioBuffer<float> shimmerFb;
};

} // namespace fofopedal
