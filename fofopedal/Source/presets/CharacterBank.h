#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>

namespace fofopedal
{

// The 12 curated characters from seriesA.spec.md §C. Each is a snapshot of:
//   • Voicing source bias
//   • 6 top-level macros (character/drive/shape/time/space/mix)
//   • 5 algorithm picks (drive/mod/pitch/delay/space type)
//   • Per-block bypasses
//   • All hidden buddy parameters
//   • Reorder toggles
//
// Loading a preset writes all 30+ APVTS values atomically (from the message
// thread), so a sub-cycle of audio never sees a half-applied character.
struct CharacterPreset
{
    const char* name;

    int   voicing;            // 0..3 (Vox/Gtr/Bass/Acoustic)

    // Top-level macros — percentage (0..100) to match APVTS units.
    float character;          // 0..100
    float drive;              // 0..100
    float shape;              // 0..100
    float timeMs;             // 1..2000 ms
    float space;              // 0..100
    float mix;                // 0..100

    int driveType;            // 0..2
    int modType;              // 0..2
    int pitchType;            // 0..2
    int delayType;            // 0..2
    int spaceType;            // 0..3

    bool characterDefeated;
    bool driveBypassed;
    bool modBypassed;
    bool pitchBypassed;
    bool delayBypassed;
    bool spaceBypassed;

    float characterLowCut;    // 20..200 Hz
    float driveTone;          // 0..100
    float driveMix;           // 0..100
    float modRate;            // 0..100
    float modDepth;           // 0..100
    float modFeedback;        // 0..100
    float modMix;             // 0..100
    float pitchAmount;        // 0..100
    float pitchShape;         // 0..100
    float pitchMix;           // 0..100
    float delayFeedback;      // 0..100
    float delayHfCut;         // 0..100
    bool  delayPingPong;
    float spacePreDelayMs;    // 0..250 ms
    float spaceShimmer;       // 0..100
    float spaceSendHpHz;      // 40..400 Hz
    float glueAmount;         // 0..100
    bool  glueDefeated;

    bool swapModPitch;
    bool swapDelaySpace;
};

class CharacterBank
{
public:
    static const std::array<CharacterPreset, 12> kFactoryPresets;

    // Push one preset's full state into APVTS. Caller is expected to be on
    // the message thread (the processor's AsyncUpdater bounces here from
    // the audio-thread parameterChanged hook).
    void applyToAPVTS (juce::AudioProcessorValueTreeState& apvts, int presetIndex) const;
};

} // namespace fofopedal
