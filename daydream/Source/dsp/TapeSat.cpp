#include "TapeSat.h"

namespace daydream
{

void TapeSat::prepare (const juce::dsp::ProcessSpec& s)
{
    spec = s;
    driveSmoothed.reset (s.sampleRate, 0.05);

    hyst   .assign (s.numChannels, {});
    eraseLP.assign (s.numChannels, {});
    dc     .assign (s.numChannels, {});
    for (auto& f : eraseLP) f.setCutoff (16000.0f, s.sampleRate);
    for (auto& b : dc)      b.setCutoff (15.0f,    s.sampleRate);
    hotEnv.prepare (2.0f, 60.0f, s.sampleRate);
}

void TapeSat::reset()
{
    driveSmoothed.setCurrentAndTargetValue (driveSmoothed.getTargetValue());
    for (auto& h : hyst)    h.reset();
    for (auto& f : eraseLP) f.reset();
    for (auto& b : dc)      b.reset();
    hotEnv.reset();
}

void TapeSat::process (juce::AudioBuffer<float>& buffer) noexcept
{
    const int nCh = juce::jmin (buffer.getNumChannels(), (int) hyst.size());
    const int nS  = buffer.getNumSamples();
    const float sr = (float) spec.sampleRate;

    for (int n = 0; n < nS; ++n)
    {
        const float d = driveSmoothed.getNextValue();
        if (d < 0.0005f)
        {
            // still tick the envelope so re-engaging is smooth
            float mono = 0.0f;
            for (int ch = 0; ch < nCh; ++ch) mono += buffer.getReadPointer (ch)[n];
            hotEnv.process (mono / (float) juce::jmax (1, nCh));
            continue;
        }

        // hysteresis amount and gain both scale with drive
        const float g = 1.0f + d * 3.0f;
        const float k = 0.10f + 0.22f * d;
        const float comp = 1.0f / std::tanh (g * 0.7f); // level compensation

        float mono = 0.0f;
        for (int ch = 0; ch < nCh; ++ch) mono += buffer.getReadPointer (ch)[n];
        const float env = hotEnv.process (mono / (float) juce::jmax (1, nCh));

        // self-erasure: hot signal pulls the cut down toward ~7 kHz
        const float hot = juce::jmin (1.0f, env * 2.0f) * d;
        const float cutHz = 16000.0f - hot * 9000.0f;
        const float a = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * cutHz / sr);

        for (int ch = 0; ch < nCh; ++ch)
        {
            float x = buffer.getWritePointer (ch)[n];
            float sat = hyst[(size_t) ch].process (x, g, k) * 0.85f * comp;
            sat = dc[(size_t) ch].process (sat);
            eraseLP[(size_t) ch].a = a;
            sat = eraseLP[(size_t) ch].process (sat);
            buffer.getWritePointer (ch)[n] = x * (1.0f - d) + sat * d;
        }
    }
}

} // namespace daydream
