#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

class FofopedalAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      private juce::Timer
{
public:
    explicit FofopedalAudioProcessorEditor (FofopedalAudioProcessor&);
    ~FofopedalAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    std::optional<juce::WebBrowserComponent::Resource>
        getResource (const juce::String& url) const;

    FofopedalAudioProcessor& processorRef;

    // Relays + attachments — populated in the constructor. Vectors give us
    // O(N) wiring code rather than ~120 lines of repetitive declarations.
    std::vector<std::unique_ptr<juce::WebSliderRelay>>                   sliderRelays;
    std::vector<std::unique_ptr<juce::WebSliderParameterAttachment>>     sliderAttachments;
    std::vector<std::unique_ptr<juce::WebToggleButtonRelay>>             toggleRelays;
    std::vector<std::unique_ptr<juce::WebToggleButtonParameterAttachment>> toggleAttachments;
    std::vector<std::unique_ptr<juce::WebComboBoxRelay>>                 comboRelays;
    std::vector<std::unique_ptr<juce::WebComboBoxParameterAttachment>>   comboAttachments;

    // unique_ptr because we construct it *after* the relays have been built
    // (the relays must exist before being passed into Options::withOptionsFrom).
    std::unique_ptr<juce::WebBrowserComponent> webView;

    float displayedInputPeak  { 0.0f };
    float displayedOutputPeak { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FofopedalAudioProcessorEditor)
};
