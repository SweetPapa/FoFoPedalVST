#pragma once
#include <juce_dsp/juce_dsp.h>

// ─────────────────────────────────────────────────────────────────────────────
// FoFoDriver — the shared DSP kernel behind the Sweet Papa pedals.
//
// The pedals used to be six hand-written serial chains: each one fused its
// signal path, parameter mapping, mix rule and modulation into a single
// process() function. That arrangement produced the same three bugs over and
// over — a block recombining dry with a latency-shifted wet, a mix knob that
// cancelled its own effect, a modulator ticked at the wrong rate — and made a
// good idea in one pedal impossible to reuse in another.
//
// The kernel exists to make that class of bug unrepresentable:
//
//   • fofo::Node   — a processor that only ever sees signal handed to it.
//   • fofo::Parallel — owns the dry snapshot, the latency compensation and
//                    the mix rule, so a node CANNOT mis-mix what it can't see.
//   • fofo::ModMatrix — one modulation system with one control rate, so a
//                    drift source can't be ticked per-block in one pedal and
//                    per-sample in another.
//
// Everything here is real-time safe after prepare(): no allocation, no locks,
// no unbounded loops in process().
// ─────────────────────────────────────────────────────────────────────────────

namespace fofo
{

// How often modulation sources are recomputed, in samples. Destination values
// are ramped linearly between control ticks, so nothing steps.
//
// This is a kernel-wide constant on purpose. The single most damaging
// modulation bug in the old code was a random walk whose one-pole coefficient
// was derived from the sample rate but which was then ticked once per audio
// block — dividing its effective corner frequency by the block size and
// freezing it solid. Sources here are prepared with controlRate(), and the
// matrix is the only thing that ticks them, so that mismatch cannot recur.
inline constexpr int kControlBlock = 32;

struct Spec
{
    double sampleRate   { 44100.0 };
    int    maxBlockSize { 512 };
    int    numChannels  { 2 };

    // The rate modulation sources actually run at.
    double controlRate() const noexcept { return sampleRate / (double) kControlBlock; }

    juce::dsp::ProcessSpec juceSpec() const noexcept
    {
        return { sampleRate, (juce::uint32) maxBlockSize, (juce::uint32) numChannels };
    }

    juce::dsp::ProcessSpec juceMonoSpec() const noexcept
    {
        return { sampleRate, (juce::uint32) maxBlockSize, 1u };
    }

    bool valid() const noexcept
    {
        return sampleRate > 0.0 && maxBlockSize > 0 && numChannels > 0;
    }
};

} // namespace fofo
