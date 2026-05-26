#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

class VroomAudioProcessorEditor : public juce::AudioProcessorEditor,
                                  private juce::Timer
{
public:
    explicit VroomAudioProcessorEditor (VroomAudioProcessor&);
    ~VroomAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    std::optional<juce::WebBrowserComponent::Resource>
        getResource (const juce::String& url) const;

    VroomAudioProcessor& processorRef;

    juce::WebSliderRelay inputRelay     { "input"     };
    juce::WebSliderRelay driveRelay     { "drive"     };
    juce::WebSliderRelay characterRelay { "character" };
    juce::WebSliderRelay levelRelay     { "level"     };

    juce::WebSliderParameterAttachment inputAttachment;
    juce::WebSliderParameterAttachment driveAttachment;
    juce::WebSliderParameterAttachment characterAttachment;
    juce::WebSliderParameterAttachment levelAttachment;

    juce::WebBrowserComponent webView;

    // Displayed meter values with slow-release decay so the bar stays readable
    // even on fast transients.
    float displayedInputPeak  { 0.0f };
    float displayedOutputPeak { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VroomAudioProcessorEditor)
};
