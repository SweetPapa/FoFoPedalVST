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

    // Attack ~5 ms (catch transients without smearing). Release maps from
    // ~60 ms at low Sag to ~600 ms at full Sag — the latter is what gives
    // notes a singing, "blooming" sustain.
    constexpr float attackMs = 5.0f;
    const float releaseMs = juce::jmap (sagAmount, 0.0f, 1.0f, 60.0f, 600.0f);
    const float attackCoef  = std::exp (-1.0f / (attackMs  * 0.001f * sr));
    const float releaseCoef = std::exp (-1.0f / (releaseMs * 0.001f * sr));

    // Max instantaneous gain reduction at full Sag: ~50%.
    const float dipDepth = sagAmount * 0.5f;

    for (int ch = 0; ch < nCh; ++ch)
    {
        auto* d = buffer.getWritePointer (ch);
        float env = envelope[(size_t) ch];

        for (int n = 0; n < nS; ++n)
        {
            const float a = std::abs (d[n]);
            const float coef = (a > env) ? attackCoef : releaseCoef;
            env = coef * env + (1.0f - coef) * a;

            // Soft-clamp the envelope to [0,1] so gain stays in a sane range
            // even with hot input.
            const float e = juce::jmin (env, 1.0f);
            const float g = 1.0f - dipDepth * e;
            d[n] *= g;
        }

        envelope[(size_t) ch] = env;
    }
}

} // namespace vroom
