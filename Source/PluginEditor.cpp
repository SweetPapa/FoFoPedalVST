#include "PluginEditor.h"
#include "BinaryData.h"

namespace
{
    juce::String mimeFor (const juce::String& path)
    {
        if (path.endsWithIgnoreCase (".html"))  return "text/html";
        if (path.endsWithIgnoreCase (".js"))    return "application/javascript";
        if (path.endsWithIgnoreCase (".mjs"))   return "application/javascript";
        if (path.endsWithIgnoreCase (".css"))   return "text/css";
        if (path.endsWithIgnoreCase (".json"))  return "application/json";
        if (path.endsWithIgnoreCase (".svg"))   return "image/svg+xml";
        if (path.endsWithIgnoreCase (".png"))   return "image/png";
        if (path.endsWithIgnoreCase (".woff2")) return "font/woff2";
        if (path.endsWithIgnoreCase (".wav"))   return "audio/wav";
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
VroomAudioProcessorEditor::getResource (const juce::String& url) const
{
    juce::String path = url.fromFirstOccurrenceOf ("/", false, false);
    if (path.isEmpty())
        path = "index.html";

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

VroomAudioProcessorEditor::VroomAudioProcessorEditor (VroomAudioProcessor& p)
    : juce::AudioProcessorEditor (&p),
      processorRef (p),
      inputAttachment     (*processorRef.getAPVTS().getParameter (vroom::ParamID::input),     inputRelay,     nullptr),
      driveAttachment     (*processorRef.getAPVTS().getParameter (vroom::ParamID::drive),     driveRelay,     nullptr),
      characterAttachment (*processorRef.getAPVTS().getParameter (vroom::ParamID::character), characterRelay, nullptr),
      levelAttachment     (*processorRef.getAPVTS().getParameter (vroom::ParamID::level),     levelRelay,     nullptr),
      webView (juce::WebBrowserComponent::Options{}
                   .withResourceProvider ([this] (const auto& url) { return getResource (url); })
                   .withNativeIntegrationEnabled()
                   .withOptionsFrom (inputRelay)
                   .withOptionsFrom (driveRelay)
                   .withOptionsFrom (characterRelay)
                   .withOptionsFrom (levelRelay))
{
    addAndMakeVisible (webView);
    setResizable (true, true);
    setResizeLimits (520, 360, 1600, 1000);
    setSize (760, 480);

    webView.goToURL (webView.getResourceProviderRoot());

    startTimerHz (30);
}

VroomAudioProcessorEditor::~VroomAudioProcessorEditor()
{
    stopTimer();
}

void VroomAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff181a1f));
}

void VroomAudioProcessorEditor::resized()
{
    webView.setBounds (getLocalBounds());
}

void VroomAudioProcessorEditor::timerCallback()
{
    // Pull max-since-last-tick from the processor; decay our local display so
    // the bar falls smoothly rather than snapping to zero between blocks.
    const float newIn  = processorRef.fetchInputPeakAndReset();
    const float newOut = processorRef.fetchOutputPeakAndReset();

    constexpr float decay = 0.80f; // per ~33 ms tick → ~150 ms fall to zero
    displayedInputPeak  = juce::jmax (newIn,  displayedInputPeak  * decay);
    displayedOutputPeak = juce::jmax (newOut, displayedOutputPeak * decay);

    auto* obj = new juce::DynamicObject();
    obj->setProperty ("in",  displayedInputPeak);
    obj->setProperty ("out", displayedOutputPeak);
    webView.emitEventIfBrowserIsVisible ("audioLevels", juce::var (obj));
}
