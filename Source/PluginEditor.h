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
    juce::WebSliderRelay bodyRelay      { "body"      };
    juce::WebSliderRelay toneRelay      { "tone"      };
    juce::WebSliderRelay sagRelay       { "sag"       };
    juce::WebSliderRelay blendRelay     { "blend"     };
    juce::WebSliderRelay levelRelay     { "level"     };

    juce::WebSliderParameterAttachment inputAttachment;
    juce::WebSliderParameterAttachment driveAttachment;
    juce::WebSliderParameterAttachment characterAttachment;
    juce::WebSliderParameterAttachment bodyAttachment;
    juce::WebSliderParameterAttachment toneAttachment;
    juce::WebSliderParameterAttachment sagAttachment;
    juce::WebSliderParameterAttachment blendAttachment;
    juce::WebSliderParameterAttachment levelAttachment;

    juce::WebBrowserComponent webView;

    float displayedInputPeak  { 0.0f };
    float displayedOutputPeak { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VroomAudioProcessorEditor)
};
