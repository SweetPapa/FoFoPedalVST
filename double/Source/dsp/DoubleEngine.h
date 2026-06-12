#pragma once
#include <juce_dsp/juce_dsp.h>
#include "spt/GrainShifter.h"
#include "spt/DriftLFO.h"
#include "spt/Shapers.h"

namespace dbl
{

// DOUBLE — "Every take you didn't record."
//
// Up to four micro-detuned grain-shifted voices, each with its own slow
// random drift on pitch, timing and level — the humanization is the whole
// point: a static detune sounds like a chorus pedal, a *wandering* detune
// sounds like a player who did another take.
//
//   THICK — detune amount + brings voices 3/4 in past halfway
//   WIDE  — stereo spread of the voices (fold to centre stays mono-safe)
//   HUMAN — how much the takes wander (pitch ±4 cents, timing ±8 ms, level)
//   MIX   — additive: dry stays untouched, doubles layer on top
//
// Mode voices the wet bus for the source: VOX / STRINGS / SYNTH.
// Wet path is HPF'd ≥120 Hz always — doubles never add mud.
class DoubleEngine
{
public:
    enum class Mode { Vox = 0, Strings = 1, Synth = 2 };
    static constexpr int kVoices = 4;

    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();

    void setThick01 (float v) noexcept { thick01 = juce::jlimit (0.0f, 1.0f, v); }
    void setWide01  (float v) noexcept { wide01  = juce::jlimit (0.0f, 1.0f, v); }
    void setHuman01 (float v) noexcept { human01 = juce::jlimit (0.0f, 1.0f, v); }
    void setMix01   (float v) noexcept { mix01   = juce::jlimit (0.0f, 1.0f, v); }
    void setMode    (Mode m)  noexcept { if (m != mode) { mode = m; modeDirty = true; } }

    int  getLatencySamples() const noexcept { return 0; }

    void process (juce::AudioBuffer<float>& buffer) noexcept;

    float fetchInputPeak()  noexcept { return inputPeak .exchange (0.0f); }
    float fetchOutputPeak() noexcept { return outputPeak.exchange (0.0f); }

private:
    void updateModeVoicing();

    juce::dsp::ProcessSpec spec {};

    Mode  mode    { Mode::Vox };
    bool  modeDirty { true };
    float thick01 { 0.5f };
    float wide01  { 0.7f };
    float human01 { 0.5f };
    float mix01   { 0.6f };

    spt::GrainShifter shifter[kVoices];
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> voiceDelay[kVoices];
    spt::DriftWalk pitchDrift[kVoices];
    spt::DriftWalk timeDrift[kVoices];
    spt::DriftWalk levelDrift[kVoices];
    juce::SmoothedValue<float> ratioSm[kVoices];
    juce::SmoothedValue<float> gainSm[kVoices];

    // Wet-bus voicing per mode.
    juce::dsp::IIR::Filter<float> wetHP[2], wetLP[2], wetDip[2];

    juce::AudioBuffer<float> wetBus;   // 2ch
    juce::AudioBuffer<float> monoSrc;  // 1ch doubling source

    std::atomic<float> inputPeak  { 0.0f };
    std::atomic<float> outputPeak { 0.0f };
};

} // namespace dbl
