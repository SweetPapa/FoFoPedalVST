#include "BandSplit.h"

namespace vroom
{

void BandSplit::prepare (const juce::dsp::ProcessSpec& s)
{
    spec = s;
    lpf1.clear(); lpf1.resize (spec.numChannels);
    lpf2.clear(); lpf2.resize (spec.numChannels);
    hpf1.clear(); hpf1.resize (spec.numChannels);
    hpf2.clear(); hpf2.resize (spec.numChannels);
    updateCoefficients();
}

void BandSplit::reset()
{
    for (auto& f : lpf1) f.reset();
    for (auto& f : lpf2) f.reset();
    for (auto& f : hpf1) f.reset();
    for (auto& f : hpf2) f.reset();
}

void BandSplit::setCrossoverHz (float hz)
{
    hz = juce::jlimit (40.0f, 1500.0f, hz);
    if (std::abs (hz - crossoverHz) < 0.01f) return;
    crossoverHz = hz;
    if (spec.sampleRate > 0.0) updateCoefficients();
}

void BandSplit::updateCoefficients()
{
    // 0.707 Q on each 2nd-order section gives Butterworth response; two of
    // them cascaded gives the LR4 -24 dB/oct skirt.
    const auto lp = juce::dsp::IIR::Coefficients<float>::makeLowPass  (spec.sampleRate, crossoverHz, 0.707f);
    const auto hp = juce::dsp::IIR::Coefficients<float>::makeHighPass (spec.sampleRate, crossoverHz, 0.707f);
    for (auto& f : lpf1) f.coefficients = lp;
    for (auto& f : lpf2) f.coefficients = lp;
    for (auto& f : hpf1) f.coefficients = hp;
    for (auto& f : hpf2) f.coefficients = hp;
}

void BandSplit::split (juce::AudioBuffer<float>& input,
                       juce::AudioBuffer<float>& lowOut) noexcept
{
    const int nCh = juce::jmin (input.getNumChannels(), (int) lpf1.size());
    const int nS  = input.getNumSamples();

    if (lowOut.getNumChannels() < nCh || lowOut.getNumSamples() < nS)
        lowOut.setSize (nCh, nS, false, false, true);

    for (int ch = 0; ch < nCh; ++ch)
    {
        auto* hi  = input.getWritePointer (ch);
        auto* low = lowOut.getWritePointer (ch);

        for (int n = 0; n < nS; ++n)
        {
            const float x = hi[n];

            float l = lpf1[(size_t) ch].processSample (x);
            l = lpf2[(size_t) ch].processSample (l);

            float h = hpf1[(size_t) ch].processSample (x);
            h = hpf2[(size_t) ch].processSample (h);

            low[n] = l;
            hi[n]  = h;
        }
    }
}

} // namespace vroom
