#include "DoubleEngine.h"

namespace dbl
{

namespace
{
    // Per-voice base delays (ms) — staggered like takes, not like taps.
    constexpr float kVoiceOffsetMs[DoubleEngine::kVoices] = { 0.0f, 7.0f, 3.5f, 11.0f };
    // Detune polarity/scale per voice: pair 1 = ±1×, pair 2 = ±2.1×.
    constexpr float kVoiceDetuneMul[DoubleEngine::kVoices] = { -1.0f, +1.0f, -2.1f, +2.1f };
    // Pan position per voice (-1..+1 at full WIDE).
    constexpr float kVoicePan[DoubleEngine::kVoices] = { -1.0f, +1.0f, +0.6f, -0.6f };
}

void DoubleEngine::prepare (const juce::dsp::ProcessSpec& s)
{
    spec = s;

    juce::dsp::ProcessSpec mono { s.sampleRate, s.maximumBlockSize, 1 };

    for (int v = 0; v < kVoices; ++v)
    {
        // 35 ms window: the dry-use sweet spot (short enough not to smear,
        // long enough not to warble).
        shifter[v].prepare (s.sampleRate, 35.0f, 0xD0B1E5u + (uint32_t) v * 7919u);

        voiceDelay[v].reset();
        voiceDelay[v].prepare (mono);
        voiceDelay[v].setMaximumDelayInSamples ((int) std::ceil (0.060 * s.sampleRate));

        pitchDrift[v].prepare (0.30f, s.sampleRate, 0xA11CE5u + (uint32_t) v * 104729u);
        timeDrift [v].prepare (0.20f, s.sampleRate, 0xB22DF6u + (uint32_t) v * 104729u);
        levelDrift[v].prepare (0.15f, s.sampleRate, 0xC33EA7u + (uint32_t) v * 104729u);

        ratioSm[v].reset (s.sampleRate, 0.05);
        ratioSm[v].setCurrentAndTargetValue (1.0f);
        gainSm[v].reset (s.sampleRate, 0.05);
        gainSm[v].setCurrentAndTargetValue (0.0f);
    }

    wetBus .setSize (2, (int) s.maximumBlockSize, false, true, true);
    monoSrc.setSize (1, (int) s.maximumBlockSize, false, true, true);

    modeDirty = true;
    updateModeVoicing();
}

void DoubleEngine::reset()
{
    for (int v = 0; v < kVoices; ++v)
    {
        shifter[v].reset();
        voiceDelay[v].reset();
    }
    for (auto& f : wetHP)  f.reset();
    for (auto& f : wetLP)  f.reset();
    for (auto& f : wetDip) f.reset();
}

void DoubleEngine::updateModeVoicing()
{
    if (! modeDirty || spec.sampleRate <= 0.0) return;
    const double sr = spec.sampleRate;

    float hpHz, lpHz, dipHz, dipDb;
    switch (mode)
    {
        case Mode::Strings: hpHz = 130.0f; lpHz = 11000.0f; dipHz = 2800.0f; dipDb = -1.5f; break; // keep doubles behind the pick attack
        case Mode::Synth:   hpHz = 110.0f; lpHz = 14000.0f; dipHz = 1000.0f; dipDb =  0.0f; break;
        case Mode::Vox:
        default:            hpHz = 160.0f; lpHz = 12000.0f; dipHz = 3200.0f; dipDb = -2.0f; break; // dip the presence so doubles sit behind the lead
    }

    auto hp  = juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, hpHz);
    auto lp  = juce::dsp::IIR::Coefficients<float>::makeLowPass  (sr, lpHz, 0.6f);
    auto dip = dipDb != 0.0f
        ? juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, dipHz, 0.9f, juce::Decibels::decibelsToGain (dipDb))
        : juce::dsp::IIR::Coefficients<float>::makeAllPass (sr, 1000.0);

    for (auto& f : wetHP)  f.coefficients = hp;
    for (auto& f : wetLP)  f.coefficients = lp;
    for (auto& f : wetDip) f.coefficients = dip;
    modeDirty = false;
}

