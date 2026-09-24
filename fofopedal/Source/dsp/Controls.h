#pragma once
#include "FofoEngine.h"
#include "../presets/CharacterBank.h"

namespace fofopedal
{

// ─────────────────────────────────────────────────────────────────────────────
// Front panel → engine. The ONE place a control value becomes a DSP setting.
//
// It lives here rather than inside the processor so the tests drive exactly
// the mapping that ships: a character is measured through the same function
// the plugin calls every block, not through a copy that can drift from it.
//
// The whole front-panel state is carried as a CharacterPreset, because that is
// already a complete snapshot of it. `timeMs` is the resolved delay time —
// the processor substitutes the host-synced value before calling this.
//
// MIX is "how much pedal". 70 plays a character exactly as it was voiced,
// 100 pushes every block ~1.4× harder, and 0 is dry. (v2 multiplied every
// block mix by MIX straight, so a character at MIX 75 had its chorus at 22%
// and its tremolo at 8% depth, and the pedal read as not doing anything.)
// ─────────────────────────────────────────────────────────────────────────────
inline constexpr float kMixAsVoiced = 0.70f;

inline void applyControls (FofoEngine& e, const CharacterPreset& c) noexcept
{
    auto pct = [] (float p) { return p * 0.01f; };
    const float g = pct (c.mix) / kMixAsVoiced;
    auto scaled = [g] (float blockMix01) { return juce::jlimit (0.0f, 1.0f, blockMix01 * g); };

    // Character — the always-on console stage; not scaled by MIX.
    e.character().setAmount01 (pct (c.character));
    e.character().setLowCutHz (c.characterLowCut);
    e.character().setDefeated (c.characterDefeated);

    e.drive().setAlgo     ((Drive::Algo) c.driveType);
    e.drive().setDrive01  (pct (c.drive));
    e.drive().setTone01   (pct (c.driveTone));
    e.drive().setMix01    (scaled (pct (c.driveMix)));
    e.drive().setBypassed (c.driveBypassed);

    // Mod — SHAPE adds up to 50% on top of the stored depth.
    const float shape = pct (c.shape);
    e.mod().setAlgo       ((Mod::Algo) c.modType);
    e.mod().setRate01     (pct (c.modRate));
    e.mod().setDepth01    (juce::jlimit (0.0f, 1.0f, pct (c.modDepth) + shape * 0.5f));
    e.mod().setShape01    (shape);
    e.mod().setFeedback01 (pct (c.modFeedback));
    e.mod().setMix01      (scaled (pct (c.modMix)));
    e.mod().setBypassed   (c.modBypassed);

    // Pitch — micro-detune amount also bumps with SHAPE on VOX/ACOUSTIC.
    const float vocalBias = (c.voicing == 0 || c.voicing == 3) ? 0.4f : 0.15f;
    e.pitch().setAlgo     ((Pitch::Algo) c.pitchType);
    e.pitch().setAmount01 (juce::jlimit (0.0f, 1.0f, pct (c.pitchAmount) + shape * vocalBias));
    e.pitch().setShape01  (pct (c.pitchShape));
    e.pitch().setMix01    (scaled (pct (c.pitchMix)));
    e.pitch().setBypassed (c.pitchBypassed);

    e.delay().setAlgo       ((Delay::Algo) c.delayType);
    e.delay().setTimeMs     (juce::jlimit (1.0f, 2000.0f, c.timeMs));
    e.delay().setFeedback01 (pct (c.delayFeedback));
    e.delay().setHfCut01    (pct (c.delayHfCut));
    e.delay().setMix01      (scaled (0.50f));
    e.delay().setPingPong   (c.delayPingPong);
    e.delay().setBypassed   (c.delayBypassed);

    // SPACE drives size and send together (Activity-knob style).
    const float space = pct (c.space);
    e.space().setAlgo       ((Space::Algo) c.spaceType);
    e.space().setSize01     (space);
    e.space().setMix01      (scaled (space * 0.85f));
    e.space().setPreDelayMs (c.spacePreDelayMs);
    e.space().setShimmer01  (pct (c.spaceShimmer));
    e.space().setSendHpHz   (c.spaceSendHpHz);
    e.space().setBypassed   (c.spaceBypassed);

    e.glue().setAmount01 (pct (c.glueAmount));
    e.glue().setDefeated (c.glueDefeated);

    e.setVoicing        ((FofoEngine::Voicing) c.voicing);
    e.setModPitchSwap   (c.swapModPitch);
    e.setDelaySpaceSwap (c.swapDelaySpace);
}

} // namespace fofopedal
