#include "ShimmerReverb.h"

namespace daydream
{

void ShimmerReverb::prepare (const juce::dsp::ProcessSpec& s)
{
    spec = s;

    verb.prepare (s.sampleRate, s.maximumBlockSize);
    verb.setVoicing (spt::FableVerb::Voicing::Hall);
    verb.setWidth01 (1.0f);

    for (int ch = 0; ch < 2; ++ch)
    {
        shifter[ch].prepare (s.sampleRate, 80.0f, 0x1357u + (uint32_t) ch * 7919u);
        fbLP[ch].setCutoff (6500.0f, s.sampleRate);
        fbHP[ch].coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (s.sampleRate, 250.0f);
    }

    shimmerFeedback.setSize (2, (int) s.maximumBlockSize, false, true, true);
    shimmerFeedback.clear();
}

void ShimmerReverb::reset()
{
    verb.reset();
    for (int ch = 0; ch < 2; ++ch)
    {
        shifter[ch].reset();
        fbLP[ch].reset();
        fbHP[ch].reset();
    }
    shimmerFeedback.clear();
}

void ShimmerReverb::process (juce::AudioBuffer<float>& buffer) noexcept
{
    const int nCh = juce::jmin (buffer.getNumChannels(), 2);
    const int nS  = buffer.getNumSamples();
    if (nCh == 0 || nS == 0) return;

    verb.setSize01  (sizeTarget);
    // Size is a macro here: bigger room = longer decay. Shimmer also raises
    // the effective decay because the loop re-energises the tank.
    verb.setDecay01 (juce::jmap (sizeTarget, 0.30f, 0.96f));
    verb.setDamp01  (juce::jmap (sizeTarget, 0.55f, 0.30f));

    // 1) Inject the previous block's pitch-shifted tail into the input.
    if (shimmerTarget > 0.001f)
    {
        const float fb = 0.62f * shimmerTarget;
        for (int ch = 0; ch < nCh; ++ch)
        {
            auto* dst = buffer.getWritePointer (ch);
            auto* fbk = shimmerFeedback.getReadPointer (ch);
            for (int n = 0; n < nS; ++n)
                dst[n] += std::tanh (fbk[n]) * fb;
        }
    }

    // 2) Run the reverb fully wet, in place.
    auto* l = buffer.getWritePointer (0);
    auto* r = buffer.getWritePointer (nCh > 1 ? 1 : 0);
    verb.processBlock (l, r, l, r, nS);

    // 3) Pitch-shift this block's tail for the next block's loop pass.
    for (int ch = 0; ch < nCh; ++ch)
    {
        auto* src = buffer.getReadPointer (ch);
        auto* dst = shimmerFeedback.getWritePointer (ch);
        for (int n = 0; n < nS; ++n)
        {
            float v = shifter[ch].process (src[n], 2.0f);
            v = fbHP[ch].processSample (v);
            v = fbLP[ch].process (v);
            dst[n] = v;
        }
    }
}

} // namespace daydream
