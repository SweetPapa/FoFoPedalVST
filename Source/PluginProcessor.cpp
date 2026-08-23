#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "spt/Shapers.h"

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

    // Blend defaults to fully wet — parallel dirt is the *trick*, not the
    // default. (A 70% default layered latency-compensated dry under every
    // sound, which read as "the pedal isn't doing much.")
    layout.add (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamID::blend, kParamVersionHint }, "Blend",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f), 100.0f, percentAttributes()));

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

    layout.add (std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { ParamID::clipShape, kParamVersionHint }, "Voice",
        juce::StringArray { "Smooth", "Crunch", "Fuzz", "Octave" }, 0));

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
    cabEnableParam  = apvts.getRawParameterValue (ParamID::cabEnable);
    cabIRParam      = apvts.getRawParameterValue (ParamID::cabIR);
    sourceModeParam = apvts.getRawParameterValue (ParamID::sourceMode);
    clipShapeParam  = apvts.getRawParameterValue (ParamID::clipShape);

    // Listen for parameter changes that have to be applied on the message
    // thread (cab IR load allocates; source mode reconfigures DSP + APVTS).
    apvts.addParameterListener (ParamID::cabEnable,  this);
    apvts.addParameterListener (ParamID::cabIR,      this);
    apvts.addParameterListener (ParamID::sourceMode, this);

    // Load the "Vroom" signature preset on first construction. If the host
    // later restores state via setStateInformation, those values overwrite
    // these — that's the right precedence (session > factory defaults).
    presetManager.loadByName ("Vroom", true);
}

VroomAudioProcessor::~VroomAudioProcessor()
{
    apvts.removeParameterListener (ParamID::cabEnable,  this);
    apvts.removeParameterListener (ParamID::cabIR,      this);
    apvts.removeParameterListener (ParamID::sourceMode, this);
    cancelPendingUpdate();
}

void VroomAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const auto numCh = (juce::uint32) juce::jmax (1, getTotalNumOutputChannels());
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, numCh };

    tone.prepare (spec);
    saturator.prepare (spec);
    sag.prepare (spec);
    cab.prepare (spec);
    bandSplit.prepare (spec);

    // Apply the current Source Mode's DSP voicing. Crucially we DON'T call
    // applyModeDefaultCab here — that would overwrite cabIR/cabEnable in
    // APVTS, which fails AU validation ("Parameter did not retain set value
    // when Initialized") because auval sets a value, calls prepareToPlay,
    // then expects to read it back unchanged.
    applyModeVoicing ((int) sourceModeParam->load());

    // Honor whatever cab params the host (or restored session) currently has.
    cab.setEnabled (cabEnableParam->load() > 0.5f);
    cab.selectSlot ((int) cabIRParam->load());

    dryBuffer .setSize ((int) numCh, samplesPerBlock, false, true, true);
    lowsBuffer.setSize ((int) numCh, samplesPerBlock, false, true, true);
    dryDelay .setMaximumDelayInSamples (64);
    lowsDelay.setMaximumDelayInSamples (64);
    dryDelay .prepare (spec);
    lowsDelay.prepare (spec);
    dryDelay .setDelay ((float) saturator.getLatencySamples());
    lowsDelay.setDelay ((float) saturator.getLatencySamples());

    inputGainSmoothed .reset (sampleRate, 0.02);
    outputGainSmoothed.reset (sampleRate, 0.02);
    blendSmoothed     .reset (sampleRate, 0.02);
    inputGainSmoothed .setCurrentAndTargetValue (juce::Decibels::decibelsToGain (inputDbParam ? inputDbParam->load() : 0.0f));
    outputGainSmoothed.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (levelDbParam ? levelDbParam->load() : 0.0f));
    blendSmoothed     .setCurrentAndTargetValue ((blendParam ? blendParam->load() : 70.0f) * 0.01f);

    setLatencySamples (saturator.getLatencySamples() + cab.getLatencySamples());
}

