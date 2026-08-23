#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <cmath>
#include <map>
#include <vector>

namespace fofo
{

// One parameter setting inside a factory preset. `value` is in the parameter's
// own units — 0..100 for the percentage knobs, the choice index for a mode
// switch — so the banks read like the pedal's front panel rather than like a
// column of normalised floats.
struct PresetValue
{
    const char* paramId;
    float       value;
};

// A named front-panel snapshot. `blurb` is the one-line "what it's for" that
// the pedal's own UI can show; hosts only ever see `name`.
struct FactoryPreset
{
    juce::String             name;
    juce::String             blurb;
    std::vector<PresetValue> values;
};

// Implements the AudioProcessor program interface over a fixed factory bank,
// so a pedal's presets appear in the host's own preset menu instead of living
// only inside our web UI. Every pedal owns one of these and forwards its five
// program methods to it.
//
// The bank has to outlive the host — pass the function-local static from the
// pedal's PresetBank.cpp — because a host may ask for a program name at any
// point in the plugin's life.
class FactoryPresetHost
{
public:
    // `defaultProgramIndex` names the preset whose values are the pedal's own
    // parameter defaults, so a freshly inserted plugin reports the preset it
    // is genuinely sitting on rather than claiming the first one in the list.
    // A bank whose entry does not match the pedal's defaults would show up as
    // edited the moment it is inserted, which is why the two are kept in step.
    FactoryPresetHost (juce::AudioProcessor& ownerProcessor,
                       juce::AudioProcessorValueTreeState& stateToDrive,
                       const std::vector<FactoryPreset>& presetBank,
                       int defaultProgramIndex = 0)
        : owner (ownerProcessor), apvts (stateToDrive), bank (presetBank)
    {
        currentIndex = isValidIndex (defaultProgramIndex) ? defaultProgramIndex : 0;

        // Baseline the edited check against the defaults APVTS just installed.
        // Without this the snapshot stays empty and nothing ever reads as
        // modified until the user happens to step to another preset.
        takeSnapshot();
    }

    int getNumPrograms() const noexcept { return (int) bank.size(); }
    int getCurrentProgram() const noexcept { return currentIndex; }

    juce::String getProgramName (int index) const
    {
        return isValidIndex (index) ? bank[(size_t) index].name : juce::String();
    }

    juce::String getProgramBlurb (int index) const
    {
        return isValidIndex (index) ? bank[(size_t) index].blurb : juce::String();
    }

    // Hosts call this on a program change. Several of them also call it with
    // the program we are already on immediately after restoring a session,
    // which would silently throw away the user's own edits — so re-selecting
    // the current program is a no-op unless `forceReload` asks for it, which
    // is what the UI's "back to the preset" gesture wants.
    void setCurrentProgram (int index, bool forceReload = false)
    {
        if (! isValidIndex (index))               return;
        if (index == currentIndex && ! forceReload) return;

        currentIndex = index;
        applyPreset (bank[(size_t) index]);
        apvts.state.setProperty (kIndexProperty, currentIndex, nullptr);
    }

    // Step through the bank from our own UI, wrapping at both ends, and tell
    // the host afterwards so its preset display keeps up with us.
    void step (int direction)
    {
        const int total = getNumPrograms();
        if (total == 0) return;

        const int delta = direction >= 0 ? 1 : -1;
        setCurrentProgram (((currentIndex + delta) % total + total) % total, true);

        owner.updateHostDisplay (
            juce::AudioProcessor::ChangeDetails{}.withProgramChanged (true));
    }

    // True once any parameter has moved away from the preset that was loaded,
    // so the UI can mark the name as edited.
    bool isModified() const
    {
        for (const auto& entry : snapshot)
            if (auto* param = apvts.getParameter (entry.first))
                if (std::abs (param->getValue() - entry.second) > 1.0e-4f)
                    return true;

        return false;
    }

    // Called from setStateInformation so a reopened session shows the preset
    // it was left on instead of snapping back to the first one. The parameter
    // values themselves have already been restored by APVTS at this point, so
    // this only recovers the label and re-baselines the edited check.
    void restoreIndexFromState()
    {
        const int stored = (int) apvts.state.getProperty (kIndexProperty, 0);
        currentIndex = isValidIndex (stored) ? stored : 0;
        takeSnapshot();
    }

private:
    static constexpr const char* kIndexProperty = "currentProgram";

    bool isValidIndex (int index) const noexcept
    {
        return index >= 0 && index < getNumPrograms();
    }

    void applyPreset (const FactoryPreset& preset)
    {
        for (const auto& value : preset.values)
            if (auto* param = apvts.getParameter (value.paramId))
                param->setValueNotifyingHost (param->convertTo0to1 (value.value));

        takeSnapshot();
    }

    // Snapshot every parameter, not just the ones the preset names, so a knob
    // the bank forgot to mention still counts as an edit.
    void takeSnapshot()
    {
        snapshot.clear();

        for (auto* param : owner.getParameters())
            if (auto* withID = dynamic_cast<juce::AudioProcessorParameterWithID*> (param))
                snapshot[withID->paramID] = withID->getValue();
    }

    juce::AudioProcessor&               owner;
    juce::AudioProcessorValueTreeState& apvts;
    const std::vector<FactoryPreset>&   bank;

    int currentIndex { 0 };
    std::map<juce::String, float> snapshot;
};

} // namespace fofo
