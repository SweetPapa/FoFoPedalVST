#pragma once

#include <juce_dsp/juce_dsp.h>
#include "spt/Shapers.h"

namespace vroom
{

// Oversampled, program-dependent saturation stage.
//
// What separates this from a bare tanh:
//   • Per-voice pre/post emphasis EQ — the tone character of each voice lives
//     in how the spectrum is tilted INTO the nonlinearity and untitled after
//     (Decapitator-style), not just in the clip curve.
//   • Envelope-driven asymmetry + headroom: an input follower shifts the bias
//     point and drops the available headroom with playing intensity (cathode
//     bias shift / supply sag). Hit hard → more even harmonics + compression;
//     let it ring → cleans up. Controlled by the Sag knob.
//   • First-order ADAA on the tube curve + polyphase oversampling.
//   • Gain-compensated drive: the knob changes texture, not just loudness
//     (≈78% loudness compensation so some reward remains).
class Saturator
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();

    // factorPower: 1 = 2x, 2 = 4x, 3 = 8x. Defaults to 4x.
    void setOversamplingFactorPower (int factorPower);

    // UI-normalised values (0..1).
    void setDrive01 (float v) noexcept     { driveUI01 = juce::jlimit (0.0f, 1.0f, v); }
    void setCharacter01 (float v) noexcept { charUI01  = juce::jlimit (0.0f, 1.0f, v); }

    // Sag depth — drives the envelope→bias and envelope→headroom modulation.
    // (The post-clip volume bloom lives in the separate Sag block; this is
    // the half that makes the clipper itself respond to playing.)
    void setSagDepth01 (float v) noexcept { sagDepth01 = juce::jlimit (0.0f, 1.0f, v); }

    // Mode-driven scalar on the max drive (Acoustic ≈ 0.6).
    void setDriveCeilingScale (float s) noexcept { driveCeilingScale = juce::jlimit (0.1f, 1.0f, s); }

    enum ClipShape
    {
        Shape_Smooth = 0,  // Tube — ADAA asymmetric tanh, bright-tilt emphasis
        Shape_Crunch = 1,  // Triode cubic w/ grid-conduction limit, presence emphasis
        Shape_Fuzz   = 2,  // High-gain squash, heavy asymmetry, dark post-voicing
        Shape_Octave = 3,  // Rectified path blended in, blend rides the envelope
        NumClipShapes
    };
    void setClipShape (int shape) noexcept { clipShape = juce::jlimit (0, (int) NumClipShapes - 1, shape); }

    int getLatencySamples() const noexcept;

    void process (juce::AudioBuffer<float>& buffer) noexcept;

private:
    void rebuildOversampler();
    void rebuildVoiceFilters();
    void updateInterstageCoefficients();

    static constexpr float kMaxDriveDb = 42.0f;
    static constexpr float kBiasMax    = 0.40f;
    static constexpr float kMakeupComp = 0.78f; // fraction of drive dB compensated

    juce::dsp::ProcessSpec spec {};
    int factorPower { 2 };  // 4x

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;

    juce::SmoothedValue<float> preGainSmoothed  { 1.0f };
    juce::SmoothedValue<float> makeupSmoothed   { 1.0f };
    juce::SmoothedValue<float> biasSmoothed     { 0.0f };

    // Per-voice emphasis EQ at base rate (pre = into the shaper, post = undo).
    std::vector<juce::dsp::IIR::Filter<float>> preEmph, postEmph;
    std::vector<juce::dsp::IIR::Filter<float>> interstageLPF; // at OS rate
    juce::dsp::IIR::Coefficients<float>::Ptr interstageCoefs;
    int builtVoice { -1 };

    // ADAA states: [channel][stage]
    std::vector<std::array<spt::ADAATanh, 2>> adaa;

    // Program-dependence: input envelope (attack 5 ms, release 150 ms).
    spt::EnvFollower sagEnv;
    std::vector<float> envScratch; // one env value per base sample

    float driveUI01 { 0.45f };
    float charUI01  { 0.60f };
    float sagDepth01 { 0.35f };
    float driveCeilingScale { 1.0f };
    int   clipShape { Shape_Smooth };
};

} // namespace vroom
