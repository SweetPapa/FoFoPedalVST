#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "dsp/DoubleEngine.h"
#include "presets/PresetBank.h"

namespace dbl
{
    namespace ParamID
    {
        inline constexpr const char* thick = "thick";
        inline constexpr const char* wide  = "wide";
        inline constexpr const char* human = "human";
        inline constexpr const char* mix   = "mix";
        inline constexpr const char* mode  = "mode";
    }
}

class DoubleAudioProcessor : public juce::AudioProcessor
{
public:
    DoubleAudioProcessor();
    ~DoubleAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "DOUBLE"; }

    bool acceptsMidi()  const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.1; }

    // Factory presets are exposed as host programs, so the DAW's own preset
    // menu lists them alongside anything the user saves as a track preset.
    int getNumPrograms() override                        { return presets.getNumPrograms(); }
    int getCurrentProgram() override                     { return presets.getCurrentProgram(); }
    void setCurrentProgram (int index) override          { presets.setCurrentProgram (index); }
    const juce::String getProgramName (int index) override { return presets.getProgramName (index); }
    void changeProgramName (int, const juce::String&) override {}

    fofo::FactoryPresetHost& getPresets() noexcept { return presets; }

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts; }

    float fetchInputPeakAndReset()  noexcept { return engine.fetchInputPeak(); }
    float fetchOutputPeakAndReset() noexcept { return engine.fetchOutputPeak(); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorValueTreeState apvts;
    fofo::FactoryPresetHost presets;

    std::atomic<float>* thickParam { nullptr };
    std::atomic<float>* wideParam  { nullptr };
    std::atomic<float>* humanParam { nullptr };
    std::atomic<float>* mixParam   { nullptr };
    std::atomic<float>* modeParam  { nullptr };

    dbl::DoubleEngine engine;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DoubleAudioProcessor)
};

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();
