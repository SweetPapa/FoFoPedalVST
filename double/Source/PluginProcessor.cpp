#include "PluginProcessor.h"
#include "PluginEditor.h"

using APVTS = juce::AudioProcessorValueTreeState;
using namespace dbl;

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

APVTS::ParameterLayout DoubleAudioProcessor::createParameterLayout()
{
    APVTS::ParameterLayout layout;
    layout.add (pct (ParamID::thick, "Thick", 50.0f));
    layout.add (pct (ParamID::wide,  "Wide",  70.0f));
    layout.add (pct (ParamID::human, "Human", 50.0f));
    layout.add (pct (ParamID::mix,   "Mix",   60.0f));
    layout.add (std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { ParamID::mode, kParamVersionHint }, "Mode",
        juce::StringArray { "Vox", "Strings", "Synth" }, 0));
    return layout;
}

DoubleAudioProcessor::DoubleAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
                              .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                              .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, juce::Identifier ("DOUBLE"), createParameterLayout())
{
    thickParam = apvts.getRawParameterValue (ParamID::thick);
    wideParam  = apvts.getRawParameterValue (ParamID::wide);
    humanParam = apvts.getRawParameterValue (ParamID::human);
    mixParam   = apvts.getRawParameterValue (ParamID::mix);
    modeParam  = apvts.getRawParameterValue (ParamID::mode);
}

void DoubleAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec {
        sampleRate,
        (juce::uint32) samplesPerBlock,
        (juce::uint32) juce::jmax (1, getTotalNumOutputChannels())
    };
    engine.prepare (spec);
    setLatencySamples (engine.getLatencySamples());
}

void DoubleAudioProcessor::releaseResources()
{
    engine.reset();
}

bool DoubleAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();

    if (in.isDisabled() || out.isDisabled()) return false;
    if (in != out) return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void DoubleAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto totalIn  = getTotalNumInputChannels();
    const auto totalOut = getTotalNumOutputChannels();
    for (auto i = totalIn; i < totalOut; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    engine.setThick01 (thickParam->load() * 0.01f);
    engine.setWide01  (wideParam ->load() * 0.01f);
    engine.setHuman01 (humanParam->load() * 0.01f);
    engine.setMix01   (mixParam  ->load() * 0.01f);
    engine.setMode    ((dbl::DoubleEngine::Mode) (int) modeParam->load());

    engine.process (buffer);
}

juce::AudioProcessorEditor* DoubleAudioProcessor::createEditor()
{
    return new DoubleAudioProcessorEditor (*this);
}

void DoubleAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void DoubleAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DoubleAudioProcessor();
}
