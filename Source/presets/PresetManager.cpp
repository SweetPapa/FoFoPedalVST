#include "PresetManager.h"
#include "../PluginProcessor.h"

namespace vroom
{

namespace
{
    // All parameter IDs we track for modified detection. Kept in lockstep with
    // PluginProcessor's APVTS layout.
    constexpr const char* kAllParamIDs[] = {
        ParamID::input, ParamID::drive, ParamID::character, ParamID::body,
        ParamID::tone,  ParamID::sag,   ParamID::blend,     ParamID::level,
        ParamID::gate,  ParamID::sourceMode, ParamID::cabEnable, ParamID::cabIR,
        ParamID::oversampling, ParamID::clipShape
    };

    juce::String sanitiseFilename (const juce::String& name)
    {
        juce::String out;
        for (auto c : name)
        {
            if (juce::CharacterFunctions::isLetterOrDigit ((juce::juce_wchar) c)
                || c == ' ' || c == '-' || c == '_' || c == '.')
                out += juce::String::charToString (c);
            else
                out += '_';
        }
        return out.trim();
    }

    int findChoiceIndex (juce::RangedAudioParameter& p, const juce::String& name)
    {
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (&p))
        {
            const int idx = choice->choices.indexOf (name);
            if (idx >= 0) return idx;
        }
        return 0;
    }

    juce::String choiceNameAtIndex (juce::RangedAudioParameter& p, int idx)
    {
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (&p))
            return choice->choices[juce::jlimit (0, choice->choices.size() - 1, idx)];
        return {};
    }

    void setFloatParam (juce::AudioProcessorValueTreeState& apvts, const char* id, float raw)
    {
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (raw));
    }

    void setChoiceParam (juce::AudioProcessorValueTreeState& apvts, const char* id, const juce::String& name)
    {
        if (auto* p = apvts.getParameter (id))
        {
            const int idx = findChoiceIndex (*p, name);
            p->setValueNotifyingHost (p->convertTo0to1 ((float) idx));
        }
    }

    void setBoolParam (juce::AudioProcessorValueTreeState& apvts, const char* id, bool v)
    {
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (v ? 1.0f : 0.0f);
    }

    float getFloatParam (juce::AudioProcessorValueTreeState& apvts, const char* id)
    {
        if (auto* p = apvts.getRawParameterValue (id)) return p->load();
        return 0.0f;
    }

    juce::String getChoiceName (juce::AudioProcessorValueTreeState& apvts, const char* id)
    {
        if (auto* p = apvts.getParameter (id))
        {
            if (auto* raw = apvts.getRawParameterValue (id))
                return choiceNameAtIndex (*p, (int) raw->load());
        }
        return {};
    }

    bool getBoolParam (juce::AudioProcessorValueTreeState& apvts, const char* id)
    {
        if (auto* raw = apvts.getRawParameterValue (id)) return raw->load() > 0.5f;
        return false;
    }
}

PresetManager::PresetManager (juce::AudioProcessorValueTreeState& a)
    : apvts (a), userPresetDir (getDefaultUserDir())
{
    if (! userPresetDir.exists())
        userPresetDir.createDirectory();
    refresh();
}

juce::File PresetManager::getDefaultUserDir()
{
    // Spec §9: macOS path. The Standalone + DAW-hosted plugin both read/write
    // this single shared location so user presets follow them across hosts.
    return juce::File::getSpecialLocation (juce::File::userHomeDirectory)
              .getChildFile ("Library/Audio/Presets/Sweet Papa Technologies/VROOM");
}

