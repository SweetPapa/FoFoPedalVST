#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

class BackporchAudioProcessorEditor : public juce::AudioProcessorEditor,
                                   private juce::Timer
{
public:
    explicit BackporchAudioProcessorEditor (BackporchAudioProcessor&);
    ~BackporchAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    std::optional<juce::WebBrowserComponent::Resource>
        getResource (const juce::String& url) const;

    BackporchAudioProcessor& processorRef;

    juce::WebSliderRelay spaceRelay { "space" };
    juce::WebSliderRelay toneRelay  { "tone" };
    juce::WebSliderRelay duckRelay { "duck" };
    juce::WebSliderRelay mixRelay   { "mix" };
    juce::WebComboBoxRelay modeRelay { "mode" };

    juce::WebSliderParameterAttachment spaceAttachment;
    juce::WebSliderParameterAttachment toneAttachment;
    juce::WebSliderParameterAttachment duckAttachment;
    juce::WebSliderParameterAttachment mixAttachment;
    juce::WebComboBoxParameterAttachment modeAttachment;

    juce::WebBrowserComponent webView;

    float displayedInputPeak  { 0.0f };
    float displayedOutputPeak { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BackporchAudioProcessorEditor)
};
