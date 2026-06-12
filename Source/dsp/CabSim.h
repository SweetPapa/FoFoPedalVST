#pragma once

#include <juce_dsp/juce_dsp.h>

namespace vroom
{

// Cabinet IR convolution stage. Per spec §6 we ship four slots:
//   0 = 1x12 Warm     (Electric default)
//   1 = 4x12 Modern   (heavier Electric)
//   2 = Bass 1x15
//   3 = Full-Range / DI  (effectively bypass)
//
// The shipped IRs are *synthetic placeholders* generated in code — small
// filter chains that approximate the broad-strokes voicing of each cab type.
// Real captured IRs can replace them later via the custom-IR loader; we
// intentionally don't ship copyrighted commercial packs (spec §6 warning).
class CabSim
{
public:
    enum SlotIndex
    {
        Slot_1x12_Warm     = 0,
        Slot_4x12_Modern   = 1,
        Slot_Bass_1x15     = 2,
        Slot_FullRange_DI  = 3,
        NumSlots
    };

    CabSim();

    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();

    void setEnabled (bool e) noexcept { enabled = e; }
    bool isEnabled() const noexcept   { return enabled; }

    // Async-safe — schedules a load on the convolution's background thread.
    // Must be called from the message thread (allocates).
    void selectSlot (int slot);
    int  getCurrentSlot() const noexcept { return currentSlot; }

    // Loads an external .wav IR. Same threading requirement as selectSlot.
    bool loadCustomIRFile (const juce::File& file);
    bool hasCustomLoaded() const noexcept { return customLoaded; }
    juce::String getCustomName() const    { return customName; }

    int  getLatencySamples() const noexcept;
    void process (juce::AudioBuffer<float>& buffer) noexcept;

private:
    juce::AudioBuffer<float> generateSyntheticIR (int slot) const;
    void loadIRForCurrentSlot();

    // Spec §6: "Full-Range / DI ... can be implemented as either a flat IR or
    // simply by bypassing the convolution". We bypass — cheaper and acoustically
    // identical to the trimmed unit impulse.
    bool isBypassSlot (int slot) const noexcept { return slot == Slot_FullRange_DI; }

    juce::dsp::Convolution convolution;
    juce::dsp::ProcessSpec spec {};
    bool enabled { true };
    int  currentSlot { Slot_1x12_Warm };
    bool customLoaded { false };
    juce::String customName;
};

} // namespace vroom
