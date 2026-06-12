#pragma once
#include <juce_dsp/juce_dsp.h>

namespace fofopedal
{

// Hidden post-bus that gives FOFOPEDAL its "all of a piece" character.
// Per spec §E: "running the entire wet output through a subtle shared 'voice'
// — tape-style HF rolloff at ~12 kHz, slight 3rd-harmonic saturation, a touch
// of bus compression — makes the whole pedal feel like one instrument rather
// than six." Single AMOUNT param (0..1), defaults ~0.30. Defeatable.
class OutputGlue
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();

    void setAmount01 (float v) noexcept;
    void setDefeated (bool  b) noexcept { defeated = b; }

    void process (juce::AudioBuffer<float>& buffer) noexcept;

private:
    void rebuildCoefficients();

    juce::dsp::ProcessSpec spec {};
    std::vector<juce::dsp::IIR::Filter<float>> hfRolloff;
    std::vector<float> compEnv;
    float ampAtk { 0.0f };
    float ampRel { 0.0f };
    float threshLin { 1.0f };
    float ratioInv  { 1.0f };
    float makeup    { 1.0f };

    float amount   { 0.30f };
    bool  defeated { false };
    bool  dirty    { true };
};

} // namespace fofopedal
