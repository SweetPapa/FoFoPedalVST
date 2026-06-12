#pragma once
#include <juce_dsp/juce_dsp.h>
#include "TapeSat.h"
#include "PitchDrift.h"
#include "ShimmerReverb.h"
#include "spt/Shapers.h"
#include "spt/DriftLFO.h"

namespace daydream
{

// Single-knob ambient/lo-fi character engine, v3.
//
// Chain:
//   in ─► HPF(80) ─► TapeSat ─► PitchDrift (wow/flutter) ─► Chorus
//        ─► (+ noise floor) ─► ShimmerReverb ─► duck vs dry ─┐
//   └────────────────────── dry tap ────────────────────────┤
//                                                  outer mix ─► LPF ─► out
//
// The DREAM knob is staged as a journey, not a level:
//   0.00–0.05  true bypass feel
//   0.05–0.35  "warm tape" — saturation blooms in, a small room opens up
//   0.35–0.65  "memory" — wow/flutter wobbles, chorus widens, the room
//              becomes a hall
//   0.65–1.00  "dream" — shimmer climbs in octaves, decay approaches
//              infinite, the wet ducks under your playing and swells back
//              in the gaps, the top end gauzes over
class DaydreamEngine
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();

    void setDream01 (float v) noexcept { dreamTarget = juce::jlimit (0.0f, 1.0f, v); }

    int  getLatencySamples() const noexcept { return 0; }

    void process (juce::AudioBuffer<float>& buffer) noexcept;

    float fetchInputPeak()  noexcept { return inputPeakMax .exchange (0.0f); }
    float fetchOutputPeak() noexcept { return outputPeakMax.exchange (0.0f); }

private:
    void updateMacros (float k01);

    static float smoothstep (float a, float b, float x) noexcept
    {
        const float t = juce::jlimit (0.0f, 1.0f, (x - a) / juce::jmax (1.0e-6f, b - a));
        return t * t * (3.0f - 2.0f * t);
    }

    juce::dsp::ProcessSpec spec {};

    std::vector<juce::dsp::IIR::Filter<float>> inputHPF;

    TapeSat                  tapeSat;
    PitchDrift               drift;
    juce::dsp::Chorus<float> chorus;
    ShimmerReverb            reverb;

    std::vector<juce::dsp::IIR::Filter<float>> outputLPF;
    float lastLPFhz { 0.0f };

    juce::AudioBuffer<float> dryBuffer;

    // Ducking: the wet wash gets out of the way while you play and swells
    // back in the gaps — the trick that keeps high DREAM settings usable.
    spt::EnvFollower duckEnv;
    float duckAmount { 0.0f };

    // Noise floor texture: ~−70 dB filtered noise into the reverb input,
    // scaled up slightly by playing energy. "The pedal is powered on."
    spt::DriftWalk noiseSrc;
    spt::OnePoleLP noiseLP;
    float noiseGain { 0.0f };

    juce::SmoothedValue<float> dryGainSmoothed { 1.0f };
    juce::SmoothedValue<float> wetGainSmoothed { 0.0f };

    float dreamTarget { 0.35f };

    std::atomic<float> inputPeakMax  { 0.0f };
    std::atomic<float> outputPeakMax { 0.0f };
};

} // namespace daydream
