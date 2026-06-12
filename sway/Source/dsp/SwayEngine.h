#pragma once
#include <juce_dsp/juce_dsp.h>
#include "spt/DriftLFO.h"
#include "spt/Shapers.h"

namespace sway
{

// SWAY — "Makes static tracks move like a band."
//
// A subtle-movement engine. Never an obvious effect: things just sound
// alive. All three modes share the same drifted-LFO core so even "perfect"
// settings wander like hardware.
//
//   MOVE  — the macro: how much total movement (depth of everything)
//   RATE  — speed (0.05–8 Hz, log)
//   COLOR — per-mode flavor:
//             Tape:     wow ↔ flutter balance
//             Ensemble: voice detune spread + width
//             Pump:     sine ↔ squashed duty-cycle trem shape
//   MIX   — crossfade (defaults to 100%: movement is a treatment, not a send)
//
// Modes: TAPE (wow/flutter pitch drift) / ENSEMBLE (tri-voice chorus) /
//        PUMP (breathing tremolo with a sympathetic high-end dip).
class SwayEngine
{
public:
    enum class Mode { Tape = 0, Ensemble = 1, Pump = 2 };

    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();

    void setMove01  (float v) noexcept { move01  = juce::jlimit (0.0f, 1.0f, v); }
    void setRate01  (float v) noexcept { rate01  = juce::jlimit (0.0f, 1.0f, v); }
    void setColor01 (float v) noexcept { color01 = juce::jlimit (0.0f, 1.0f, v); }
    void setMix01   (float v) noexcept { mix01   = juce::jlimit (0.0f, 1.0f, v); }
    void setMode    (Mode m)  noexcept { mode = m; }

    int  getLatencySamples() const noexcept { return 0; }

    void process (juce::AudioBuffer<float>& buffer) noexcept;

    float fetchInputPeak()  noexcept { return inputPeak .exchange (0.0f); }
    float fetchOutputPeak() noexcept { return outputPeak.exchange (0.0f); }

private:
    juce::dsp::ProcessSpec spec {};

    Mode  mode    { Mode::Tape };
    float move01  { 0.45f };
    float rate01  { 0.35f };
    float color01 { 0.50f };
    float mix01   { 1.00f };

    // Tape: one modulated line per channel (quadrature wow), flutter on top.
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> tapeLine[2];
    spt::DriftLFO wowLfo[2], flutterLfo[2];

    // Ensemble: three drifted voices (mono-fed, panned).
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> ensLine[3];
    spt::DriftLFO ensLfo[3];
    spt::OnePoleLP ensDark[3];

    // Pump: drifted trem + sympathetic tone dip.
    spt::DriftLFO pumpLfo;
    spt::OnePoleLP pumpDip[2];

    juce::AudioBuffer<float> drySnap;

    std::atomic<float> inputPeak  { 0.0f };
    std::atomic<float> outputPeak { 0.0f };
};

} // namespace sway
