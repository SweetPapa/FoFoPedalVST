#include "PluginEditor.h"
#include "BinaryData.h"

namespace
{
    juce::String mimeFor (const juce::String& path)
    {
        if (path.endsWithIgnoreCase (".html"))  return "text/html";
        if (path.endsWithIgnoreCase (".js"))    return "application/javascript";
        if (path.endsWithIgnoreCase (".css"))   return "text/css";
        if (path.endsWithIgnoreCase (".json"))  return "application/json";
        if (path.endsWithIgnoreCase (".svg"))   return "image/svg+xml";
        return "application/octet-stream";
    }

    juce::String mangleForBinaryData (const juce::String& filename)
    {
        juce::String out;
        out.preallocateBytes ((size_t) filename.length() + 4);
        for (auto c : filename)
        {
            if (juce::CharacterFunctions::isLetterOrDigit ((juce::juce_wchar) c))
                out += juce::String::charToString (c);
            else
                out += '_';
        }
        return out;
    }
}

std::optional<juce::WebBrowserComponent::Resource>
DaydreamAudioProcessorEditor::getResource (const juce::String& url) const
{
    juce::String path = url.fromFirstOccurrenceOf ("/", false, false);
    if (path.isEmpty()) path = "index.html";

    const auto mangled = mangleForBinaryData (path);

    int size = 0;
    if (const char* data = BinaryData::getNamedResource (mangled.toRawUTF8(), size))
    {
        juce::WebBrowserComponent::Resource res;
        res.data.assign (reinterpret_cast<const std::byte*> (data),
                         reinterpret_cast<const std::byte*> (data) + size);
        res.mimeType = mimeFor (path).toStdString();
        return res;
    }
    return std::nullopt;
}

juce::var DaydreamAudioProcessorEditor::buildPresetStateVar() const
{
    auto& presets = processorRef.getPresets();
    const int index = presets.getCurrentProgram();

    auto* obj = new juce::DynamicObject();
    obj->setProperty ("name",     presets.getProgramName (index));
    obj->setProperty ("blurb",    presets.getProgramBlurb (index));
    obj->setProperty ("index",    index);
    obj->setProperty ("count",    presets.getNumPrograms());
    obj->setProperty ("modified", presets.isModified());
    return juce::var (obj);
}

DaydreamAudioProcessorEditor::DaydreamAudioProcessorEditor (DaydreamAudioProcessor& p)
    : juce::AudioProcessorEditor (&p),
      processorRef (p),
      dreamAttachment (*processorRef.getAPVTS().getParameter (daydream::ParamID::dream), dreamRelay, nullptr),
      webView (juce::WebBrowserComponent::Options{}
                   .withResourceProvider ([this] (const auto& url) { return getResource (url); })
                   .withNativeIntegrationEnabled()
                   .withOptionsFrom (dreamRelay)
                   .withNativeFunction (
                       juce::Identifier ("resizeTo"),
                       [this] (const juce::Array<juce::var>& args,
                               juce::WebBrowserComponent::NativeFunctionCompletion completion)
                       {
                           // The native WKWebView covers JUCE's corner resizer,
                           // so the web UI's drag grip drives resizing instead.
                           if (args.size() >= 2)
                               setSize (juce::jlimit (420, 1200, (int) args[0]),
                                        juce::jlimit (396, 1130, (int) args[1]));
                           completion ({});
                       })
                   .withNativeFunction (
                       juce::Identifier ("stepPreset"),
                       [this] (const juce::Array<juce::var>& args,
                               juce::WebBrowserComponent::NativeFunctionCompletion completion)
                       {
                           processorRef.getPresets().step (args.size() > 0 ? (int) args[0] : 1);
                           completion (buildPresetStateVar());
                       })
                   .withNativeFunction (
                       juce::Identifier ("getPresetState"),
                       [this] (const juce::Array<juce::var>&,
                               juce::WebBrowserComponent::NativeFunctionCompletion completion)
                       {
                           completion (buildPresetStateVar());
                       }))
{
    addAndMakeVisible (webView);
    setResizable (true, true);
    setResizeLimits (420, 396, 1200, 1130);
    setSize (520, 490);

    webView.goToURL (webView.getResourceProviderRoot());

    startTimerHz (30);
}

DaydreamAudioProcessorEditor::~DaydreamAudioProcessorEditor()
{
    stopTimer();
}

void DaydreamAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1a0e2e));
}

void DaydreamAudioProcessorEditor::resized()
{
    webView.setBounds (getLocalBounds());
}

void DaydreamAudioProcessorEditor::timerCallback()
{
    const float newIn  = processorRef.fetchInputPeakAndReset();
    const float newOut = processorRef.fetchOutputPeakAndReset();

    constexpr float decay = 0.80f;
    displayedInputPeak  = juce::jmax (newIn,  displayedInputPeak  * decay);
    displayedOutputPeak = juce::jmax (newOut, displayedOutputPeak * decay);

    auto* obj = new juce::DynamicObject();
    obj->setProperty ("in",  displayedInputPeak);
    obj->setProperty ("out", displayedOutputPeak);
    webView.emitEventIfBrowserIsVisible ("audioLevels", juce::var (obj));

    // The preset can change from the host as well as from our own header, so
    // the UI is told about it here rather than only when it asks.
    auto& presets = processorRef.getPresets();
    const auto name     = presets.getProgramName (presets.getCurrentProgram());
    const bool modified = presets.isModified();

    if (! havePushedPresetState || name != lastEmittedPresetName || modified != lastEmittedModified)
    {
        webView.emitEventIfBrowserIsVisible ("presetState", buildPresetStateVar());
        lastEmittedPresetName = name;
        lastEmittedModified   = modified;
        havePushedPresetState = true;
    }
}
