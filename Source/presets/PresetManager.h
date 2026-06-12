#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <map>
#include <vector>

namespace vroom
{

// On-disk preset schema is JSON (spec §9). This struct mirrors it 1:1 in
// memory; values are stored in their native parameter ranges (0..100 for
// percentage knobs, dB for input/level, choice names as strings) so the JSON
// is human-readable / git-friendly.
struct Preset
{
    int          schemaVersion { 1 };
    juce::String name;
    juce::String category;    // "Electric" / "Acoustic" / "Bass"
    juce::String vibe;        // "Smooth" / "Crunch" / "Lead" / "Fuzz" / "Fat" / "Custom"
    juce::String author { "Factory" };
    bool         isFactory { false };
    juce::File   userFile;    // valid only for non-factory presets

    float input { 0.0f };
    float drive { 45.0f };
    float character { 60.0f };
    float body { 55.0f };
    float tone { 50.0f };
    float sag { 35.0f };
    float blend { 70.0f };
    float level { 0.0f };
    float gate { 0.0f };
    juce::String sourceMode { "Electric" };
    bool         cabEnable { true };
    juce::String cabIR { "1x12 Warm" };
    juce::String oversampling { "4x" };
    juce::String clipShape { "Smooth" };
};

class PresetManager
{
public:
    explicit PresetManager (juce::AudioProcessorValueTreeState& apvts);

    // Reload factory list (constant) + rescan user dir for .json files.
    void refresh();

    const std::vector<Preset>& getFactoryPresets() const noexcept { return factory; }
    const std::vector<Preset>& getUserPresets()    const noexcept { return user; }

    // True if `name` (case-sensitive) exists in either bucket; isFactory tells
    // which one. Loads the preset's values into APVTS.
    bool loadByName (const juce::String& name, bool isFactory);

    // Step through the combined list (factory first, then user, alphabetical
    // within each). Wraps at edges.
    bool loadNext();
    bool loadPrevious();

    // Saves the *current* APVTS state to disk under `name` in the user dir.
    // Overwrites if a user preset with that name exists. Returns false on
    // disk I/O failure or invalid name.
    bool saveAsUser (const juce::String& name);

    // Deletes a user preset by name. Factory presets cannot be deleted.
    bool deleteUserPreset (const juce::String& name);

    juce::String getCurrentName()     const noexcept { return currentName; }
    juce::String getCurrentCategory() const noexcept { return currentCategory; }
    bool         isCurrentFactory()   const noexcept { return currentIsFactory; }

    // True if any APVTS parameter has moved since the last successful load/save.
    bool isCurrentModified() const;

    juce::File getUserPresetDir() const noexcept { return userPresetDir; }

    // True while a preset load is in progress — PluginProcessor checks this
    // inside its async-update callback so the mode change doesn't overwrite
    // the preset's cab choice with the mode's default cab.
    bool isLoadInProgress() const noexcept { return loadInProgress; }
    void clearLoadInProgress() noexcept { loadInProgress = false; }

private:
    static juce::File getDefaultUserDir();
    static const std::vector<Preset>& getFactoryDefaults();

    void   applyPresetToAPVTS (const Preset& p);
    void   snapshotCurrentValues();
    Preset readJsonFile (const juce::File& f) const;
    bool   writeJsonFile (const Preset& p, const juce::File& f) const;

    int    indexInCombinedOrder() const;
    bool   loadAtCombinedIndex (int idx);

    juce::AudioProcessorValueTreeState& apvts;
    juce::File userPresetDir;
    std::vector<Preset> factory;
    std::vector<Preset> user;

    juce::String currentName;
    juce::String currentCategory;
    bool         currentIsFactory { true };
    bool         loadInProgress { false };

    // Normalised-value snapshot for "modified" detection. Set by
    // snapshotCurrentValues() at load/save time.
    std::map<juce::String, float> snapshot;
};

} // namespace vroom
