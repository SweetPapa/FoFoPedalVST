#pragma once
#include "Spec.h"
#include "Delay.h"
#include <vector>
#include <memory>

namespace fofo
{

// ─────────────────────────────────────────────────────────────────────────────
// Node — a processor that only ever sees the signal handed to it.
//
// The contract is deliberately narrow. A node processes in place, reports how
// many samples of latency it introduces, and does NOT know about dry signal,
// mix knobs, or what it is connected to. Everything a node cannot see, it
// cannot get wrong.
// ─────────────────────────────────────────────────────────────────────────────
class Node
{
public:
    virtual ~Node() = default;

    virtual void prepare (const Spec& spec) = 0;
    virtual void reset() = 0;
    virtual void process (juce::AudioBuffer<float>& buffer, int numSamples) noexcept = 0;

    // Samples of delay this node adds between input and output. Parallel and
    // Chain use this to keep sums phase-aligned; the plugin reports the graph
    // total to the host.
    virtual int latencySamples() const noexcept { return 0; }

    // The unrounded latency. Alignment wants the exact value: rounding an
    // oversampler's fractional latency up to a whole sample before delaying
    // the dry leaves a fraction of a sample of misalignment, which is a
    // shallower version of the same comb F1 was. Defaults to the integer.
    virtual float latencyExact() const noexcept { return (float) latencySamples(); }

    void setBypassed (bool b) noexcept { bypassed = b; }
    bool isBypassed() const noexcept   { return bypassed; }

protected:
    bool bypassed { false };
};

// ─── Chain — nodes in series ─────────────────────────────────────────────────
class Chain : public Node
{
public:
    Chain& add (std::unique_ptr<Node> n)
    {
        if (n != nullptr) nodes.push_back (std::move (n));
        return *this;
    }

    int size() const noexcept { return (int) nodes.size(); }
    Node* at (int i) noexcept { return (i >= 0 && i < (int) nodes.size()) ? nodes[(size_t) i].get() : nullptr; }

    void prepare (const Spec& spec) override
    {
        for (auto& n : nodes) n->prepare (spec);
    }

    void reset() override
    {
        for (auto& n : nodes) n->reset();
    }

    void process (juce::AudioBuffer<float>& buffer, int numSamples) noexcept override
    {
        if (bypassed) return;
        for (auto& n : nodes)
            if (! n->isBypassed())
                n->process (buffer, numSamples);
    }

    int latencySamples() const noexcept override
    {
        int total = 0;
        for (const auto& n : nodes)
            if (! n->isBypassed()) total += n->latencySamples();
        return total;
    }

    float latencyExact() const noexcept override
    {
        float total = 0.0f;
        for (const auto& n : nodes)
            if (! n->isBypassed()) total += n->latencyExact();
        return total;
    }

private:
    std::vector<std::unique_ptr<Node>> nodes;
};

// ─── Mix rules ───────────────────────────────────────────────────────────────
//
// One canonical set, applied in one place. Previously each block invented its
// own recombine, which is how the same mistake shipped three times.
enum class MixRule
{
    // Straight crossfade. mix 0 = dry, 1 = wet.
    Blend,

    // Dry passes at unity and wet is layered on top. Correct for anything
    // additive — doublers, parallel saturation — where attenuating the dry to
    // make room for the effect is exactly wrong.
    Additive,

    // Soundtoys-style: wet rises to unity by 70%, and only past that does the
    // dry start coming down. Gives a usable range across the whole knob
    // instead of cramming everything useful into the first third.
    Ducked
};

// ─────────────────────────────────────────────────────────────────────────────
// Parallel — the structural fix.
//
// Owns the dry snapshot, the latency compensation and the mix rule. The wet
// node is handed a buffer and never sees anything else, so the three bugs that
// kept recurring are now unrepresentable rather than merely fixed:
//
//   • A wet branch through an oversampler used to be summed against an
//     undelayed dry, producing a comb. Parallel reads the wet node's own
//     reported latency and holds the dry back to match.
//   • A "mix" knob used to crossfade a modulated delay against undelayed dry,
//     which is a flanger, not the effect anyone asked for. Choosing a MixRule
//     is now an explicit decision at construction, next to the node it governs.
//   • A clipper used to sit on the summed output, distorting the dry path.
//     There is no shaper here at all; the sum is the sum.
// ─────────────────────────────────────────────────────────────────────────────
class Parallel : public Node
{
public:
    Parallel (std::unique_ptr<Node> wetNode, MixRule r = MixRule::Blend, float mix01 = 1.0f)
        : wet (std::move (wetNode)), rule (r), mix (juce::jlimit (0.0f, 1.0f, mix01)) {}

