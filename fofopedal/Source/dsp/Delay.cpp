#include "Delay.h"

namespace fofopedal
{

namespace
{
    constexpr float kTwoPi = 6.28318530717958647692f;
}

void Delay::prepare (const juce::dsp::ProcessSpec& s)
{
    spec = s;

    // 2 s max delay + 30 ms headroom for wow swing.
    maxDelaySamples = (int) std::ceil (2.05 * s.sampleRate);

    lines.clear();
    lines.resize ((size_t) s.numChannels);
    for (auto& l : lines)
    {
        l.reset();
        l.prepare (s);
        l.setMaximumDelayInSamples (maxDelaySamples);
    }

    fbHpf.clear(); fbHpf.resize ((size_t) s.numChannels);
    fbLpf.clear(); fbLpf.resize ((size_t) s.numChannels);
    fbBump.clear(); fbBump.resize ((size_t) s.numChannels);
    fbDC.assign ((size_t) s.numChannels, {});
    for (auto& b : fbDC) b.setCutoff (12.0f, s.sampleRate);
    duckEnv.prepare (5.0f, 320.0f, s.sampleRate);

    delaySamplesSmoothed.reset (s.sampleRate, 0.080); // 80 ms tape-style ramp
    delaySamplesSmoothed.setCurrentAndTargetValue (
        juce::jlimit (1.0f, (float) maxDelaySamples, timeTargetMs * 0.001f * (float) s.sampleRate));

    filterDirty = true;
    rebuildFeedbackFilters();
}

void Delay::reset()
{
    for (auto& l : lines) l.reset();
    for (auto& f : fbHpf) f.reset();
    for (auto& f : fbLpf) f.reset();
    for (auto& f : fbBump) f.reset();
    for (auto& b : fbDC)  b.reset();
    wowPhase = flutterPhase = bbdDriftPhase = 0.0f;
}

void Delay::rebuildFeedbackFilters()
{
    if (spec.sampleRate <= 0.0) return;
    const double sr = spec.sampleRate;

    // HPF on feedback: tighten subs so repeats don't muddy.
    {
        auto c = juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, 110.0);
        for (auto& f : fbHpf) f.coefficients = c;
    }

    // LPF cutoff depends on algo and shift-HF-cut knob. BBD is the dark one —
    // real bucket brigades were band-limited to ~3 kHz to fight clock noise,
    // and that darkness compounding through the loop is the BBD sound.
    float baseHz;
    switch (algo)
    {
        case Algo::Digital: baseHz = 12000.0f; break;
        case Algo::BBD:     baseHz = 3200.0f;  break;
        case Algo::Tape:    baseHz = 5500.0f;  break;
        case Algo::NumAlgos:
        default:            baseHz = 10000.0f; break;
    }
    const float hz = juce::jmap (hfCut01, 0.0f, 1.0f, baseHz, baseHz * 0.35f);
    {
        auto c = juce::dsp::IIR::Coefficients<float>::makeLowPass (sr, juce::jmax (1200.0f, hz), 0.65f);
        for (auto& f : fbLpf) f.coefficients = c;
    }

    // Tape head bump: +2.5 dB @ 90 Hz on each pass — repeats get rounder
    // and fatter instead of just duller. (Flat for the other algos.)
    {
        const bool tape = (algo == Algo::Tape);
        auto c = tape
            ? juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, 90.0, 0.7f, juce::Decibels::decibelsToGain (2.5f))
            : juce::dsp::IIR::Coefficients<float>::makeAllPass (sr, 1000.0);
        for (auto& f : fbBump) f.coefficients = c;
    }
    filterDirty = false;
}

