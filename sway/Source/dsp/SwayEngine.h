#pragma once
#include "fofo/Fofo.h"
#include "fofo/Tape.h"
#include "fofo/Saturation.h"
#include <atomic>
#include <memory>

namespace sway
{

// ─────────────────────────────────────────────────────────────────────────────
// SWAY — "Makes static tracks move like a band."
//
// Rebuilt on the FoFoDriver kernel. The v1 engine had two structural problems
// that no amount of tuning could fix:
//
//   • MIX cancelled the effect in two of three modes. Tape crossfaded a 10 ms
//     modulated delay against undelayed dry — summing a signal with a delayed
//     copy of itself is a comb filter, and sweeping that delay makes it a
//     flanger. Wow and flutter are pitch modulation; they only exist at 100%
//     wet. Pump had the mirror problem: a tremolo blended 50/50 with dry is a
//     tremolo at half depth.
//   • There was no tape in the tape mode. It was a modulated delay line and
//     nothing else.
//
// So MIX now means one thing from the player's side — "how much pedal" — and
// is implemented per mode according to what the mode physically is:
//
//   Tape     inherently 100% wet (it is a machine you play through), so MIX
//            scales the depth of everything: wow, flutter, saturation, head
//            bump, HF loss, dropouts, hiss.
//   Ensemble genuinely IS dry plus detuned copies — the interaction with dry
//            is the effect, not an artefact — so MIX is a real blend, made
//            explicit through fofo::Parallel.
//   Pump     a gain effect, inherently 100% wet, so MIX scales depth.
//
// Controls are unchanged, so the UI and every saved preset still apply:
//
//   MOVE  — how much movement
//   RATE  — speed
//   COLOR — per mode:
//             Tape:     machine condition, from serviced studio deck (0) to
//                       tired portastudio (1) — flutter, wear, dropouts and
//                       saturation all rise together on one axis
//             Ensemble: width and voice spread
//             Pump:     shape, from a gentle sine breath to a squashed
//                       duty-cycle pulse, with the filter movement to match
//   MIX   — how much pedal
// ─────────────────────────────────────────────────────────────────────────────
class SwayEngine
{
public:
    enum class Mode { Tape = 0, Ensemble = 1, Pump = 2 };

    SwayEngine();
    ~SwayEngine();

    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();

    void setMove01  (float v) noexcept { move01  = juce::jlimit (0.0f, 1.0f, v); }
    void setRate01  (float v) noexcept { rate01  = juce::jlimit (0.0f, 1.0f, v); }
    void setColor01 (float v) noexcept { color01 = juce::jlimit (0.0f, 1.0f, v); }
    void setMix01   (float v) noexcept { mix01   = juce::jlimit (0.0f, 1.0f, v); }
    void setMode    (Mode m)  noexcept { mode = m; }

    // Tape reports the transport's centre delay so the host can compensate it
    // and the wow reads as movement around zero rather than as a slab of
    // delay. The other two modes are latency-free.
    int  getLatencySamples() const noexcept;

    void process (juce::AudioBuffer<float>& buffer) noexcept;

    float fetchInputPeak()  noexcept { return inputPeak .exchange (0.0f); }
    float fetchOutputPeak() noexcept { return outputPeak.exchange (0.0f); }

private:
    void applyTapeParams();
    void applyEnsembleParams();
    void applyPumpParams();

    void processTape     (juce::AudioBuffer<float>& b, int nS) noexcept;
    void processEnsemble (juce::AudioBuffer<float>& b, int nS) noexcept;
    void processPump     (juce::AudioBuffer<float>& b, int nS) noexcept;

    fofo::Spec spec {};

    Mode  mode    { Mode::Tape };
    float move01  { 0.45f };
    float rate01  { 0.35f };
    float color01 { 0.50f };
    float mix01   { 1.00f };

    // ── Tape ─────────────────────────────────────────────────────────────
    fofo::ModMatrix           tapeMod;
    fofo::ModMatrix::DestId   dTapeDelayL {}, dTapeDelayR {};
    fofo::ModMatrix::SourceId sWowL {}, sWowR {}, sFltL {}, sFltR {}, sSlip {};
    int rWowL {}, rWowR {}, rFltL {}, rFltR {}, rSlipL {}, rSlipR {};
    fofo::TapeSaturator       tapeSat;
    fofo::TapeTransport       tapeTransport;

    // ── Ensemble ─────────────────────────────────────────────────────────
    static constexpr int kEnsVoices = 3;
    fofo::ModMatrix           ensMod;
    fofo::ModMatrix::DestId   dEnsDelay[kEnsVoices] {};
    fofo::ModMatrix::SourceId sEnsLfo[kEnsVoices] {}, sEnsDrift[kEnsVoices] {};
    int rEnsLfo[kEnsVoices] {}, rEnsDrift[kEnsVoices] {};
    fofo::DelayLine           ensLine[kEnsVoices];
    fofo::Svf                 ensDark[kEnsVoices];
    juce::AudioBuffer<float> ensWet;

    // ── Pump ─────────────────────────────────────────────────────────────
    fofo::ModMatrix           pumpMod;
    fofo::ModMatrix::DestId   dPumpGain {}, dPumpCutoff {};
    fofo::ModMatrix::SourceId sPumpLfo {};
    int rPumpGain {}, rPumpCutoff {};
    fofo::Svf                 pumpFilter[2];

    // Dry snapshot + one canonical recombine for Ensemble.
    juce::AudioBuffer<float> drySnap;

    // Latency must not change when the mode does — a host that has already
    // compensated for Tape's transport delay will not re-query on a parameter
    // change, so switching to Pump would leave the track sitting early. All
    // three modes therefore report the same figure and the two latency-free
    // modes are padded to match.
    fofo::AlignDelay outputPad;
    int reportedLatency { 0 };

    std::atomic<float> inputPeak  { 0.0f };
    std::atomic<float> outputPeak { 0.0f };
};

} // namespace sway
