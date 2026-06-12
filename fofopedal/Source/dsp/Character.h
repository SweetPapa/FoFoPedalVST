#pragma once
#include <juce_dsp/juce_dsp.h>

namespace fofopedal
{

// Always-on input conditioner / global voice. Series chain of:
//   1) Low-cut (hidden buddy of CHARACTER knob, 20..200 Hz, off at 20)
//   2) Opto-style compressor with program-dependent release
//   3) Tilt EQ around 800 Hz (light sheen, scales with amount)
//   4) Gentle low-shelf cleanup below ~100 Hz (fixed, tightens mud)
//   5) Tape-style high-cut above ~8 kHz (gets darker with amount)
//
// One macro (amount 0..1) drives compression depth + tape darkness, so the
// player can dial in the whole "produced front-end" feel from a single knob.
// At amount=0 the chain is near-transparent but still cleans subsonic mud.
// "Defeat" bypasses the entire block — for trust-building A/B comparison.
class Character
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();

    void setAmount01    (float v) noexcept;
    void setLowCutHz    (float hz) noexcept;
    void setDefeated    (bool  b) noexcept { defeated = b; }

    void process (juce::AudioBuffer<float>& buffer) noexcept;

    // Inspection — used by the engine to derive Voicing biases.
    float getAmount01() const noexcept { return amount; }
    bool  isDefeated()  const noexcept { return defeated; }

private:
    void updateCoefficients();

    juce::dsp::ProcessSpec spec {};

    std::vector<float> envState;
    float ampAtk        { 0.0f };
    float ampRelFast    { 0.0f };
    float ampRelSlow    { 0.0f };
    float threshLin     { 1.0f };
    float ratioInv      { 1.0f };
    const float kneeWidth { 6.0f };
    float makeupGain    { 1.0f };

    std::vector<juce::dsp::IIR::Filter<float>> lowCut;
    std::vector<juce::dsp::IIR::Filter<float>> tiltLow;
    std::vector<juce::dsp::IIR::Filter<float>> tiltHigh;
    std::vector<juce::dsp::IIR::Filter<float>> lowShelf;
    std::vector<juce::dsp::IIR::Filter<float>> hiCut;

    float amount   { 0.0f };
    float lowCutHz { 20.0f };
    bool  defeated { false };
};

} // namespace fofopedal
