#include "PresetBank.h"
#include "../PluginProcessor.h"

namespace sway
{

// SWAY moves a static track the way a band moves it: MOVE is how far things
// drift, RATE how fast, COLOR how dark the modulated path sits under the dry,
// MIX how much of it you hear. The three modes are different machines — Tape
// is one wobbling playback head, Ensemble is a bank of detuned voices, Pump
// breathes with the signal — so a preset picks its mode first and dials from
// there.
//
// MIX sits at 100 for the Tape and Pump modes on purpose: those two are
// meant to *be* the sound of the track, not sit beside it. Ensemble is the
// one that blends.
const std::vector<fofo::FactoryPreset>& getFactoryPresets()
{
    static const std::vector<fofo::FactoryPreset> bank = {
        { "Gentle Drift", "barely-there movement, for things that shouldn't sound processed",
          { { ParamID::move, 25.0f }, { ParamID::rate, 15.0f },
            { ParamID::color, 50.0f }, { ParamID::mix, 100.0f }, { ParamID::mode, 0.0f } } },

        // kDefaultPreset — these are SWAY's own parameter defaults.
        { "Tape Wobble", "the default sway — a tired machine playing your track back",
          { { ParamID::move, 45.0f }, { ParamID::rate, 35.0f },
            { ParamID::color, 50.0f }, { ParamID::mix, 100.0f }, { ParamID::mode, 0.0f } } },

        { "Warped Cassette", "a tape that has been left in the car; pitch never settles",
          { { ParamID::move, 78.0f }, { ParamID::rate, 42.0f },
            { ParamID::color, 32.0f }, { ParamID::mix, 100.0f }, { ParamID::mode, 0.0f } } },

        { "Slow Chorus", "wide and unhurried under a clean guitar or a pad",
          { { ParamID::move, 40.0f }, { ParamID::rate, 22.0f },
            { ParamID::color, 58.0f }, { ParamID::mix, 60.0f }, { ParamID::mode, 1.0f } } },

        { "Lush Ensemble", "several voices' worth of width without a pitch shifter",
          { { ParamID::move, 65.0f }, { ParamID::rate, 35.0f },
            { ParamID::color, 62.0f }, { ParamID::mix, 72.0f }, { ParamID::mode, 1.0f } } },

        { "Vibrato", "fast and deep with the dry gone — the sound leaves the centre",
          { { ParamID::move, 80.0f }, { ParamID::rate, 55.0f },
            { ParamID::color, 50.0f }, { ParamID::mix, 100.0f }, { ParamID::mode, 1.0f } } },

        { "Breathing Pad", "long swells that lean on the beat rather than chop it",
          { { ParamID::move, 42.0f }, { ParamID::rate, 20.0f },
            { ParamID::color, 66.0f }, { ParamID::mix, 85.0f }, { ParamID::mode, 2.0f } } },

        { "Sidechain Pump", "the four-on-the-floor duck, without routing a compressor",
          { { ParamID::move, 58.0f }, { ParamID::rate, 50.0f },
            { ParamID::color, 48.0f }, { ParamID::mix, 100.0f }, { ParamID::mode, 2.0f } } },
    };

    return bank;
}

} // namespace sway
