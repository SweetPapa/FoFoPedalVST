#pragma once
#include "fofo/Fofo.h"
#include <atomic>
#include <array>
#include <memory>

namespace drip
{

// ─────────────────────────────────────────────────────────────────────────────
// DREAMRIPPER — "The whole rig, from Seattle sludge to modern chug."
//
// The catalogue already has a dirt pedal. VROOM is a *saturator*: one clipper
// you push a clean signal through, voiced to land in a mix. DREAMRIPPER is the
// other thing entirely — an amplifier. What makes a high-gain amp sound like an
// amp is not the clip curve, it is everything arranged around it:
//
//   • A gate in front, because above ~30 dB of gain the noise floor of the
//     source is part of the instrument and has to be dealt with, not tolerated.
//   • A resonant highpass BEFORE the gain. Low end that reaches a clipper
//     intermodulates with everything above it, which is exactly the difference
//     between a chug and a flub. Every real metal rig tightens here first.
//   • CASCADED stages, not one. Two or three shapers in series with their own
//     coupling filters between them generate a completely different harmonic
//     structure from a single stage driven equally hard — the later stages are
//     clipping a signal that is already clipped and already band-limited.
//   • Mid EQ on both sides of the distortion. A mid boost *into* the gain is
//     more saturation and more cut; a mid scoop *after* it is the metal "V".
//     Same knob, opposite ends, and they are not interchangeable.
//   • A speaker. Nothing that skips the cab sounds like an amp, and the
//     3–6 kHz cliff with a resonant peak sitting on its edge is most of what
//     the ear identifies as "guitar".
//
// Everything above runs inside the FoFoDriver kernel, so the mix rule, the dry
// snapshot and the latency compensation are not this file's business:
//
//     Parallel(Blend, MIX)
//       └── Chain
//             ├── front end   gate → tight HP → bite → mid push → sag
//             ├── Oversampled 4×   2–3 ADAA stages + interstage filters
//             └── back end    mid scoop → cab → level
//
// Controls:
//
//   RIP    how hard the stages are driven. Loudness-compensated, so it is a
//          texture control, not a volume control.
//   TIGHT  how much low end is allowed into the gain — flub to chug.
//   SCOOP  the mid axis. Below 50 the mids are pushed into the gain (grunge,
//          stoner, lead); above 50 they are scooped out after it (metal).
//   CAB    speaker character, from a dark closed-back 4×12 to a modern bright
//          one. It moves the cliff, the presence peak and the fizz notch
//          together, because on a real cab those are one physical object.
//   LEVEL  the amp's master, 50 = unity, ±20 dB.
//   GATE   the noise gate's threshold. 0 = the gate is out of the circuit.
//   MIX    how much pedal. 100 (the default) is the amp; below that the
//          untouched dry comes back underneath, which is how you keep the low
//          end of a bass while destroying everything above it.
//
// The four modes are four different amplifiers, not four tone presets: they
// change the number of gain stages, where the coupling filters sit, how
// asymmetric each stage is, and how stiff the power supply is.
// ─────────────────────────────────────────────────────────────────────────────

// ─── Noise gate ──────────────────────────────────────────────────────────────
//
// A downward gate rather than an expander: high gain wants the noise gone
// between notes, not merely quieter. Stereo-linked by construction — there is
// one detector fed the louder of the two channels, so a gate can never open on
// one side and stay shut on the other and pull the image sideways.
//
// The three things that make a gate musical rather than a stutter box:
//   • a soft knee, so a decaying note fades out instead of being cut off;
//   • a hold, so the detector dipping through zero between two cycles of a low
//     note does not slam the gate;
//   • a fast open and a slow close.
class NoiseGate
{
public:
    void prepare (double sampleRate) noexcept
    {
        sr = sampleRate;
        recalc();
        reset();
    }

    void reset() noexcept
    {
        detector = 0.0f;
        gain     = 1.0f;
        holdLeft = 0;
    }

    // -90 dB or below is treated as "the gate is not in the circuit".
    void setThresholdDb (float db) noexcept
    {
        thresholdDb = db;
        active = db > -89.0f;
    }

