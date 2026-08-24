#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

class DreamRipperAudioProcessorEditor : public juce::AudioProcessorEditor,
                                        private juce::Timer
{
public:
    explicit DreamRipperAudioProcessorEditor (DreamRipperAudioProcessor&);
    ~DreamRipperAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    std::optional<juce::WebBrowserComponent::Resource>
        getResource (const juce::String& url) const;

    DreamRipperAudioProcessor& processorRef;

    juce::WebSliderRelay ripRelay   { "rip" };
    juce::WebSliderRelay tightRelay { "tight" };
    juce::WebSliderRelay scoopRelay { "scoop" };
    juce::WebSliderRelay cabRelay   { "cab" };
    juce::WebSliderRelay levelRelay { "level" };
    juce::WebSliderRelay gateRelay  { "gate" };
    juce::WebSliderRelay mixRelay   { "mix" };
    juce::WebComboBoxRelay modeRelay { "mode" };

    juce::WebSliderParameterAttachment ripAttachment;
    juce::WebSliderParameterAttachment tightAttachment;
    juce::WebSliderParameterAttachment scoopAttachment;
    juce::WebSliderParameterAttachment cabAttachment;
    juce::WebSliderParameterAttachment levelAttachment;
    juce::WebSliderParameterAttachment gateAttachment;
    juce::WebSliderParameterAttachment mixAttachment;
    juce::WebComboBoxParameterAttachment modeAttachment;

    juce::WebBrowserComponent webView;

    // Preset name/edited state pushed to the web UI's header stepper.
    juce::var buildPresetStateVar() const;

    juce::String lastEmittedPresetName;
    bool         lastEmittedModified { false };
    bool         havePushedPresetState { false };

    float displayedInputPeak  { 0.0f };
    float displayedOutputPeak { 0.0f };
    float displayedGateGain   { 1.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DreamRipperAudioProcessorEditor)
};
