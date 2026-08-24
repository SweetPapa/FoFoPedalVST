#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

class DoubleAudioProcessorEditor : public juce::AudioProcessorEditor,
                                   private juce::Timer
{
public:
    explicit DoubleAudioProcessorEditor (DoubleAudioProcessor&);
    ~DoubleAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    std::optional<juce::WebBrowserComponent::Resource>
        getResource (const juce::String& url) const;

    DoubleAudioProcessor& processorRef;

    juce::WebSliderRelay thickRelay { "thick" };
    juce::WebSliderRelay wideRelay  { "wide" };
    juce::WebSliderRelay humanRelay { "human" };
    juce::WebSliderRelay mixRelay   { "mix" };
    juce::WebComboBoxRelay modeRelay { "mode" };

    juce::WebSliderParameterAttachment thickAttachment;
    juce::WebSliderParameterAttachment wideAttachment;
    juce::WebSliderParameterAttachment humanAttachment;
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DoubleAudioProcessorEditor)
};