    // How hard the gate is being asked to work, 0..1. Turning a gate up should
    // make it both more sensitive AND faster — a threshold that rises while
    // the release stays long gives you a gate that clamps onto quiet passages
    // and then takes half a second to admit it. One knob, both behaviours.
    void setAggression (float amount01) noexcept
    {
        aggression = juce::jlimit (0.0f, 1.0f, amount01);
        recalc();
    }

    bool isActive() const noexcept { return active; }
    float currentGain() const noexcept { return gain; }

    // One sample. `key` is the stereo-linked detector input; the caller applies
    // the returned gain to both channels.
    inline float processGain (float key) noexcept
    {
        if (! active) { gain = 1.0f; return 1.0f; }

        const float rectified = std::abs (key);
        detector = rectified > detector
                     ? detAtk * detector + (1.0f - detAtk) * rectified
                     : detRel * detector + (1.0f - detRel) * rectified;

        const float db = 20.0f * std::log10 (detector + 1.0e-9f);

        // Soft knee: fully shut a knee below the threshold, fully open at it,
        // smoothstep in between so the transition has no corner in it.
        float target;
        if (db >= thresholdDb)                    target = 1.0f;
        else if (db <= thresholdDb - kKneeDb)     target = 0.0f;
        else
        {
            const float u = (db - (thresholdDb - kKneeDb)) / kKneeDb;
            target = u * u * (3.0f - 2.0f * u);
        }

        // Hold. Once open, stay open for a while — otherwise the detector
        // passing through a trough inside one cycle of a low note chatters.
        if (target > 0.5f) holdLeft = holdSamples;
        else if (holdLeft > 0) { --holdLeft; target = juce::jmax (target, gain); }

        const float a = target > gain ? gainAtk : gainRel;
        gain = a * gain + (1.0f - a) * target;
        return gain;
    }

private:
    static constexpr float kKneeDb = 11.0f;

    void recalc() noexcept
    {
        if (sr <= 0.0) return;
        auto coef = [this] (float ms) { return std::exp (-1.0f / juce::jmax (1.0e-6f, 0.001f * ms * (float) sr)); };

        detAtk  = coef (0.20f);

        // The detector's release, not the gain's, is what decides how long
        // after the last note the gate even starts to move: it has to fall
        // from the note's level all the way to the threshold first. At 28 ms
        // that took 160 ms of silence before anything happened, which reads
        // as a gate that does not work.
        detRel  = coef (5.0f);

        gainAtk = coef (0.60f);     // opens inside one period of a low string
        gainRel = coef (juce::jmap (aggression, 90.0f, 18.0f));
        holdSamples = (int) (0.001 * juce::jmap (aggression, 60.0f, 12.0f) * sr);
    }

    double sr { 48000.0 };
    float thresholdDb { -90.0f };
    float aggression { 0.5f };
    bool  active { false };

    float detAtk { 0.0f }, detRel { 0.0f }, gainAtk { 0.0f }, gainRel { 0.0f };
    int   holdSamples { 0 }, holdLeft { 0 };
    float detector { 0.0f }, gain { 1.0f };
};

// ─────────────────────────────────────────────────────────────────────────────
class DreamRipperEngine
{
public:
    enum class Mode { Sludge = 0, Grunge = 1, Metal = 2, Djent = 3 };
    static constexpr int kNumModes  = 4;
    static constexpr int kMaxStages = 3;

    DreamRipperEngine();
    ~DreamRipperEngine();

    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();

    void setRip01   (float v) noexcept { rip01   = juce::jlimit (0.0f, 1.0f, v); }
    void setTight01 (float v) noexcept { tight01 = juce::jlimit (0.0f, 1.0f, v); }
    void setScoop01 (float v) noexcept { scoop01 = juce::jlimit (0.0f, 1.0f, v); }
    void setCab01   (float v) noexcept { cab01   = juce::jlimit (0.0f, 1.0f, v); }
    void setLevel01 (float v) noexcept { level01 = juce::jlimit (0.0f, 1.0f, v); }
    void setGate01  (float v) noexcept { gate01  = juce::jlimit (0.0f, 1.0f, v); }
    void setMix01   (float v) noexcept { mix01   = juce::jlimit (0.0f, 1.0f, v); }
    void setMode    (Mode m)  noexcept { mode = m; }

