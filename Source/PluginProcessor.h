#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "dsp/Saturator.h"
#include "dsp/ToneStack.h"
#include "dsp/Sag.h"
#include "dsp/CabSim.h"
#include "dsp/BandSplit.h"
#include "modes/SourceMode.h"
#include "presets/PresetManager.h"

namespace vroom
{
    namespace ParamID
    {
        inline constexpr const char* input        = "input";
        inline constexpr const char* drive        = "drive";
        inline constexpr const char* character    = "character";
        inline constexpr const char* body         = "body";
        inline constexpr const char* tone         = "tone";
        inline constexpr const char* sag          = "sag";
        inline constexpr const char* blend        = "blend";
        inline constexpr const char* level        = "level";
        inline constexpr const char* gate         = "gate";
        inline constexpr const char* sourceMode   = "sourceMode";
        inline constexpr const char* cabEnable    = "cabEnable";
        inline constexpr const char* cabIR        = "cabIR";
        inline constexpr const char* oversampling = "oversampling";
        inline constexpr const char* clipShape    = "clipShape";
    }
}

class VroomAudioProcessor : public juce::AudioProcessor,
                            private juce::AudioProcessorValueTreeState::Listener,
                            private juce::AsyncUpdater
{
public:
    VroomAudioProcessor();
    ~VroomAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "VROOM"; }

    bool acceptsMidi()  const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    // The host's program list mirrors the factory bank the PresetManager
    // already owns. User presets deliberately stay out of it: the list has to
    // be stable for the host, and the user's folder can change under us.
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts; }

    float fetchInputPeakAndReset()  noexcept { return inputPeakMax .exchange (0.0f); }
    float fetchOutputPeakAndReset() noexcept { return outputPeakMax.exchange (0.0f); }

    // Called from the editor (message thread) when the user picks a custom IR.
    // Returns the displayable name, or empty string on failure.
    juce::String loadCustomIR (const juce::File& irFile);

    juce::String getCurrentIRDisplayName() const;

    vroom::PresetManager& getPresetManager() noexcept { return presetManager; }

private:
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void handleAsyncUpdate() override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorValueTreeState apvts;

    std::atomic<float>* inputDbParam     { nullptr };
    std::atomic<float>* driveParam       { nullptr };
    std::atomic<float>* characterParam   { nullptr };
    std::atomic<float>* bodyParam        { nullptr };
    std::atomic<float>* toneParam        { nullptr };
    std::atomic<float>* sagParam         { nullptr };
    std::atomic<float>* blendParam       { nullptr };
    std::atomic<float>* levelDbParam     { nullptr };
    std::atomic<float>* cabEnableParam   { nullptr };
    std::atomic<float>* cabIRParam       { nullptr };
    std::atomic<float>* sourceModeParam  { nullptr };
    std::atomic<float>* clipShapeParam   { nullptr };

    vroom::ToneStack tone;
    vroom::Saturator saturator;
    vroom::Sag       sag;
    vroom::CabSim    cab;
    vroom::BandSplit bandSplit;
    bool             bandSplitActive { false };
    int              currentSourceMode { vroom::Mode_Electric };
    float            sagModeScale { 1.0f }; // mode-driven scale shared by Sag block + saturator touch response

    // Splits "mode change" into two parts so preset loads can apply only the
    // hidden DSP voicing without overwriting the preset's cab choice with the
    // mode's default cab (which is what user-driven mode switches still do).
    void applyModeVoicing      (int modeIdx);
    void applyModeDefaultCab   (int modeIdx);
    void applySourceModeConfig (int modeIdx); // voicing + default cab

    juce::SmoothedValue<float> inputGainSmoothed  { 1.0f };
    juce::SmoothedValue<float> outputGainSmoothed { 1.0f };
    juce::SmoothedValue<float> blendSmoothed      { 0.7f };

    vroom::PresetManager presetManager { apvts };

    // Which factory slot the host should report while a user preset is live.
    int lastFactoryProgram { 0 };

    juce::AudioBuffer<float> dryBuffer;
    juce::AudioBuffer<float> lowsBuffer;  // bass-mode clean low band
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> dryDelay  { 64 };
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> lowsDelay { 64 };

    std::atomic<float> inputPeakMax  { 0.0f };
    std::atomic<float> outputPeakMax { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VroomAudioProcessor)
};

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();