const std::vector<Preset>& PresetManager::getFactoryDefaults()
{
    static const std::vector<Preset> kFactory = []
    {
        auto make = [] (juce::String name, juce::String vibe, juce::String category,
                        juce::String clipShape,
                        float drive, float character, float body, float tone,
                        float sag, float blend, float level,
                        juce::String cabIR) -> Preset
        {
            Preset p;
            p.isFactory  = true;
            p.author     = "Factory";
            p.name       = std::move (name);
            p.category   = category;
            p.vibe       = std::move (vibe);
            p.sourceMode = category;
            p.clipShape  = std::move (clipShape);
            p.drive      = drive;
            p.character  = character;
            p.body       = body;
            p.tone       = tone;
            p.sag        = sag;
            p.blend      = blend;
            p.level      = level;
            p.cabIR      = std::move (cabIR);
            p.cabEnable  = true; // DI selections effectively bypass regardless
            return p;
        };

        std::vector<Preset> v;

        // 🧈 SMOOTH — gentle warmth, dynamics intact
        // (drive values sit ~8 points higher than v1 — the dB-tapered drive
        // curve is cleaner in its bottom quarter, which is what makes the
        // rest of the knob's travel usable)
        // Blend sits at/near 100 for electric dirt — parallel blend is kept
        // as the deliberate trick on acoustic and bass, where clean attack
        // under the dirt is the point.
        v.push_back (make ("Buttery",          "Smooth", "Electric", "Smooth", 32, 80, 50, 65, 25, 90,  0.0f, "1x12 Warm"));
        v.push_back (make ("Velvet OD",        "Smooth", "Electric", "Smooth", 34, 65, 45, 60, 25, 90,  1.0f, "1x12 Warm"));
        v.push_back (make ("Acoustic Warmth",  "Smooth", "Acoustic", "Smooth", 24, 80, 40, 70, 15, 45,  0.0f, "Full-Range / DI"));
        v.push_back (make ("Acoustic Body",    "Smooth", "Acoustic", "Smooth", 20, 70, 70, 62, 10, 40,  1.0f, "Full-Range / DI"));
        v.push_back (make ("Bass Warmth",      "Smooth", "Bass",     "Smooth", 26, 70, 60, 52, 15, 45,  0.0f, "Full-Range / DI"));

        // 🪨 CRUNCH — classic mid-driven distortion
        v.push_back (make ("Vroom",            "Crunch", "Electric", "Crunch", 45, 70, 65, 45, 40, 100,  0.0f, "1x12 Warm"));
        v.push_back (make ("Crunch",           "Crunch", "Electric", "Crunch", 60, 45, 50, 55, 35, 100,  0.0f, "4x12 Modern"));
        v.push_back (make ("Garage Rock",      "Crunch", "Electric", "Crunch", 55, 55, 58, 52, 40, 100,  0.0f, "4x12 Modern"));
        v.push_back (make ("Acoustic Grit",    "Crunch", "Acoustic", "Crunch", 42, 60, 45, 58, 20, 50,  0.0f, "Full-Range / DI"));

        // 🎸 LEAD — singing, sustaining, expressive
        v.push_back (make ("Lead Bloom",       "Lead",   "Electric", "Smooth", 70, 75, 60, 42, 65, 100,  2.0f, "1x12 Warm"));
        v.push_back (make ("Soaring Lead",     "Lead",   "Electric", "Smooth", 60, 80, 55, 50, 75, 100,  3.0f, "1x12 Warm"));
        v.push_back (make ("Wail",             "Lead",   "Electric", "Octave", 75, 60, 50, 55, 70, 100,  2.0f, "1x12 Warm"));
        v.push_back (make ("Acoustic Lead",    "Lead",   "Acoustic", "Smooth", 50, 75, 50, 55, 45, 55,  2.0f, "Full-Range / DI"));
        v.push_back (make ("Bass Lead",        "Lead",   "Bass",     "Smooth", 58, 70, 55, 55, 55, 60,  1.0f, "Full-Range / DI"));

        // 🔥 FUZZ — gnarly, harmonically dense
        v.push_back (make ("Stacked Wall",     "Fuzz",   "Electric", "Fuzz",   85, 30, 60, 40, 55, 100, -1.0f, "4x12 Modern"));
        v.push_back (make ("Doom",             "Fuzz",   "Electric", "Fuzz",   95, 25, 70, 30, 60, 100, -2.0f, "4x12 Modern"));
        v.push_back (make ("Bass Fuzz",        "Fuzz",   "Bass",     "Fuzz",   80, 40, 60, 45, 45, 55, -1.0f, "Full-Range / DI"));

        // 🐺 FAT — thick body, wide low-mid presence
        v.push_back (make ("Wooly",            "Fat",    "Electric", "Smooth", 50, 75, 80, 40, 50, 95,  0.0f, "1x12 Warm"));
        v.push_back (make ("Bass Growl",       "Fat",    "Bass",     "Crunch", 50, 55, 55, 50, 30, 50,  0.0f, "Full-Range / DI"));
        v.push_back (make ("Chunky Bass",      "Fat",    "Bass",     "Crunch", 60, 60, 75, 50, 35, 55,  0.0f, "Full-Range / DI"));

        return v;
    }();
    return kFactory;
}

void PresetManager::refresh()
{
    factory = getFactoryDefaults();

    user.clear();
    if (userPresetDir.exists())
    {
        for (const auto& entry : juce::RangedDirectoryIterator (userPresetDir, false, "*.json"))
        {
            auto p = readJsonFile (entry.getFile());
            if (p.name.isNotEmpty())
            {
                p.isFactory = false;
                p.userFile  = entry.getFile();
                user.push_back (std::move (p));
            }
        }
        std::sort (user.begin(), user.end(),
                   [] (const Preset& a, const Preset& b) { return a.name.compareIgnoreCase (b.name) < 0; });
    }
}

