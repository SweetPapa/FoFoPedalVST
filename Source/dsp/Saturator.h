#pragma once

#include <juce_dsp/juce_dsp.h>

namespace vroom
{

// Oversampled, cascaded, asymmetric soft-clip stage. Per spec §7:
//   y = tanh(drive * (x + bias)) - tanh(drive * bias)
// Two cascaded clip stages with a mild interstage LPF, all wrapped in
// juce::dsp::Oversampling to suppress aliasing in the harmonics generated
// by the nonlinearity. DC blocker downstream (in ToneStack) removes the
// offset the bias introduces.
class Saturator
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();

    // factorPower: 1 = 2x, 2 = 4x, 3 = 8x. Defaults to 4x (spec §4).
    void setOversamplingFactorPower (int factorPower);

    // UI-normalised values (0..1).
    void setDrive01 (float v) noexcept     { driveUI01 = juce::jlimit (0.0f, 1.0f, v); }
    void setCharacter01 (float v) noexcept { charUI01  = juce::jlimit (0.0f, 1.0f, v); }

    // Total latency this block introduces, reported at base sample rate.
    int getLatencySamples() const noexcept;

    void process (juce::AudioBuffer<float>& buffer) noexcept;

private:
    void rebuildOversampler();
    void updateInterstageCoefficients();

    // Spec §12: drive ceiling (50.0) and asymmetry bias max (0.40) are
    // marked dial-to-taste. Centralised here so they're easy to tweak.
    static constexpr float kDriveCeiling = 50.0f;
    static constexpr float kBiasMax      = 0.40f;

    static float mapDrive (float ui01) noexcept;
    static float mapBias  (float ui01) noexcept;

    juce::dsp::ProcessSpec spec {};
    int factorPower { 2 };  // 4x

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;

    // Smoothed at base rate; sampled inside the oversampled loop. The slight
    // rate mismatch is inaudible and avoids zipper noise on knob moves.
    juce::SmoothedValue<float> driveSmoothed { 1.0f };
    juce::SmoothedValue<float> biasSmoothed  { 0.0f };

    // Interstage LPF, one filter per channel, runs at the oversampled rate.
    // Spec §7 calls for a 1st-order LPF ~8 kHz between stages to tame the
    // harmonic buildup before the second clip stage hits.
    std::vector<juce::dsp::IIR::Filter<float>> interstageLPF;
    juce::dsp::IIR::Coefficients<float>::Ptr interstageCoefs;

    float driveUI01 { 0.45f };
    float charUI01  { 0.60f };
};

} // namespace vroom
