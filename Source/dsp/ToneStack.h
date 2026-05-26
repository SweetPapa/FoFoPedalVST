#pragma once

#include <juce_dsp/juce_dsp.h>

namespace vroom
{

// Pre/post filter chain around the Saturator. Phase 2: Pre-HPF (tightens lows
// before clipping) and DC blocker (removes the offset introduced by the
// asymmetric clipper). Body EQ and Tone LPF land in Phase 3.
class ToneStack
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();

    // Mode-driven config. Electric default: 90 Hz pre-HPF (spec §5).
    void setPreHPFHz (float hz);

    void processPre  (juce::AudioBuffer<float>& buffer) noexcept; // before saturator
    void processPost (juce::AudioBuffer<float>& buffer) noexcept; // after saturator

private:
    void updatePreHPF();
    void updateDCBlocker();

    juce::dsp::ProcessSpec spec {};
    float preHPFHz { 90.0f };

    std::vector<juce::dsp::IIR::Filter<float>> preHPF;
    std::vector<juce::dsp::IIR::Filter<float>> dcBlocker;
};

} // namespace vroom