void Delay::process (juce::AudioBuffer<float>& buffer) noexcept
{
    if (bypassed || spec.sampleRate <= 0.0) return;

    if (filterDirty) rebuildFeedbackFilters();

    const int nCh = juce::jmin (buffer.getNumChannels(), (int) lines.size());
    const int nS  = buffer.getNumSamples();

    const float baseDelay = juce::jlimit (1.0f, (float) maxDelaySamples,
                                          timeTargetMs * 0.001f * (float) spec.sampleRate);
    delaySamplesSmoothed.setTargetValue (baseDelay);

    // Map feedback knob: cap at 1.05 max (slight self-oscillation territory)
    // but always run through tanh, so runaway is impossible.
    const float fbGain = juce::jmap (feedback01, 0.0f, 1.0f, 0.0f, 1.05f);

    // Algo-specific modulation depths.
    float wowRate = 0.0f, wowDepthSamp = 0.0f;
    float flutterRate = 0.0f, flutterDepthSamp = 0.0f;
    float bbdDriftRate = 0.0f, bbdDriftDepthSamp = 0.0f;
    const float sr = (float) spec.sampleRate;
    switch (algo)
    {
        case Algo::Tape:
            wowRate          = 0.40f;
            wowDepthSamp     = 0.0025f * sr;  // ±2.5 ms wow
            flutterRate      = 5.5f;
            flutterDepthSamp = 0.00025f * sr; // ±0.25 ms flutter
            break;
        case Algo::BBD:
            bbdDriftRate     = 0.12f;
            bbdDriftDepthSamp = 0.0008f * sr; // ±0.8 ms gentle drift
            break;
        case Algo::Digital:
        case Algo::NumAlgos:
        default: break;
    }

    const float wowPhaseInc     = wowRate      / sr;
    const float flutterPhaseInc = flutterRate  / sr;
    const float bbdPhaseInc     = bbdDriftRate / sr;

    for (int n = 0; n < nS; ++n)
    {
        const float baseSamp = delaySamplesSmoothed.getNextValue();
        const float wowOff   = std::sin (kTwoPi * wowPhase)     * wowDepthSamp;
        const float flutOff  = std::sin (kTwoPi * flutterPhase) * flutterDepthSamp;
        const float bbdOff   = std::sin (kTwoPi * bbdDriftPhase) * bbdDriftDepthSamp;
        const float modOff   = wowOff + flutOff + bbdOff;

        // Per-channel: small phase offset for stereo wow movement.
        const float baseSampL = juce::jlimit (1.0f, (float) maxDelaySamples - 4.0f, baseSamp + modOff);
        const float baseSampR = juce::jlimit (1.0f, (float) maxDelaySamples - 4.0f,
                                              baseSamp + modOff * 0.85f); // slight phase offset

        // Read taps.
        float tap[2] = { 0.0f, 0.0f };
        for (int ch = 0; ch < nCh; ++ch)
        {
            const float d = (ch == 0) ? baseSampL : baseSampR;
            tap[ch] = lines[(size_t) ch].popSample (0, d, true);
        }

        // Apply feedback-path tone shaping + soft-clip. The DC blocker is
        // what lets the tanh'd loop sit at unity-plus feedback (playable
        // self-oscillation) without slowly latching to one rail.
        float fbProc[2] = { 0.0f, 0.0f };
        for (int ch = 0; ch < nCh; ++ch)
        {
            float v = tap[ch];
            v = fbHpf[(size_t) ch].processSample (v);
            v = fbBump[(size_t) ch].processSample (v);
            v = fbLpf[(size_t) ch].processSample (v);
            if (algo == Algo::BBD || algo == Algo::Tape)
                v = std::tanh (v * 1.15f);
            v = fbDC[(size_t) ch].process (v);
            fbProc[ch] = v;
        }

        // Write to delay lines: input + (cross- or self-)feedback.
        for (int ch = 0; ch < nCh; ++ch)
        {
            const float in = buffer.getReadPointer (ch)[n];
            const int fbSrc = pingPong ? (1 - ch) : ch;
            const float fb = fbProc[juce::jlimit (0, 1, fbSrc)] * fbGain;
            // Dry input stays clean; only the feedback path is soft-limited
            // so repeats can't run away even at unity-plus feedback.
            const float toWrite = in + std::tanh (fb);
            lines[(size_t) ch].pushSample (0, toWrite);
        }

        // Mix. The wet tap is gently ducked against the input (fixed 25%) —
        // repeats tuck behind the playing and bloom in the gaps. Only the
        // output tap ducks; the feedback loop keeps regenerating underneath.
        float monoIn = 0.0f;
        for (int ch = 0; ch < nCh; ++ch) monoIn += buffer.getReadPointer (ch)[n];
        const float duck = 1.0f - 0.25f * juce::jmin (1.0f, duckEnv.process (monoIn / (float) juce::jmax (1, nCh)) * 3.0f);

        const float wet = mix01 * duck;
        const float dry = 1.0f - mix01 * 0.4f; // Soundtoys-style curve: dry barely drops until wet is big
        for (int ch = 0; ch < nCh; ++ch)
        {
            const float w = tap[ch];
            const float d = buffer.getReadPointer (ch)[n];
            buffer.getWritePointer (ch)[n] = dry * d + wet * w;
        }

        wowPhase     += wowPhaseInc;     if (wowPhase     >= 1.0f) wowPhase     -= 1.0f;
        flutterPhase += flutterPhaseInc; if (flutterPhase >= 1.0f) flutterPhase -= 1.0f;
        bbdDriftPhase += bbdPhaseInc;    if (bbdDriftPhase >= 1.0f) bbdDriftPhase -= 1.0f;
    }
}

} // namespace fofopedal
