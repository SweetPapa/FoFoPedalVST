#pragma once
#include "fofo/Fofo.h"
#include <atomic>

namespace bkpr
{

// ─────────────────────────────────────────────────────────────────────────────
// BACKPORCH — "Sounds produced, not wet."
//
// Rebuilt on the FoFoDriver kernel. v1 was the strongest of the six — the
// ducking and the mix curve were already right — but it had one structural
// gap and one inherited defect:
//
//   • No early reflections. It ran a Dattorro tank and nothing else, so it
//     made a *tail* rather than a *place*. The first ~50 ms of a real room is
//     a handful of discrete reflections off nearby surfaces, and that pattern
//     is most of what tells a listener how big the space is. This was the
//     cheapest large improvement left in the catalogue.
//   • A cubic soft clip sat on the dry+wet sum at base rate (F9), adding
//     third-harmonic distortion and aliasing to the dry path on hot sources.
//
// v2 is early reflections into an eight-line FDN with per-band decay. The
// per-band part is what the identity depends on: a single damping lowpass —
// all v1 had — can make highs die sooner but cannot stop low end piling up,
// and low end piling up is exactly what makes a reverb swamp a track. Now the
// lows die faster than the mids by default, which is what "produced, not wet"
// actually means in DSP terms.
//
// Controls are unchanged, so the UI and every saved preset still apply:
//
//   SPACE — size and decay together
//   TONE  — tilt of the tail, dark to bright
//   DUCK  — how far the tail gets out of the way while you play
//   MIX   — Soundtoys curve: wet reaches unity at 70%, then dry comes down
//
// Modes: SLAP (one dark repeat plus a hint of room) / ROOM (early field
// forward, short tail) / PLATE (no discrete early field, dense bright tail).
// ─────────────────────────────────────────────────────────────────────────────
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
    void applyParams();

    fofo::Spec spec {};

    Mode  mode    { Mode::Room };
    float space01 { 0.45f };
    float tone01  { 0.50f };
    float duck01  { 0.35f };
    float mix01   { 0.35f };

    // Send conditioning: the wet path is high-passed before it ever reaches
    // the tank, so low end never gets a chance to accumulate in the tail.
    fofo::Svf       sendHp[2];
    fofo::DelayLine preDelay[2];
    float           preSamp { 1.0f };

    fofo::EarlyReflections early;
    float                  earlyGain { 0.6f };

    fofo::Fdn8 tank;
    float      tankGain { 1.0f };

    // Slap: one dark discrete repeat, which IS the effect in that mode.
    fofo::DelayLine slapLine[2];
    fofo::Svf       slapDark[2];
    float           slapSamp { 4800.0f }, slapFeedback { 0.0f }, slapGain { 0.0f };

    fofo::Svf tailTiltLow[2], tailTiltHigh[2];

    // Ducking, keyed by the mono-summed dry so both channels move together.
    fofo::ModMatrix           duckMod;
    fofo::ModMatrix::SourceId sDuckEnv {};
    fofo::ModMatrix::DestId   dDuck {};
    int                       rDuck {};

    juce::AudioBuffer<float> drySnap, wet;

    std::atomic<float> inputPeak  { 0.0f };
    std::atomic<float> outputPeak { 0.0f };
};

} // namespace bkpr
