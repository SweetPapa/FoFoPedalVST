#pragma once
#include <juce_dsp/juce_dsp.h>
#include "spt/Shapers.h"

namespace daydream
{

// Tape colour stage, v2. Two ingredients that a plain tanh doesn't have:
//   • One-sample-feedback hysteresis: y = tanh(g·x − k·y₁). Output depends
//     on history → program-dependent even/odd harmonic blend and a soft
//     "weight" that plain memoryless tanh never develops.
//   • Self-erasure: an envelope follower (2 ms / 60 ms) pulls a one-pole
//     high-cut down from 16 kHz toward ~7 kHz as the signal gets hot —
//     loud passages dull and squash exactly the way tape does.
// `drive` 0..1; 0 = transparent.
class TapeSat
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();
    void setDrive01 (float v) noexcept { driveSmoothed.setTargetValue (juce::jlimit (0.0f, 1.0f, v)); }
    void process   (juce::AudioBuffer<float>& buffer) noexcept;

private:
    juce::dsp::ProcessSpec spec {};
    juce::SmoothedValue<float> driveSmoothed { 0.0f };

    std::vector<spt::TapeHysteresis> hyst;
    std::vector<spt::OnePoleLP>      eraseLP;
    std::vector<spt::DCBlocker>      dc;
    spt::EnvFollower hotEnv;
};

} // namespace daydream
