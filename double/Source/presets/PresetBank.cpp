#include "PresetBank.h"
#include "../PluginProcessor.h"

namespace dbl
{

// DOUBLE is the take you didn't record: four drifting voices beside the dry,
// which stays untouched. THICK is how much voice there is, WIDE how far apart
// they sit, HUMAN how sloppy their timing and tuning are, MIX how loud they
// come up. The mode picks what the voices are imitating — a second singer, a
// string section, or an oscillator bank.
//
// HUMAN is the knob that decides whether this reads as a double or as a
// chorus: low is a tight overdub, high is a different person on a different
// day. Synth material wants it low, because a machine doesn't drift.
const std::vector<fofo::FactoryPreset>& getFactoryPresets()
{
    static const std::vector<fofo::FactoryPreset> bank = {
        // kDefaultPreset — these are DOUBLE's own parameter defaults.
        { "Lead Vocal Double", "the house sound — one more of you, slightly late",
          { { ParamID::thick, 50.0f }, { ParamID::wide, 70.0f },
            { ParamID::human, 50.0f }, { ParamID::mix, 60.0f }, { ParamID::mode, 0.0f } } },

        { "Subtle Thicken", "not a double, just a wider version of the take you have",
          { { ParamID::thick, 30.0f }, { ParamID::wide, 38.0f },
            { ParamID::human, 28.0f }, { ParamID::mix, 30.0f }, { ParamID::mode, 0.0f } } },

        { "Mono-Safe Wide", "width that survives the club, the phone and the mono fold",
          { { ParamID::thick, 50.0f }, { ParamID::wide, 55.0f },
            { ParamID::human, 40.0f }, { ParamID::mix, 50.0f }, { ParamID::mode, 0.0f } } },

        { "Backing Vocals", "loose enough to sound like other people singing along",
          { { ParamID::thick, 58.0f }, { ParamID::wide, 85.0f },
            { ParamID::human, 70.0f }, { ParamID::mix, 55.0f }, { ParamID::mode, 0.0f } } },

        { "Wide Chorus Stack", "the big one — hard left and right, obviously an effect",
          { { ParamID::thick, 72.0f }, { ParamID::wide, 92.0f },
            { ParamID::human, 58.0f }, { ParamID::mix, 65.0f }, { ParamID::mode, 0.0f } } },

        { "String Section", "turns two tracked lines into a section that isn't in time",
          { { ParamID::thick, 65.0f }, { ParamID::wide, 80.0f },
            { ParamID::human, 55.0f }, { ParamID::mix, 58.0f }, { ParamID::mode, 1.0f } } },

        { "Choir", "many voices, badly rehearsed, which is what makes it a choir",
          { { ParamID::thick, 82.0f }, { ParamID::wide, 95.0f },
            { ParamID::human, 76.0f }, { ParamID::mix, 68.0f }, { ParamID::mode, 1.0f } } },

        { "Synth Unison", "detuned oscillator stack — machines drift less than people",
          { { ParamID::thick, 62.0f }, { ParamID::wide, 76.0f },
            { ParamID::human, 22.0f }, { ParamID::mix, 70.0f }, { ParamID::mode, 2.0f } } },
    };

    return bank;
}

} // namespace dbl
