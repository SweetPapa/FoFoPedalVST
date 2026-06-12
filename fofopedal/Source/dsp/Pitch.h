#pragma once
#include <juce_dsp/juce_dsp.h>

namespace fofopedal
{

// Pitch / Texture block. Three modes share a two-grain Hann-windowed
// pitch-shifter substrate (lifted from DAYDREAM's OctaveUp, generalised to
// arbitrary ratio):
//
//   MicroDetune — ±25 cents stereo spread (L slightly sharp, R slightly flat,
//                 amount knob = cents). The "expensive vocal" trick.
//   OctaveHarm  — clean ±1 octave or scale-locked 3rd/5th (SHAPE picks).
//   Freeze      — input-threshold-triggered freeze pad with crossfaded loop
//                 boundaries and an LFO all-pass to keep the held pad alive.
//                 Above threshold the dry passes through; below threshold a
//                 frozen capture of the last ~1.5 s plays back.
//
// Output is mix-blended back with dry at the block boundary.
class Pitch
{
public:
    enum class Algo { MicroDetune = 0, OctaveHarm = 1, Freeze = 2, NumAlgos = 3 };

    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();

    void setAlgo     (Algo  a) noexcept { algo = a; }
    void setAmount01 (float v) noexcept { amount01 = juce::jlimit (0.0f, 1.0f, v); }
    void setShape01  (float v) noexcept { shape01  = juce::jlimit (0.0f, 1.0f, v); }
    void setMix01    (float v) noexcept { mix01    = juce::jlimit (0.0f, 1.0f, v); }
    void setBypassed (bool  b) noexcept { bypassed = b; }

    void process (juce::AudioBuffer<float>& buffer) noexcept;

private:
    void processGrainShifter (juce::AudioBuffer<float>& buffer, float ratioL, float ratioR) noexcept;
    void processFreeze       (juce::AudioBuffer<float>& buffer) noexcept;

    juce::dsp::ProcessSpec spec {};
    Algo  algo     { Algo::MicroDetune };
    float amount01 { 0.5f };
    float shape01  { 0.5f };
    float mix01    { 0.5f };
    bool  bypassed { false };

    // ── Two-grain pitch shifter ────────────────────────────────────────
    struct Grain { float readPos { 0.0f }; float phase { 0.0f }; };
    std::vector<std::array<Grain, 2>> grains;
    juce::AudioBuffer<float> grainBuffer;
    int  grainBufSize   { 0 };
    int  grainSize      { 0 };
    std::vector<int> grainWriteHead;

    juce::AudioBuffer<float> dryBuffer;

    // ── Freeze state ───────────────────────────────────────────────────
    juce::AudioBuffer<float> freezeRing;
    int   freezeRingSize { 0 };
    std::vector<int> freezeWriteHead;
    float envFollow     { 0.0f };
    float envAtk        { 0.0f };
    float envRel        { 0.0f };
    bool  frozen        { false };
    float frozenFadeIn  { 0.0f };   // 0..1 ramp when entering frozen state
    int   loopStart     { 0 };
    int   loopEnd       { 0 };
    int   loopXfade     { 0 };
    float freezeReadPos { 0.0f };
    float allpassState  { 0.0f };
    float allpassLfoPhase { 0.0f };
};

} // namespace fofopedal
