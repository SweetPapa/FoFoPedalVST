#include "Mod.h"

namespace fofopedal
{

namespace
{
    constexpr float kTwoPi = 6.28318530717958647692f;

    // Map 0..1 to musical LFO rates 0.05 Hz → 8 Hz, exponentially.
    inline float mapRateHz (float r01) noexcept
    {
        return 0.05f * std::pow (160.0f, juce::jlimit (0.0f, 1.0f, r01));
    }

    // Tri-chorus voice constants: staggered base delays and detuned rate
    // ratios (relative to the rate knob). The 0°/120°/240° phase offsets
    // keep total delayed energy near-constant — dimension, not "whoosh".
    constexpr float kVoiceBaseMs[3]  = { 5.0f, 7.0f, 9.0f };
    constexpr float kVoiceRateMul[3] = { 1.00f, 1.18f, 1.42f };
    constexpr float kVoicePhase[3]   = { 0.0f, 1.0f / 3.0f, 2.0f / 3.0f };
}

void Mod::prepare (const juce::dsp::ProcessSpec& s)
{
    spec = s;

    // Chorus voices — base up to 9 ms + depth swing up to ±3 ms + headroom.
    const int maxChorusD = (int) std::ceil (0.020 * s.sampleRate);
    for (int v = 0; v < 3; ++v)
    {
        auto& d = chorusD[v];
        d.reset();
        d.prepare (s);
        d.setMaximumDelayInSamples (maxChorusD);

        chorusLfo[v].prepare (0.6f * kVoiceRateMul[v], s.sampleRate, 0xC0FFEEu + (uint32_t) v * 7919u);
        chorusLfo[v].resetPhase (kVoicePhase[v]);
        chorusBBD[v].setCutoff (4500.0f, s.sampleRate);
    }

    phaser.clear();
    phaser.resize ((size_t) s.numChannels);
    phaserLfo.prepare (0.5f, s.sampleRate, 0xBADC0DEu);
    phaserLfo.ampDriftAmt = 0.1f;

    const int maxVibD = (int) std::ceil (0.030 * s.sampleRate);
    vibLine.reset();
    vibLine.prepare (s);
    vibLine.setMaximumDelayInSamples (maxVibD);
    tremLfo.prepare (4.0f, s.sampleRate, 0x7E40D1Au);
    tremLfo.ampDriftAmt = 0.08f;

    dryBuffer.setSize ((int) s.numChannels, (int) s.maximumBlockSize, false, true, true);

    reset();
}

void Mod::reset()
{
    for (auto& d : chorusD) d.reset();
    for (auto& p : phaser) p = PhaserChan{};
    for (auto& b : chorusBBD) b.reset();
    vibLine.reset();
    for (int v = 0; v < 3; ++v) chorusLfo[v].resetPhase (kVoicePhase[v]);
}

void Mod::process (juce::AudioBuffer<float>& buffer) noexcept
{
    if (bypassed || spec.sampleRate <= 0.0) return;

    const int nCh = buffer.getNumChannels();
    const int nS  = buffer.getNumSamples();

    // Snapshot dry for global mix.
    for (int ch = 0; ch < nCh; ++ch)
        dryBuffer.copyFrom (ch, 0, buffer, ch, 0, nS);

    const float rateHz = mapRateHz (rate01);

    if (algo == Algo::Chorus)
    {
        for (int v = 0; v < 3; ++v)
            chorusLfo[v].setRateHz (rateHz * kVoiceRateMul[v], spec.sampleRate);

        const float swingMs  = juce::jmap (depth01, 0.0f, 1.0f, 0.4f, 3.0f);
        const float msToSamp = 0.001f * (float) spec.sampleRate;
        const float sidePan  = juce::jlimit (0.0f, 1.0f, shape01);

        for (int n = 0; n < nS; ++n)
        {
            float monoIn = 0.0f;
            for (int ch = 0; ch < nCh; ++ch) monoIn += buffer.getReadPointer (ch)[n];
            if (nCh > 0) monoIn /= (float) nCh;

            float voice[3];
            for (int v = 0; v < 3; ++v)
            {
                const float lfo = chorusLfo[v].next();
                const float dSamp = juce::jmax (1.0f,
                    (kVoiceBaseMs[v] + swingMs * lfo) * msToSamp);

                chorusD[v].pushSample (0, monoIn);
                float tap = chorusD[v].popSample (0, dSamp, true);

                // BBD flavour: darken + light squash per voice.
                tap = chorusBBD[v].process (tap);
                voice[v] = std::tanh (tap * 1.2f) * 0.85f;
            }

            // L leans on voice 0, R on voice 2, both share the centre.
            const float wetL = voice[1] * 0.5f + voice[0] * (0.5f + 0.5f * sidePan);
            const float wetR = voice[1] * 0.5f + voice[2] * (0.5f + 0.5f * sidePan);

            const float dry = 1.0f - mix01;
            const float wet = mix01;

            if (nCh >= 2)
            {
                buffer.getWritePointer (0)[n] = dry * dryBuffer.getReadPointer (0)[n] + wet * wetL;
                buffer.getWritePointer (1)[n] = dry * dryBuffer.getReadPointer (1)[n] + wet * wetR;
            }
            else if (nCh == 1)
            {
                buffer.getWritePointer (0)[n] = dry * dryBuffer.getReadPointer (0)[n]
                                              + wet * 0.5f * (wetL + wetR);
            }
        }
    }
    else if (algo == Algo::Phaser)
    {
        phaserLfo.setRateHz (rateHz, spec.sampleRate);

        // SHAPE picks the active stage count (4 or 6) by gating later stages.
        const int activeStages = (shape01 < 0.5f) ? 4 : 6;
        const float fbAmt = juce::jmap (feedback01, 0.0f, 1.0f, 0.0f, 0.85f);

        for (int n = 0; n < nS; ++n)
        {
            const float lfo   = juce::jlimit (-1.2f, 1.2f, phaserLfo.next());
            const float sweep = 0.5f * (1.0f + lfo);   // ~0..1

            // Exponential sweep 250 Hz → 250·16^depth — a linear sweep parks
            // in the top octave most of the cycle; log motion dances evenly.
            const float spanOct = 4.0f * juce::jmax (0.05f, depth01);
            const float fcMod = 250.0f * std::pow (2.0f, spanOct * sweep);

            for (int ch = 0; ch < juce::jmin (nCh, (int) phaser.size()); ++ch)
            {
                auto& st = phaser[(size_t) ch];
                float x = buffer.getReadPointer (ch)[n];
                x += st.fb * fbAmt;

                float y = x;
                for (int s = 0; s < activeStages; ++s)
                {
                    const float fc = juce::jmin (fcMod * (1.0f + 0.15f * (float) s),
                                                 (float) spec.sampleRate * 0.45f);
                    const float t  = std::tan (juce::MathConstants<float>::pi * fc / (float) spec.sampleRate);
                    const float a  = (t - 1.0f) / (t + 1.0f);
                    const float xn = y;
                    const float yn = a * xn + st.ap1[s] - a * st.ap2[s];
                    st.ap1[s] = xn;
                    st.ap2[s] = yn;
                    y = yn;
                }
                st.fb = y;

                const float wet = mix01;
                const float dry = 1.0f - mix01;
                buffer.getWritePointer (ch)[n] = dry * dryBuffer.getReadPointer (ch)[n] + wet * y;
            }
        }
    }
    else // TremVib
    {
        tremLfo.setRateHz (rateHz, spec.sampleRate);

        // SHAPE: 0 = pure tremolo, 1 = pure vibrato.
        const float shape = shape01;
        const float depthAmp = depth01 * 0.9f;
        const float depthVibMs = juce::jmap (depth01, 0.0f, 1.0f, 0.0f, 4.0f);
        const float centreMs = 6.0f;
        const float msToSamp = 0.001f * (float) spec.sampleRate;

        for (int n = 0; n < nS; ++n)
        {
            const float lfo  = juce::jlimit (-1.0f, 1.0f, tremLfo.next());
            const float lfo01 = 0.5f * (1.0f + lfo);
            const float tremGain = 1.0f - depthAmp * (1.0f - lfo01);
            const float vibD = juce::jmax (1.0f, (centreMs + depthVibMs * lfo) * msToSamp);

            for (int ch = 0; ch < nCh; ++ch)
            {
                const float x = buffer.getReadPointer (ch)[n];
                vibLine.pushSample (ch, x);
                const float vib = vibLine.popSample (ch, vibD, true);
                const float trem = x * tremGain;
                const float wet = (1.0f - shape) * trem + shape * vib;

                const float w = mix01;
                const float dryC = 1.0f - mix01;
                buffer.getWritePointer (ch)[n] = dryC * dryBuffer.getReadPointer (ch)[n] + w * wet;
            }
        }
    }

    juce::ignoreUnused (kTwoPi);
}

} // namespace fofopedal
