#pragma once
#include "fofo/Fofo.h"
#include "fofo/Pitch.h"

namespace fofopedal
{

// Pitch block, v3 — rebuilt on the FoFoDriver kernel.
//
// v2's three algorithms all came out of one grain shifter that read its tap
// with linear interpolation (F8), whose moving read pointer imposes
// amplitude-modulated high-frequency loss — the "veiled and slightly grainy"
// quality. Micro-detune was hit hardest, because that construction cannot
// produce a small shift at all: see fofo/Pitch.h for the arithmetic.
class Pitch
{
public:
    enum class Algo { MicroDetune = 0, OctaveHarm = 1, Freeze = 2, NumAlgos = 3 };

    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();

    void setAlgo     (Algo  a) noexcept { if (a != algo) { algo = a; dirty = true; } }
    void setAmount01 (float v) noexcept { amount01 = juce::jlimit (0.0f, 1.0f, v); }
    void setShape01  (float v) noexcept { shape01  = juce::jlimit (0.0f, 1.0f, v); }
    void setMix01    (float v) noexcept { mix01    = juce::jlimit (0.0f, 1.0f, v); }
    void setBypassed (bool  b) noexcept { bypassed = b; }

    void process (juce::AudioBuffer<float>& buffer) noexcept;

private:
    void processShift  (juce::AudioBuffer<float>& b, int nS, float ratioL, float ratioR) noexcept;
    void processFreeze (juce::AudioBuffer<float>& b, int nS) noexcept;

    fofo::Spec spec {};

    Algo  algo     { Algo::MicroDetune };
    float amount01 { 0.5f };
    float shape01  { 0.5f };
    float mix01    { 0.5f };
    bool  bypassed { false };
    bool  dirty    { true };

    fofo::PitchShifter shifter[2];

    // Freeze: a captured loop with a crossfade, plus a drift so the held
    // sound is never mechanically static.
    juce::AudioBuffer<float> ring;
    int   ringSize  { 0 };
    int   writeHead { 0 };
    bool  frozen    { false };
    float fadeIn    { 0.0f };
    float readPos   { 0.0f };
    int   loopLen   { 0 }, xfade { 0 };

    fofo::ModMatrix           mod;
    fofo::ModMatrix::SourceId sEnv {}, sDrift {};
    fofo::ModMatrix::DestId   dEnv {}, dDrift {};

    juce::AudioBuffer<float> drySnap;
};

} // namespace fofopedal
