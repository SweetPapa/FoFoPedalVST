#include "SwayEngine.h"

namespace sway
{

namespace
{
    inline float mapRateHz (float r01) noexcept
    {
        return 0.05f * std::pow (160.0f, r01); // 0.05 → 8 Hz log
    }

    constexpr float kEnsRateMul[3] = { 1.00f, 1.18f, 1.42f };
    constexpr float kEnsPhase[3]   = { 0.0f, 1.0f / 3.0f, 2.0f / 3.0f };
    constexpr float kEnsBaseMs[3]  = { 5.0f, 7.0f, 9.0f };
    constexpr float kEnsPan[3]     = { -1.0f, 0.0f, +1.0f };
}

void SwayEngine::prepare (const juce::dsp::ProcessSpec& s)
{
    spec = s;
    juce::dsp::ProcessSpec mono { s.sampleRate, s.maximumBlockSize, 1 };

    const int maxTape = (int) std::ceil (0.030 * s.sampleRate);
    for (int ch = 0; ch < 2; ++ch)
    {
        tapeLine[ch].reset();
        tapeLine[ch].prepare (mono);
        tapeLine[ch].setMaximumDelayInSamples (maxTape + 8);
        wowLfo[ch]    .prepare (0.45f, s.sampleRate, 0x5A11u + (uint32_t) ch * 7919u);
        flutterLfo[ch].prepare (6.20f, s.sampleRate, 0x5B22u + (uint32_t) ch * 7919u);
        wowLfo[ch].resetPhase (ch * 0.25f); // quadrature L/R
        pumpDip[ch].setCutoff (18000.0f, s.sampleRate);
    }

    const int maxEns = (int) std::ceil (0.015 * s.sampleRate);
    for (int v = 0; v < 3; ++v)
    {
        ensLine[v].reset();
        ensLine[v].prepare (mono);
        ensLine[v].setMaximumDelayInSamples (maxEns + 8);
        ensLfo[v].prepare (0.6f * kEnsRateMul[v], s.sampleRate, 0xE57u + (uint32_t) v * 104729u);
        ensLfo[v].resetPhase (kEnsPhase[v]);
        ensDark[v].setCutoff (5000.0f, s.sampleRate);
    }

    pumpLfo.prepare (4.0f, s.sampleRate, 0x9F3Au);
    pumpLfo.ampDriftAmt = 0.06f;

    drySnap.setSize (2, (int) s.maximumBlockSize, false, true, true);
}

void SwayEngine::reset()
{
    for (auto& d : tapeLine) d.reset();
    for (auto& d : ensLine)  d.reset();
    for (auto& f : ensDark)  f.reset();
    for (auto& f : pumpDip)  f.reset();
}

void SwayEngine::process (juce::AudioBuffer<float>& buffer) noexcept
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

    for (int ch = 0; ch < nCh; ++ch)
        drySnap.copyFrom (ch, 0, buffer, ch, 0, nS);

    const float rateHz   = mapRateHz (rate01);
    const float msToSamp = 0.001f * (float) spec.sampleRate;
    const float wet = mix01;
    const float dry = 1.0f - mix01;

    if (mode == Mode::Tape)
    {
        // COLOR balances slow wow against fast flutter; MOVE scales both.
        const float wowMs     = move01 * (1.0f - 0.6f * color01) * 2.4f;  // up to ±2.4 ms
        const float flutterMs = move01 * color01 * 0.45f;                 // up to ±0.45 ms
        const float centreMs  = 10.0f;

        for (int ch = 0; ch < nCh; ++ch)
        {
            wowLfo[ch]    .setRateHz (rateHz * 0.45f / 0.6f * 0.6f, spec.sampleRate); // wow tracks RATE directly
            flutterLfo[ch].setRateHz (juce::jlimit (4.0f, 9.0f, rateHz * 8.0f), spec.sampleRate);

            auto* d = buffer.getWritePointer (ch);
            for (int n = 0; n < nS; ++n)
            {
                const float modMs = wowLfo[ch].next() * wowMs + flutterLfo[ch].next() * flutterMs;
                const float dSamp = juce::jmax (1.0f, (centreMs + modMs) * msToSamp);
                tapeLine[ch].pushSample (0, d[n]);
                const float out = tapeLine[ch].popSample (0, dSamp, true);
                d[n] = dry * d[n] + wet * out;
            }
        }
    }
    else if (mode == Mode::Ensemble)
    {
        // COLOR widens: swing depth spread + stereo placement.
        const float swingMs = juce::jmap (move01, 0.0f, 1.0f, 0.15f, 2.2f);
        const float width   = 0.4f + 0.6f * color01;

        for (int v = 0; v < 3; ++v)
            ensLfo[v].setRateHz (rateHz * kEnsRateMul[v], spec.sampleRate);

        for (int n = 0; n < nS; ++n)
        {
            float mono = 0.0f;
            for (int ch = 0; ch < nCh; ++ch) mono += buffer.getReadPointer (ch)[n];
            mono /= (float) nCh;

            float outL = 0.0f, outR = 0.0f;
            for (int v = 0; v < 3; ++v)
            {
                const float dSamp = juce::jmax (1.0f, (kEnsBaseMs[v] + swingMs * ensLfo[v].next()) * msToSamp);
                ensLine[v].pushSample (0, mono);
                float tap = ensDark[v].process (ensLine[v].popSample (0, dSamp, true));

                const float pan = kEnsPan[v] * width;
                outL += tap * std::sqrt (0.5f * (1.0f - pan));
                outR += tap * std::sqrt (0.5f * (1.0f + pan));
            }
            outL *= 0.578f; // 1/sqrt(3)
            outR *= 0.578f;

            for (int ch = 0; ch < nCh; ++ch)
            {
                const float w = (nCh == 1) ? 0.5f * (outL + outR) : (ch == 0 ? outL : outR);
                auto* d = buffer.getWritePointer (ch);
                d[n] = dry * d[n] + wet * w;
            }
        }
    }
    else // Pump
    {
        pumpLfo.setRateHz (rateHz, spec.sampleRate);
        const float depth = move01 * 0.85f;
        const float shapePow = 1.0f + color01 * 3.0f; // sine → squashed duty cycle

        for (int n = 0; n < nS; ++n)
        {
            const float lfo  = juce::jlimit (-1.0f, 1.0f, pumpLfo.next());
            float g01 = 0.5f * (1.0f + lfo);
            g01 = std::pow (g01, shapePow);
            const float gain = 1.0f - depth * (1.0f - g01);

            // Sympathetic top dip: the quiet half of the cycle also darkens
            // slightly — reads as "breathing", not chopping.
            const float dipHz = 18000.0f - (1.0f - g01) * depth * 9000.0f;
            const float a = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * dipHz / (float) spec.sampleRate);

            for (int ch = 0; ch < nCh; ++ch)
            {
                auto* d = buffer.getWritePointer (ch);
                pumpDip[ch].a = a;
                const float w = pumpDip[ch].process (d[n]) * gain;
                d[n] = dry * d[n] + wet * w;
            }
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

} // namespace sway
