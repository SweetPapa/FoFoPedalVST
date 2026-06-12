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

    // Param ID lists used to instantiate relays + attachments.
    const std::vector<const char*>& sliderIds()
    {
        static const std::vector<const char*> ids = {
            fofopedal::ParamID::character,
            fofopedal::ParamID::characterLowCut,
            fofopedal::ParamID::drive,
            fofopedal::ParamID::driveTone,
            fofopedal::ParamID::driveMix,
            fofopedal::ParamID::shape,
            fofopedal::ParamID::modRate,
            fofopedal::ParamID::modDepth,
            fofopedal::ParamID::modFeedback,
            fofopedal::ParamID::modMix,
            fofopedal::ParamID::pitchAmount,
            fofopedal::ParamID::pitchShape,
            fofopedal::ParamID::pitchMix,
            fofopedal::ParamID::timeMs,
            fofopedal::ParamID::delayFeedback,
            fofopedal::ParamID::delayHfCut,
            fofopedal::ParamID::space,
            fofopedal::ParamID::spacePreDelay,
            fofopedal::ParamID::spaceShimmer,
            fofopedal::ParamID::spaceSendHp,
            fofopedal::ParamID::mix,
            fofopedal::ParamID::glueAmount,
        };
        return ids;
    }
    const std::vector<const char*>& toggleIds()
    {
        static const std::vector<const char*> ids = {
            fofopedal::ParamID::characterDefeated,
            fofopedal::ParamID::driveBypassed,
            fofopedal::ParamID::modBypassed,
            fofopedal::ParamID::pitchBypassed,
            fofopedal::ParamID::delayBypassed,
            fofopedal::ParamID::spaceBypassed,
            fofopedal::ParamID::glueDefeated,
            fofopedal::ParamID::swapModPitch,
            fofopedal::ParamID::swapDelaySpace,
            fofopedal::ParamID::hostSync,
            fofopedal::ParamID::delayPingPong,
        };
        return ids;
    }
    const std::vector<const char*>& comboIds()
    {
        static const std::vector<const char*> ids = {
            fofopedal::ParamID::voicing,
            fofopedal::ParamID::driveType,
            fofopedal::ParamID::modType,
            fofopedal::ParamID::pitchType,
            fofopedal::ParamID::delayType,
            fofopedal::ParamID::spaceType,
            fofopedal::ParamID::characterPreset,
            fofopedal::ParamID::syncDivision,
        };
        return ids;
    }
}

std::optional<juce::WebBrowserComponent::Resource>
FofopedalAudioProcessorEditor::getResource (const juce::String& url) const
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

FofopedalAudioProcessorEditor::FofopedalAudioProcessorEditor (FofopedalAudioProcessor& p)
    : juce::AudioProcessorEditor (&p),
      processorRef (p)
{
    auto& apvts = processorRef.getAPVTS();

    // Build relays first so their references can be passed into the WebView
    // Options before the browser component is constructed.
    for (auto* id : sliderIds())
    {
        sliderRelays.push_back (std::make_unique<juce::WebSliderRelay> (id));
        sliderAttachments.push_back (std::make_unique<juce::WebSliderParameterAttachment> (
            *apvts.getParameter (id), *sliderRelays.back(), nullptr));
    }
    for (auto* id : toggleIds())
    {
        toggleRelays.push_back (std::make_unique<juce::WebToggleButtonRelay> (id));
        toggleAttachments.push_back (std::make_unique<juce::WebToggleButtonParameterAttachment> (
            *apvts.getParameter (id), *toggleRelays.back(), nullptr));
    }
    for (auto* id : comboIds())
    {
        comboRelays.push_back (std::make_unique<juce::WebComboBoxRelay> (id));
        comboAttachments.push_back (std::make_unique<juce::WebComboBoxParameterAttachment> (
            *apvts.getParameter (id), *comboRelays.back(), nullptr));
    }

    auto opts = juce::WebBrowserComponent::Options{}
                    .withResourceProvider ([this] (const auto& url) { return getResource (url); })
                    .withNativeIntegrationEnabled();
    for (auto& r : sliderRelays) opts = opts.withOptionsFrom (*r);
    for (auto& r : toggleRelays) opts = opts.withOptionsFrom (*r);
    for (auto& r : comboRelays)  opts = opts.withOptionsFrom (*r);
    opts = opts.withNativeFunction (
                       juce::Identifier ("resizeTo"),
                       [this] (const juce::Array<juce::var>& args,
                               juce::WebBrowserComponent::NativeFunctionCompletion completion)
                       {
                           // The native WKWebView covers JUCE's corner resizer,
                           // so the web UI's drag grip drives resizing instead.
                           if (args.size() >= 2)
                               setSize (juce::jlimit (640, 1960, (int) args[0]),
                                        juce::jlimit (522, 1600, (int) args[1]));
                           completion ({});
                       });

    webView = std::make_unique<juce::WebBrowserComponent> (std::move (opts));
    addAndMakeVisible (*webView);

    setResizable (true, true);
    setResizeLimits (640, 522, 1960, 1600);
    setSize (980, 800);

    webView->goToURL (webView->getResourceProviderRoot());

    startTimerHz (30);
}

FofopedalAudioProcessorEditor::~FofopedalAudioProcessorEditor()
{
    stopTimer();
}

void FofopedalAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Warm paper backdrop in case the WebView hasn't finished loading.
    g.fillAll (juce::Colour (0xfff3ecdd));
}

void FofopedalAudioProcessorEditor::resized()
{
    if (webView) webView->setBounds (getLocalBounds());
}

void FofopedalAudioProcessorEditor::timerCallback()
{
    if (! webView) return;

    const float newIn  = processorRef.fetchInputPeakAndReset();
    const float newOut = processorRef.fetchOutputPeakAndReset();

    constexpr float decay = 0.80f;
    displayedInputPeak  = juce::jmax (newIn,  displayedInputPeak  * decay);
    displayedOutputPeak = juce::jmax (newOut, displayedOutputPeak * decay);

    auto* obj = new juce::DynamicObject();
    obj->setProperty ("in",  displayedInputPeak);
    obj->setProperty ("out", displayedOutputPeak);
    webView->emitEventIfBrowserIsVisible ("audioLevels", juce::var (obj));
}
