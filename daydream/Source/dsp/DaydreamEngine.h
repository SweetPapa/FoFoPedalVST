#pragma once
#include "fofo/Fofo.h"
#include "fofo/Pitch.h"
#include "fofo/Tape.h"
#include <atomic>

namespace daydream
{

// ─────────────────────────────────────────────────────────────────────────────
// DAYDREAM — "One knob, from warm tape to dream."
//
// Rebuilt on the FoFoDriver kernel. The one-knob staging was good design; it
// was sitting on thin primitives. Everything it was built from had a defect
// the audit named:
//
//   F6  every filter came from a first-order toolkit, so nothing anywhere in
//       the signal path could resonate or move
//   F8  the shimmer, the octave and the drift all came from one grain shifter
//       reading its tap with linear interpolation
//   F11 no spectral or early-reflection machinery of any kind, so the "room"
//       that opens up was a bare tank
//
// The knob's journey is unchanged, because that part was right:
//
//   0.00–0.05  effectively bypassed
//   0.05–0.35  warm tape — saturation blooms in, a small room opens
//   0.35–0.65  memory — wow and flutter wobble, the field widens, the room
//              becomes a hall
//   0.65–1.00  dream — shimmer climbs in octaves, the decay stretches toward
//              endless, the wash ducks under playing and swells back in the
//              gaps, and the top gauzes over
//
// What is new underneath: a real tape machine rather than a saturator (head
// bump, gap loss, self-erasure, hiss), early reflections in front of an FDN
// with per-band decay so the wash never turns to mud, a pitch shifter that
// does not veil the top end, and one modulation matrix driving all of it.
// ─────────────────────────────────────────────────────────────────────────────
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

    fofo::Spec spec {};

    fofo::Svf inputHp[2];

    // Tape: the "warm" third of the knob, and the wow that arrives with the
    // middle third.
    fofo::TapeSaturator tapeSat;
    fofo::TapeTransport transport;

    // Space: early reflections into an FDN, with a pitch shifter inside the
    // feedback path for the shimmer.
    fofo::EarlyReflections early;
    fofo::Fdn8             tank;
    fofo::PitchShifter     shimmer[2];
    fofo::Svf              shimHp[2], shimLp[2];
    float                  shimFbL { 0.0f }, shimFbR { 0.0f };

    fofo::Svf outputLp[2];

    // One matrix: transport wow, the widening chorus, the ducker, and the
    // noise floor all hang off it.
    fofo::ModMatrix           mod;
    fofo::ModMatrix::SourceId sWow {}, sWowR {}, sDuck {};
    fofo::ModMatrix::DestId   dWowL {}, dWowR {}, dDuck {};
    int                       rWowL {}, rWowR {}, rDuck {};

    // The transport advances the matrix once per sample, and the ducker keys
    // off the DRY signal — so the tick callback needs to reach the dry buffer
    // at the sample it is currently on.
    int          tickIndex { 0 };
    const float* tickDryL { nullptr };
    const float* tickDryR { nullptr };

    fofo::Rng noiseRng;
    fofo::Svf noiseLp;

    juce::AudioBuffer<float> drySnap, wetBus;

    juce::SmoothedValue<float> dryGainSm { 1.0f };
    juce::SmoothedValue<float> wetGainSm { 0.0f };

    float dreamTarget { 0.35f };
    float dreamSmoothed { 0.35f };

    // Macro-derived, recomputed per block.
    float tapeAmt { 0.0f }, wowMs { 0.0f }, shimAmt { 0.0f };
    float duckAmt { 0.0f }, noiseGain { 0.0f }, earlyAmt { 0.0f };

    std::atomic<float> inputPeakMax  { 0.0f };
    std::atomic<float> outputPeakMax { 0.0f };
};

} // namespace daydream
