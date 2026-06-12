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
SwayAudioProcessorEditor::getResource (const juce::String& url) const
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

SwayAudioProcessorEditor::SwayAudioProcessorEditor (SwayAudioProcessor& p)
    : juce::AudioProcessorEditor (&p),
      processorRef (p),
      moveAttachment (*p.getAPVTS().getParameter (sway::ParamID::move), moveRelay, nullptr),
      rateAttachment  (*p.getAPVTS().getParameter (sway::ParamID::rate),  rateRelay,  nullptr),
      colorAttachment (*p.getAPVTS().getParameter (sway::ParamID::color), colorRelay, nullptr),
      mixAttachment   (*p.getAPVTS().getParameter (sway::ParamID::mix),   mixRelay,   nullptr),
      modeAttachment  (*p.getAPVTS().getParameter (sway::ParamID::mode),  modeRelay,  nullptr),
      webView (juce::WebBrowserComponent::Options{}
                   .withResourceProvider ([this] (const auto& url) { return getResource (url); })
                   .withNativeIntegrationEnabled()
                   .withOptionsFrom (moveRelay)
                   .withOptionsFrom (rateRelay)
                   .withOptionsFrom (colorRelay)
                   .withOptionsFrom (mixRelay)
                   .withOptionsFrom (modeRelay)
                   .withNativeFunction (
                       juce::Identifier ("resizeTo"),
                       [this] (const juce::Array<juce::var>& args,
                               juce::WebBrowserComponent::NativeFunctionCompletion completion)
                       {
                           // The native WKWebView covers JUCE's corner resizer,
                           // so the web UI's drag grip drives resizing instead.
                           if (args.size() >= 2)
                               setSize (juce::jlimit (560, 1400, (int) args[0]),
                                        juce::jlimit (360, 900, (int) args[1]));
                           completion ({});
                       }))
{
    addAndMakeVisible (webView);
    setResizable (true, true);
    setResizeLimits (560, 360, 1400, 900);
    setSize (680, 420);

    webView.goToURL (webView.getResourceProviderRoot());

    startTimerHz (30);
}

SwayAudioProcessorEditor::~SwayAudioProcessorEditor()
{
    stopTimer();
}

void SwayAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff14181f));
}

void SwayAudioProcessorEditor::resized()
{
    webView.setBounds (getLocalBounds());
}

void SwayAudioProcessorEditor::timerCallback()
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
}
