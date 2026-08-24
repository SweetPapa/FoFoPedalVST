#pragma once

#include "fofo/Presets.h"

namespace daydream
{

// DAYDREAM's factory bank. Defined in PresetBank.cpp so this header stays
// clear of PluginProcessor.h — the processor includes it, so it cannot
// include back.
const std::vector<fofo::FactoryPreset>& getFactoryPresets();

// Which preset in that bank carries the pedal's own parameter defaults, so a
// freshly inserted plugin names the preset it is actually sitting on. Keep
// this and the bank entry in step — PresetTests.cpp checks that they agree.
inline constexpr int kDefaultPresetIndex = 3;

} // namespace daydream
