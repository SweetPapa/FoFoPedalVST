#include "PitchDrift.h"

namespace daydream
{

void PitchDrift::prepare (const juce::dsp::ProcessSpec& s)
{
    spec = s;

    const int maxDelaySamples = (int) std::ceil (s.sampleRate * 0.030); // 30 ms slop
    delays.clear();
    delays.resize (s.numChannels);
    for (auto& d : delays)
    {
        d.setMaximumDelayInSamples (maxDelaySamples);
        d.prepare (s);
    }

    wowPhase    .assign (s.numChannels, 0.0f);
    flutterPhase.assign (s.numChannels, 0.0f);
    // Quadrature offset between L and R for stereo wideness.
    for (size_t ch = 0; ch < wowPhase.size(); ++ch)
    {
        wowPhase[ch]     = (float) ch * juce::MathConstants<float>::halfPi;
        flutterPhase[ch] = (float) ch * juce::MathConstants<float>::halfPi;
    }

    wowDepth    .reset (s.sampleRate, 0.10);
    flutterDepth.reset (s.sampleRate, 0.10);
}

void PitchDrift::reset()
{
    for (auto& d : delays) d.reset();
}

void PitchDrift::process (juce::AudioBuffer<float>& buffer) noexcept
{
    wowDepth    .setTargetValue (wowDepthTarget);
    flutterDepth.setTargetValue (flutterDepthTarget);

    const int nCh = juce::jmin (buffer.getNumChannels(), (int) delays.size());
    const int nS  = buffer.getNumSamples();
    const float sr = (float) spec.sampleRate;
    const float wowInc     = juce::MathConstants<float>::twoPi * kWowHz     / sr;
    const float flutterInc = juce::MathConstants<float>::twoPi * kFlutterHz / sr;
    const float centerSamples = sr * 0.001f * kCenterDelMs;

    for (int n = 0; n < nS; ++n)
    {
        const float wDepth = wowDepth    .getNextValue();
        const float fDepth = flutterDepth.getNextValue();

        // If both depths are essentially zero, just pass the buffer through —
        // but we still tick the delay lines so they remain warm for when the
        // user dials drift back in.
        for (int ch = 0; ch < nCh; ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            const float in = d[n];

            const float wowMs     = std::sin (wowPhase[(size_t) ch])     * wDepth * kWowMaxMs;
            const float flutterMs = std::sin (flutterPhase[(size_t) ch]) * fDepth * kFlutterMaxMs;
            const float delaySamples = centerSamples + sr * 0.001f * (wowMs + flutterMs);

            delays[(size_t) ch].pushSample (0, in);
            const float out = delays[(size_t) ch].popSample (0, delaySamples);

            // Crossfade — only mix in the drifted signal proportionally to
            // total modulation depth so 0 drift = exact passthrough.
            const float mix = juce::jmin (1.0f, wDepth + fDepth);
            d[n] = in * (1.0f - mix) + out * mix;

            wowPhase[(size_t) ch]     += wowInc;
            flutterPhase[(size_t) ch] += flutterInc;
        }

        if (wowPhase[0]     > juce::MathConstants<float>::twoPi * 1000.0f) for (auto& p : wowPhase)     p = std::fmod (p, juce::MathConstants<float>::twoPi);
        if (flutterPhase[0] > juce::MathConstants<float>::twoPi * 1000.0f) for (auto& p : flutterPhase) p = std::fmod (p, juce::MathConstants<float>::twoPi);
    }
}

} // namespace daydream
