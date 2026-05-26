#include "PluginProcessor.h"
#include "PluginEditor.h"

using APVTS = juce::AudioProcessorValueTreeState;
using namespace vroom;

namespace
{
    constexpr int kParamVersionHint = 1;

    juce::AudioParameterFloatAttributes percentAttributes()
    {
        return juce::AudioParameterFloatAttributes{}
            .withStringFromValueFunction ([](float v, int) { return juce::String ((int) std::round (v)); });
    }

    juce::AudioParameterFloatAttributes dbAttrs()
    {
        return juce::AudioParameterFloatAttributes{}
            .withLabel ("dB")
            .withStringFromValueFunction ([](float v, int) { return juce::String (v, 1) + " dB"; });
    }
}

APVTS::ParameterLayout VroomAudioProcessor::createParameterLayout()
{
    APVTS::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamID::input, kParamVersionHint }, "Input",
        juce::NormalisableRange<float> (-12.0f, 12.0f, 0.01f), 0.0f, dbAttrs()));

    layout.add (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamID::drive, kParamVersionHint }, "Drive",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f), 45.0f, percentAttributes()));

    layout.add (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamID::character, kParamVersionHint }, "Character",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f), 60.0f, percentAttributes()));

    layout.add (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamID::body, kParamVersionHint }, "Body",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f), 55.0f, percentAttributes()));

    layout.add (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamID::tone, kParamVersionHint }, "Tone",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f), 50.0f, percentAttributes()));

    layout.add (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamID::sag, kParamVersionHint }, "Sag",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f), 35.0f, percentAttributes()));

    layout.add (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamID::blend, kParamVersionHint }, "Blend",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f), 70.0f, percentAttributes()));

    layout.add (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamID::level, kParamVersionHint }, "Level",
        juce::NormalisableRange<float> (-24.0f, 12.0f, 0.01f), 0.0f, dbAttrs()));

    layout.add (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamID::gate, kParamVersionHint }, "Gate",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f), 0.0f, percentAttributes()));

    layout.add (std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { ParamID::sourceMode, kParamVersionHint }, "Source Mode",
        juce::StringArray { "Electric", "Acoustic", "Bass" }, 0));

    layout.add (std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { ParamID::cabEnable, kParamVersionHint }, "Cab Enable", true));

    layout.add (std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { ParamID::cabIR, kParamVersionHint }, "Cab IR",
        juce::StringArray { "1x12 Warm", "4x12 Modern", "Bass 1x15", "Full-Range / DI" }, 0));

    layout.add (std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { ParamID::oversampling, kParamVersionHint }, "Oversampling",
        juce::StringArray { "2x", "4x", "8x" }, 1));

    return layout;
}

VroomAudioProcessor::VroomAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
                              .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                              .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, juce::Identifier ("VROOM"), createParameterLayout())
{
    inputDbParam   = apvts.getRawParameterValue (ParamID::input);
    driveParam     = apvts.getRawParameterValue (ParamID::drive);
    characterParam = apvts.getRawParameterValue (ParamID::character);
    levelDbParam   = apvts.getRawParameterValue (ParamID::level);
}

void VroomAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec {
        sampleRate,
        (juce::uint32) samplesPerBlock,
        (juce::uint32) juce::jmax (1, getTotalNumOutputChannels())
    };

    tone.prepare (spec);
    saturator.prepare (spec);

    // Electric Guitar default voicing (spec §5). Mode switching arrives in Phase 5.
    tone.setPreHPFHz (90.0f);

    inputGainSmoothed .reset (sampleRate, 0.02);
    outputGainSmoothed.reset (sampleRate, 0.02);
    inputGainSmoothed .setCurrentAndTargetValue (juce::Decibels::decibelsToGain (inputDbParam ? inputDbParam->load() : 0.0f));
    outputGainSmoothed.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (levelDbParam ? levelDbParam->load() : 0.0f));

    setLatencySamples (saturator.getLatencySamples());
}

void VroomAudioProcessor::releaseResources()
{
    tone.reset();
    saturator.reset();
}

bool VroomAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();

    if (in.isDisabled() || out.isDisabled()) return false;
    if (in != out) return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void VroomAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto totalIn  = getTotalNumInputChannels();
    const auto totalOut = getTotalNumOutputChannels();
    for (auto i = totalIn; i < totalOut; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // Push current parameter values into smoothers / DSP blocks.
    inputGainSmoothed .setTargetValue (juce::Decibels::decibelsToGain (inputDbParam->load()));
    outputGainSmoothed.setTargetValue (juce::Decibels::decibelsToGain (levelDbParam->load()));

    saturator.setDrive01     ((driveParam     ? driveParam    ->load() : 0.0f) * 0.01f);
    saturator.setCharacter01 ((characterParam ? characterParam->load() : 0.0f) * 0.01f);

    const int nS = buffer.getNumSamples();

    // Capture input peak BEFORE any processing — answers "is signal arriving?"
    {
        float peak = 0.0f;
        for (int ch = 0; ch < totalOut; ++ch)
            peak = juce::jmax (peak, buffer.getMagnitude (ch, 0, nS));

        // Lock-free max: only overwrite if our new sample is bigger.
        float cur = inputPeakMax.load (std::memory_order_relaxed);
        while (peak > cur && ! inputPeakMax.compare_exchange_weak (cur, peak, std::memory_order_release, std::memory_order_relaxed)) {}
    }

    // Input trim — advance smoother once per sample-frame, apply to every channel.
    for (int n = 0; n < nS; ++n)
    {
        const float g = inputGainSmoothed.getNextValue();
        for (int ch = 0; ch < totalOut; ++ch)
            buffer.getWritePointer (ch)[n] *= g;
    }

    tone.processPre (buffer);
    saturator.process (buffer);
    tone.processPost (buffer);

    for (int n = 0; n < nS; ++n)
    {
        const float g = outputGainSmoothed.getNextValue();
        for (int ch = 0; ch < totalOut; ++ch)
            buffer.getWritePointer (ch)[n] *= g;
    }

    // Capture output peak AFTER processing.
    {
        float peak = 0.0f;
        for (int ch = 0; ch < totalOut; ++ch)
            peak = juce::jmax (peak, buffer.getMagnitude (ch, 0, nS));

        float cur = outputPeakMax.load (std::memory_order_relaxed);
        while (peak > cur && ! outputPeakMax.compare_exchange_weak (cur, peak, std::memory_order_release, std::memory_order_relaxed)) {}
    }
}

juce::AudioProcessorEditor* VroomAudioProcessor::createEditor()
{
    return new VroomAudioProcessorEditor (*this);
}

void VroomAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void VroomAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VroomAudioProcessor();
}
