#pragma once
#include <juce_dsp/juce_dsp.h>
#include "spt/FableVerb.h"
#include "spt/Shapers.h"

namespace bkpr
{

// BACKPORCH — "Sounds produced, not wet."
//
// A short, dark production space where the mix-hygiene is the identity, not
// a feature panel: the send is high-passed at 150 Hz (always), the tail is
// pre-delayed so it sits BEHIND the source, and a ducker keyed by the dry
// signal pushes the tail down while you play and lets it breathe back in
// the gaps. You should be able to put this on a lead vocal at MIX 50% and
// never worry about it.
//
//   SPACE — size + decay macro (per mode)
//   TONE  — dark ↔ bright tilt of the tail only
//   DUCK  — how much the tail hides while you play (default 35%)
//   MIX   — Soundtoys curve: dry holds at unity until 70%
//
// Modes: SLAP (one dark echo + a cup of room) / ROOM / PLATE.
class BackporchEngine
{
public:
    enum class Mode { Slap = 0, Room = 1, Plate = 2 };

    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();

    void setSpace01 (float v) noexcept { space01 = juce::jlimit (0.0f, 1.0f, v); }
    void setTone01  (float v) noexcept { tone01  = juce::jlimit (0.0f, 1.0f, v); }
    void setDuck01  (float v) noexcept { duck01  = juce::jlimit (0.0f, 1.0f, v); }
    void setMix01   (float v) noexcept { mix01   = juce::jlimit (0.0f, 1.0f, v); }
    void setMode    (Mode m)  noexcept { mode = m; }

    int  getLatencySamples() const noexcept { return 0; }

    void process (juce::AudioBuffer<float>& buffer) noexcept;

    float fetchInputPeak()  noexcept { return inputPeak .exchange (0.0f); }
    float fetchOutputPeak() noexcept { return outputPeak.exchange (0.0f); }

private:
    juce::dsp::ProcessSpec spec {};

    Mode  mode    { Mode::Room };
    float space01 { 0.45f };
    float tone01  { 0.45f };
    float duck01  { 0.35f };
    float mix01   { 0.40f };

    // Send conditioning (identity, not options).
    juce::dsp::IIR::Filter<float> sendHP[2];
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> preDelay[2];
    int preMaxSamp { 0 };

    spt::FableVerb verb;

    // Slap voice: one dark echo feeding (and beside) the small room.
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> slapLine[2];
    juce::dsp::IIR::Filter<float> slapLP[2];
    int slapMaxSamp { 0 };
    juce::SmoothedValue<float> slapTimeSm;

    spt::TiltEQ tailTilt[2];
    spt::EnvFollower duckEnv;

    juce::AudioBuffer<float> drySnap;
    juce::AudioBuffer<float> wet;
    juce::AudioBuffer<float> verbScratch; // slap mode: room rendered beside the echo

    std::atomic<float> inputPeak  { 0.0f };
    std::atomic<float> outputPeak { 0.0f };
};

} // namespace bkpr
