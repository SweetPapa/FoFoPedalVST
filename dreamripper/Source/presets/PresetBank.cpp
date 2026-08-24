#include "PresetBank.h"
#include "../PluginProcessor.h"

namespace drip
{

// The bank is laid out four presets per mode, in mode order, because the modes
// are four different amplifiers rather than four flavours of one — stepping
// through the bank walks you from the fuzziest end of 1990 to the tightest end
// of now without ever landing somewhere that needs a rescue on the knobs.
//
// MIX sits at 100 almost everywhere on purpose: an amplifier is not a send.
// The two exceptions are the bass presets, where keeping some untouched low
// end underneath the dirt is the entire technique.
const std::vector<fofo::FactoryPreset>& getFactoryPresets()
{
    static const std::vector<fofo::FactoryPreset> bank = {
        // ── Sludge ───────────────────────────────────────────────────────
        { "Tar Pit", "slow, enormous and woolly — the riff that never quite ends",
          { { ParamID::rip, 78.0f }, { ParamID::tight, 22.0f }, { ParamID::scoop, 62.0f },
            { ParamID::cab, 28.0f }, { ParamID::level, 50.0f }, { ParamID::gate, 20.0f },
            { ParamID::mix, 100.0f }, { ParamID::mode, 0.0f } } },

        { "Cascade Fuzz", "the wall of fuzz: scooped, saturated, and it will not decay",
          { { ParamID::rip, 88.0f }, { ParamID::tight, 34.0f }, { ParamID::scoop, 70.0f },
            { ParamID::cab, 42.0f }, { ParamID::level, 48.0f }, { ParamID::gate, 25.0f },
            { ParamID::mix, 100.0f }, { ParamID::mode, 0.0f } } },

        { "Fuzz Lead", "mids pushed hard into the gain so single notes sing over the band",
          { { ParamID::rip, 84.0f }, { ParamID::tight, 46.0f }, { ParamID::scoop, 26.0f },
            { ParamID::cab, 55.0f }, { ParamID::level, 52.0f }, { ParamID::gate, 30.0f },
            { ParamID::mix, 100.0f }, { ParamID::mode, 0.0f } } },

        { "Bass Ruin", "for bass — everything above the fundamental destroyed, the low end kept",
          { { ParamID::rip, 70.0f }, { ParamID::tight, 12.0f }, { ParamID::scoop, 40.0f },
            { ParamID::cab, 20.0f }, { ParamID::level, 46.0f }, { ParamID::gate, 15.0f },
            { ParamID::mix, 62.0f }, { ParamID::mode, 0.0f } } },

        // ── Grunge ───────────────────────────────────────────────────────
        // kDefaultPreset — these are DREAMRIPPER's own parameter defaults.
        { "Flannel", "a cranked combo on the edge — back the guitar off and it cleans up",
          { { ParamID::rip, 55.0f }, { ParamID::tight, 45.0f }, { ParamID::scoop, 42.0f },
            { ParamID::cab, 50.0f }, { ParamID::level, 50.0f }, { ParamID::gate, 35.0f },
            { ParamID::mix, 100.0f }, { ParamID::mode, 1.0f } } },

        { "Garage Crunch", "the rhythm sound of a band in somebody's basement",
          { { ParamID::rip, 68.0f }, { ParamID::tight, 52.0f }, { ParamID::scoop, 50.0f },
            { ParamID::cab, 58.0f }, { ParamID::level, 50.0f }, { ParamID::gate, 38.0f },
            { ParamID::mix, 100.0f }, { ParamID::mode, 1.0f } } },

        { "Broken Amp", "falling apart in the good way: everything sagging, nothing tight",
          { { ParamID::rip, 92.0f }, { ParamID::tight, 30.0f }, { ParamID::scoop, 34.0f },
            { ParamID::cab, 36.0f }, { ParamID::level, 47.0f }, { ParamID::gate, 30.0f },
            { ParamID::mix, 100.0f }, { ParamID::mode, 1.0f } } },

        { "Verse Grit", "barely dirty, for the quiet half of a loud-quiet-loud song",
          { { ParamID::rip, 24.0f }, { ParamID::tight, 40.0f }, { ParamID::scoop, 38.0f },
            { ParamID::cab, 52.0f }, { ParamID::level, 54.0f }, { ParamID::gate, 22.0f },
            { ParamID::mix, 100.0f }, { ParamID::mode, 1.0f } } },

        // ── Metal ────────────────────────────────────────────────────────
        { "Bay Area", "fast, scooped and tight enough that every picked note lands",
          { { ParamID::rip, 72.0f }, { ParamID::tight, 62.0f }, { ParamID::scoop, 74.0f },
            { ParamID::cab, 62.0f }, { ParamID::level, 50.0f }, { ParamID::gate, 48.0f },
            { ParamID::mix, 100.0f }, { ParamID::mode, 2.0f } } },

        { "Scooped Chug", "palm mutes that hit like a door slamming",
          { { ParamID::rip, 80.0f }, { ParamID::tight, 70.0f }, { ParamID::scoop, 86.0f },
            { ParamID::cab, 55.0f }, { ParamID::level, 49.0f }, { ParamID::gate, 55.0f },
            { ParamID::mix, 100.0f }, { ParamID::mode, 2.0f } } },

        { "Lead Cut", "mids back in front so the solo sits on top of the wall",
          { { ParamID::rip, 86.0f }, { ParamID::tight, 58.0f }, { ParamID::scoop, 30.0f },
            { ParamID::cab, 70.0f }, { ParamID::level, 53.0f }, { ParamID::gate, 45.0f },
            { ParamID::mix, 100.0f }, { ParamID::mode, 2.0f } } },

        { "Frostbite", "thin, rasping and cold — tremolo-picked and recorded in a shed",
          { { ParamID::rip, 90.0f }, { ParamID::tight, 84.0f }, { ParamID::scoop, 66.0f },
            { ParamID::cab, 82.0f }, { ParamID::level, 48.0f }, { ParamID::gate, 60.0f },
            { ParamID::mix, 100.0f }, { ParamID::mode, 2.0f } } },

        // ── Djent ────────────────────────────────────────────────────────
        { "Modern Chug", "the tight, gated, low-tuned rhythm sound, straight out of the box",
          { { ParamID::rip, 74.0f }, { ParamID::tight, 72.0f }, { ParamID::scoop, 64.0f },
            { ParamID::cab, 66.0f }, { ParamID::level, 50.0f }, { ParamID::gate, 62.0f },
            { ParamID::mix, 100.0f }, { ParamID::mode, 3.0f } } },

        { "Drop Tune", "for sevens and eights — the lowest string stays a note, not a rumble",
          { { ParamID::rip, 68.0f }, { ParamID::tight, 88.0f }, { ParamID::scoop, 58.0f },
            { ParamID::cab, 60.0f }, { ParamID::level, 51.0f }, { ParamID::gate, 66.0f },
            { ParamID::mix, 100.0f }, { ParamID::mode, 3.0f } } },

        { "Machine Gun", "gate slammed shut: staccato, silent between the hits",
          { { ParamID::rip, 82.0f }, { ParamID::tight, 78.0f }, { ParamID::scoop, 70.0f },
            { ParamID::cab, 72.0f }, { ParamID::level, 49.0f }, { ParamID::gate, 82.0f },
            { ParamID::mix, 100.0f }, { ParamID::mode, 3.0f } } },

        { "Nu Bounce", "scooped, bouncy and very late-nineties, with the dry still under it",
          { { ParamID::rip, 62.0f }, { ParamID::tight, 55.0f }, { ParamID::scoop, 80.0f },
            { ParamID::cab, 48.0f }, { ParamID::level, 52.0f }, { ParamID::gate, 50.0f },
            { ParamID::mix, 88.0f }, { ParamID::mode, 3.0f } } },
    };

    return bank;
}

} // namespace drip
