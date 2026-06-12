#pragma once
#include "Character.h"
#include "Drive.h"
#include "Mod.h"
#include "Pitch.h"
#include "Delay.h"
#include "Space.h"
#include "OutputGlue.h"

namespace fofopedal
{

// FofoEngine — orchestrates the six DSP blocks plus the Output Glue post-bus.
//
// Chain (logical):
//   [Character] → [Drive] → [Mod ↔ Pitch] → [Delay ↔ Space] → [OutputGlue]
//
// Two reorder toggles (Mod↔Pitch and Delay↔Space) flip the middle two
// stages. Character is always first, Drive always second, OutputGlue
// always last — per spec §A.
//
// Voicing (VOX/GTR/BASS/ACOUSTIC) is primarily preset metadata in V1: the
// curated characters bake in algorithm + parameter defaults tuned for each
// source type. The engine keeps the flag accessible for UI display and
// preset round-tripping but does not currently re-route blocks based on it.
class FofoEngine
{
public:
    enum class Voicing { Vox = 0, Gtr = 1, Bass = 2, Acoustic = 3 };

    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();

    // Block accessors — used by the processor for parameter wiring.
    Character&  character()  noexcept { return characterBlock; }
    Drive&      drive()      noexcept { return driveBlock; }
    Mod&        mod()        noexcept { return modBlock; }
    Pitch&      pitch()      noexcept { return pitchBlock; }
    Delay&      delay()      noexcept { return delayBlock; }
    Space&      space()      noexcept { return spaceBlock; }
    OutputGlue& glue()       noexcept { return glueBlock; }

    // Per-block bypass — wired to the menu drawer's on/off switches.
    void setCharacterBypassed (bool b) noexcept { characterBlock.setDefeated (b); }
    void setDriveBypassed     (bool b) noexcept { driveBlock    .setBypassed (b); }
    void setModBypassed       (bool b) noexcept { modBlock      .setBypassed (b); }
    void setPitchBypassed     (bool b) noexcept { pitchBlock    .setBypassed (b); }
    void setDelayBypassed     (bool b) noexcept { delayBlock    .setBypassed (b); }
    void setSpaceBypassed     (bool b) noexcept { spaceBlock    .setBypassed (b); }
    void setGlueDefeated      (bool b) noexcept { glueBlock     .setDefeated (b); }

    void setVoicing        (Voicing v) noexcept { voicing = v; }
    Voicing getVoicing()    const noexcept { return voicing; }

    void setModPitchSwap   (bool b) noexcept { swapModPitch   = b; }
    void setDelaySpaceSwap (bool b) noexcept { swapDelaySpace = b; }
    bool getModPitchSwap()    const noexcept { return swapModPitch; }
    bool getDelaySpaceSwap()  const noexcept { return swapDelaySpace; }

    int getLatencySamples() const noexcept { return driveBlock.getLatencySamples(); }

    void process (juce::AudioBuffer<float>& buffer) noexcept;

    float fetchInputPeak()  noexcept { return inputPeak .exchange (0.0f); }
    float fetchOutputPeak() noexcept { return outputPeak.exchange (0.0f); }

private:
    Character  characterBlock;
    Drive      driveBlock;
    Mod        modBlock;
    Pitch      pitchBlock;
    Delay      delayBlock;
    Space      spaceBlock;
    OutputGlue glueBlock;

    Voicing voicing { Voicing::Gtr };
    bool swapModPitch    { false };
    bool swapDelaySpace  { false };

    std::atomic<float> inputPeak  { 0.0f };
    std::atomic<float> outputPeak { 0.0f };
};

} // namespace fofopedal
