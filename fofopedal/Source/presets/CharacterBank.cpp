#include "CharacterBank.h"
#include "../PluginProcessor.h"

namespace fofopedal
{

// ── The 12 hand-tuned characters (v2) ──────────────────────────────────────
// Retuned for the magnitude-macro MIX (it scales every block's intensity,
// RC-20 style). v2 rule, from the research pass: each character is ONE vibe —
// at most two featured blocks plus the console (Character + Glue). The v1
// characters lit up four or five blocks at once, which is why everything
// sounded like wet cardboard.
// "Voicing": 0=Vox, 1=Gtr, 2=Bass, 3=Acoustic.
// Algorithms: drive {Tube,Tape,Iron}; mod {Chorus,Phaser,TremVib};
// pitch {Detune,Harmony,Freeze}; delay {Digital,BBD,Tape}; space {Plate,Hall,Room,Shimmer}.
const std::array<CharacterPreset, 12> CharacterBank::kFactoryPresets = {{
    // 1 — Front Porch (Acoustic guitar / piano): a hall, nothing else.
    { "Front Porch",
      /*voicing*/ 3,
      /*char*/ 18.0f, /*drive*/  0.0f, /*shape*/ 30.0f, /*time*/ 350.0f, /*space*/ 45.0f, /*mix*/ 70.0f,
      /*drvT*/0,/*modT*/0,/*pchT*/0,/*dlyT*/0,/*spcT*/1 /*Hall*/,
      /*charOff*/false, /*drvOff*/true,/*modOff*/true,/*pchOff*/true,/*dlyOff*/true,/*spcOff*/false,
      /*lowCut*/ 28.0f, /*drvTone*/50.0f,/*drvMix*/100.0f,
      /*modRate*/30,/*modDep*/40,/*modFb*/15,/*modMix*/25,
      /*pchAmt*/ 0.0f,/*pchShp*/ 50.0f,/*pchMix*/ 0.0f,
      /*dlyFb*/  25.0f,/*dlyHf*/  35.0f,/*pp*/false,
      /*preDly*/45.0f,/*shimmer*/0.0f,/*sendHP*/130.0f, /*glue*/30.0f,/*glueOff*/false,
      /*swapMP*/false,/*swapDS*/false },

    // 2 — Cassette Sunday (Lo-fi vocal/guitar): tape + slow chorus + dark BBD.
    { "Cassette Sunday",
      /*voicing*/ 0,
      /*char*/ 65.0f,/*drive*/ 35.0f,/*shape*/ 45.0f,/*time*/ 470.0f,/*space*/ 25.0f,/*mix*/ 75.0f,
      /*drvT*/1 /*Tape*/, /*modT*/0,/*pchT*/0,/*dlyT*/1 /*BBD*/,/*spcT*/0 /*Plate*/,
      /*charOff*/false,/*drvOff*/false,/*modOff*/false,/*pchOff*/true,/*dlyOff*/false,/*spcOff*/false,
      /*lowCut*/ 50.0f,/*drvTone*/35.0f,/*drvMix*/100.0f,
      /*modRate*/22,/*modDep*/55,/*modFb*/10,/*modMix*/30,
      /*pchAmt*/0,/*pchShp*/50,/*pchMix*/0,
      /*dlyFb*/ 30.0f,/*dlyHf*/ 70.0f,/*pp*/false,
      /*preDly*/15.0f,/*shimmer*/0.0f,/*sendHP*/120.0f, /*glue*/45.0f,/*glueOff*/false,
      /*swapMP*/false,/*swapDS*/false },

    // 3 — Cathedral Larynx (Massive vocal): detune into shimmer hall. No phaser.
    { "Cathedral Larynx",
      /*voicing*/ 0,
      /*char*/ 35.0f,/*drive*/  0.0f,/*shape*/ 55.0f,/*time*/ 500.0f,/*space*/ 78.0f,/*mix*/ 80.0f,
      /*drvT*/0,/*modT*/0,/*pchT*/0 /*Detune*/,/*dlyT*/0,/*spcT*/3 /*Shimmer*/,
      /*charOff*/false,/*drvOff*/true,/*modOff*/true,/*pchOff*/false,/*dlyOff*/true,/*spcOff*/false,
      /*lowCut*/ 60.0f,/*drvTone*/50.0f,/*drvMix*/100.0f,
      /*modRate*/12,/*modDep*/40,/*modFb*/30,/*modMix*/25,
      /*pchAmt*/ 60.0f,/*pchShp*/ 50.0f,/*pchMix*/ 45.0f,
      /*dlyFb*/  22.0f,/*dlyHf*/  30.0f,/*pp*/false,
      /*preDly*/55.0f,/*shimmer*/55.0f,/*sendHP*/140.0f, /*glue*/35.0f,/*glueOff*/false,
      /*swapMP*/false,/*swapDS*/false },

    // 4 — Dub Lounge: ping-pong tape echo, a cup of plate, hint of tube.
    { "Dub Lounge",
      /*voicing*/ 1,
      /*char*/ 30.0f,/*drive*/ 18.0f,/*shape*/ 35.0f,/*time*/ 280.0f,/*space*/ 30.0f,/*mix*/ 80.0f,
      /*drvT*/0 /*Tube*/,/*modT*/0,/*pchT*/0,/*dlyT*/2 /*Tape*/,/*spcT*/0 /*Plate*/,
      /*charOff*/false,/*drvOff*/false,/*modOff*/true,/*pchOff*/true,/*dlyOff*/false,/*spcOff*/false,
      /*lowCut*/ 30.0f,/*drvTone*/50.0f,/*drvMix*/ 60.0f,
      /*modRate*/20,/*modDep*/30,/*modFb*/15,/*modMix*/25,
      /*pchAmt*/0,/*pchShp*/50,/*pchMix*/0,
      /*dlyFb*/  60.0f,/*dlyHf*/  55.0f,/*pp*/true,
      /*preDly*/15.0f,/*shimmer*/0.0f,/*sendHP*/110.0f, /*glue*/30.0f,/*glueOff*/false,
      /*swapMP*/false,/*swapDS*/false },

    // 5 — Shoebox Shoegaze: the deliberate maximal one — iron wall + octave-up + hall.
    { "Shoebox Shoegaze",
      /*voicing*/ 1,
      /*char*/ 25.0f,/*drive*/ 70.0f,/*shape*/ 65.0f,/*time*/ 320.0f,/*space*/ 80.0f,/*mix*/ 85.0f,
      /*drvT*/2 /*Iron*/,/*modT*/0,/*pchT*/1 /*Harmony*/,/*dlyT*/0,/*spcT*/1 /*Hall*/,
      /*charOff*/false,/*drvOff*/false,/*modOff*/false,/*pchOff*/false,/*dlyOff*/true,/*spcOff*/false,
      /*lowCut*/ 60.0f,/*drvTone*/55.0f,/*drvMix*/ 80.0f,
      /*modRate*/45,/*modDep*/65,/*modFb*/25,/*modMix*/40,
      /*pchAmt*/ 50.0f,/*pchShp*/ 92.0f /*octave up*/,/*pchMix*/ 30.0f,
      /*dlyFb*/  35.0f,/*dlyHf*/  35.0f,/*pp*/false,
      /*preDly*/0.0f,/*shimmer*/0.0f,/*sendHP*/100.0f, /*glue*/40.0f,/*glueOff*/false,
      /*swapMP*/false,/*swapDS*/false },

    // 6 — Nylon Velvet (Acoustic / classical): a small room, full stop.
    { "Nylon Velvet",
      /*voicing*/ 3,
      /*char*/ 15.0f,/*drive*/  0.0f,/*shape*/ 20.0f,/*time*/ 250.0f,/*space*/ 30.0f,/*mix*/ 60.0f,
      /*drvT*/0,/*modT*/0,/*pchT*/0,/*dlyT*/0,/*spcT*/2 /*Room*/,
      /*charOff*/false,/*drvOff*/true,/*modOff*/true,/*pchOff*/true,/*dlyOff*/true,/*spcOff*/false,
      /*lowCut*/ 30.0f,/*drvTone*/50.0f,/*drvMix*/100.0f,
      /*modRate*/8,/*modDep*/25,/*modFb*/10,/*modMix*/18,
      /*pchAmt*/0,/*pchShp*/50,/*pchMix*/0,
      /*dlyFb*/ 20.0f,/*dlyHf*/  50.0f,/*pp*/false,
      /*preDly*/20.0f,/*shimmer*/0.0f,/*sendHP*/130.0f, /*glue*/22.0f,/*glueOff*/false,
      /*swapMP*/false,/*swapDS*/false },

    // 7 — Garage Vox (Rock vocal): tube grit + short slap + plate. No octave.
    { "Garage Vox",
      /*voicing*/ 0,
      /*char*/ 55.0f,/*drive*/ 45.0f,/*shape*/ 40.0f,/*time*/  90.0f,/*space*/ 45.0f,/*mix*/ 78.0f,
      /*drvT*/0 /*Tube*/,/*modT*/0,/*pchT*/0,/*dlyT*/0,/*spcT*/0 /*Plate*/,
      /*charOff*/false,/*drvOff*/false,/*modOff*/true,/*pchOff*/true,/*dlyOff*/false,/*spcOff*/false,
      /*lowCut*/ 60.0f,/*drvTone*/55.0f,/*drvMix*/ 85.0f,
      /*modRate*/20,/*modDep*/30,/*modFb*/15,/*modMix*/25,
      /*pchAmt*/0,/*pchShp*/50,/*pchMix*/0,
      /*dlyFb*/  12.0f,/*dlyHf*/  35.0f,/*pp*/false,
      /*preDly*/30.0f,/*shimmer*/0.0f,/*sendHP*/120.0f, /*glue*/40.0f,/*glueOff*/false,
      /*swapMP*/false,/*swapDS*/false },

    // 8 — Pillow Bass: low tape squash + a breath of room. Subs stay clean.
    { "Pillow Bass",
      /*voicing*/ 2,
      /*char*/ 25.0f,/*drive*/ 18.0f,/*shape*/ 15.0f,/*time*/ 200.0f,/*space*/ 20.0f,/*mix*/ 70.0f,
      /*drvT*/1 /*Tape*/,/*modT*/0,/*pchT*/0,/*dlyT*/0,/*spcT*/2 /*Room*/,
      /*charOff*/false,/*drvOff*/false,/*modOff*/true,/*pchOff*/true,/*dlyOff*/true,/*spcOff*/false,
      /*lowCut*/ 20.0f,/*drvTone*/45.0f,/*drvMix*/ 65.0f,
      /*modRate*/15,/*modDep*/20,/*modFb*/10,/*modMix*/15,
      /*pchAmt*/0,/*pchShp*/50,/*pchMix*/0,
      /*dlyFb*/  15.0f,/*dlyHf*/  55.0f,/*pp*/false,
      /*preDly*/8.0f,/*shimmer*/0.0f,/*sendHP*/200.0f /* keep subs out */, /*glue*/25.0f,/*glueOff*/false,
      /*swapMP*/false,/*swapDS*/false },

    // 9 — Synth Bath (Keys / pads): chorus into long BBD into shimmer. No freeze.
    { "Synth Bath",
      /*voicing*/ 1,
      /*char*/ 30.0f,/*drive*/  0.0f,/*shape*/ 55.0f,/*time*/ 750.0f,/*space*/ 70.0f,/*mix*/ 80.0f,
      /*drvT*/0 /*Tube*/,/*modT*/0 /*Chorus*/,/*pchT*/0,/*dlyT*/1 /*BBD*/,/*spcT*/3 /*Shimmer*/,
      /*charOff*/false,/*drvOff*/true,/*modOff*/false,/*pchOff*/true,/*dlyOff*/false,/*spcOff*/false,
      /*lowCut*/ 25.0f,/*drvTone*/55.0f,/*drvMix*/ 80.0f,
      /*modRate*/14,/*modDep*/50,/*modFb*/20,/*modMix*/35,
      /*pchAmt*/0,/*pchShp*/50,/*pchMix*/0,
      /*dlyFb*/  35.0f,/*dlyHf*/  45.0f,/*pp*/false,
      /*preDly*/30.0f,/*shimmer*/50.0f,/*sendHP*/100.0f, /*glue*/38.0f,/*glueOff*/false,
      /*swapMP*/false,/*swapDS*/false },

    // 10 — Tin-Can Telephone (Lo-fi extreme): crushed character + iron + trem.
    { "Tin-Can Telephone",
      /*voicing*/ 0,
      /*char*/ 88.0f,/*drive*/ 75.0f,/*shape*/ 80.0f,/*time*/ 120.0f,/*space*/ 15.0f,/*mix*/ 85.0f,
      /*drvT*/2 /*Iron*/,/*modT*/2 /*Trem*/,/*pchT*/0,/*dlyT*/0,/*spcT*/2 /*Room*/,
      /*charOff*/false,/*drvOff*/false,/*modOff*/false,/*pchOff*/true,/*dlyOff*/true,/*spcOff*/false,
      /*lowCut*/120.0f,/*drvTone*/65.0f,/*drvMix*/ 90.0f,
      /*modRate*/70,/*modDep*/85,/*modFb*/10,/*modMix*/55,
      /*pchAmt*/0,/*pchShp*/50,/*pchMix*/0,
      /*dlyFb*/  10.0f,/*dlyHf*/  60.0f,/*pp*/false,
      /*preDly*/5.0f,/*shimmer*/0.0f,/*sendHP*/180.0f, /*glue*/55.0f,/*glueOff*/false,
      /*swapMP*/false,/*swapDS*/false },

    // 11 — Slap and Float (Funk guitar / clav): phaser swirl + tight slap.
    { "Slap and Float",
      /*voicing*/ 1,
      /*char*/ 50.0f,/*drive*/ 35.0f,/*shape*/ 50.0f,/*time*/ 250.0f,/*space*/ 30.0f,/*mix*/ 75.0f,
      /*drvT*/0 /*Tube*/,/*modT*/1 /*Phaser*/,/*pchT*/0,/*dlyT*/0,/*spcT*/0 /*Plate*/,
      /*charOff*/false,/*drvOff*/false,/*modOff*/false,/*pchOff*/true,/*dlyOff*/false,/*spcOff*/false,
      /*lowCut*/ 65.0f,/*drvTone*/60.0f,/*drvMix*/ 85.0f,
      /*modRate*/40,/*modDep*/55,/*modFb*/45,/*modMix*/45,
      /*pchAmt*/0,/*pchShp*/50,/*pchMix*/0,
      /*dlyFb*/  18.0f,/*dlyHf*/  20.0f,/*pp*/false,
      /*preDly*/15.0f,/*shimmer*/0.0f,/*sendHP*/110.0f, /*glue*/30.0f,/*glueOff*/false,
      /*swapMP*/false,/*swapDS*/false },

    // 12 — Vapor Trail (Experimental): freeze + vibrato + tape echo + shimmer.
    // The one character allowed to light up everything — clearly labeled weird.
    { "Vapor Trail",
      /*voicing*/ 0,
      /*char*/ 38.0f,/*drive*/  0.0f,/*shape*/ 60.0f,/*time*/1000.0f,/*space*/ 85.0f,/*mix*/ 88.0f,
      /*drvT*/0,/*modT*/2 /*Vib*/,/*pchT*/2 /*Freeze*/,/*dlyT*/2 /*Tape*/,/*spcT*/3 /*Shimmer*/,
      /*charOff*/false,/*drvOff*/true,/*modOff*/false,/*pchOff*/false,/*dlyOff*/false,/*spcOff*/false,
      /*lowCut*/ 50.0f,/*drvTone*/50.0f,/*drvMix*/100.0f,
      /*modRate*/10,/*modDep*/55,/*modFb*/15,/*modMix*/50,
      /*pchAmt*/ 30.0f,/*pchShp*/ 60.0f,/*pchMix*/ 50.0f,
      /*dlyFb*/  55.0f,/*dlyHf*/  55.0f,/*pp*/true,
      /*preDly*/60.0f,/*shimmer*/72.0f,/*sendHP*/120.0f, /*glue*/45.0f,/*glueOff*/false,
      /*swapMP*/true /*pitch before mod for haunted texture*/,/*swapDS*/false },
}};

namespace
{
    inline void setF (juce::AudioProcessorValueTreeState& a, const char* id, float v)
    {
        if (auto* p = a.getParameter (id))
        {
            const auto norm = p->convertTo0to1 (v);
            p->setValueNotifyingHost (norm);
        }
    }
    inline void setI (juce::AudioProcessorValueTreeState& a, const char* id, int v)
    {
        if (auto* p = a.getParameter (id))
        {
            const auto norm = p->convertTo0to1 ((float) v);
            p->setValueNotifyingHost (norm);
        }
    }
    inline void setB (juce::AudioProcessorValueTreeState& a, const char* id, bool v)
    {
        if (auto* p = a.getParameter (id))
        {
            p->setValueNotifyingHost (v ? 1.0f : 0.0f);
        }
    }
}

void CharacterBank::applyToAPVTS (juce::AudioProcessorValueTreeState& apvts, int presetIndex) const
{
    if (presetIndex < 0 || presetIndex >= (int) kFactoryPresets.size()) return;
    const auto& p = kFactoryPresets[(size_t) presetIndex];

    setF (apvts, ParamID::character,         p.character);
    setF (apvts, ParamID::drive,             p.drive);
    setF (apvts, ParamID::shape,             p.shape);
    setF (apvts, ParamID::timeMs,            p.timeMs);
    setF (apvts, ParamID::space,             p.space);
    setF (apvts, ParamID::mix,               p.mix);

    setI (apvts, ParamID::voicing,           p.voicing);
    setI (apvts, ParamID::driveType,         p.driveType);
    setI (apvts, ParamID::modType,           p.modType);
    setI (apvts, ParamID::pitchType,         p.pitchType);
    setI (apvts, ParamID::delayType,         p.delayType);
    setI (apvts, ParamID::spaceType,         p.spaceType);

    setB (apvts, ParamID::characterDefeated, p.characterDefeated);
    setB (apvts, ParamID::driveBypassed,     p.driveBypassed);
    setB (apvts, ParamID::modBypassed,       p.modBypassed);
    setB (apvts, ParamID::pitchBypassed,     p.pitchBypassed);
    setB (apvts, ParamID::delayBypassed,     p.delayBypassed);
    setB (apvts, ParamID::spaceBypassed,     p.spaceBypassed);

    setF (apvts, ParamID::characterLowCut,   p.characterLowCut);
    setF (apvts, ParamID::driveTone,         p.driveTone);
    setF (apvts, ParamID::driveMix,          p.driveMix);
    setF (apvts, ParamID::modRate,           p.modRate);
    setF (apvts, ParamID::modDepth,          p.modDepth);
    setF (apvts, ParamID::modFeedback,       p.modFeedback);
    setF (apvts, ParamID::modMix,            p.modMix);
    setF (apvts, ParamID::pitchAmount,       p.pitchAmount);
    setF (apvts, ParamID::pitchShape,        p.pitchShape);
    setF (apvts, ParamID::pitchMix,          p.pitchMix);
    setF (apvts, ParamID::delayFeedback,     p.delayFeedback);
    setF (apvts, ParamID::delayHfCut,        p.delayHfCut);
    setB (apvts, ParamID::delayPingPong,     p.delayPingPong);
    setF (apvts, ParamID::spacePreDelay,     p.spacePreDelayMs);
    setF (apvts, ParamID::spaceShimmer,      p.spaceShimmer);
    setF (apvts, ParamID::spaceSendHp,       p.spaceSendHpHz);
    setF (apvts, ParamID::glueAmount,        p.glueAmount);
    setB (apvts, ParamID::glueDefeated,      p.glueDefeated);

    setB (apvts, ParamID::swapModPitch,      p.swapModPitch);
    setB (apvts, ParamID::swapDelaySpace,    p.swapDelaySpace);
}

} // namespace fofopedal
