#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "dsp/FofoEngine.h"
#include "presets/CharacterBank.h"

namespace fofopedal
{

namespace ParamID
{
    // ── Top-level 6 macros + global mix ─────────────────────────────────────
    inline constexpr const char* character        = "character";
    inline constexpr const char* drive            = "drive";
    inline constexpr const char* shape            = "shape";
    inline constexpr const char* timeMs           = "timeMs";
    inline constexpr const char* space            = "space";
    inline constexpr const char* mix              = "mix";

    // ── Algorithm choices (toggle row) ──────────────────────────────────────
    inline constexpr const char* voicing          = "voicing";
    inline constexpr const char* driveType        = "driveType";
    inline constexpr const char* modType          = "modType";
    inline constexpr const char* pitchType        = "pitchType";
    inline constexpr const char* delayType        = "delayType";
    inline constexpr const char* spaceType        = "spaceType";

    // ── Hidden (shift) ──────────────────────────────────────────────────────
    inline constexpr const char* characterLowCut  = "characterLowCut";
    inline constexpr const char* driveTone        = "driveTone";
    inline constexpr const char* driveMix         = "driveMix";
    inline constexpr const char* modRate          = "modRate";
    inline constexpr const char* modDepth         = "modDepth";
    inline constexpr const char* modFeedback      = "modFeedback";
    inline constexpr const char* modMix           = "modMix";
    inline constexpr const char* pitchAmount      = "pitchAmount";
    inline constexpr const char* pitchShape       = "pitchShape";
    inline constexpr const char* pitchMix         = "pitchMix";
    inline constexpr const char* delayFeedback    = "delayFeedback";
    inline constexpr const char* delayHfCut       = "delayHfCut";
    inline constexpr const char* delayPingPong    = "delayPingPong";
    inline constexpr const char* spacePreDelay    = "spacePreDelay";
    inline constexpr const char* spaceShimmer     = "spaceShimmer";
    inline constexpr const char* spaceSendHp      = "spaceSendHp";
    inline constexpr const char* glueAmount       = "glueAmount";
    inline constexpr const char* glueDefeated     = "glueDefeated";

    // ── Per-block bypasses ──────────────────────────────────────────────────
    inline constexpr const char* characterDefeated = "characterDefeated";
    inline constexpr const char* driveBypassed     = "driveBypassed";
    inline constexpr const char* modBypassed       = "modBypassed";
    inline constexpr const char* pitchBypassed     = "pitchBypassed";
    inline constexpr const char* delayBypassed     = "delayBypassed";
    inline constexpr const char* spaceBypassed     = "spaceBypassed";

    // ── Routing / housekeeping ──────────────────────────────────────────────
    inline constexpr const char* swapModPitch     = "swapModPitch";
    inline constexpr const char* swapDelaySpace   = "swapDelaySpace";
    inline constexpr const char* characterPreset  = "characterPreset";
    inline constexpr const char* hostSync         = "hostSync";
    inline constexpr const char* syncDivision     = "syncDivision";
}

}

class FofopedalAudioProcessor : public juce::AudioProcessor,
                                private juce::AudioProcessorValueTreeState::Listener,
                                private juce::AsyncUpdater
{
public:
    FofopedalAudioProcessor();
    ~FofopedalAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "FOFOPEDAL"; }

    bool acceptsMidi()  const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    // The 12 characters are already a curated preset bank driven by the
    // characterPreset parameter, so the host's program list is just a second
    // door onto the same 12 rather than a parallel set of its own.
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts; }

    float fetchInputPeakAndReset()  noexcept { return engine.fetchInputPeak(); }
    float fetchOutputPeakAndReset() noexcept { return engine.fetchOutputPeak(); }

    const fofopedal::CharacterBank& getCharacterBank() const noexcept { return bank; }

    // Used by editor to apply A↔B and store snapshots — V1 stub.
    void applyCharacterPresetByIndex (int idx);

private:
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void handleAsyncUpdate() override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void wireBlockParameters() noexcept;

    juce::AudioProcessorValueTreeState apvts;

    fofopedal::FofoEngine engine;
    fofopedal::CharacterBank bank;

    // Pre-allocated dry buffer for the global MIX crossfade (no audio-thread
    // allocation).
    juce::AudioBuffer<float> globalDryBuffer;

    // Pending preset index — applied on the message thread via AsyncUpdater
    // so a 30+-param preset push isn't half-applied on the audio thread.
    std::atomic<int>  pendingPresetIndex { -1 };
    std::atomic<bool> loadInProgress     { false };

    // Cached atomic pointers — looked up once in ctor for speed in processBlock.
    std::atomic<float>* pCharacter         { nullptr };
    std::atomic<float>* pCharacterLowCut   { nullptr };
    std::atomic<float>* pCharacterDefeated { nullptr };
    std::atomic<float>* pDrive             { nullptr };
    std::atomic<float>* pDriveType         { nullptr };
    std::atomic<float>* pDriveTone         { nullptr };
    std::atomic<float>* pDriveMix          { nullptr };
    std::atomic<float>* pDriveBypassed     { nullptr };
    std::atomic<float>* pShape             { nullptr };
    std::atomic<float>* pModType           { nullptr };
    std::atomic<float>* pModRate           { nullptr };
    std::atomic<float>* pModDepth          { nullptr };
    std::atomic<float>* pModFeedback       { nullptr };
    std::atomic<float>* pModMix            { nullptr };
    std::atomic<float>* pModBypassed       { nullptr };
    std::atomic<float>* pPitchType         { nullptr };
    std::atomic<float>* pPitchAmount       { nullptr };
    std::atomic<float>* pPitchShape        { nullptr };
    std::atomic<float>* pPitchMix          { nullptr };
    std::atomic<float>* pPitchBypassed     { nullptr };
    std::atomic<float>* pTimeMs            { nullptr };
    std::atomic<float>* pDelayType         { nullptr };
    std::atomic<float>* pDelayFeedback     { nullptr };
    std::atomic<float>* pDelayHfCut        { nullptr };
    std::atomic<float>* pDelayPingPong     { nullptr };
    std::atomic<float>* pDelayBypassed     { nullptr };
    std::atomic<float>* pSpace             { nullptr };
    std::atomic<float>* pSpaceType         { nullptr };
    std::atomic<float>* pSpacePreDelay     { nullptr };
    std::atomic<float>* pSpaceShimmer      { nullptr };
    std::atomic<float>* pSpaceSendHp       { nullptr };
    std::atomic<float>* pSpaceBypassed     { nullptr };
    std::atomic<float>* pMix               { nullptr };
    std::atomic<float>* pGlueAmount        { nullptr };
    std::atomic<float>* pGlueDefeated      { nullptr };
    std::atomic<float>* pVoicing           { nullptr };
    std::atomic<float>* pSwapModPitch      { nullptr };
    std::atomic<float>* pSwapDelaySpace    { nullptr };
    std::atomic<float>* pHostSync          { nullptr };
    std::atomic<float>* pSyncDivision      { nullptr };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FofopedalAudioProcessor)
};

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();
