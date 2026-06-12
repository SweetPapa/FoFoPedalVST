#pragma once

namespace vroom
{

enum SourceMode
{
    Mode_Electric = 0,
    Mode_Acoustic = 1,
    Mode_Bass     = 2,
    NumSourceModes
};

// Hidden engine config bound to each Source Mode (spec §5). Switching mode
// updates this voicing — user knobs keep their positions, but the engine
// behind them reshapes.
struct SourceModeConfig
{
    float preHPFHz;          // tighten the lows before clipping
    float bodyCenterHz;      // Body-EQ peak center frequency

    bool  acousticVoicing;   // enables piezo tamer (-3 dB @ 3 kHz) + air shelf (+2.5 dB above 8 kHz)
    float driveCeilingScale; // 1.0 = normal, <1.0 = capped (Acoustic ≈ 0.6 → reduced max drive)
    float sagResponseScale;  // 1.0 = normal, <1.0 = lighter (Acoustic ≈ 0.5)

    bool  bandSplitDrive;    // Bass: only drive the high band
    float bandSplitHz;       // crossover for the above

    int   defaultCabSlot;    // CabSim slot to load on entering this mode
    bool  defaultCabEnabled; // and whether to start with cab on/off
};

inline const SourceModeConfig& getModeConfig (SourceMode m) noexcept
{
    // Order must match the SourceMode enum and the AudioParameterChoice
    // declaration in PluginProcessor.
    static constexpr SourceModeConfig kConfigs[NumSourceModes] = {
        // Electric (spec §5)
        {
            /* preHPFHz          */ 90.0f,
            /* bodyCenterHz      */ 300.0f,
            /* acousticVoicing   */ false,
            /* driveCeilingScale */ 1.0f,
            /* sagResponseScale  */ 1.0f,
            /* bandSplitDrive    */ false,
            /* bandSplitHz       */ 0.0f,
            /* defaultCabSlot    */ 0,    // 1x12 Warm
            /* defaultCabEnabled */ true,
        },
        // Acoustic (spec §5)
        {
            /* preHPFHz          */ 60.0f,
            /* bodyCenterHz      */ 200.0f,
            /* acousticVoicing   */ true,
            /* driveCeilingScale */ 0.6f, // ~40 % less max drive
            /* sagResponseScale  */ 0.5f, // preserve dynamics
            /* bandSplitDrive    */ false,
            /* bandSplitHz       */ 0.0f,
            /* defaultCabSlot    */ 3,    // Full-Range / DI
            /* defaultCabEnabled */ false,
        },
        // Bass (spec §5)
        {
            /* preHPFHz          */ 30.0f, // band-split owns the lows already
            /* bodyCenterHz      */ 130.0f,
            /* acousticVoicing   */ false,
            /* driveCeilingScale */ 1.0f,
            /* sagResponseScale  */ 1.0f,
            /* bandSplitDrive    */ true,
            /* bandSplitHz       */ 180.0f,
            /* defaultCabSlot    */ 3,    // DI (bass cab also OK if loaded)
            /* defaultCabEnabled */ false,
        },
    };

    const int idx = (m < 0 || m >= NumSourceModes) ? 0 : (int) m;
    return kConfigs[idx];
}

} // namespace vroom
