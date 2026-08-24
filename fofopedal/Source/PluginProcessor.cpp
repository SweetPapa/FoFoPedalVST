#include "PluginProcessor.h"
#include "PluginEditor.h"

using APVTS = juce::AudioProcessorValueTreeState;
using namespace fofopedal;

namespace
{
    constexpr int kParamVersionHint = 1;

    // Number divisor for "host-sync" time. Order matters — keep aligned with
    // the syncDivision choice param indices and the UI labels.
    constexpr float kSyncBeatMultiplier[] = {
        1.0f,   // 1/4 note
        0.5f,   // 1/8 note
        0.75f,  // 1/8 dotted
        1.0f / 3.0f, // 1/8 triplet
        0.25f,  // 1/16 note
    };

    inline float pct (float p) { return p * 0.01f; }
}

APVTS::ParameterLayout FofopedalAudioProcessor::createParameterLayout()
{
    APVTS::ParameterLayout layout;

    auto floatPercent = [] (const char* id, const char* label, float def)
    {
        return std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { id, kParamVersionHint }, label,
            juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f), def,
            juce::AudioParameterFloatAttributes{}
                .withStringFromValueFunction ([](float v, int) { return juce::String ((int) std::round (v)); }));
    };

    auto choice = [] (const char* id, const char* label, juce::StringArray opts, int def)
    {
        return std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID { id, kParamVersionHint }, label, opts, def);
    };

    auto boolean = [] (const char* id, const char* label, bool def)
    {
        return std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID { id, kParamVersionHint }, label, def);
    };

    // ── 6 top-level macros ──────────────────────────────────────────────────
    layout.add (floatPercent (ParamID::character, "Character", 25.0f));
    layout.add (floatPercent (ParamID::drive,     "Drive",     20.0f));
    layout.add (floatPercent (ParamID::shape,     "Shape",     50.0f));

    // TIME has real units (ms), not percent.
    layout.add (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamID::timeMs, kParamVersionHint }, "Time",
        juce::NormalisableRange<float> (1.0f, 2000.0f, 0.1f, 0.4f), 350.0f,
        juce::AudioParameterFloatAttributes{}
            .withStringFromValueFunction ([](float v, int) {
                return v < 1000.0f
                    ? juce::String ((int) std::round (v)) + " ms"
                    : juce::String (v / 1000.0f, 2) + " s";
            })));

    layout.add (floatPercent (ParamID::space, "Space", 30.0f));
    // MIX is the magnitude macro ("how much pedal") — 70 is the calibrated
    // character level, not a wet/dry crossfade default.
    layout.add (floatPercent (ParamID::mix,   "Mix",   70.0f));

    // ── Algorithm choices ───────────────────────────────────────────────────
    layout.add (choice (ParamID::voicing,   "Voicing",
        { "Vox", "Gtr", "Bass", "Acoustic" }, 1));
    layout.add (choice (ParamID::driveType, "Drive Type",
        { "Tube", "Tape", "Iron" }, 0));
    layout.add (choice (ParamID::modType,   "Mod Type",
        { "Chorus", "Phaser", "Trem/Vib" }, 0));
    layout.add (choice (ParamID::pitchType, "Pitch Type",
        { "Detune", "Harmony", "Freeze" }, 0));
    layout.add (choice (ParamID::delayType, "Delay Type",
        { "Digital", "BBD", "Tape" }, 0));
    layout.add (choice (ParamID::spaceType, "Space Type",
        { "Plate", "Hall", "Room", "Shimmer" }, 1));

    // ── Hidden (shift) ──────────────────────────────────────────────────────
    layout.add (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamID::characterLowCut, kParamVersionHint }, "Low Cut",
        juce::NormalisableRange<float> (20.0f, 200.0f, 0.5f, 0.5f), 20.0f,
        juce::AudioParameterFloatAttributes{}
            .withStringFromValueFunction ([](float v, int) { return juce::String ((int) v) + " Hz"; })));
    layout.add (floatPercent (ParamID::driveTone,    "Drive Tone", 50.0f));
    layout.add (floatPercent (ParamID::driveMix,     "Drive Mix",  100.0f));
    layout.add (floatPercent (ParamID::modRate,      "Mod Rate",   30.0f));
    layout.add (floatPercent (ParamID::modDepth,     "Mod Depth",  50.0f));
    layout.add (floatPercent (ParamID::modFeedback,  "Mod FB",     20.0f));
    layout.add (floatPercent (ParamID::modMix,       "Mod Mix",    30.0f));
    layout.add (floatPercent (ParamID::pitchAmount,  "Pitch Amt",  35.0f));
    layout.add (floatPercent (ParamID::pitchShape,   "Pitch Shape",50.0f));
    layout.add (floatPercent (ParamID::pitchMix,     "Pitch Mix",  40.0f));
    layout.add (floatPercent (ParamID::delayFeedback,"Delay FB",   30.0f));
    layout.add (floatPercent (ParamID::delayHfCut,   "Delay HF Cut",50.0f));
    layout.add (boolean      (ParamID::delayPingPong,"Ping-Pong",  false));
    layout.add (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamID::spacePreDelay, kParamVersionHint }, "Pre-Delay",
        juce::NormalisableRange<float> (0.0f, 250.0f, 0.5f), 30.0f,
        juce::AudioParameterFloatAttributes{}
            .withStringFromValueFunction ([](float v, int) { return juce::String ((int) v) + " ms"; })));
    layout.add (floatPercent (ParamID::spaceShimmer, "Shimmer",    0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamID::spaceSendHp, kParamVersionHint }, "Send HP",
        juce::NormalisableRange<float> (40.0f, 400.0f, 1.0f, 0.5f), 90.0f,
        juce::AudioParameterFloatAttributes{}
            .withStringFromValueFunction ([](float v, int) { return juce::String ((int) v) + " Hz"; })));
    layout.add (floatPercent (ParamID::glueAmount,   "Glue",       30.0f));
    layout.add (boolean      (ParamID::glueDefeated, "Glue Off",   false));

    // ── Per-block bypasses ──────────────────────────────────────────────────
    layout.add (boolean (ParamID::characterDefeated, "Character Off", false));
    layout.add (boolean (ParamID::driveBypassed,     "Drive Off",     false));
    layout.add (boolean (ParamID::modBypassed,       "Mod Off",       true));
    layout.add (boolean (ParamID::pitchBypassed,     "Pitch Off",     true));
    layout.add (boolean (ParamID::delayBypassed,     "Delay Off",     true));
    layout.add (boolean (ParamID::spaceBypassed,     "Space Off",     false));

    // ── Routing + presets + sync ────────────────────────────────────────────
    layout.add (boolean (ParamID::swapModPitch,   "Swap Mod/Pitch",   false));
    layout.add (boolean (ParamID::swapDelaySpace, "Swap Delay/Space", false));

    juce::StringArray presetNames;
    for (auto& p : CharacterBank::kFactoryPresets) presetNames.add (p.name);
    layout.add (choice (ParamID::characterPreset, "Character", presetNames, 0));

    layout.add (boolean (ParamID::hostSync, "Host Sync", false));
    layout.add (choice (ParamID::syncDivision, "Division",
        { "1/4", "1/8", "1/8 dotted", "1/8 triplet", "1/16" }, 1));

    return layout;
}

