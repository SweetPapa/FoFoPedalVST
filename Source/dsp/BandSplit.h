#pragma once

#include <juce_dsp/juce_dsp.h>

namespace vroom
{

// Linkwitz-Riley 4th-order crossover used by Bass mode (spec §5). The wet
// chain only drives the high band; the low band passes clean. LR4 = two
// cascaded 2nd-order Butterworth filters per band, summed back into an
// all-pass — keeps the recombination phase-coherent across the crossover.
class BandSplit
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();

    void setCrossoverHz (float hz);
    float getCrossoverHz() const noexcept { return crossoverHz; }

    // `input` becomes the high band; the low band is written to `lowOut`.
    // lowOut is resized as needed.
    void split (juce::AudioBuffer<float>& input,
                juce::AudioBuffer<float>& lowOut) noexcept;

private:
    void updateCoefficients();

    juce::dsp::ProcessSpec spec {};
    float crossoverHz { 180.0f };

    std::vector<juce::dsp::IIR::Filter<float>> lpf1, lpf2; // LR4 low band
    std::vector<juce::dsp::IIR::Filter<float>> hpf1, hpf2; // LR4 high band
};

} // namespace vroom
