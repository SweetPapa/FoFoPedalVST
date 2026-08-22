#pragma once
#include "Spec.h"
#include "Node.h"
#include <memory>
#include <functional>

namespace fofo
{

// ─────────────────────────────────────────────────────────────────────────────
// Oversampled — runs a per-sample shaper at 2×/4×/8× and reports its own
// latency honestly.
//
// Oversampling used to be applied by hand around exactly two shapers in the
// whole catalogue; every other nonlinearity — bus saturation, output clippers,
// tremolo shaping, tanh inside feedback loops — ran at base rate and aliased.
// Wrapping it as a Node means the decision is "does this shaper alias?" rather
// than "did I remember to set up an oversampler?", and the latency lands in
// the graph automatically instead of being forgotten at the recombine.
// ─────────────────────────────────────────────────────────────────────────────
class Oversampled : public Node
{
public:
    // factorPower: 1 = 2×, 2 = 4×, 3 = 8×.
    // shaper is called once per oversampled sample, per channel.
    using Shaper = std::function<float (float x, int channel)>;

    Oversampled (Shaper s, int factorPower = 2)
        : shaper (std::move (s)), power (juce::jlimit (0, 3, factorPower)) {}

    void setShaper (Shaper s) { shaper = std::move (s); }

    void setFactorPower (int p)
    {
        p = juce::jlimit (0, 3, p);
        if (p == power) return;
        power = p;
        if (spec_.valid()) build();
    }

    void prepare (const Spec& spec) override
    {
        spec_ = spec;
        build();
    }

    void reset() override { if (os) os->reset(); }

    void process (juce::AudioBuffer<float>& buffer, int numSamples) noexcept override
    {
        if (bypassed || ! os || ! shaper) return;

        const int nCh = juce::jmin (buffer.getNumChannels(), spec_.numChannels);
        if (nCh == 0 || numSamples == 0) return;

        juce::dsp::AudioBlock<float> blk (buffer.getArrayOfWritePointers(),
                                          (size_t) nCh, 0, (size_t) numSamples);
        auto up = os->processSamplesUp (blk);

        const int osN  = (int) up.getNumSamples();
        const int osCh = (int) up.getNumChannels();

        for (int ch = 0; ch < osCh; ++ch)
        {
            auto* d = up.getChannelPointer ((size_t) ch);
            for (int n = 0; n < osN; ++n)
                d[n] = shaper (d[n], ch);
        }

        os->processSamplesDown (blk);
    }

    int latencySamples() const noexcept override
    {
        return os ? (int) std::ceil (os->getLatencyInSamples()) : 0;
    }

    float latencyExact() const noexcept override
    {
        return os ? (float) os->getLatencyInSamples() : 0.0f;
    }

    int factor() const noexcept { return 1 << power; }

private:
    void build()
    {
        if (! spec_.valid()) return;
        if (power == 0) { os.reset(); return; }

        os = std::make_unique<juce::dsp::Oversampling<float>> (
                (size_t) spec_.numChannels, (size_t) power,
                juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
        os->initProcessing ((size_t) spec_.maxBlockSize);
        os->reset();
    }

    Shaper shaper;
    int    power { 2 };
    Spec   spec_ {};
    std::unique_ptr<juce::dsp::Oversampling<float>> os;
};

} // namespace fofo