FofopedalAudioProcessor::FofopedalAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
                              .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                              .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, juce::Identifier ("FOFOPEDAL"), createParameterLayout())
{
    pCharacter         = apvts.getRawParameterValue (ParamID::character);
    pCharacterLowCut   = apvts.getRawParameterValue (ParamID::characterLowCut);
    pCharacterDefeated = apvts.getRawParameterValue (ParamID::characterDefeated);
    pDrive             = apvts.getRawParameterValue (ParamID::drive);
    pDriveType         = apvts.getRawParameterValue (ParamID::driveType);
    pDriveTone         = apvts.getRawParameterValue (ParamID::driveTone);
    pDriveMix          = apvts.getRawParameterValue (ParamID::driveMix);
    pDriveBypassed     = apvts.getRawParameterValue (ParamID::driveBypassed);
    pShape             = apvts.getRawParameterValue (ParamID::shape);
    pModType           = apvts.getRawParameterValue (ParamID::modType);
    pModRate           = apvts.getRawParameterValue (ParamID::modRate);
    pModDepth          = apvts.getRawParameterValue (ParamID::modDepth);
    pModFeedback       = apvts.getRawParameterValue (ParamID::modFeedback);
    pModMix            = apvts.getRawParameterValue (ParamID::modMix);
    pModBypassed       = apvts.getRawParameterValue (ParamID::modBypassed);
    pPitchType         = apvts.getRawParameterValue (ParamID::pitchType);
    pPitchAmount       = apvts.getRawParameterValue (ParamID::pitchAmount);
    pPitchShape        = apvts.getRawParameterValue (ParamID::pitchShape);
    pPitchMix          = apvts.getRawParameterValue (ParamID::pitchMix);
    pPitchBypassed     = apvts.getRawParameterValue (ParamID::pitchBypassed);
    pTimeMs            = apvts.getRawParameterValue (ParamID::timeMs);
    pDelayType         = apvts.getRawParameterValue (ParamID::delayType);
    pDelayFeedback     = apvts.getRawParameterValue (ParamID::delayFeedback);
    pDelayHfCut        = apvts.getRawParameterValue (ParamID::delayHfCut);
    pDelayPingPong     = apvts.getRawParameterValue (ParamID::delayPingPong);
    pDelayBypassed     = apvts.getRawParameterValue (ParamID::delayBypassed);
    pSpace             = apvts.getRawParameterValue (ParamID::space);
    pSpaceType         = apvts.getRawParameterValue (ParamID::spaceType);
    pSpacePreDelay     = apvts.getRawParameterValue (ParamID::spacePreDelay);
    pSpaceShimmer      = apvts.getRawParameterValue (ParamID::spaceShimmer);
    pSpaceSendHp       = apvts.getRawParameterValue (ParamID::spaceSendHp);
    pSpaceBypassed     = apvts.getRawParameterValue (ParamID::spaceBypassed);
    pMix               = apvts.getRawParameterValue (ParamID::mix);
    pGlueAmount        = apvts.getRawParameterValue (ParamID::glueAmount);
    pGlueDefeated      = apvts.getRawParameterValue (ParamID::glueDefeated);
    pVoicing           = apvts.getRawParameterValue (ParamID::voicing);
    pSwapModPitch      = apvts.getRawParameterValue (ParamID::swapModPitch);
    pSwapDelaySpace    = apvts.getRawParameterValue (ParamID::swapDelaySpace);
    pHostSync          = apvts.getRawParameterValue (ParamID::hostSync);
    pSyncDivision      = apvts.getRawParameterValue (ParamID::syncDivision);

    // Only listen to the preset chooser — every other param is sampled
    // per-block from the cached atomic pointers in processBlock.
    apvts.addParameterListener (ParamID::characterPreset, this);
}