Preset PresetManager::readJsonFile (const juce::File& f) const
{
    Preset p;
    juce::var root = juce::JSON::parse (f);
    if (! root.isObject()) return p;

    p.schemaVersion = (int)  root.getProperty ("schemaVersion", 1);
    p.name          = root.getProperty ("name", f.getFileNameWithoutExtension()).toString();
    p.category      = root.getProperty ("category", "Electric").toString();
    p.vibe          = root.getProperty ("vibe", "Custom").toString();
    p.author        = root.getProperty ("author", "User").toString();

    auto params = root.getProperty ("parameters", juce::var());
    if (params.isObject())
    {
        p.input        = (float) (double) params.getProperty ("input", p.input);
        p.drive        = (float) (double) params.getProperty ("drive", p.drive);
        p.character    = (float) (double) params.getProperty ("character", p.character);
        p.body         = (float) (double) params.getProperty ("body", p.body);
        p.tone         = (float) (double) params.getProperty ("tone", p.tone);
        p.sag          = (float) (double) params.getProperty ("sag", p.sag);
        p.blend        = (float) (double) params.getProperty ("blend", p.blend);
        p.level        = (float) (double) params.getProperty ("level", p.level);
        p.gate         = (float) (double) params.getProperty ("gate", p.gate);
        p.sourceMode   = params.getProperty ("sourceMode", p.sourceMode).toString();
        p.cabEnable    = (bool) params.getProperty ("cabEnable", p.cabEnable);
        p.cabIR        = params.getProperty ("cabIR", p.cabIR).toString();
        p.oversampling = params.getProperty ("oversampling", p.oversampling).toString();
        p.clipShape    = params.getProperty ("clipShape", p.clipShape).toString();
    }
    return p;
}

bool PresetManager::writeJsonFile (const Preset& p, const juce::File& f) const
{
    auto* root = new juce::DynamicObject();
    root->setProperty ("schemaVersion", p.schemaVersion);
    root->setProperty ("name",          p.name);
    root->setProperty ("category",      p.category);
    root->setProperty ("vibe",          p.vibe);
    root->setProperty ("author",        p.author);

    auto* params = new juce::DynamicObject();
    params->setProperty ("input",        p.input);
    params->setProperty ("drive",        p.drive);
    params->setProperty ("character",    p.character);
    params->setProperty ("body",         p.body);
    params->setProperty ("tone",         p.tone);
    params->setProperty ("sag",          p.sag);
    params->setProperty ("blend",        p.blend);
    params->setProperty ("level",        p.level);
    params->setProperty ("gate",         p.gate);
    params->setProperty ("sourceMode",   p.sourceMode);
    params->setProperty ("cabEnable",    p.cabEnable);
    params->setProperty ("cabIR",        p.cabIR);
    params->setProperty ("oversampling", p.oversampling);
    params->setProperty ("clipShape",    p.clipShape);

    root->setProperty ("parameters", juce::var (params));

    return f.replaceWithText (juce::JSON::toString (juce::var (root), true /* allOnOneLine = false */));
}

bool PresetManager::loadByName (const juce::String& name, bool isFactory)
{
    const auto& bucket = isFactory ? factory : user;
    auto it = std::find_if (bucket.begin(), bucket.end(),
                            [&] (const Preset& p) { return p.name == name; });
    if (it == bucket.end()) return false;

    // Flag stays true through the upcoming async update — handleAsyncUpdate
    // reads it then calls clearLoadInProgress(). The callAsync below is a
    // safety net in case no async update is actually pending (e.g. preset
    // values matched current state exactly).
    loadInProgress = true;
    applyPresetToAPVTS (*it);

    currentName      = it->name;
    currentCategory  = it->category;
    currentIsFactory = isFactory;
    snapshotCurrentValues();

    juce::MessageManager::callAsync ([this] { loadInProgress = false; });
    return true;
}

void PresetManager::applyPresetToAPVTS (const Preset& p)
{
    setFloatParam  (apvts, ParamID::input,        p.input);
    setFloatParam  (apvts, ParamID::drive,        p.drive);
    setFloatParam  (apvts, ParamID::character,    p.character);
    setFloatParam  (apvts, ParamID::body,         p.body);
    setFloatParam  (apvts, ParamID::tone,         p.tone);
    setFloatParam  (apvts, ParamID::sag,          p.sag);
    setFloatParam  (apvts, ParamID::blend,        p.blend);
    setFloatParam  (apvts, ParamID::level,        p.level);
    setFloatParam  (apvts, ParamID::gate,         p.gate);
    setChoiceParam (apvts, ParamID::sourceMode,   p.sourceMode);
    setBoolParam   (apvts, ParamID::cabEnable,    p.cabEnable);
    setChoiceParam (apvts, ParamID::cabIR,        p.cabIR);
    setChoiceParam (apvts, ParamID::oversampling, p.oversampling);
    setChoiceParam (apvts, ParamID::clipShape,    p.clipShape);
}

