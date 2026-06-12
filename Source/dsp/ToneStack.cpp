#include "ToneStack.h"

namespace vroom
{

namespace
{
    // Body maps 0..100 → −8 .. +14 dB peaking gain.
    constexpr float kBodyDbMin = -8.0f;
    constexpr float kBodyDbMax = 14.0f;
    constexpr float kBodyQ     = 0.7f; // wide bell — body, not surgical

    // Tone maps 0..100 → 1.2 kHz .. 16 kHz, log. A ±3 dB shelf at 900 Hz
    // moves with it so the top half of the knob still audibly changes the
    // sound after the LPF has left the audible band.
    constexpr float kToneHzMin = 1200.0f;
    constexpr float kToneHzMax = 16000.0f;
}

void ToneStack::prepare (const juce::dsp::ProcessSpec& s)
{
    spec = s;

    // juce::dsp::IIR::Filter has no copy assignment, so we default-construct
    // each channel slot rather than using assign(n, value).
    preHPF    .clear(); preHPF    .resize (spec.numChannels);
    dcBlocker .clear(); dcBlocker .resize (spec.numChannels);
    bodyEQ    .clear(); bodyEQ    .resize (spec.numChannels);
    toneLPF   .clear(); toneLPF   .resize (spec.numChannels);
    toneTilt  .clear(); toneTilt  .resize (spec.numChannels);
    piezoTamer.clear(); piezoTamer.resize (spec.numChannels);
    airShelf  .clear(); airShelf  .resize (spec.numChannels);

    updatePreHPF();
    updateDCBlocker();
    updateBody();
    updateTone();
    updatePiezoTamer();
    updateAirShelf();
}

void ToneStack::reset()
{
    for (auto& f : preHPF)     f.reset();
    for (auto& f : dcBlocker)  f.reset();
    for (auto& f : bodyEQ)     f.reset();
    for (auto& f : toneLPF)    f.reset();
    for (auto& f : toneTilt)   f.reset();
    for (auto& f : piezoTamer) f.reset();
    for (auto& f : airShelf)   f.reset();
}

void ToneStack::setAcousticVoicing (bool enabled)
{
    if (enabled == acousticVoicing) return;
    acousticVoicing = enabled;
    // No coefficient rebuild needed — voicing filters keep their coefs and
    // we just stop running them. Reset their state so cached samples don't
    // pop when re-enabled later.
    for (auto& f : piezoTamer) f.reset();
    for (auto& f : airShelf)   f.reset();
}

void ToneStack::setPreHPFHz (float hz)
{
    hz = juce::jlimit (10.0f, 500.0f, hz);
    if (std::abs (hz - preHPFHz) < 0.01f) return;
    preHPFHz = hz;
    if (spec.sampleRate > 0.0) updatePreHPF();
}

void ToneStack::setBodyCenterHz (float hz)
{
    hz = juce::jlimit (60.0f, 2000.0f, hz);
    if (std::abs (hz - bodyCenterHz) < 0.01f) return;
    bodyCenterHz = hz;
    if (spec.sampleRate > 0.0) updateBody();
}

void ToneStack::setBody01 (float v)
{
    v = juce::jlimit (0.0f, 1.0f, v);
    if (std::abs (v - bodyUI01) < 1.0e-4f) return;
    bodyUI01 = v;
    if (spec.sampleRate > 0.0) updateBody();
}

void ToneStack::setTone01 (float v)
{
    v = juce::jlimit (0.0f, 1.0f, v);
    if (std::abs (v - toneUI01) < 1.0e-4f) return;
    toneUI01 = v;
    if (spec.sampleRate > 0.0) updateTone();
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

void ToneStack::updateBody()
{
    const float dB = juce::jmap (bodyUI01, 0.0f, 1.0f, kBodyDbMin, kBodyDbMax);
    const float gainFactor = juce::Decibels::decibelsToGain (dB);
    const auto coefs = juce::dsp::IIR::Coefficients<float>::makePeakFilter (
        spec.sampleRate, bodyCenterHz, kBodyQ, gainFactor);
    for (auto& f : bodyEQ) f.coefficients = coefs;
}

void ToneStack::updateTone()
{
    // Log-interpolate cutoff so the knob feels musically even across its range.
    const float t = std::exp (std::log (kToneHzMin) + (std::log (kToneHzMax) - std::log (kToneHzMin)) * toneUI01);
    const auto coefs = juce::dsp::IIR::Coefficients<float>::makeLowPass (spec.sampleRate, t);
    for (auto& f : toneLPF) f.coefficients = coefs;

    // Shelf rides with the knob: dark settings also pull the upper mids down
    // a touch, bright settings push them up — so every position of the knob
    // does something audible, not just the bottom third.
    const float shelfDb = juce::jmap (toneUI01, 0.0f, 1.0f, -3.0f, 3.0f);
    const auto tiltCoefs = juce::dsp::IIR::Coefficients<float>::makeHighShelf (
        spec.sampleRate, 900.0f, 0.6f, juce::Decibels::decibelsToGain (shelfDb));
    for (auto& f : toneTilt) f.coefficients = tiltCoefs;
}

void ToneStack::updatePiezoTamer()
{
    // Spec §5: wide -3 dB dip around 3 kHz to soften piezo "quack".
    const auto coefs = juce::dsp::IIR::Coefficients<float>::makePeakFilter (
        spec.sampleRate, 3000.0f, 0.7f, juce::Decibels::decibelsToGain (-3.0f));
    for (auto& f : piezoTamer) f.coefficients = coefs;
}

void ToneStack::updateAirShelf()
{
    // Spec §5: gentle high shelf (+2.5 dB) above ~8 kHz to restore sparkle.
    const auto coefs = juce::dsp::IIR::Coefficients<float>::makeHighShelf (
        spec.sampleRate, 8000.0f, 0.707f, juce::Decibels::decibelsToGain (2.5f));
    for (auto& f : airShelf) f.coefficients = coefs;
}

namespace
{
    void processFilters (std::vector<juce::dsp::IIR::Filter<float>>& filters,
                         juce::AudioBuffer<float>& buffer) noexcept
    {
        const int nCh = juce::jmin ((int) filters.size(), buffer.getNumChannels());
        const int nS  = buffer.getNumSamples();
        for (int ch = 0; ch < nCh; ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            for (int n = 0; n < nS; ++n)
                d[n] = filters[(size_t) ch].processSample (d[n]);
        }
    }
}

void ToneStack::processPre (juce::AudioBuffer<float>& b) noexcept
{
    processFilters (preHPF, b);
    if (acousticVoicing) processFilters (piezoTamer, b);
}

void ToneStack::processDCBlock (juce::AudioBuffer<float>& b) noexcept
{
    processFilters (dcBlocker, b);
}

void ToneStack::processBodyAndTone (juce::AudioBuffer<float>& b) noexcept
{
    processFilters (bodyEQ, b);
    processFilters (toneTilt, b);
    processFilters (toneLPF, b);
    if (acousticVoicing) processFilters (airShelf, b);
}

} // namespace vroom