FofopedalAudioProcessor::~FofopedalAudioProcessor()
{
    apvts.removeParameterListener (ParamID::characterPreset, this);
}

void FofopedalAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec {
        sampleRate,
        (juce::uint32) samplesPerBlock,
        (juce::uint32) juce::jmax (1, getTotalNumOutputChannels())
    };
    engine.prepare (spec);
    globalDryBuffer.setSize (juce::jmax (1, getTotalNumOutputChannels()), samplesPerBlock, false, true, true);
    setLatencySamples (engine.getLatencySamples());
}

void FofopedalAudioProcessor::releaseResources()
{
    engine.reset();
}

bool FofopedalAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    if (in.isDisabled() || out.isDisabled()) return false;
    if (in != out) return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void FofopedalAudioProcessor::wireBlockParameters() noexcept
{
    // Sample-and-forward all APVTS values into the engine. Block setters
    // are cheap (early-out on no-change) so we can do this every buffer.

    // Global MIX is an RC-20-style magnitude macro: it scales every block's
    // intensity in place. (v1 instead crossfaded the whole processed chain
    // against raw dry — but the chain already contains the dry via per-block
    // mixes, so that doubled the dry and diluted everything into mud.)
    const float g = pct (pMix->load());

    // Character — the always-on console stage; not scaled by MIX.
    engine.character().setAmount01    (pct (pCharacter        ->load()));
    engine.character().setLowCutHz    (        pCharacterLowCut->load());
    engine.character().setDefeated    (        pCharacterDefeated->load() > 0.5f);

    // Drive.
    engine.drive().setAlgo     ((Drive::Algo) (int) pDriveType->load());
    engine.drive().setDrive01  (pct (pDrive->load()));
    engine.drive().setTone01   (pct (pDriveTone->load()));
    engine.drive().setMix01    (pct (pDriveMix->load()) * g);
    engine.drive().setBypassed (pDriveBypassed->load() > 0.5f);

    // Mod — modulation depth gets a SHAPE bias for context-sensitive feel.
    {
        const float shape = pct (pShape->load());
        engine.mod().setAlgo       ((Mod::Algo) (int) pModType->load());
        engine.mod().setRate01     (pct (pModRate->load()));
        // SHAPE adds up to 50% on top of the user's stored mod depth.
        const float depthBase = pct (pModDepth->load());
        const float depthOut  = juce::jlimit (0.0f, 1.0f, depthBase + shape * 0.5f);
        engine.mod().setDepth01    (depthOut);
        engine.mod().setShape01    (shape);
        engine.mod().setFeedback01 (pct (pModFeedback->load()));
        engine.mod().setMix01      (pct (pModMix->load()) * g);
        engine.mod().setBypassed   (pModBypassed->load() > 0.5f);
    }

    // Pitch — micro-detune amount also bumps with SHAPE on VOX/ACOUSTIC.
    {
        const int voiceIdx = (int) pVoicing->load();
        const float shape  = pct (pShape->load());
        const float vocalBias = (voiceIdx == 0 /*Vox*/ || voiceIdx == 3 /*Acoustic*/) ? 0.4f : 0.15f;
        engine.pitch().setAlgo     ((Pitch::Algo) (int) pPitchType->load());
        engine.pitch().setAmount01 (juce::jlimit (0.0f, 1.0f, pct (pPitchAmount->load()) + shape * vocalBias));
        engine.pitch().setShape01  (pct (pPitchShape->load()));
        engine.pitch().setMix01    (pct (pPitchMix->load()) * g);
        engine.pitch().setBypassed (pPitchBypassed->load() > 0.5f);
    }

    // Delay — resolve TIME from either raw ms or host BPM + division.
    {
        float timeMs = pTimeMs->load();
        if (pHostSync->load() > 0.5f)
        {
            if (auto* ph = getPlayHead())
            {
                if (auto info = ph->getPosition())
                {
                    if (auto bpm = info->getBpm())
                    {
                        const float beatMs = 60000.0f / (float) *bpm;
                        const int divIdx = juce::jlimit (0, 4, (int) pSyncDivision->load());
                        timeMs = beatMs * kSyncBeatMultiplier[divIdx];
                    }
                }
            }
        }
        engine.delay().setAlgo       ((Delay::Algo) (int) pDelayType->load());
        engine.delay().setTimeMs     (juce::jlimit (1.0f, 2000.0f, timeMs));
        engine.delay().setFeedback01 (pct (pDelayFeedback->load()));
        engine.delay().setHfCut01    (pct (pDelayHfCut->load()));
        engine.delay().setMix01      (0.38f * g); // v1 never wired this — repeats sat at a fixed internal level
        engine.delay().setPingPong   (pDelayPingPong->load() > 0.5f);
        engine.delay().setBypassed   (pDelayBypassed->load() > 0.5f);
    }

    // Space — SPACE macro drives size + mix together (Activity-knob style).
    {
        const float spaceMacro = pct (pSpace->load());
        engine.space().setAlgo        ((Space::Algo) (int) pSpaceType->load());
        engine.space().setSize01      (spaceMacro);
        engine.space().setMix01       (spaceMacro * 0.85f * g); // tail bias — mix ramps a little behind size
        engine.space().setPreDelayMs  (pSpacePreDelay->load());
        engine.space().setShimmer01   (pct (pSpaceShimmer->load()));
        engine.space().setSendHpHz    (pSpaceSendHp->load());
        engine.space().setBypassed    (pSpaceBypassed->load() > 0.5f);
    }

    // Output Glue.
    engine.glue().setAmount01 (pct (pGlueAmount->load()));
    engine.glue().setDefeated (pGlueDefeated->load() > 0.5f);

    // Voicing + reorder.
    engine.setVoicing       ((FofoEngine::Voicing) (int) pVoicing->load());
    engine.setModPitchSwap  (pSwapModPitch->load() > 0.5f);
    engine.setDelaySpaceSwap(pSwapDelaySpace->load() > 0.5f);
}

void FofopedalAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto totalIn  = getTotalNumInputChannels();
    const auto totalOut = getTotalNumOutputChannels();
    for (auto i = totalIn; i < totalOut; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    wireBlockParameters();

    // MIX is applied inside wireBlockParameters as a magnitude macro across
    // all block intensities (RC-20 style) — no outer crossfade. The old
    // chain-vs-dry crossfade layered the dry signal twice (the chain output
    // already contains it) and was the main source of the "muddled" sound.
    engine.process (buffer);
}

void FofopedalAudioProcessor::parameterChanged (const juce::String& parameterID, float newValue)
{
    if (parameterID == ParamID::characterPreset)
    {
        const int idx = (int) std::round (newValue);
        pendingPresetIndex.store (idx);
        if (! loadInProgress.exchange (true))
            triggerAsyncUpdate();
    }
}

void FofopedalAudioProcessor::handleAsyncUpdate()
{
    const int idx = pendingPresetIndex.load();
    if (idx >= 0 && idx < (int) CharacterBank::kFactoryPresets.size())
    {
        bank.applyToAPVTS (apvts, idx);
    }
    loadInProgress.store (false);
}

int FofopedalAudioProcessor::getNumPrograms()
{
    return (int) CharacterBank::kFactoryPresets.size();
}