    void setMix (float m) noexcept   { mix = juce::jlimit (0.0f, 1.0f, m); }
    float getMix() const noexcept    { return mix; }
    void setRule (MixRule r) noexcept { rule = r; }
    Node* wetNode() noexcept          { return wet.get(); }

    void prepare (const Spec& spec) override
    {
        spec_ = spec;
        if (wet) wet->prepare (spec);

        dry.setSize (spec.numChannels, spec.maxBlockSize, false, true, true);
        dry.clear();

        const float lat = wet ? wet->latencyExact() : 0.0f;
        align.prepare (spec, juce::jmax (64.0f, lat + 8.0f));
        align.setDelay (lat);
    }

    void reset() override
    {
        if (wet) wet->reset();
        align.reset();
        dry.clear();
    }

    // Latency has to be re-read whenever the wet branch's own latency can
    // change (an oversampling factor switch, a node bypassed). Cheap, so the
    // caller may just call it after any such change.
    void refreshLatency() noexcept
    {
        align.setDelay (wet ? wet->latencyExact() : 0.0f);
    }

    void process (juce::AudioBuffer<float>& buffer, int numSamples) noexcept override
    {
        if (bypassed || wet == nullptr) return;

        const int nCh = juce::jmin (buffer.getNumChannels(), dry.getNumChannels());
        if (nCh == 0 || numSamples == 0) return;

        // 1) snapshot dry
        for (int ch = 0; ch < nCh; ++ch)
            dry.copyFrom (ch, 0, buffer, ch, 0, numSamples);

        // 2) wet branch does its thing, in place, knowing nothing about dry
        wet->process (buffer, numSamples);

        // 3) hold the dry back to match whatever latency the wet branch added
        align.process (dry, numSamples);

        // 4) one recombine, no shaper
        float dryGain = 1.0f, wetGain = 1.0f;
        switch (rule)
        {
            case MixRule::Blend:
                dryGain = 1.0f - mix; wetGain = mix;
                break;
            case MixRule::Additive:
                dryGain = 1.0f;       wetGain = mix;
                break;
            case MixRule::Ducked:
                if (mix <= 0.70f) { wetGain = mix / 0.70f;             dryGain = 1.0f; }
                else              { wetGain = 1.0f; dryGain = 1.0f - (mix - 0.70f) / 0.30f; }
                break;
        }

        for (int ch = 0; ch < nCh; ++ch)
        {
            auto* w = buffer.getWritePointer (ch);
            const auto* d = dry.getReadPointer (ch);
            for (int n = 0; n < numSamples; ++n)
                w[n] = d[n] * dryGain + w[n] * wetGain;
        }
    }

    int latencySamples() const noexcept override
    {
        // Both branches leave aligned, so the pair's latency is the wet
        // branch's — which the host then compensates for the whole plugin.
        return wet ? wet->latencySamples() : 0;
    }

    float latencyExact() const noexcept override
    {
        return wet ? wet->latencyExact() : 0.0f;
    }

private:
    std::unique_ptr<Node> wet;
    MixRule rule { MixRule::Blend };
    float   mix  { 1.0f };

    Spec spec_ {};
    juce::AudioBuffer<float> dry;
    AlignDelay align;
};

// ─── A node that does nothing, for tests and for holding a slot open ─────────
class PassThrough : public Node
{
public:
    void prepare (const Spec&) override {}
    void reset() override {}
    void process (juce::AudioBuffer<float>&, int) noexcept override {}
};

// ─── Adapter: wrap a lambda as a Node ────────────────────────────────────────
// Convenient for small in-place operations that don't deserve their own class.
template <typename Fn>
class FnNode : public Node
{
public:
    explicit FnNode (Fn f, int latency = 0) : fn (std::move (f)), lat (latency) {}

    void prepare (const Spec& s) override { spec_ = s; }
    void reset() override {}
    void process (juce::AudioBuffer<float>& b, int n) noexcept override { fn (b, n); }
    int  latencySamples() const noexcept override { return lat; }

private:
    Fn   fn;
    int  lat { 0 };
    Spec spec_ {};
};

template <typename Fn>
std::unique_ptr<Node> makeFnNode (Fn f, int latency = 0)
{
    return std::make_unique<FnNode<Fn>> (std::move (f), latency);
}

} // namespace fofo
