#include "PresetBank.h"
#include "../PluginProcessor.h"

namespace bkpr
{

// BACKPORCH is a reverb that stays behind the source: SPACE is size, TONE is
// how bright the tail comes back, DUCK is how far it gets out of the way while
// you're playing, and MIX is how much is there at all. Slap is a single short
// reflection, Room is early reflections, Plate is the long smooth one.
//
// The house style is "sounds produced, not wet": the space sits behind the
// source and ducks out of its way. That is about WHERE the reverb sits, not
// whether you can hear it — v2 took it as the latter, kept MIX at 20-45, and
// most presets measured as barely changing the sound. MIX now runs 40-70.
const std::vector<fofo::FactoryPreset>& getFactoryPresets()
{
    static const std::vector<fofo::FactoryPreset> bank = {
        // kDefaultPreset — these are BACKPORCH's own parameter defaults.
        { "Front Porch", "the house sound — a small room you don't notice until it's gone",
          { { ParamID::space, 45.0f }, { ParamID::tone, 45.0f },
            { ParamID::duck, 35.0f }, { ParamID::mix, 55.0f }, { ParamID::mode, 1.0f } } },

        { "Nashville Slap", "one bright reflection behind a vocal or a telecaster",
          { { ParamID::space, 30.0f }, { ParamID::tone, 58.0f },
            { ParamID::duck, 45.0f }, { ParamID::mix, 60.0f }, { ParamID::mode, 0.0f } } },

        { "Tight Ambience", "just enough room to stop a dry track sounding pasted on",
          { { ParamID::space, 24.0f }, { ParamID::tone, 50.0f },
            { ParamID::duck, 60.0f }, { ParamID::mix, 40.0f }, { ParamID::mode, 1.0f } } },

        { "Drum Room", "hard ducking so the tail lives between hits, not on top of them",
          { { ParamID::space, 40.0f }, { ParamID::tone, 42.0f },
            { ParamID::duck, 62.0f }, { ParamID::mix, 45.0f }, { ParamID::mode, 1.0f } } },

        { "Vocal Plate", "the smooth one, dark enough to sit under a lead vocal",
          { { ParamID::space, 52.0f }, { ParamID::tone, 46.0f },
            { ParamID::duck, 44.0f }, { ParamID::mix, 45.0f }, { ParamID::mode, 2.0f } } },

        { "Guitar Verb", "brighter plate for cleans and arpeggios",
          { { ParamID::space, 56.0f }, { ParamID::tone, 60.0f },
            { ParamID::duck, 30.0f }, { ParamID::mix, 55.0f }, { ParamID::mode, 2.0f } } },

        { "Wide Ballad", "big and dark, ducked hard enough to keep the words intelligible",
          { { ParamID::space, 72.0f }, { ParamID::tone, 38.0f },
            { ParamID::duck, 55.0f }, { ParamID::mix, 60.0f }, { ParamID::mode, 2.0f } } },

        { "Long Tail", "the one you can hear — for endings, pads and empty bars",
          { { ParamID::space, 88.0f }, { ParamID::tone, 34.0f },
            { ParamID::duck, 50.0f }, { ParamID::mix, 70.0f }, { ParamID::mode, 2.0f } } },
    };

    return bank;
}

} // namespace bkpr
