#pragma once
#include <juce_dsp/juce_dsp.h>
#include "spt/Shapers.h"

namespace fofopedal
{

// Saturation block, v2. Three algorithms share a 4× polyphase-IIR
// oversampled nonlinear stage; each has its own curve AND its own
// program-dependence — static waveshapers are what made v1 boring.
//
//   Tube — first-order ADAA asymmetric tanh with grid-conduction limit on
//          the positive half. The bias point rides an input envelope
//          follower: dig in → more even harmonics + compression, let it
//          ring → cleans up.
//   Tape — one-sample-feedback hysteresis (y = tanh(g·x − k·y₁)) plus the
//          drive-tracking HF rolloff. History-dependent weight, soft squash.
//   Iron — only the sub-500 Hz band is driven hard via pre/de-emphasis
//          shelves; 3rd/5th-harmonic low-mid thickness, highs stay clean.
//
// Drive knob is dB-tapered and ~78% loudness-compensated so it changes
// texture, not just volume.
//
// Controls: DRIVE, TONE (post tilt), MIX (parallel wet/dry — Decapitator-style).
class Drive
{
public:
    enum class Algo { Tube = 0, Tape = 1, Iron = 2, NumAlgos = 3 };

    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();

    void setAlgo     (Algo a)   noexcept;
    void setDrive01  (float v)  noexcept { drive01 = juce::jlimit (0.0f, 1.0f, v); }
    void setTone01   (float v)  noexcept { tone01  = juce::jlimit (0.0f, 1.0f, v); markToneDirty(); }
    void setMix01    (float v)  noexcept { mix01   = juce::jlimit (0.0f, 1.0f, v); }
    void setBypassed (bool  b)  noexcept { bypassed = b; }

    int getLatencySamples() const noexcept;

    void process (juce::AudioBuffer<float>& buffer) noexcept;

private:
    void rebuildOversampler();
    void rebuildFilters();
    void updateDryDelay();
    void markToneDirty() noexcept { toneDirty = true; }

    juce::dsp::ProcessSpec spec {};
    std::unique_ptr<juce::dsp::Oversampling<float>> os;

    Algo  algo     { Algo::Tube };
    float drive01  { 0.0f };
    float tone01   { 0.5f };
    float mix01    { 1.0f };
    bool  bypassed { false };

    float lastTone { -1.0f };
    bool  toneDirty { true };

    // Iron uses pre-emphasis / de-emphasis low shelves around the shaper so
    // the low band is driven harder (generating the transformer 3rd/5th
    // harmonics) without a parallel band that would need latency matching.
    std::vector<juce::dsp::IIR::Filter<float>> ironPreShelf;
    std::vector<juce::dsp::IIR::Filter<float>> ironPostShelf;

    // Tape drive-tracking HF rolloff (native rate, post-OS).
    std::vector<juce::dsp::IIR::Filter<float>> tapeRolloff;
    float lastTapeHz { -1.0f };

    // Post-tone tilt — low-shelf + high-shelf @ 800 Hz, ganged opposite.
    std::vector<juce::dsp::IIR::Filter<float>> toneLow;
    std::vector<juce::dsp::IIR::Filter<float>> toneHigh;

    // Per-channel nonlinear state (ticks at the oversampled rate).
    std::vector<spt::ADAATanh>       adaa;
    std::vector<spt::TapeHysteresis> hyst;
    std::vector<spt::DCBlocker>      dcBlock;
    spt::EnvFollower touchEnv;        // base-rate input follower → bias drift
    std::vector<float> envScratch;    // one env value per base sample

    juce::AudioBuffer<float> dryBuffer;

    // The parallel MIX blend recombines the dry snapshot with a wet path that
    // has been through the oversampler — which delays it by a fractional
    // number of samples. Summing the two without matching that delay is a
    // comb filter, and since the global MIX macro scales this block's mix,
    // it would fire on every character below 100%. Delay the dry to match.
    // (Same fix VROOM already applies in its PluginProcessor.)
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> dryDelay { 64 };
};

} // namespace fofopedal
