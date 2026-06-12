#pragma once
#include <juce_dsp/juce_dsp.h>
#include "spt/FableVerb.h"
#include "spt/GrainShifter.h"
#include "spt/Shapers.h"

namespace daydream
{

// Shimmer reverb v3: Dattorro-style modulated-tank hall (spt::FableVerb)
// with a +1-octave granular pitch shifter in the feedback loop:
//
//   in ─►(+)─► FableVerb ─► out
//        ▲          │
//        └ ×fb ◄ tanh ◄ LP 6.5k ◄ HP 250 ◄ +1 oct ◄┘
//
// Each pass around the loop climbs another octave — that's the Eno/Lanois
// bloom. The filters stop the loop accumulating rumble (HP) or piling
// glassy energy at Nyquist (LP); the tanh keeps max settings blooming and
// holding instead of exploding. Window is 80 ms — granular smear hides in
// the tail, warble would not.
//
// Output is fully wet — the engine handles the dry/wet crossfade.
class ShimmerReverb
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();

    void setSize01    (float v) noexcept { sizeTarget    = juce::jlimit (0.0f, 1.0f, v); }
    void setShimmer01 (float v) noexcept { shimmerTarget = juce::jlimit (0.0f, 1.0f, v); }

    void process (juce::AudioBuffer<float>& buffer) noexcept;

private:
    juce::dsp::ProcessSpec spec {};
    spt::FableVerb verb;

    spt::GrainShifter shifter[2];
    spt::OnePoleLP    fbLP[2];
    juce::dsp::IIR::Filter<float> fbHP[2];

    // Pitch-shifted reverb tail from the previous block, fed back into this
    // block's reverb input.
    juce::AudioBuffer<float> shimmerFeedback;

    float sizeTarget    { 0.7f };
    float shimmerTarget { 0.0f };
};

} // namespace daydream
