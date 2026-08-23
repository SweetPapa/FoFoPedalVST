#include "PluginProcessor.h"
#include "PluginEditor.h"

using APVTS = juce::AudioProcessorValueTreeState;
using namespace daydream;

namespace
{
    constexpr int kParamVersionHint = 1;

    // The pedal boots on its default preset, so the preset bank is the single
    // source of that truth and the parameter defaults are read back out of it.
    // Hard-coding them here as well is how the two silently drift apart.
    float defaultFor (const char* id)
    {
        const auto& preset = daydream::getFactoryPresets()[(size_t) daydream::kDefaultPresetIndex];

        for (const auto& value : preset.values)
            if (juce::String (value.paramId) == id)
                return value.value;

        jassertfalse; // the default preset is missing a parameter
        return 0.0f;
    }
}

APVTS::ParameterLayout DaydreamAudioProcessor::createParameterLayout()
{
    APVTS::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamID::dream, kParamVersionHint }, "Dream",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f), defaultFor (ParamID::dream),
        juce::AudioParameterFloatAttributes{}
            .withStringFromValueFunction ([](float v, int) { return juce::String ((int) std::round (v)); })));

    return layout;
}

DaydreamAudioProcessor::DaydreamAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
                              .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                              .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, juce::Identifier ("DAYDREAM"), createParameterLayout()),
      presets (*this, apvts, daydream::getFactoryPresets(), daydream::kDefaultPresetIndex)
{
    dreamParam = apvts.getRawParameterValue (ParamID::dream);
}

void DaydreamAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec {
        sampleRate,
        (juce::uint32) samplesPerBlock,
        (juce::uint32) juce::jmax (1, getTotalNumOutputChannels())
    };
    engine.prepare (spec);
    setLatencySamples (engine.getLatencySamples());
}

void DaydreamAudioProcessor::releaseResources()
{
    engine.reset();
}

bool DaydreamAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();

    if (in.isDisabled() || out.isDisabled()) return false;
    if (in != out) return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void DaydreamAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto totalIn  = getTotalNumInputChannels();
    const auto totalOut = getTotalNumOutputChannels();
    for (auto i = totalIn; i < totalOut; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    if (dreamParam) engine.setDream01 (dreamParam->load() * 0.01f);
    engine.process (buffer);
}

juce::AudioProcessorEditor* DaydreamAudioProcessor::createEditor()
{
    return new DaydreamAudioProcessorEditor (*this);
}

void DaydreamAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void DaydreamAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));

            // The values are back; this recovers which preset name to show
            // and re-baselines the edited marker against them.
            presets.restoreIndexFromState();
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DaydreamAudioProcessor();
}
