#include "CabSim.h"
#include <cstdint>

namespace vroom
{

namespace
{
    // Analog-style speaker voicing. The audible anatomy of a real cab IR:
    //   • 2nd-order HP — closed-back low rolloff
    //   • low resonance peak (Q ≈ 1.5) — the cab/speaker "thump"
    //   • low-mid dip — removes the boxiness all real cabs notch out
    //   • presence peak — cone breakup edge
    //   • 4th-order LP where the FIRST stage is resonant (Q ≈ 1.6) — this
    //     puts a bump right at the cliff edge, which is what reads as
    //     "speaker" instead of "blanket over the amp"
    struct CabVoicing
    {
        float hpfHz;
        float lowPeakHz,  lowPeakDb,  lowPeakQ;
        float dipHz,      dipDb,      dipQ;
        float presHz,     presDb,     presQ;
        float lpfHz,      lpfQ1;      // stage 2 fixed at Q 0.6
        float lengthMs;
    };

    constexpr CabVoicing kVoicings[CabSim::NumSlots] = {
        // 0  1x12 Warm: open low-mids, rounded 5.2 kHz edge
        {  85.0f,   120.0f, 3.5f, 1.5f,   450.0f, -2.5f, 0.8f,   2400.0f, 3.0f, 1.2f,   5200.0f, 1.6f,   30.0f },
        // 1  4x12 Modern: bigger thump, deeper box-notch, harder 4.6 kHz cliff
        {  75.0f,   105.0f, 4.0f, 1.5f,   400.0f, -3.0f, 0.8f,   2800.0f, 4.5f, 1.3f,   4600.0f, 1.7f,   35.0f },
        // 2  Bass 1x15: sub extension, soft top, low presence around 1.4 kHz
        {  38.0f,    80.0f, 4.0f, 1.3f,   500.0f, -2.0f, 0.8f,   1400.0f, 2.5f, 1.0f,   3200.0f, 1.4f,   40.0f },
        // 3  Full-Range / DI: unused — convolution is bypassed
        {  20.0f,   100.0f, 0.0f, 1.0f,   500.0f,  0.0f, 1.0f,   1000.0f, 0.0f, 1.0f,  20000.0f, 0.7f,    2.0f },
    };
}

CabSim::CabSim()
    : convolution (juce::dsp::Convolution::Latency { 0 }) // zero added latency
{
}

void CabSim::prepare (const juce::dsp::ProcessSpec& s)
{
    spec = s;
    convolution.prepare (spec);
    loadIRForCurrentSlot();
}

void CabSim::reset()
{
    convolution.reset();
}

void CabSim::selectSlot (int slot)
{
    slot = juce::jlimit (0, (int) NumSlots - 1, slot);
    if (slot == currentSlot && ! customLoaded) return;
    currentSlot = slot;
    customLoaded = false;
    customName.clear();
    if (spec.sampleRate > 0.0) loadIRForCurrentSlot();
}

bool CabSim::loadCustomIRFile (const juce::File& file)
{
    if (! file.existsAsFile()) return false;

    convolution.loadImpulseResponse (
        file,
        juce::dsp::Convolution::Stereo::yes,
        juce::dsp::Convolution::Trim::yes,
        0,  // 0 = use full IR length
        juce::dsp::Convolution::Normalise::yes);

    customLoaded = true;
    customName   = file.getFileNameWithoutExtension();
    return true;
}

int CabSim::getLatencySamples() const noexcept
{
    return (int) convolution.getLatency();
}

void CabSim::loadIRForCurrentSlot()
{
    if (isBypassSlot (currentSlot))
    {
        // Load a unit impulse so processing still works if user toggles back
        // and forth quickly — the convolution stage stays initialised.
        juce::AudioBuffer<float> impulse (1, 1);
        impulse.setSample (0, 0, 1.0f);
        convolution.loadImpulseResponse (
            std::move (impulse),
            spec.sampleRate,
            juce::dsp::Convolution::Stereo::no,
            juce::dsp::Convolution::Trim::no,
            juce::dsp::Convolution::Normalise::no);
        return;
    }

    auto ir = generateSyntheticIR (currentSlot);
    convolution.loadImpulseResponse (
        std::move (ir),
        spec.sampleRate,
        juce::dsp::Convolution::Stereo::no,
        juce::dsp::Convolution::Trim::yes,
        juce::dsp::Convolution::Normalise::yes);
}

juce::AudioBuffer<float> CabSim::generateSyntheticIR (int slot) const
{
    const auto& v = kVoicings[slot];
    const double sr = spec.sampleRate > 0.0 ? spec.sampleRate : 48000.0;
    const int length = juce::jmax (256, (int) std::round (v.lengthMs * 0.001 * sr));

    juce::AudioBuffer<float> ir (1, length);
    ir.clear();

    auto* d = ir.getWritePointer (0);
    d[0] = 1.0f; // pure unit impulse — the filter stack IS the speaker

    juce::dsp::IIR::Filter<float> hpf, lowPeak, dip, pres, lpf1, lpf2;
    hpf.coefficients     = juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, v.hpfHz, 0.707f);
    lowPeak.coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter (
        sr, v.lowPeakHz, v.lowPeakQ, juce::Decibels::decibelsToGain (v.lowPeakDb));
    dip.coefficients     = juce::dsp::IIR::Coefficients<float>::makePeakFilter (
        sr, v.dipHz, v.dipQ, juce::Decibels::decibelsToGain (v.dipDb));
    pres.coefficients    = juce::dsp::IIR::Coefficients<float>::makePeakFilter (
        sr, v.presHz, v.presQ, juce::Decibels::decibelsToGain (v.presDb));
    lpf1.coefficients    = juce::dsp::IIR::Coefficients<float>::makeLowPass (sr, v.lpfHz, v.lpfQ1);
    lpf2.coefficients    = juce::dsp::IIR::Coefficients<float>::makeLowPass (sr, v.lpfHz, 0.6f);

    for (int i = 0; i < length; ++i)
    {
        float x = d[i];
        x = hpf    .processSample (x);
        x = lowPeak.processSample (x);
        x = dip    .processSample (x);
        x = pres   .processSample (x);
        x = lpf1   .processSample (x);
        x = lpf2   .processSample (x);
        d[i] = x;
    }

    // Gentle fade over the last quarter so truncating the filter ringing
    // doesn't leave a step (the resonant filters ring longer than lengthMs).
    const int fadeStart = length * 3 / 4;
    for (int i = fadeStart; i < length; ++i)
    {
        const float t = (float) (i - fadeStart) / (float) (length - fadeStart);
        d[i] *= 0.5f + 0.5f * std::cos (t * juce::MathConstants<float>::pi);
    }

    // Normalise to -3 dBFS peak so loading with Normalise::yes doesn't pump
    // the level on top of the body/mid boosts.
    float peak = 0.0f;
    for (int i = 0; i < length; ++i)
        peak = juce::jmax (peak, std::abs (d[i]));
    if (peak > 0.0f)
    {
        const float target = juce::Decibels::decibelsToGain (-3.0f);
        const float g = target / peak;
        for (int i = 0; i < length; ++i) d[i] *= g;
    }

    return ir;
}

void CabSim::process (juce::AudioBuffer<float>& buffer) noexcept
{
    if (! enabled) return;
    if (isBypassSlot (currentSlot) && ! customLoaded) return;

    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> ctx (block);
    convolution.process (ctx);
}

} // namespace vroom