void DoubleEngine::process (juce::AudioBuffer<float>& buffer) noexcept
{
    const int nCh = juce::jmin (buffer.getNumChannels(), 2);
    const int nS  = buffer.getNumSamples();
    if (nCh == 0 || nS == 0) return;

    {
        float peak = 0.0f;
        for (int ch = 0; ch < nCh; ++ch)
            peak = juce::jmax (peak, buffer.getMagnitude (ch, 0, nS));
        float cur = inputPeak.load (std::memory_order_relaxed);
        while (peak > cur && ! inputPeak.compare_exchange_weak (cur, peak, std::memory_order_release, std::memory_order_relaxed)) {}
    }

    updateModeVoicing();

    // ── doubling source: mono sum (a second take tracks the performance) ──
    {
        auto* m = monoSrc.getWritePointer (0);
        for (int n = 0; n < nS; ++n)
        {
            float s = 0.0f;
            for (int ch = 0; ch < nCh; ++ch) s += buffer.getReadPointer (ch)[n];
            m[n] = s / (float) nCh;
        }
    }

    // ── voice setup for this block ─────────────────────────────────────────
    const float detCents = 4.0f + 10.0f * thick01;          // ±4..14 cents
    const float pair2In  = juce::jlimit (0.0f, 1.0f, (thick01 - 0.5f) * 2.5f); // voices 3/4 fade in past half
    const float baseMs   = (mode == Mode::Vox ? 18.0f : mode == Mode::Strings ? 24.0f : 14.0f);
    const float msToSamp = 0.001f * (float) spec.sampleRate;

    int activeVoices = 2;
    if (pair2In > 0.01f) activeVoices = 4;
    const float busNorm = 1.0f / std::sqrt ((float) activeVoices);

    float voiceGainTgt[kVoices], voiceRatioTgt[kVoices];
    for (int v = 0; v < kVoices; ++v)
    {
        const float driftCents = pitchDrift[v].next() * 4.0f * human01 * 60.0f; // walk output is small; ×60 → ±~4 cents
        const float cents = detCents * kVoiceDetuneMul[v] + driftCents;
        voiceRatioTgt[v] = std::pow (2.0f, cents / 1200.0f);

        const float lvlWob = 1.0f + levelDrift[v].next() * 10.0f * human01 * 0.18f;
        voiceGainTgt[v] = (v < 2 ? 1.0f : pair2In) * juce::jlimit (0.7f, 1.3f, lvlWob);

        ratioSm[v].setTargetValue (voiceRatioTgt[v]);
        gainSm[v].setTargetValue (voiceGainTgt[v]);
    }

    // ── render voices into the wet bus ─────────────────────────────────────
    wetBus.clear();
    auto* wl = wetBus.getWritePointer (0);
    auto* wr = wetBus.getWritePointer (1);
    const auto* src = monoSrc.getReadPointer (0);

    for (int n = 0; n < nS; ++n)
    {
        for (int v = 0; v < kVoices; ++v)
        {
            const float g = gainSm[v].getNextValue();
            const float ratio = ratioSm[v].getNextValue();
            if (g < 0.001f) { shifter[v].process (src[n], ratio); continue; } // keep state warm

            // pitch
            float tap = shifter[v].process (src[n], ratio);

            // timing: base offset + slow wander (±8 ms at full HUMAN)
            const float wanderMs = timeDrift[v].next() * 10.0f * human01 * 8.0f;
            const float dSamp = juce::jmax (1.0f, (baseMs + kVoiceOffsetMs[v] + wanderMs) * msToSamp);
            voiceDelay[v].pushSample (0, tap);
            tap = voiceDelay[v].popSample (0, dSamp, true);

            tap *= g;

            // constant-power-ish pan, folded toward centre by (1-WIDE)
            const float pan = kVoicePan[v] * wide01; // -1..1
            const float gl = std::sqrt (0.5f * (1.0f - pan));
            const float gr = std::sqrt (0.5f * (1.0f + pan));
            wl[n] += tap * gl;
            wr[n] += tap * gr;
        }
    }

    // ── wet-bus voicing (mode EQ + always-on HPF) ──────────────────────────
    for (int ch = 0; ch < 2; ++ch)
    {
        auto* w = wetBus.getWritePointer (ch);
        for (int n = 0; n < nS; ++n)
        {
            float x = w[n];
            x = wetHP [ch].processSample (x);
            x = wetDip[ch].processSample (x);
            x = wetLP [ch].processSample (x);
            w[n] = x * busNorm;
        }
    }

    // ── additive mix: dry untouched, doubles layered on top ───────────────
    const float wetGain = mix01 * 0.95f;
    for (int ch = 0; ch < nCh; ++ch)
    {
        auto* d = buffer.getWritePointer (ch);
        const auto* w = wetBus.getReadPointer (nCh == 1 ? 0 : ch);
        const auto* wOther = wetBus.getReadPointer (nCh == 1 ? 1 : ch);
        for (int n = 0; n < nS; ++n)
        {
            const float wet = (nCh == 1) ? 0.5f * (w[n] + wOther[n]) : w[n];
            d[n] = spt::softClipCubic (d[n] + wet * wetGain);
        }
    }

    {
        float peak = 0.0f;
        for (int ch = 0; ch < nCh; ++ch)
            peak = juce::jmax (peak, buffer.getMagnitude (ch, 0, nS));
        float cur = outputPeak.load (std::memory_order_relaxed);
        while (peak > cur && ! outputPeak.compare_exchange_weak (cur, peak, std::memory_order_release, std::memory_order_relaxed)) {}
    }
}

} // namespace dbl
