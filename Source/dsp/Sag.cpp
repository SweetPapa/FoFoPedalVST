#include "Sag.h"

namespace vroom
{

void Sag::prepare (const juce::dsp::ProcessSpec& s)
{
    spec = s;
    envelope.assign (spec.numChannels, 0.0f);
}

void Sag::reset()
{
    std::fill (envelope.begin(), envelope.end(), 0.0f);
}

void Sag::process (juce::AudioBuffer<float>& buffer) noexcept
{
    if (sagAmount <= 0.0001f) return; // bypass cleanly when knob is at 0

    const int nCh = juce::jmin (buffer.getNumChannels(), (int) envelope.size());
    const int nS  = buffer.getNumSamples();
    const float sr = (float) spec.sampleRate;

    // Attack ~8 ms — let the pick attack through BEFORE the supply dips, then
    // duck and bloom back. (An instant dip just reads as compression; the
    // slight lag is what reads as a power supply being yanked down.)
    // Release maps from ~80 ms at low Sag to ~500 ms at full Sag.
    constexpr float attackMs = 8.0f;
    const float releaseMs = juce::jmap (sagAmount, 0.0f, 1.0f, 80.0f, 500.0f);
    const float attackCoef  = std::exp (-1.0f / (attackMs  * 0.001f * sr));
    const float releaseCoef = std::exp (-1.0f / (releaseMs * 0.001f * sr));

    // Max post-clip dip ~35% — the other half of the sag effect (bias and
    // headroom modulation) now lives inside the Saturator, so this block
    // only needs the level bloom, not the whole illusion.
    const float dipDepth = sagAmount * 0.35f * responseScale;

    for (int ch = 0; ch < nCh; ++ch)
    {
        auto* d = buffer.getWritePointer (ch);
        float env = envelope[(size_t) ch];

        for (int n = 0; n < nS; ++n)
        {
            const float a = std::abs (d[n]);
            const float coef = (a > env) ? attackCoef : releaseCoef;
            env = coef * env + (1.0f - coef) * a;

            // env^1.5 keeps the dip focused on real transients — light
            // playing barely moves it, digging in pulls the supply down.
            const float e = juce::jmin (env, 1.0f);
            const float g = 1.0f - dipDepth * e * std::sqrt (e);
            d[n] *= g;
        }

        envelope[(size_t) ch] = env;
    }
}

} // namespace vroom
