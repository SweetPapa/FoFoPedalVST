#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "dsp/Saturator.h"
#include "dsp/ToneStack.h"
#include "dsp/Sag.h"

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
    }
}

class VroomAudioProcessor : public juce::AudioProcessor
{
public:
    VroomAudioProcessor();
    ~VroomAudioProcessor() override = default;

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

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts; }

    float fetchInputPeakAndReset()  noexcept { return inputPeakMax .exchange (0.0f); }
    float fetchOutputPeakAndReset() noexcept { return outputPeakMax.exchange (0.0f); }

private:
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

    vroom::ToneStack tone;
    vroom::Saturator saturator;
    vroom::Sag       sag;

    juce::SmoothedValue<float> inputGainSmoothed  { 1.0f };
    juce::SmoothedValue<float> outputGainSmoothed { 1.0f };
    juce::SmoothedValue<float> blendSmoothed      { 0.7f };

    // Dry tap that runs parallel to the wet chain (Pre-HPF → drive → DC → Sag
    // → Body → Tone). Delayed to match the wet path's oversampling latency so
    // the parallel sum stays phase-coherent and we don't get comb filtering.
    juce::AudioBuffer<float> dryBuffer;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> dryDelay { 64 };

    std::atomic<float> inputPeakMax  { 0.0f };
    std::atomic<float> outputPeakMax { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VroomAudioProcessor)
};

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();
