#include "Saturator.h"

namespace vroom
{

float Saturator::mapDrive (float ui01) noexcept
{
    // 0..1 → 1.0..kDriveCeiling, logarithmic so the knob "opens up" near the top.
    return std::pow (kDriveCeiling, ui01);
}

float Saturator::mapBias (float ui01) noexcept
{
    return kBiasMax * ui01;
}

void Saturator::prepare (const juce::dsp::ProcessSpec& s)
{
    spec = s;

    rebuildOversampler();

    interstageLPF.clear();
    interstageLPF.resize (spec.numChannels);
    updateInterstageCoefficients();

    const double rampTime = 0.02; // 20 ms (spec §3)
    driveSmoothed.reset (spec.sampleRate, rampTime);
    biasSmoothed .reset (spec.sampleRate, rampTime);
    driveSmoothed.setCurrentAndTargetValue (mapDrive (driveUI01));
    biasSmoothed .setCurrentAndTargetValue (mapBias  (charUI01));
}

void Saturator::reset()
{
    if (oversampler) oversampler->reset();
    for (auto& f : interstageLPF) f.reset();
}

void Saturator::setOversamplingFactorPower (int newPower)
{
    newPower = juce::jlimit (1, 3, newPower);
    if (newPower == factorPower) return;
    factorPower = newPower;
    if (spec.sampleRate > 0.0)
    {
        rebuildOversampler();
        updateInterstageCoefficients();
    }
}

void Saturator::rebuildOversampler()
{
    if (spec.numChannels == 0 || spec.maximumBlockSize == 0) return;

    oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
        spec.numChannels,
        factorPower,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
        true /* maxQuality */);
    oversampler->initProcessing (spec.maximumBlockSize);
    oversampler->reset();
}

void Saturator::updateInterstageCoefficients()
{
    // Cutoff at the oversampled rate. ~8 kHz interstage tame (spec §7).
    const double overRate = spec.sampleRate * (double) (1 << factorPower);
    constexpr float cutoff = 8000.0f;
    interstageCoefs = juce::dsp::IIR::Coefficients<float>::makeFirstOrderLowPass (overRate, cutoff);
    for (auto& f : interstageLPF) f.coefficients = interstageCoefs;
}

int Saturator::getLatencySamples() const noexcept
{
    return oversampler ? (int) std::ceil (oversampler->getLatencyInSamples()) : 0;
}

void Saturator::process (juce::AudioBuffer<float>& buffer) noexcept
{
    if (! oversampler) return;

    driveSmoothed.setTargetValue (mapDrive (driveUI01));
    biasSmoothed .setTargetValue (mapBias  (charUI01));

    juce::dsp::AudioBlock<float> block (buffer);
    auto upBlock = oversampler->processSamplesUp (block);

    const auto numChannels = (int) upBlock.getNumChannels();
    const auto numSamplesUp = (int) upBlock.getNumSamples();
    const int upPerBase = 1 << factorPower;

    // We tick the smoothed values once per base sample (not per oversampled
    // sample) so the 20 ms ramp matches its prepared rate. Inside the inner
    // loop we just hold the value between base ticks.
    for (int n = 0; n < numSamplesUp; n += upPerBase)
    {
        const float drive = driveSmoothed.getNextValue();
        const float bias  = biasSmoothed .getNextValue();
        const float biasOffset = std::tanh (drive * bias); // DC introduced by asymmetry; subtracted off

        for (int k = 0; k < upPerBase && (n + k) < numSamplesUp; ++k)
        {
            for (int ch = 0; ch < numChannels; ++ch)
            {
                auto* d = upBlock.getChannelPointer ((size_t) ch);

                // First clip stage.
                float a = std::tanh (drive * (d[n + k] + bias)) - biasOffset;

                // Interstage LPF tames pre-2nd-stage harmonic buildup.
                a = interstageLPF[(size_t) ch].processSample (a);

                // Second clip stage at slightly reduced level so the cascade
                // is smoother and bigger-sounding than one hard stage.
                d[n + k] = std::tanh (drive * (a * 0.8f + bias)) - biasOffset;
            }
        }
    }

    oversampler->processSamplesDown (block);
}

} // namespace vroom
