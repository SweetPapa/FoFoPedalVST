#pragma once
#include "fofo/Fofo.h"
#include "fofo/Pitch.h"

namespace fofopedal
{

// Space block, v3 — rebuilt on the FoFoDriver kernel.
//
// v2 was a Dattorro tank and nothing else, so every algorithm made a *tail*
// rather than a *place*, and a single damping lowpass meant low end could only
// pile up. The shimmer came from the grain shifter that read with linear
// interpolation, and its "Freeze" cousin from the same file (F8, F11).
//
// Now: early reflections in front of an eight-line FDN with per-band decay, so
// the algorithms are genuinely different spaces and the lows can be made to
// die sooner than the mids. Shimmer uses fofo::PitchShifter inside the tank's
// feedback, band-limited and soft-clipped in the loop, with feedback capped at
// 0.62 — a pitch shifter in a feedback path is the classic way to build an
// oscillator by accident.
class Space
{
public:
    enum class Algo { Plate = 0, Hall = 1, Room = 2, Shimmer = 3, NumAlgos = 4 };

    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();

    void setAlgo        (Algo  a) noexcept { if (a != algo) { algo = a; dirty = true; } }
    void setSize01      (float v) noexcept { size01 = juce::jlimit (0.0f, 1.0f, v); dirty = true; }
    void setMix01       (float v) noexcept { mix01  = juce::jlimit (0.0f, 1.0f, v); }
    void setPreDelayMs  (float ms) noexcept { preDelayMs = juce::jlimit (0.0f, 250.0f, ms); }
    void setSendHpHz    (float hz) noexcept { sendHpHz = juce::jlimit (40.0f, 400.0f, hz); dirty = true; }
    void setShimmer01   (float v) noexcept { shimmer01 = juce::jlimit (0.0f, 1.0f, v); }
    void setBypassed    (bool  b) noexcept { bypassed = b; }

    void process (juce::AudioBuffer<float>& buffer) noexcept;

private:
    void updateAll();

    fofo::Spec spec {};

    Algo  algo        { Algo::Hall };
    float size01      { 0.50f };
    float mix01       { 0.30f };
    float preDelayMs  { 25.0f };
    float sendHpHz    { 90.0f };
    float shimmer01   { 0.0f };
    bool  bypassed    { false };
    bool  dirty       { true };

    fofo::Svf       sendHp[2];
    fofo::DelayLine pre[2];

    fofo::EarlyReflections early;
    float                  earlyGain { 0.0f };
    fofo::Fdn8             tank;

    fofo::PitchShifter shifter[2];
    fofo::Svf          shimHp[2], shimLp[2];
    float              shimFb[2] { 0.0f, 0.0f };

    // Ducking keyed by the block input, mono-summed so both sides move as one.
    fofo::ModMatrix           duckMod;
    fofo::ModMatrix::SourceId sDuck {};
    fofo::ModMatrix::DestId   dDuck {};
    int                       rDuck {};

    juce::AudioBuffer<float> drySnap;
};

} // namespace fofopedal
