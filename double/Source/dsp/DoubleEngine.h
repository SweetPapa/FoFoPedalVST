#pragma once
#include "fofo/Fofo.h"
#include "fofo/Pitch.h"
#include <atomic>

namespace dbl
{

// ─────────────────────────────────────────────────────────────────────────────
// DOUBLE — "Every take you didn't record."
//
// Rebuilt on the FoFoDriver kernel. v1's own header said it best: "a static
// detune sounds like a chorus pedal, a wandering detune sounds like a player
// who did another take." It then implemented the first one, because the random
// walks driving pitch and level were advanced once per audio BLOCK while their
// coefficients were derived from the sample rate — dividing their effective
// corner by the block size and freezing each voice at a constant offset.
//
// That was patched in place first (it was too damaging to leave), but the
// patch could not stop it recurring. Here the drift runs through the kernel's
// ModMatrix, which is prepared with one control rate and ticked only by
// itself, so the mismatch is no longer expressible.
//
// The other half is F8: every voice was pitched by a grain shifter reading its
// tap with linear interpolation, whose moving read pointer imposes amplitude-
// modulated high-frequency loss. Four of them summed compounds it. Voices now
// use fofo::PitchShifter, which reads with cubic Hermite.
//
// Controls are unchanged:
//
//   THICK — detune spread, and brings voices 3 and 4 in past halfway
//   WIDE  — stereo spread of the voices, folding to centre for mono safety
//   HUMAN — how far the takes wander in pitch, timing and level
//   MIX   — additive: the dry is never attenuated to make room for the doubles
//
// Modes voice the wet bus for the source: VOX / STRINGS / SYNTH. The wet bus
// is always high-passed, so doubles never add mud, and the dry path is never
// touched — not by a mix rule, and not by a clipper.
// ─────────────────────────────────────────────────────────────────────────────
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
    void applyParams();

    fofo::Spec spec {};

    Mode  mode      { Mode::Vox };
    bool  modeDirty { true };
    float thick01   { 0.5f };
    float wide01    { 0.7f };
    float human01   { 0.5f };
    float mix01     { 0.6f };

    fofo::PitchShifter shifter[kVoices];
    fofo::DelayLine    voiceDelay[kVoices];

    // One matrix for every voice's humanisation. Three destinations per voice
    // — pitch, timing, level — each fed by its own drift source.
    fofo::ModMatrix           human;
    fofo::ModMatrix::SourceId sPitch[kVoices] {}, sTime[kVoices] {}, sLevel[kVoices] {};
    fofo::ModMatrix::DestId   dPitch[kVoices] {}, dTime[kVoices] {}, dLevel[kVoices] {};
    int                       rPitch[kVoices] {}, rTime[kVoices] {}, rLevel[kVoices] {};

    juce::SmoothedValue<float> ratioSm[kVoices];
    juce::SmoothedValue<float> gainSm[kVoices];
    juce::SmoothedValue<float> busNormSm;

    fofo::Svf wetHp[2], wetDip[2], wetLp[2];

    juce::AudioBuffer<float> wetBus, monoSrc;

    std::atomic<float> inputPeak  { 0.0f };
    std::atomic<float> outputPeak { 0.0f };
};

} // namespace dbl
