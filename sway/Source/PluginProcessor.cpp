#include "PluginProcessor.h"
#include "PluginEditor.h"

using APVTS = juce::AudioProcessorValueTreeState;
using namespace sway;

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

APVTS::ParameterLayout SwayAudioProcessor::createParameterLayout()
{
    APVTS::ParameterLayout layout;
    layout.add (pct (ParamID::move, "Move", 45.0f));
    layout.add (pct (ParamID::rate,  "Rate",  35.0f));
    layout.add (pct (ParamID::color, "Color", 50.0f));
    layout.add (pct (ParamID::mix,   "Mix",   100.0f));
    layout.add (std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { ParamID::mode, kParamVersionHint }, "Mode",
        juce::StringArray { "Tape", "Ensemble", "Pump" }, 0));
    return layout;
}

SwayAudioProcessor::SwayAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
                              .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                              .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, juce::Identifier ("SWAY"), createParameterLayout())
{
    moveParam = apvts.getRawParameterValue (ParamID::move);
    rateParam  = apvts.getRawParameterValue (ParamID::rate);
    colorParam = apvts.getRawParameterValue (ParamID::color);
    mixParam   = apvts.getRawParameterValue (ParamID::mix);
    modeParam  = apvts.getRawParameterValue (ParamID::mode);
}

void SwayAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec {
        sampleRate,
        (juce::uint32) samplesPerBlock,
        (juce::uint32) juce::jmax (1, getTotalNumOutputChannels())
    };
    engine.prepare (spec);
    setLatencySamples (engine.getLatencySamples());
}

void SwayAudioProcessor::releaseResources()
{
    engine.reset();
}

bool SwayAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();

    if (in.isDisabled() || out.isDisabled()) return false;
    if (in != out) return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void SwayAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto totalIn  = getTotalNumInputChannels();
    const auto totalOut = getTotalNumOutputChannels();
    for (auto i = totalIn; i < totalOut; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    engine.setMove01 (moveParam->load() * 0.01f);
    engine.setRate01  (rateParam ->load() * 0.01f);
    engine.setColor01 (colorParam->load() * 0.01f);
    engine.setMix01   (mixParam  ->load() * 0.01f);
    engine.setMode    ((sway::SwayEngine::Mode) (int) modeParam->load());

    engine.process (buffer);
}

juce::AudioProcessorEditor* SwayAudioProcessor::createEditor()
{
    return new SwayAudioProcessorEditor (*this);
}

void SwayAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void SwayAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SwayAudioProcessor();
}
