#include "Character.h"

namespace fofopedal
{

void Character::prepare (const juce::dsp::ProcessSpec& s)
{
    spec = s;
    envState.assign ((size_t) s.numChannels, 0.0f);

    const auto coefFromMs = [&] (float ms)
    {
        return std::exp (-1.0f / (0.001f * ms * (float) s.sampleRate));
    };
    ampAtk     = coefFromMs (5.0f);
    ampRelFast = coefFromMs (80.0f);
    ampRelSlow = coefFromMs (400.0f);

    lowCut   .clear(); lowCut   .resize ((size_t) s.numChannels);
    tiltLow  .clear(); tiltLow  .resize ((size_t) s.numChannels);
    tiltHigh .clear(); tiltHigh .resize ((size_t) s.numChannels);
    lowShelf .clear(); lowShelf .resize ((size_t) s.numChannels);
    hiCut    .clear(); hiCut    .resize ((size_t) s.numChannels);

    updateCoefficients();
}

void Character::reset()
{
    std::fill (envState.begin(), envState.end(), 0.0f);
    for (auto& f : lowCut)   f.reset();
    for (auto& f : tiltLow)  f.reset();
    for (auto& f : tiltHigh) f.reset();
    for (auto& f : lowShelf) f.reset();
    for (auto& f : hiCut)    f.reset();
}

void Character::setAmount01 (float v) noexcept
{
    v = juce::jlimit (0.0f, 1.0f, v);
    if (std::abs (v - amount) < 1.0e-4f) return;
    amount = v;
    updateCoefficients();
}

void Character::setLowCutHz (float hz) noexcept
{
    hz = juce::jlimit (20.0f, 200.0f, hz);
    if (std::abs (hz - lowCutHz) < 0.5f) return;
    lowCutHz = hz;
    updateCoefficients();
}

void Character::updateCoefficients()
{
    if (spec.sampleRate <= 0.0) return;
    const double sr = spec.sampleRate;

    // ── Compressor targets ─────────────────────────────────────────────
    // Threshold drops, ratio rises, with amount.
    const float threshDb = juce::jmap (amount, 0.0f, 1.0f, -16.0f, -30.0f);
    threshLin = juce::Decibels::decibelsToGain (threshDb);
    const float ratio = juce::jmap (amount, 0.0f, 1.0f, 1.8f, 4.5f);
    ratioInv = 1.0f / ratio;

    // Soft auto-makeup. Roughly compensates the average gain reduction so
    // that perceived loudness stays even across the dial.
    const float refOverDb = 6.0f;
    const float avgReductionDb = -refOverDb * (1.0f - ratioInv) * 0.5f;
    makeupGain = juce::Decibels::decibelsToGain (-avgReductionDb);

    // ── Filters ────────────────────────────────────────────────────────
    {
        const bool active = lowCutHz > 22.0f;
        auto coef = active
            ? juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, lowCutHz)
            : juce::dsp::IIR::Coefficients<float>::makeAllPass  (sr, 1000.0);
        for (auto& f : lowCut) f.coefficients = coef;
    }

    // Tilt EQ around 800 Hz pivot — light, ganged opposite directions.
    const float tiltDb = juce::jmap (amount, 0.0f, 1.0f, 0.0f, 2.0f);
    {
        auto cLow  = juce::dsp::IIR::Coefficients<float>::makeLowShelf  (sr, 800.0, 0.707f, juce::Decibels::decibelsToGain (-tiltDb));
        auto cHigh = juce::dsp::IIR::Coefficients<float>::makeHighShelf (sr, 800.0, 0.707f, juce::Decibels::decibelsToGain ( tiltDb));
        for (auto& f : tiltLow)  f.coefficients = cLow;
        for (auto& f : tiltHigh) f.coefficients = cHigh;
    }

    // Gentle, fixed sub-100 Hz low-shelf cut — tightens mud without thinning.
    {
        auto cls = juce::dsp::IIR::Coefficients<float>::makeLowShelf (sr, 90.0, 0.707f, juce::Decibels::decibelsToGain (-1.5f));
        for (auto& f : lowShelf) f.coefficients = cls;
    }

    // Tape-style HF rolloff — scales with amount. 18 kHz → 6.5 kHz.
    {
        const float hiCutHz = juce::jmap (amount, 0.0f, 1.0f, 18000.0f, 6500.0f);
        auto cHi = juce::dsp::IIR::Coefficients<float>::makeLowPass (sr, hiCutHz, 0.5f);
        for (auto& f : hiCut) f.coefficients = cHi;
    }
}

void Character::process (juce::AudioBuffer<float>& buffer) noexcept
{
    if (defeated) return;

    const int nCh = juce::jmin (buffer.getNumChannels(), (int) envState.size());
    const int nS  = buffer.getNumSamples();

    const float kneeHalf = kneeWidth * 0.5f;
    const float threshDb = juce::Decibels::gainToDecibels (threshLin + 1.0e-9f);

    for (int ch = 0; ch < nCh; ++ch)
    {
        auto* x = buffer.getWritePointer (ch);
        float env = envState[(size_t) ch];

        for (int n = 0; n < nS; ++n)
        {
            float in = x[n];

            in = lowCut[(size_t) ch].processSample (in);

            const float absIn = std::abs (in);
            float coef;
            if (absIn > env)
            {
                coef = ampAtk;
            }
            else
            {
                // Program-dependent release: bigger envelope → slower release
                // (sustained tones don't pump); small envelope → faster release
                // (transients recover quickly).
                const float vel = juce::jlimit (0.0f, 1.0f, env * 4.0f);
                coef = ampRelFast + (ampRelSlow - ampRelFast) * vel;
            }
            env = coef * env + (1.0f - coef) * absIn;

            const float envDb = juce::Decibels::gainToDecibels (env + 1.0e-9f);
            const float over  = envDb - threshDb;

            float reductionDb = 0.0f;
            if (over >= kneeHalf)
            {
                reductionDb = (ratioInv - 1.0f) * over;
            }
            else if (over > -kneeHalf)
            {
                const float kneeIn = over + kneeHalf;            // 0..W
                reductionDb = (ratioInv - 1.0f) * kneeIn * kneeIn / (2.0f * kneeWidth);
            }

            const float gainLin = juce::Decibels::decibelsToGain (reductionDb) * makeupGain;
            in *= gainLin;

            in = tiltLow  [(size_t) ch].processSample (in);
            in = tiltHigh [(size_t) ch].processSample (in);
            in = lowShelf [(size_t) ch].processSample (in);
            in = hiCut    [(size_t) ch].processSample (in);

            x[n] = in;
        }

        envState[(size_t) ch] = env;
    }
}

} // namespace fofopedal
