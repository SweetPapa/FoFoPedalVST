#pragma once
#include <juce_dsp/juce_dsp.h>

namespace daydream
{

// Tape-style pitch drift: a fractional delay line with read position
// modulated by two LFOs:
//   - Wow:     ~0.4 Hz, broad pitch wobble (up to ±3 ms = ~½ semitone)
//   - Flutter: ~6.5 Hz, fine "warble" (up to ±0.5 ms)
// Stereo phase is offset 90° per channel so the modulation also widens
// the stereo image at high settings.
class PitchDrift
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();

    void setWowDepth01     (float v) noexcept { wowDepthTarget     = juce::jlimit (0.0f, 1.0f, v); }
    void setFlutterDepth01 (float v) noexcept { flutterDepthTarget = juce::jlimit (0.0f, 1.0f, v); }

    void process (juce::AudioBuffer<float>& buffer) noexcept;

private:
    juce::dsp::ProcessSpec spec {};

    std::vector<juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd>> delays;
    std::vector<float> wowPhase;
    std::vector<float> flutterPhase;

    float wowDepthTarget     { 0.0f };
    float flutterDepthTarget { 0.0f };
    juce::SmoothedValue<float> wowDepth     { 0.0f };
    juce::SmoothedValue<float> flutterDepth { 0.0f };

    static constexpr float kWowHz       = 0.4f;
    static constexpr float kFlutterHz   = 6.5f;
    static constexpr float kCenterDelMs = 10.0f;
    static constexpr float kWowMaxMs    = 3.0f;
    static constexpr float kFlutterMaxMs = 0.5f;
};

} // namespace daydream
