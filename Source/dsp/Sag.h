#pragma once

#include <juce_dsp/juce_dsp.h>

namespace vroom
{

// Simple feed-forward "supply sag" emulation. Per-sample peak envelope follower
// drives a downward gain reduction; the release time stretches with the Sag
// knob so high settings produce the bloom/sustain of a tube amp whose power
// supply is being momentarily pulled down by a transient.
//
// This is intentionally light-weight — spec §12 marks the Sag response curve
// as dial-to-taste, so the shape will get refined by ear once everything else
// is in place.
class Sag
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();

    void setSag01 (float v) noexcept { sagAmount = juce::jlimit (0.0f, 1.0f, v); }

    void process (juce::AudioBuffer<float>& buffer) noexcept;

private:
    juce::dsp::ProcessSpec spec {};
    float sagAmount { 0.35f };
    std::vector<float> envelope; // one peak follower per channel
};

} // namespace vroom
