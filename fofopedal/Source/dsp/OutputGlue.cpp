#include "OutputGlue.h"

namespace fofopedal
{

void OutputGlue::prepare (const juce::dsp::ProcessSpec& s)
{
    spec = s;

    hfRolloff.clear(); hfRolloff.resize ((size_t) s.numChannels);
    compEnv  .assign ((size_t) s.numChannels, 0.0f);

    const auto coefMs = [&] (float ms)
    {
        return std::exp (-1.0f / (0.001f * ms * (float) s.sampleRate));
    };
    ampAtk = coefMs (20.0f);
    ampRel = coefMs (180.0f);

    dirty = true;
    rebuildCoefficients();
}

void OutputGlue::reset()
{
    for (auto& f : hfRolloff) f.reset();
    std::fill (compEnv.begin(), compEnv.end(), 0.0f);
}

void OutputGlue::setAmount01 (float v) noexcept
{
    v = juce::jlimit (0.0f, 1.0f, v);
    if (std::abs (v - amount) < 1.0e-4f) return;
    amount = v;
    dirty = true;
}

void OutputGlue::rebuildCoefficients()
{
    if (spec.sampleRate <= 0.0) return;

    // HF rolloff — engages more aggressively as amount rises.
    const float hz = juce::jmap (amount, 0.0f, 1.0f, 22000.0f, 11000.0f);
    auto c = juce::dsp::IIR::Coefficients<float>::makeLowPass (spec.sampleRate, hz, 0.5f);
    for (auto& f : hfRolloff) f.coefficients = c;

    // Bus comp — very gentle. Threshold drops with amount, ratio rises a hair.
    const float threshDb = juce::jmap (amount, 0.0f, 1.0f, -8.0f, -16.0f);
    threshLin = juce::Decibels::decibelsToGain (threshDb);
    const float ratio = juce::jmap (amount, 0.0f, 1.0f, 1.6f, 2.5f);
    ratioInv = 1.0f / ratio;

    // Tiny auto-makeup so amount sweep is loudness-neutral on average.
    makeup = juce::Decibels::decibelsToGain (amount * 1.2f);

    dirty = false;
}

void OutputGlue::process (juce::AudioBuffer<float>& buffer) noexcept
{
    if (defeated || amount < 1.0e-3f) return;
    if (dirty) rebuildCoefficients();

    const int nCh = juce::jmin (buffer.getNumChannels(), (int) compEnv.size());
    const int nS  = buffer.getNumSamples();

    // Mild 3rd-harmonic shaper: x - α x³ (Chebyshev-T3-flavoured), tanh
    // backstop for safety.
    const float satAmt = amount * 0.18f; // very gentle

    for (int ch = 0; ch < nCh; ++ch)
    {
        auto* d = buffer.getWritePointer (ch);
        float env = compEnv[(size_t) ch];

        for (int n = 0; n < nS; ++n)
        {
            float x = d[n];

            // 1) Subtle 3rd-harmonic saturation.
            if (satAmt > 0.0f)
            {
                const float xc = juce::jlimit (-1.5f, 1.5f, x);
                const float shaped = xc - satAmt * (xc * xc * xc);
                x = 0.85f * shaped + 0.15f * std::tanh (x);
            }

            // 2) Bus comp (soft-knee).
            const float ax = std::abs (x);
            const float c = (ax > env) ? ampAtk : ampRel;
            env = c * env + (1.0f - c) * ax;

            const float envDb   = juce::Decibels::gainToDecibels (env + 1.0e-9f);
            const float threshDb = juce::Decibels::gainToDecibels (threshLin + 1.0e-9f);
            const float over    = envDb - threshDb;
            float reductionDb = 0.0f;
            if (over > 0.0f) reductionDb = (ratioInv - 1.0f) * over;
            x *= juce::Decibels::decibelsToGain (reductionDb) * makeup;

            // 3) HF rolloff.
            x = hfRolloff[(size_t) ch].processSample (x);

            d[n] = x;
        }

        compEnv[(size_t) ch] = env;
    }
}

} // namespace fofopedal
