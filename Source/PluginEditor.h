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

    void launchIRFileChooser (juce::WebBrowserComponent::NativeFunctionCompletion completion);

    // Builds the {current, list} object emitted on the `presetState` event.
    juce::var buildPresetStateVar() const;
    void emitPresetStateIfChanged();

    VroomAudioProcessor& processorRef;

    juce::WebSliderRelay inputRelay     { "input"     };
    juce::WebSliderRelay driveRelay     { "drive"     };
    juce::WebSliderRelay characterRelay { "character" };
    juce::WebSliderRelay bodyRelay      { "body"      };
    juce::WebSliderRelay toneRelay      { "tone"      };
    juce::WebSliderRelay sagRelay       { "sag"       };
    juce::WebSliderRelay blendRelay     { "blend"     };
    juce::WebSliderRelay levelRelay     { "level"     };

    juce::WebToggleButtonRelay cabEnableRelay  { "cabEnable"  };
    juce::WebComboBoxRelay     cabIRRelay      { "cabIR"      };
    juce::WebComboBoxRelay     sourceModeRelay { "sourceMode" };
    juce::WebComboBoxRelay     clipShapeRelay  { "clipShape"  };

    juce::WebSliderParameterAttachment inputAttachment;
    juce::WebSliderParameterAttachment driveAttachment;
    juce::WebSliderParameterAttachment characterAttachment;
    juce::WebSliderParameterAttachment bodyAttachment;
    juce::WebSliderParameterAttachment toneAttachment;
    juce::WebSliderParameterAttachment sagAttachment;
    juce::WebSliderParameterAttachment blendAttachment;
    juce::WebSliderParameterAttachment levelAttachment;

    juce::WebToggleButtonParameterAttachment cabEnableAttachment;
    juce::WebComboBoxParameterAttachment     cabIRAttachment;
    juce::WebComboBoxParameterAttachment     sourceModeAttachment;
    juce::WebComboBoxParameterAttachment     clipShapeAttachment;

    juce::WebBrowserComponent webView;

    std::shared_ptr<juce::FileChooser> activeChooser;

    float displayedInputPeak  { 0.0f };
    float displayedOutputPeak { 0.0f };

    // Cached last-emitted preset state so we only push the event when it
    // actually changes (name, category, factory flag, or modified flag).
    juce::String lastEmittedName;
    juce::String lastEmittedCategory;
    bool         lastEmittedFactory  { true };
    bool         lastEmittedModified { false };
    int          lastEmittedListVersion { -1 };
    int          presetListVersion { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VroomAudioProcessorEditor)
};
