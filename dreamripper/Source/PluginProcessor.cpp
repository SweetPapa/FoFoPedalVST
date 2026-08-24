#include "PluginProcessor.h"
#include "PluginEditor.h"

using APVTS = juce::AudioProcessorValueTreeState;
using namespace drip;

namespace
{
    constexpr int kParamVersionHint = 1;

    // The pedal boots on its default preset, so the preset bank is the single
    // source of that truth and the parameter defaults are read back out of it.
    // Hard-coding them here as well is how the two silently drift apart.
    float defaultFor (const char* id)
    {
        const auto& preset = drip::getFactoryPresets()[(size_t) drip::kDefaultPresetIndex];

        for (const auto& value : preset.values)
            if (juce::String (value.paramId) == id)
                return value.value;

        jassertfalse; // the default preset is missing a parameter
        return 0.0f;
    }

    std::unique_ptr<juce::AudioParameterFloat> pct (const char* id, const char* name, float def)
    {
        return std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { id, kParamVersionHint }, name,
            juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f), def,
            juce::AudioParameterFloatAttributes{}
                .withStringFromValueFunction ([](float v, int) { return juce::String ((int) std::round (v)); }));
    }
}

APVTS::ParameterLayout DreamRipperAudioProcessor::createParameterLayout()
{
    APVTS::ParameterLayout layout;
    layout.add (pct (ParamID::rip,   "Rip",   defaultFor (ParamID::rip)));
    layout.add (pct (ParamID::tight, "Tight", defaultFor (ParamID::tight)));
    layout.add (pct (ParamID::scoop, "Scoop", defaultFor (ParamID::scoop)));
    layout.add (pct (ParamID::cab,   "Cab",   defaultFor (ParamID::cab)));
    layout.add (pct (ParamID::level, "Level", defaultFor (ParamID::level)));
    layout.add (pct (ParamID::gate,  "Gate",  defaultFor (ParamID::gate)));
    layout.add (pct (ParamID::mix,   "Mix",   defaultFor (ParamID::mix)));
    layout.add (std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { ParamID::mode, kParamVersionHint }, "Mode",
        juce::StringArray { "Sludge", "Grunge", "Metal", "Djent" },
        (int) defaultFor (ParamID::mode)));
    return layout;
}

DreamRipperAudioProcessor::DreamRipperAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
                              .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                              .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, juce::Identifier ("DREAMRIPPER"), createParameterLayout()),
      presets (*this, apvts, drip::getFactoryPresets(), drip::kDefaultPresetIndex)
{
    ripParam   = apvts.getRawParameterValue (ParamID::rip);
    tightParam = apvts.getRawParameterValue (ParamID::tight);
    scoopParam = apvts.getRawParameterValue (ParamID::scoop);
    cabParam   = apvts.getRawParameterValue (ParamID::cab);
    levelParam = apvts.getRawParameterValue (ParamID::level);
    gateParam  = apvts.getRawParameterValue (ParamID::gate);
    mixParam   = apvts.getRawParameterValue (ParamID::mix);
    modeParam  = apvts.getRawParameterValue (ParamID::mode);
}

void DreamRipperAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec {
        sampleRate,
        (juce::uint32) samplesPerBlock,
        (juce::uint32) juce::jmax (1, getTotalNumOutputChannels())
    };
    engine.prepare (spec);
    setLatencySamples (engine.getLatencySamples());
}

void DreamRipperAudioProcessor::releaseResources()
{
    engine.reset();
}

bool DreamRipperAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();

    if (in.isDisabled() || out.isDisabled()) return false;
    if (in != out) return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void DreamRipperAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto totalIn  = getTotalNumInputChannels();
    const auto totalOut = getTotalNumOutputChannels();
    for (auto i = totalIn; i < totalOut; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    engine.setRip01   (ripParam  ->load() * 0.01f);
    engine.setTight01 (tightParam->load() * 0.01f);
    engine.setScoop01 (scoopParam->load() * 0.01f);
    engine.setCab01   (cabParam  ->load() * 0.01f);
    engine.setLevel01 (levelParam->load() * 0.01f);
    engine.setGate01  (gateParam ->load() * 0.01f);
    engine.setMix01   (mixParam  ->load() * 0.01f);
    engine.setMode    ((drip::DreamRipperEngine::Mode) (int) modeParam->load());

    engine.process (buffer);
}

juce::AudioProcessorEditor* DreamRipperAudioProcessor::createEditor()
{
    return new DreamRipperAudioProcessorEditor (*this);
}

void DreamRipperAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void DreamRipperAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
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
    return new DreamRipperAudioProcessor();
}
