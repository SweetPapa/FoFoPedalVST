#include "Space.h"

namespace fofopedal
{

void Space::prepare (const juce::dsp::ProcessSpec& s)
{
    spec = s;

    preMaxSamp = juce::jmax (256, (int) std::ceil (0.300 * s.sampleRate));
    pre.clear(); pre.resize ((size_t) s.numChannels);
    for (auto& p : pre)
    {
        p.reset();
        p.prepare (s);
        p.setMaximumDelayInSamples (preMaxSamp);
    }

    sendHpf.clear(); sendHpf.resize ((size_t) s.numChannels);

    verb.prepare (s.sampleRate, s.maximumBlockSize);

    for (int ch = 0; ch < 2; ++ch)
    {
        shifter[ch].prepare (s.sampleRate, 80.0f, 0xF0F0u + (uint32_t) ch * 104729u);
        shimLP[ch].setCutoff (6500.0f, s.sampleRate);
        shimHP[ch].coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (s.sampleRate, 250.0f);
    }
    shimmerFb.setSize (2, (int) s.maximumBlockSize, false, true, true);
    shimmerFb.clear();

    drySnap.setSize ((int) s.numChannels, (int) s.maximumBlockSize, false, true, true);
    drySnap.clear();

    duckEnv.prepare (5.0f, 380.0f, s.sampleRate);

    algoChanged = true;
    paramsChanged = true;
    updateAll();
}

void Space::reset()
{
    for (auto& p : pre) p.reset();
    for (auto& f : sendHpf) f.reset();
    verb.reset();
    for (int ch = 0; ch < 2; ++ch)
    {
        shifter[ch].reset();
        shimLP[ch].reset();
        shimHP[ch].reset();
    }
    shimmerFb.clear();
}

void Space::updateAll()
{
    if (spec.sampleRate <= 0.0) return;

    // SIZE macro per algorithm: room scale + decay + damping move together,
    // each algo with its own curve so they stay recognisably different rooms
    // across the whole knob.
    switch (algo)
    {
        case Algo::Plate:
            verb.setVoicing (spt::FableVerb::Voicing::Plate);
            verb.setSize01  (size01);
            verb.setDecay01 (juce::jmap (size01, 0.25f, 0.85f));
            verb.setDamp01  (juce::jmap (size01, 0.40f, 0.20f));
            verb.setWidth01 (0.85f);
            break;
        case Algo::Hall:
            verb.setVoicing (spt::FableVerb::Voicing::Hall);
            verb.setSize01  (size01);
            verb.setDecay01 (juce::jmap (size01, 0.30f, 0.90f));
            verb.setDamp01  (juce::jmap (size01, 0.55f, 0.35f));
            verb.setWidth01 (1.0f);
            break;
        case Algo::Room:
            verb.setVoicing (spt::FableVerb::Voicing::Room);
            verb.setSize01  (size01);
            verb.setDecay01 (juce::jmap (size01, 0.20f, 0.75f));
            verb.setDamp01  (0.45f);
            verb.setWidth01 (0.75f);
            break;
        case Algo::Shimmer:
            verb.setVoicing (spt::FableVerb::Voicing::Hall);
            verb.setSize01  (juce::jmap (size01, 0.30f, 1.00f));
            verb.setDecay01 (juce::jmap (size01, 0.45f, 0.93f));
            verb.setDamp01  (0.30f);
            verb.setWidth01 (1.0f);
            break;
        case Algo::NumAlgos:
        default: break;
    }

    auto c = juce::dsp::IIR::Coefficients<float>::makeHighPass (spec.sampleRate, sendHpHz);
    for (auto& f : sendHpf) f.coefficients = c;

    algoChanged = false;
    paramsChanged = false;
}

void Space::process (juce::AudioBuffer<float>& buffer) noexcept
{
    if (bypassed || spec.sampleRate <= 0.0) return;
    if (algoChanged || paramsChanged) updateAll();

    const int nCh = juce::jmin (buffer.getNumChannels(), (int) pre.size());
    const int nS  = buffer.getNumSamples();
    if (nCh == 0 || nS == 0) return;

    for (int ch = 0; ch < nCh; ++ch)
        drySnap.copyFrom (ch, 0, buffer, ch, 0, nS);

    // 1) HPF on the send + pre-delay.
    const float preSamp = juce::jlimit (0.0f, (float) preMaxSamp - 1.0f,
                                        preDelayMs * 0.001f * (float) spec.sampleRate);
    for (int ch = 0; ch < nCh; ++ch)
    {
        auto* d = buffer.getWritePointer (ch);
        for (int n = 0; n < nS; ++n)
        {
            float v = sendHpf[(size_t) ch].processSample (d[n]);
            pre[(size_t) ch].pushSample (0, v);
            d[n] = pre[(size_t) ch].popSample (0, preSamp, true);
        }
    }

    // 2) Shimmer: inject the previous block's pitched tail into the input.
    const bool shimmerOn = (algo == Algo::Shimmer && shimmer01 > 1.0e-3f);
    if (shimmerOn)
    {
        const float fb = 0.62f * shimmer01;
        for (int ch = 0; ch < juce::jmin (nCh, 2); ++ch)
        {
            auto* dst = buffer.getWritePointer (ch);
            auto* fbk = shimmerFb.getReadPointer (ch);
            for (int n = 0; n < nS; ++n)
                dst[n] += std::tanh (fbk[n]) * fb;
        }
    }

    // 3) Reverb, fully wet, in place.
    auto* l = buffer.getWritePointer (0);
    auto* r = buffer.getWritePointer (nCh > 1 ? 1 : 0);
    verb.processBlock (l, r, l, r, nS);

    // 4) Pitch-shift this block's tail for the next loop pass.
    if (shimmerOn)
    {
        for (int ch = 0; ch < juce::jmin (nCh, 2); ++ch)
        {
            auto* src = buffer.getReadPointer (ch);
            auto* dst = shimmerFb.getWritePointer (ch);
            for (int n = 0; n < nS; ++n)
            {
                float v = shifter[ch].process (src[n], 2.0f);
                v = shimHP[ch].processSample (v);
                v = shimLP[ch].process (v);
                dst[n] = v;
            }
        }
    }
    else
    {
        shimmerFb.clear();
    }

    // 5) Wet/dry mix (Soundtoys curve: dry holds up until wet > 70%).
    const float mix = mix01;
    float wetGain, dryGain;
    if (mix <= 0.70f)
    {
        wetGain = mix / 0.70f;
        dryGain = 1.0f;
    }
    else
    {
        wetGain = 1.0f;
        dryGain = 1.0f - (mix - 0.70f) / 0.30f;
    }

    for (int n = 0; n < nS; ++n)
    {
        // Fixed 30% duck keyed by the block's input — tail hides while you
        // play, swells back in the gaps.
        float mono = 0.0f;
        for (int ch = 0; ch < nCh; ++ch) mono += drySnap.getReadPointer (ch)[n];
        const float env  = duckEnv.process (mono / (float) juce::jmax (1, nCh));
        const float duck = 1.0f - 0.30f * juce::jmin (1.0f, env * 3.0f);

        for (int ch = 0; ch < nCh; ++ch)
        {
            auto* w = buffer.getWritePointer (ch);
            const float d = drySnap.getReadPointer (ch)[n];
            w[n] = dryGain * d + wetGain * duck * w[n];
        }
    }
}

} // namespace fofopedal
