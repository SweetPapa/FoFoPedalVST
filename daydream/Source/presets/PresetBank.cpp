#include "PresetBank.h"
#include "../PluginProcessor.h"

namespace daydream
{

// DAYDREAM has exactly one knob, so its presets are named stops along the one
// journey the engine describes (see DaydreamEngine.h):
//
//   0-5     effectively bypassed
//   5-35    warm tape — saturation blooms in, a small room opens
//   35-65   memory — wow and flutter wobble, the field widens, room becomes hall
//   65-100  dream — shimmer climbs in octaves, the decay stretches toward endless
//
// That looks like a thin excuse for a preset list, and for a bigger pedal it
// would be. Here it is the whole point: nobody can tell you from the front
// panel that 78 is where the shimmer arrives. These names are the map.
const std::vector<fofo::FactoryPreset>& getFactoryPresets()
{
    static const std::vector<fofo::FactoryPreset> bank = {
        { "Barely There", "the tape is on, and that is all — glue, not an effect",
          { { ParamID::dream, 9.0f } } },

        { "Warm Tape", "saturation and a small room; still sounds like the dry track",
          { { ParamID::dream, 22.0f } } },

        { "Cassette Room", "the top end starts to gauze over and the room gets real",
          { { ParamID::dream, 32.0f } } },

        // kDefaultPreset — this is DAYDREAM's own parameter default, and it
        // lands exactly where the memory zone starts.
        { "Memory", "the house sound — wow and flutter arrive, the field opens up",
          { { ParamID::dream, 35.0f } } },

        { "Wide Hall", "the room has become a hall and the wobble is unmistakable",
          { { ParamID::dream, 62.0f } } },

        { "Shimmer", "octaves begin climbing out of the tail",
          { { ParamID::dream, 76.0f } } },

        { "Endless", "the decay stops resolving; the wash swells back in the gaps",
          { { ParamID::dream, 88.0f } } },

        { "Full Dream", "everything, all of it — for endings and empty bars",
          { { ParamID::dream, 97.0f } } },
    };

    return bank;
}

} // namespace daydream