    // Constant across modes on purpose: it comes only from the oversampler,
    // which never changes factor. A host compensates once and does not
    // re-query when a parameter moves, so a mode-dependent latency would
    // leave the track sitting early or late after a mode change.
    int getLatencySamples() const noexcept { return reportedLatency; }

    void process (juce::AudioBuffer<float>& buffer) noexcept;

    float fetchInputPeak()  noexcept { return inputPeak .exchange (0.0f); }
    float fetchOutputPeak() noexcept { return outputPeak.exchange (0.0f); }

    // 1 = the gate is open (or out of the circuit), 0 = fully shut. Drives the
    // gate lamp in the UI, which is the only way to tune a threshold by eye.
    float fetchGateGain() const noexcept { return gateGain.load (std::memory_order_relaxed); }

    // Per-mode amplifier definition. Public so the tests can assert against the
    // same table the audio path reads rather than against copied numbers.
    struct Voicing
    {
        const char* name;
        int   stages;             // cascaded gain stages
        float tightLoHz, tightHiHz, tightQ;
        float biteHz, biteDb;
        float midHz;
        float driveDbMax;         // total, distributed across the stages
        float bias;               // stage asymmetry at full RIP
        float interLoHz;          // coupling highpass between stages
        float interHiHz;          // coupling lowpass between stages
        float sagDepth;           // how much the supply droops when hit
        float cabHpHz, cabResHz, cabPresHz;
        float cabLpLoHz, cabLpHiHz;
        float makeupDb;           // static trim so the four modes match
        float ripCompDb;          // dB given back across the RIP sweep
    };

    static const Voicing& voicingFor (Mode m) noexcept;
    const Voicing& voicing() const noexcept { return voicingFor (mode); }

private:
    void buildGraph();
    void applyParams() noexcept;

    void  frontEnd (juce::AudioBuffer<float>& buffer, int numSamples) noexcept;
    float cascadeSample (float x, int channel) noexcept;
    void  backEnd  (juce::AudioBuffer<float>& buffer, int numSamples) noexcept;

    fofo::Spec spec {};

    Mode  mode    { Mode::Grunge };
    float rip01   { 0.55f };
    float tight01 { 0.45f };
    float scoop01 { 0.42f };
    float cab01   { 0.50f };
    float level01 { 0.50f };
    float gate01  { 0.35f };
    float mix01   { 1.00f };

    // ── per-channel state ────────────────────────────────────────────────
    struct Channel
    {
        // front end, base rate
        fofo::Svf tightHp, bite, midPush;
        // cascade, oversampled rate
        fofo::AdaaTanh  shaper[kMaxStages];
        fofo::Svf       interHp[kMaxStages], interLp[kMaxStages];
        fofo::DcBlocker stageDc[kMaxStages];
        // back end, base rate
        fofo::Svf       midScoop, cabHp, cabRes, cabDip, cabPres, cabLp1, cabLp2, cabFizz;
        fofo::DcBlocker outDc;
    };
    std::array<Channel, 2> chan {};

    NoiseGate gate;

    // Supply sag. One follower for both channels — a droop that happens on one
    // side only is a stereo image that moves when you dig in.
    float sagEnv { 0.0f }, sagAtk { 0.0f }, sagRel { 0.0f };
    float sagDepth { 0.0f };

    // Values the oversampled shaper reads. Set once per block by applyParams().
    int   activeStages { 2 };
    float stageGain[kMaxStages] { 1.0f, 1.0f, 1.0f };
    float stageBias { 0.0f };

    float preGain  { 1.0f };
    float postGain { 1.0f };

    std::unique_ptr<fofo::Parallel> graph;
    fofo::Oversampled* cascade { nullptr };

    int reportedLatency { 0 };

    std::atomic<float> inputPeak  { 0.0f };
    std::atomic<float> outputPeak { 0.0f };
    std::atomic<float> gateGain   { 1.0f };
};

} // namespace drip
