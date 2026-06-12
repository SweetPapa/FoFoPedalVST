#include "FofoEngine.h"

namespace fofopedal
{

void FofoEngine::prepare (const juce::dsp::ProcessSpec& s)
{
    characterBlock.prepare (s);
    driveBlock    .prepare (s);
    modBlock      .prepare (s);
    pitchBlock    .prepare (s);
    delayBlock    .prepare (s);
    spaceBlock    .prepare (s);
    glueBlock     .prepare (s);
}

void FofoEngine::reset()
{
    characterBlock.reset();
    driveBlock    .reset();
    modBlock      .reset();
    pitchBlock    .reset();
    delayBlock    .reset();
    spaceBlock    .reset();
    glueBlock     .reset();
}

void FofoEngine::process (juce::AudioBuffer<float>& buffer) noexcept
{
    const int nCh = buffer.getNumChannels();
    const int nS  = buffer.getNumSamples();

    // Input peak (pre-anything).
    {
        float peak = 0.0f;
        for (int ch = 0; ch < nCh; ++ch)
            peak = juce::jmax (peak, buffer.getMagnitude (ch, 0, nS));
        float cur = inputPeak.load (std::memory_order_relaxed);
        while (peak > cur && ! inputPeak.compare_exchange_weak (cur, peak, std::memory_order_release, std::memory_order_relaxed)) {}
    }

    // [Character] → [Drive] → [Mod ↔ Pitch] → [Delay ↔ Space] → [OutputGlue]
    characterBlock.process (buffer);
    driveBlock    .process (buffer);

    if (swapModPitch)
    {
        pitchBlock.process (buffer);
        modBlock  .process (buffer);
    }
    else
    {
        modBlock  .process (buffer);
        pitchBlock.process (buffer);
    }

    if (swapDelaySpace)
    {
        spaceBlock.process (buffer);
        delayBlock.process (buffer);
    }
    else
    {
        delayBlock.process (buffer);
        spaceBlock.process (buffer);
    }

    glueBlock.process (buffer);

    // Output peak.
    {
        float peak = 0.0f;
        for (int ch = 0; ch < nCh; ++ch)
            peak = juce::jmax (peak, buffer.getMagnitude (ch, 0, nS));
        float cur = outputPeak.load (std::memory_order_relaxed);
        while (peak > cur && ! outputPeak.compare_exchange_weak (cur, peak, std::memory_order_release, std::memory_order_relaxed)) {}
    }
}

} // namespace fofopedal