int PresetManager::indexInCombinedOrder() const
{
    int idx = 0;
    for (const auto& p : factory)
    {
        if (currentIsFactory && p.name == currentName) return idx;
        ++idx;
    }
    for (const auto& p : user)
    {
        if (! currentIsFactory && p.name == currentName) return idx;
        ++idx;
    }
    return -1;
}

bool PresetManager::loadAtCombinedIndex (int idx)
{
    const int total = (int) (factory.size() + user.size());
    if (total == 0) return false;
    idx = ((idx % total) + total) % total; // wrap

    if (idx < (int) factory.size())
        return loadByName (factory[(size_t) idx].name, true);
    return loadByName (user[(size_t) (idx - (int) factory.size())].name, false);
}

bool PresetManager::loadNext()
{
    int idx = indexInCombinedOrder();
    if (idx < 0) idx = -1; // step to 0 below
    return loadAtCombinedIndex (idx + 1);
}

bool PresetManager::loadPrevious()
{
    int idx = indexInCombinedOrder();
    if (idx < 0) idx = 1; // step to 0 below
    return loadAtCombinedIndex (idx - 1);
}

bool PresetManager::saveAsUser (const juce::String& nameRaw)
{
    const auto cleanName = sanitiseFilename (nameRaw);
    if (cleanName.isEmpty()) return false;

    Preset p;
    p.name         = cleanName;
    p.author       = "User";
    p.input        = getFloatParam  (apvts, ParamID::input);
    p.drive        = getFloatParam  (apvts, ParamID::drive);
    p.character    = getFloatParam  (apvts, ParamID::character);
    p.body         = getFloatParam  (apvts, ParamID::body);
    p.tone         = getFloatParam  (apvts, ParamID::tone);
    p.sag          = getFloatParam  (apvts, ParamID::sag);
    p.blend        = getFloatParam  (apvts, ParamID::blend);
    p.level        = getFloatParam  (apvts, ParamID::level);
    p.gate         = getFloatParam  (apvts, ParamID::gate);
    p.sourceMode   = getChoiceName  (apvts, ParamID::sourceMode);
    p.cabEnable    = getBoolParam   (apvts, ParamID::cabEnable);
    p.cabIR        = getChoiceName  (apvts, ParamID::cabIR);
    p.oversampling = getChoiceName  (apvts, ParamID::oversampling);
    p.clipShape    = getChoiceName  (apvts, ParamID::clipShape);
    p.category     = p.sourceMode; // categories track source mode
    p.vibe         = "Custom";     // user presets show up in their own group

    if (! userPresetDir.exists()) userPresetDir.createDirectory();
    const auto file = userPresetDir.getChildFile (cleanName + ".json");

    if (! writeJsonFile (p, file)) return false;

    refresh();
    currentName      = cleanName;
    currentCategory  = p.category;
    currentIsFactory = false;
    snapshotCurrentValues();
    return true;
}

bool PresetManager::deleteUserPreset (const juce::String& name)
{
    auto it = std::find_if (user.begin(), user.end(),
                            [&] (const Preset& p) { return p.name == name; });
    if (it == user.end()) return false;

    const bool wasCurrent = (! currentIsFactory && currentName == name);
    if (! it->userFile.deleteFile()) return false;

    refresh();
    if (wasCurrent)
    {
        // Drop the "current preset" badge but leave the audio state alone —
        // the user explicitly deleted the bookmark, not the sound.
        currentName.clear();
        currentCategory.clear();
        currentIsFactory = true;
        snapshot.clear();
    }
    return true;
}

void PresetManager::snapshotCurrentValues()
{
    snapshot.clear();
    for (auto* id : kAllParamIDs)
        if (auto* p = apvts.getParameter (id))
            snapshot[id] = p->getValue();
}

bool PresetManager::isCurrentModified() const
{
    if (snapshot.empty()) return false;
    for (auto* id : kAllParamIDs)
    {
        auto it = snapshot.find (id);
        if (it == snapshot.end()) continue;
        if (auto* p = apvts.getParameter (id))
            if (std::abs (p->getValue() - it->second) > 1.0e-4f)
                return true;
    }
    return false;
}

} // namespace vroom