int FofopedalAudioProcessor::getCurrentProgram()
{
    if (auto* p = apvts.getParameter (ParamID::characterPreset))
        return (int) std::round (p->convertFrom0to1 (p->getValue()));

    return 0;
}

void FofopedalAudioProcessor::setCurrentProgram (int index)
{
    // Hosts re-assert the current program after restoring a session; letting
    // that through would re-apply the character over the user's own edits.
    if (index == getCurrentProgram()) return;

    applyCharacterPresetByIndex (index);
}

const juce::String FofopedalAudioProcessor::getProgramName (int index)
{
    if (index < 0 || index >= getNumPrograms()) return {};

    return CharacterBank::kFactoryPresets[(size_t) index].name;
}

void FofopedalAudioProcessor::applyCharacterPresetByIndex (int idx)
{
    if (idx < 0 || idx >= (int) CharacterBank::kFactoryPresets.size()) return;
    // Setting the characterPreset param triggers parameterChanged which
    // schedules the AsyncUpdater push.
    if (auto* p = apvts.getParameter (ParamID::characterPreset))
    {
        const auto normalised = p->convertTo0to1 ((float) idx);
        p->setValueNotifyingHost (normalised);
    }
}

juce::AudioProcessorEditor* FofopedalAudioProcessor::createEditor()
{
    return new FofopedalAudioProcessorEditor (*this);
}

void FofopedalAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void FofopedalAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FofopedalAudioProcessor();
}
