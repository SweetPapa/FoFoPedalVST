#include "PluginProcessor.h"
#include "PluginEditor.h"

using APVTS = juce::AudioProcessorValueTreeState;
using namespace bkpr;

namespace
{
    constexpr int kParamVersionHint = 1;

    std::unique_ptr<juce::AudioParameterFloat> pct (const char* id, const char* name, float def)
    {
        return std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { id, kParamVersionHint }, name,
            juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f), def,
            juce::AudioParameterFloatAttributes{}
                .withStringFromValueFunction ([](float v, int) { return juce::String ((int) std::round (v)); }));
    }
}

APVTS::ParameterLayout BackporchAudioProcessor::createParameterLayout()
{
    APVTS::ParameterLayout layout;
    layout.add (pct (ParamID::space, "Space", 45.0f));
    layout.add (pct (ParamID::tone,  "Tone",  45.0f));
    layout.add (pct (ParamID::duck, "Duck", 35.0f));
    layout.add (pct (ParamID::mix,   "Mix",   40.0f));
    layout.add (std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { ParamID::mode, kParamVersionHint }, "Mode",
        juce::StringArray { "Slap", "Room", "Plate" }, 1));
    return layout;
}

BackporchAudioProcessor::BackporchAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
                              .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                              .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, juce::Identifier ("BACKPORCH"), createParameterLayout())
{
    spaceParam = apvts.getRawParameterValue (ParamID::space);
    toneParam  = apvts.getRawParameterValue (ParamID::tone);
    duckParam = apvts.getRawParameterValue (ParamID::duck);
    mixParam   = apvts.getRawParameterValue (ParamID::mix);
    modeParam  = apvts.getRawParameterValue (ParamID::mode);
}

void BackporchAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec {
        sampleRate,
        (juce::uint32) samplesPerBlock,
        (juce::uint32) juce::jmax (1, getTotalNumOutputChannels())
    };
    engine.prepare (spec);
    setLatencySamples (engine.getLatencySamples());
}

void BackporchAudioProcessor::releaseResources()
{
    engine.reset();
}

bool BackporchAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();

    if (in.isDisabled() || out.isDisabled()) return false;
    if (in != out) return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void BackporchAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto totalIn  = getTotalNumInputChannels();
    const auto totalOut = getTotalNumOutputChannels();
    for (auto i = totalIn; i < totalOut; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    engine.setSpace01 (spaceParam->load() * 0.01f);
    engine.setTone01  (toneParam ->load() * 0.01f);
    engine.setDuck01 (duckParam->load() * 0.01f);
    engine.setMix01   (mixParam  ->load() * 0.01f);
    engine.setMode    ((bkpr::BackporchEngine::Mode) (int) modeParam->load());

    engine.process (buffer);
}

juce::AudioProcessorEditor* BackporchAudioProcessor::createEditor()
{
    return new BackporchAudioProcessorEditor (*this);
}

void BackporchAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void BackporchAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BackporchAudioProcessor();
}
