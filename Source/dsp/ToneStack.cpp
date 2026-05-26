#include "ToneStack.h"

namespace vroom
{

void ToneStack::prepare (const juce::dsp::ProcessSpec& s)
{
    spec = s;
    // juce::dsp::IIR::Filter has no copy assignment, so we default-construct
    // each channel slot rather than using assign(n, value).
    preHPF   .clear();
    preHPF   .resize (spec.numChannels);
    dcBlocker.clear();
    dcBlocker.resize (spec.numChannels);
    updatePreHPF();
    updateDCBlocker();
}

void ToneStack::reset()
{
    for (auto& f : preHPF)    f.reset();
    for (auto& f : dcBlocker) f.reset();
}

void ToneStack::setPreHPFHz (float hz)
{
    hz = juce::jlimit (10.0f, 500.0f, hz);
    if (std::abs (hz - preHPFHz) < 0.01f) return;
    preHPFHz = hz;
    if (spec.sampleRate > 0.0) updatePreHPF();
}

void ToneStack::updatePreHPF()
{
    const auto coefs = juce::dsp::IIR::Coefficients<float>::makeHighPass (spec.sampleRate, preHPFHz);
    for (auto& f : preHPF) f.coefficients = coefs;
}

void ToneStack::updateDCBlocker()
{
    // Spec §3: HPF at ~20 Hz right after the clipper.
    const auto coefs = juce::dsp::IIR::Coefficients<float>::makeHighPass (spec.sampleRate, 20.0f);
    for (auto& f : dcBlocker) f.coefficients = coefs;
}

void ToneStack::processPre (juce::AudioBuffer<float>& buffer) noexcept
{
    const int nCh = juce::jmin ((int) preHPF.size(), buffer.getNumChannels());
    const int nS  = buffer.getNumSamples();
    for (int ch = 0; ch < nCh; ++ch)
    {
        auto* d = buffer.getWritePointer (ch);
        for (int n = 0; n < nS; ++n)
            d[n] = preHPF[(size_t) ch].processSample (d[n]);
    }
}

void ToneStack::processPost (juce::AudioBuffer<float>& buffer) noexcept
{
    const int nCh = juce::jmin ((int) dcBlocker.size(), buffer.getNumChannels());
    const int nS  = buffer.getNumSamples();
    for (int ch = 0; ch < nCh; ++ch)
    {
        auto* d = buffer.getWritePointer (ch);
        for (int n = 0; n < nS; ++n)
            d[n] = dcBlocker[(size_t) ch].processSample (d[n]);
    }
}

} // namespace vroom