void VroomAudioProcessor::releaseResources()
{
    tone.reset();
    saturator.reset();
    sag.reset();
    cab.reset();
    bandSplit.reset();
    dryDelay.reset();
    lowsDelay.reset();
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
    saturator.setClipShape   ((int) (clipShapeParam ? clipShapeParam->load() : 0.0f));
    saturator.setSagDepth01  (sagParam->load() * 0.01f * sagModeScale);
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

    // ── Bass mode: pre-split, drive only the high band ─────────────────────
    // bandSplit.split() leaves the high band in `buffer` and writes the clean
    // low band to `lowsBuffer`. The lows skip the saturator entirely so they
    // stay punchy (spec §5).
    if (bandSplitActive)
        bandSplit.split (buffer, lowsBuffer);

    // ── Wet chain (spec §3) ────────────────────────────────────────────────
    tone.processPre (buffer);          // Pre-HPF (tighten lows) + Acoustic piezo tamer
    saturator.process (buffer);        // Oversampled asymmetric cascaded clip
    tone.processDCBlock (buffer);      // Kill the bias-induced DC offset
    sag.process (buffer);              // Tube-amp supply sag / bloom
    tone.processBodyAndTone (buffer);  // Body EQ + Tone LPF + Acoustic air shelf

    // ── Latency compensation for dry (+ bass-mode lows) ────────────────────
    {
        juce::dsp::AudioBlock<float> dryBlock (dryBuffer.getArrayOfWritePointers(),
                                               (size_t) totalOut, 0, (size_t) nS);
        juce::dsp::ProcessContextReplacing<float> ctx (dryBlock);
        dryDelay.process (ctx);
    }

    if (bandSplitActive)
    {
        // Delay the clean low band by the same wet-chain latency so it sums
        // back in phase with the driven highs — otherwise the LR4 crossover
        // smears.
        juce::dsp::AudioBlock<float> lowsBlock (lowsBuffer.getArrayOfWritePointers(),
                                                (size_t) totalOut, 0, (size_t) nS);
        juce::dsp::ProcessContextReplacing<float> ctx (lowsBlock);
        lowsDelay.process (ctx);

        // Re-add the clean lows to the driven highs — this is the "wet" output
        // before the Blend mix.
        for (int ch = 0; ch < totalOut; ++ch)
            buffer.addFrom (ch, 0, lowsBuffer, ch, 0, nS);
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

    // ── Cabinet IR convolution (applied equally to wet+dry sum) ────────────
    cab.process (buffer);

    // ── Output level (Level knob) + cubic safety clip ──────────────────────
    // The soft clip is transparent below ~−6 dBFS and rounds off anything
    // hotter, so transients never crack the host meter.
    for (int n = 0; n < nS; ++n)
    {
        const float g = outputGainSmoothed.getNextValue();
        for (int ch = 0; ch < totalOut; ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            d[n] = spt::softClipCubic (d[n] * g);
        }
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

void VroomAudioProcessor::parameterChanged (const juce::String& parameterID, float)
{
    // Bounce to message thread — convolution loads allocate, and applying a
    // mode change touches multiple APVTS parameters which we shouldn't do
    // from the audio thread.
    if (parameterID == ParamID::cabEnable
        || parameterID == ParamID::cabIR
        || parameterID == ParamID::sourceMode)
        triggerAsyncUpdate();
}

void VroomAudioProcessor::handleAsyncUpdate()
{
    const bool wasPresetLoad = presetManager.isLoadInProgress();

    if (sourceModeParam)
    {
        const int mode = (int) sourceModeParam->load();
        if (mode != currentSourceMode)
        {
            // During a preset load we only apply the hidden voicing — the
            // preset already specifies its own cabEnable/cabIR and we don't
            // want the mode's default cab to clobber those.
            if (wasPresetLoad)
                applyModeVoicing (mode);
            else
                applySourceModeConfig (mode);
        }
    }

    if (cabEnableParam) cab.setEnabled (cabEnableParam->load() > 0.5f);
    if (cabIRParam)
    {
        const int slot = (int) cabIRParam->load();
        if (slot != cab.getCurrentSlot()) cab.selectSlot (slot);
    }

    presetManager.clearLoadInProgress();
}

void VroomAudioProcessor::applyModeVoicing (int modeIdx)
{
    using namespace vroom;
    modeIdx = juce::jlimit (0, (int) NumSourceModes - 1, modeIdx);
    const auto& cfg = getModeConfig ((SourceMode) modeIdx);

    tone.setPreHPFHz       (cfg.preHPFHz);
    tone.setBodyCenterHz   (cfg.bodyCenterHz);
    tone.setAcousticVoicing (cfg.acousticVoicing);
    saturator.setDriveCeilingScale (cfg.driveCeilingScale);
    sag.setResponseScale   (cfg.sagResponseScale);
    sagModeScale = cfg.sagResponseScale;

    bandSplitActive = cfg.bandSplitDrive;
    if (cfg.bandSplitDrive)
        bandSplit.setCrossoverHz (cfg.bandSplitHz);
    bandSplit.reset();
    lowsDelay.reset();

    currentSourceMode = modeIdx;
}

void VroomAudioProcessor::applyModeDefaultCab (int modeIdx)
{
    using namespace vroom;
    modeIdx = juce::jlimit (0, (int) NumSourceModes - 1, modeIdx);
    const auto& cfg = getModeConfig ((SourceMode) modeIdx);

    if (auto* p = apvts.getParameter (ParamID::cabEnable))
        p->setValueNotifyingHost (cfg.defaultCabEnabled ? 1.0f : 0.0f);
    if (auto* p = apvts.getParameter (ParamID::cabIR))
        p->setValueNotifyingHost (p->convertTo0to1 ((float) cfg.defaultCabSlot));
}

void VroomAudioProcessor::applySourceModeConfig (int modeIdx)
{
    applyModeVoicing    (modeIdx);
    applyModeDefaultCab (modeIdx);
}

juce::String VroomAudioProcessor::loadCustomIR (const juce::File& irFile)
{
    if (! cab.loadCustomIRFile (irFile))
        return {};
    return cab.getCustomName();
}

juce::String VroomAudioProcessor::getCurrentIRDisplayName() const
{
    if (cab.hasCustomLoaded())
        return cab.getCustomName();

    static const char* names[] = { "1x12 Warm", "4x12 Modern", "Bass 1x15", "Full-Range / DI" };
    const int s = juce::jlimit (0, 3, cab.getCurrentSlot());
    return names[s];
}

int VroomAudioProcessor::getNumPrograms()
{
    return (int) presetManager.getFactoryPresets().size();
}

int VroomAudioProcessor::getCurrentProgram()
{
    // A user preset is not in the host's list at all, so report the factory
    // preset the session last sat on rather than inventing an index.
    if (! presetManager.isCurrentFactory()) return lastFactoryProgram;

    const auto& bank = presetManager.getFactoryPresets();
    const auto  name = presetManager.getCurrentName();

    for (size_t i = 0; i < bank.size(); ++i)
        if (bank[i].name == name)
            return (int) i;

    return lastFactoryProgram;
}

void VroomAudioProcessor::setCurrentProgram (int index)
{
    if (index < 0 || index >= getNumPrograms()) return;

    // Hosts re-assert the current program after restoring a session, which
    // would overwrite whatever the user had dialled in.
    if (index == getCurrentProgram()) return;

    lastFactoryProgram = index;
    presetManager.loadByName (presetManager.getFactoryPresets()[(size_t) index].name, true);
}

const juce::String VroomAudioProcessor::getProgramName (int index)
{
    if (index < 0 || index >= getNumPrograms()) return {};

    return presetManager.getFactoryPresets()[(size_t) index].name;
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
