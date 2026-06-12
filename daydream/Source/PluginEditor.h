#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

class DaydreamAudioProcessorEditor : public juce::AudioProcessorEditor,
                                     private juce::Timer
{
public:
    explicit DaydreamAudioProcessorEditor (DaydreamAudioProcessor&);
    ~DaydreamAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    std::optional<juce::WebBrowserComponent::Resource>
        getResource (const juce::String& url) const;

    DaydreamAudioProcessor& processorRef;

    juce::WebSliderRelay dreamRelay { "dream" };
    juce::WebSliderParameterAttachment dreamAttachment;

    juce::WebBrowserComponent webView;

    float displayedInputPeak  { 0.0f };
    float displayedOutputPeak { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DaydreamAudioProcessorEditor)
};
