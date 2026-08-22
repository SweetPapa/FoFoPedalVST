#pragma once
#include "fofo/Fofo.h"

namespace fofopedal
{

// Mod block, v3 — rebuilt on the FoFoDriver kernel.
//
// The phaser is the reason this block is worth rebuilding. v2 hand-rolled six
// first-order allpass stages with their own state, which is the only kind of
// filter the old toolkit could offer (F6) — so the notches could sweep but
// nothing could resonate, and a phaser without resonance is a tone wobble.
// The stages are now zero-delay-feedback allpasses from fofo::Svf, which stay
// stable while their cutoff is swept at audio rate and have real feedback
// resonance around the notches.
//
// All three algorithms take their modulation from one ModMatrix, so the drift
// that keeps them from sounding mechanical is the same drift everywhere
// rather than a private LFO per block (F7).
class Mod
{
public:
    enum class Algo { Chorus = 0, Phaser = 1, TremVib = 2, NumAlgos = 3 };

    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();

    void setAlgo       (Algo  a) noexcept { algo = a; }
    void setRate01     (float v) noexcept { rate01     = juce::jlimit (0.0f, 1.0f, v); }
    void setDepth01    (float v) noexcept { depth01    = juce::jlimit (0.0f, 1.0f, v); }
    void setShape01    (float v) noexcept { shape01    = juce::jlimit (0.0f, 1.0f, v); }
    void setFeedback01 (float v) noexcept { feedback01 = juce::jlimit (0.0f, 1.0f, v); }
    void setMix01      (float v) noexcept { mix01      = juce::jlimit (0.0f, 1.0f, v); }
    void setBypassed   (bool  b) noexcept { bypassed = b; }

    void process (juce::AudioBuffer<float>& buffer) noexcept;

private:
    void applyParams();

    fofo::Spec spec {};

    Algo  algo       { Algo::Chorus };
    float rate01     { 0.30f };
    float depth01    { 0.50f };
    float shape01    { 0.50f };
    float feedback01 { 0.20f };
    float mix01      { 0.40f };
    bool  bypassed   { false };

    static constexpr int kVoices = 3;
    fofo::DelayLine chorusLine[kVoices];
    fofo::Svf       chorusDark[kVoices];

    static constexpr int kStages = 6;
    fofo::Svf phaserAp[2][kStages];
    float     phaserFb[2] { 0.0f, 0.0f };

    fofo::DelayLine vibLine[2];

    fofo::ModMatrix           mod;
    fofo::ModMatrix::SourceId sVoice[kVoices] {}, sDrift[kVoices] {}, sPhaser {}, sTrem {};
    fofo::ModMatrix::DestId   dVoice[kVoices] {}, dPhaser {}, dTrem {};
    int                       rVoice[kVoices] {}, rDrift[kVoices] {}, rPhaser {}, rTrem {};

    juce::AudioBuffer<float> drySnap;
};

} // namespace fofopedal
