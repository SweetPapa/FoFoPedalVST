#pragma once

#include <juce_dsp/juce_dsp.h>

namespace vroom
{

// Pre/post filter chain around the Saturator (spec §3 order):
//   Pre-HPF → [Saturator] → DC blocker → [Sag] → Body EQ → Tone LPF → ...
// The three post-saturator stages are exposed separately so the processor can
// interleave Sag between the DC blocker and Body/Tone without ToneStack having
// to know about it.
class ToneStack
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();

    // Mode-driven config (Electric defaults per spec §5).
    void setPreHPFHz   (float hz);
    void setBodyCenterHz (float hz);

    // UI knob values, 0..1 normalised.
    void setBody01 (float v); // -6 .. +12 dB peaking gain at body center
    void setTone01 (float v); // 1.5 kHz .. 12 kHz log low-pass cutoff

    void processPre        (juce::AudioBuffer<float>& buffer) noexcept; // before saturator
    void processDCBlock    (juce::AudioBuffer<float>& buffer) noexcept; // after saturator
    void processBodyAndTone(juce::AudioBuffer<float>& buffer) noexcept; // after Sag

private:
    void updatePreHPF();
    void updateDCBlocker();
    void updateBody();
    void updateTone();

    juce::dsp::ProcessSpec spec {};

    float preHPFHz     { 90.0f };  // Electric default
    float bodyCenterHz { 300.0f }; // Electric default

    float bodyUI01 { 0.55f };
    float toneUI01 { 0.50f };

    std::vector<juce::dsp::IIR::Filter<float>> preHPF;
    std::vector<juce::dsp::IIR::Filter<float>> dcBlocker;
    std::vector<juce::dsp::IIR::Filter<float>> bodyEQ;
    std::vector<juce::dsp::IIR::Filter<float>> toneLPF;
};

} // namespace vroom
