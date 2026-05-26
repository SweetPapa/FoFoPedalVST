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
    bodyParam      = apvts.getRawParameterValue (ParamID::body);
    toneParam      = apvts.getRawParameterValue (ParamID::tone);
    sagParam       = apvts.getRawParameterValue (ParamID::sag);
    blendParam     = apvts.getRawParameterValue (ParamID::blend);
    levelDbParam   = apvts.getRawParameterValue (ParamID::level);
}

void VroomAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const auto numCh = (juce::uint32) juce::jmax (1, getTotalNumOutputChannels());
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, numCh };

    tone.prepare (spec);
    saturator.prepare (spec);
    sag.prepare (spec);

    // Electric Guitar default voicing (spec §5). Mode switching arrives in Phase 5.
    tone.setPreHPFHz (90.0f);
    tone.setBodyCenterHz (300.0f);

    // Dry buffer + latency-compensated delay line for the parallel blend.
    dryBuffer.setSize ((int) numCh, samplesPerBlock, false, true, true);

    // Max needs to cover the worst-case oversampling latency. Oversampler at
    // 8× gives a handful of base-rate samples; 64 is comfortably above the
    // ceiling for any reasonable filter design.
    dryDelay.setMaximumDelayInSamples (64);
    dryDelay.prepare (spec);
    dryDelay.setDelay ((float) saturator.getLatencySamples());

    inputGainSmoothed .reset (sampleRate, 0.02);
    outputGainSmoothed.reset (sampleRate, 0.02);
    blendSmoothed     .reset (sampleRate, 0.02);
    inputGainSmoothed .setCurrentAndTargetValue (juce::Decibels::decibelsToGain (inputDbParam ? inputDbParam->load() : 0.0f));
    outputGainSmoothed.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (levelDbParam ? levelDbParam->load() : 0.0f));
    blendSmoothed     .setCurrentAndTargetValue ((blendParam ? blendParam->load() : 70.0f) * 0.01f);

    setLatencySamples (saturator.getLatencySamples());
}

void VroomAudioProcessor::releaseResources()
{
    tone.reset();
    saturator.reset();
    sag.reset();
    dryDelay.reset();
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

    const int nS = buffer.getNumSamples();

    // ── Input peak (before any processing) ────────────────────────────────
    {
        float peak = 0.0f;
        for (int ch = 0; ch < totalOut; ++ch)
            peak = juce::jmax (peak, buffer.getMagnitude (ch, 0, nS));
        float cur = inputPeakMax.load (std::memory_order_relaxed);
        while (peak > cur && ! inputPeakMax.compare_exchange_weak (cur, peak, std::memory_order_release, std::memory_order_relaxed)) {}
    }

    // ── Push live parameter values into DSP blocks ─────────────────────────
    inputGainSmoothed .setTargetValue (juce::Decibels::decibelsToGain (inputDbParam->load()));
    outputGainSmoothed.setTargetValue (juce::Decibels::decibelsToGain (levelDbParam->load()));
    blendSmoothed     .setTargetValue (blendParam->load() * 0.01f);

    saturator.setDrive01     (driveParam    ->load() * 0.01f);
    saturator.setCharacter01 (characterParam->load() * 0.01f);
    tone.setBody01 (bodyParam->load() * 0.01f);
    tone.setTone01 (toneParam->load() * 0.01f);
    sag .setSag01  (sagParam ->load() * 0.01f);

    // ── Input trim ─────────────────────────────────────────────────────────
    for (int n = 0; n < nS; ++n)
    {
        const float g = inputGainSmoothed.getNextValue();
        for (int ch = 0; ch < totalOut; ++ch)
            buffer.getWritePointer (ch)[n] *= g;
    }

    // ── Split: copy trimmed input into dryBuffer BEFORE wet processing ─────
    for (int ch = 0; ch < totalOut; ++ch)
        dryBuffer.copyFrom (ch, 0, buffer, ch, 0, nS);

    // ── Wet chain (spec §3) ────────────────────────────────────────────────
    tone.processPre (buffer);          // Pre-HPF (tighten lows)
    saturator.process (buffer);        // Oversampled asymmetric cascaded clip
    tone.processDCBlock (buffer);      // Kill the bias-induced DC offset
    sag.process (buffer);              // Tube-amp supply sag / bloom
    tone.processBodyAndTone (buffer);  // Body EQ + Tone LPF

    // ── Dry path latency compensation ──────────────────────────────────────
    {
        juce::dsp::AudioBlock<float> dryBlock (dryBuffer.getArrayOfWritePointers(),
                                               (size_t) totalOut, 0, (size_t) nS);
        juce::dsp::ProcessContextReplacing<float> ctx (dryBlock);
        dryDelay.process (ctx);
    }

    // ── Parallel blend (wet * blend + dry * (1-blend)) ─────────────────────
    for (int n = 0; n < nS; ++n)
    {
        const float wetMix = blendSmoothed.getNextValue();
        const float dryMix = 1.0f - wetMix;
        for (int ch = 0; ch < totalOut; ++ch)
            buffer.getWritePointer (ch)[n] =
                buffer.getWritePointer (ch)[n] * wetMix +
                dryBuffer.getReadPointer (ch)[n] * dryMix;
    }

    // ── Output level (Level knob) ──────────────────────────────────────────
    for (int n = 0; n < nS; ++n)
    {
        const float g = outputGainSmoothed.getNextValue();
        for (int ch = 0; ch < totalOut; ++ch)
            buffer.getWritePointer (ch)[n] *= g;
    }

    // ── Output peak ────────────────────────────────────────────────────────
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
