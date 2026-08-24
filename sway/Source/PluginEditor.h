#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

class SwayAudioProcessorEditor : public juce::AudioProcessorEditor,
                                   private juce::Timer
{
public:
    explicit SwayAudioProcessorEditor (SwayAudioProcessor&);
    ~SwayAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    std::optional<juce::WebBrowserComponent::Resource>
        getResource (const juce::String& url) const;

    SwayAudioProcessor& processorRef;

    juce::WebSliderRelay moveRelay { "move" };
    juce::WebSliderRelay rateRelay  { "rate" };
    juce::WebSliderRelay colorRelay { "color" };
    juce::WebSliderRelay mixRelay   { "mix" };
    juce::WebComboBoxRelay modeRelay { "mode" };

    juce::WebSliderParameterAttachment moveAttachment;
    juce::WebSliderParameterAttachment rateAttachment;
    juce::WebSliderParameterAttachment colorAttachment;
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SwayAudioProcessorEditor)
};
