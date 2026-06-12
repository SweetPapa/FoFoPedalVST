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

    juce::var presetSummaryToVar (const vroom::Preset& p)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("name",      p.name);
        o->setProperty ("category",  p.category);
        o->setProperty ("vibe",      p.vibe);
        o->setProperty ("isFactory", p.isFactory);
        return juce::var (o);
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

void VroomAudioProcessorEditor::launchIRFileChooser (juce::WebBrowserComponent::NativeFunctionCompletion completion)
{
    activeChooser = std::make_shared<juce::FileChooser> (
        "Load impulse response", juce::File{}, "*.wav;*.aif;*.aiff;*.flac");

    auto chooser = activeChooser;
    chooser->launchAsync (
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this, chooser, completion = std::move (completion)] (const juce::FileChooser& fc) mutable
        {
            const auto file = fc.getResult();
            juce::String resultName;
            if (file.existsAsFile())
                resultName = processorRef.loadCustomIR (file);
            completion (juce::var (resultName));
            activeChooser.reset();
        });
}

juce::var VroomAudioProcessorEditor::buildPresetStateVar() const
{
    auto& pm = processorRef.getPresetManager();

    auto* current = new juce::DynamicObject();
    current->setProperty ("name",      pm.getCurrentName());
    current->setProperty ("category",  pm.getCurrentCategory());
    current->setProperty ("isFactory", pm.isCurrentFactory());
    current->setProperty ("modified",  pm.isCurrentModified());

    juce::Array<juce::var> list;
    for (const auto& p : pm.getFactoryPresets()) list.add (presetSummaryToVar (p));
    for (const auto& p : pm.getUserPresets())    list.add (presetSummaryToVar (p));

    auto* root = new juce::DynamicObject();
    root->setProperty ("current", juce::var (current));
    root->setProperty ("presets", list);
    return juce::var (root);
}

void VroomAudioProcessorEditor::emitPresetStateIfChanged()
{
    auto& pm = processorRef.getPresetManager();
    const juce::String name = pm.getCurrentName();
    const juce::String cat  = pm.getCurrentCategory();
    const bool factory      = pm.isCurrentFactory();
    const bool modified     = pm.isCurrentModified();

    const bool stateChanged = name != lastEmittedName
                           || cat  != lastEmittedCategory
                           || factory  != lastEmittedFactory
                           || modified != lastEmittedModified
                           || presetListVersion != lastEmittedListVersion;

    if (! stateChanged) return;

    webView.emitEventIfBrowserIsVisible ("presetState", buildPresetStateVar());

    lastEmittedName     = name;
    lastEmittedCategory = cat;
    lastEmittedFactory  = factory;
    lastEmittedModified = modified;
    lastEmittedListVersion = presetListVersion;
}

VroomAudioProcessorEditor::VroomAudioProcessorEditor (VroomAudioProcessor& p)
    : juce::AudioProcessorEditor (&p),
      processorRef (p),
      inputAttachment     (*processorRef.getAPVTS().getParameter (vroom::ParamID::input),     inputRelay,     nullptr),
      driveAttachment     (*processorRef.getAPVTS().getParameter (vroom::ParamID::drive),     driveRelay,     nullptr),
      characterAttachment (*processorRef.getAPVTS().getParameter (vroom::ParamID::character), characterRelay, nullptr),
      bodyAttachment      (*processorRef.getAPVTS().getParameter (vroom::ParamID::body),      bodyRelay,      nullptr),
      toneAttachment      (*processorRef.getAPVTS().getParameter (vroom::ParamID::tone),      toneRelay,      nullptr),
      sagAttachment       (*processorRef.getAPVTS().getParameter (vroom::ParamID::sag),       sagRelay,       nullptr),
      blendAttachment     (*processorRef.getAPVTS().getParameter (vroom::ParamID::blend),     blendRelay,     nullptr),
      levelAttachment     (*processorRef.getAPVTS().getParameter (vroom::ParamID::level),     levelRelay,     nullptr),
      cabEnableAttachment  (*processorRef.getAPVTS().getParameter (vroom::ParamID::cabEnable),  cabEnableRelay,  nullptr),
      cabIRAttachment      (*processorRef.getAPVTS().getParameter (vroom::ParamID::cabIR),      cabIRRelay,      nullptr),
      sourceModeAttachment (*processorRef.getAPVTS().getParameter (vroom::ParamID::sourceMode), sourceModeRelay, nullptr),
      clipShapeAttachment  (*processorRef.getAPVTS().getParameter (vroom::ParamID::clipShape),  clipShapeRelay,  nullptr),
      webView (juce::WebBrowserComponent::Options{}
                   .withResourceProvider ([this] (const auto& url) { return getResource (url); })
                   .withNativeIntegrationEnabled()
                   .withOptionsFrom (inputRelay)
                   .withOptionsFrom (driveRelay)
                   .withOptionsFrom (characterRelay)
                   .withOptionsFrom (bodyRelay)
                   .withOptionsFrom (toneRelay)
                   .withOptionsFrom (sagRelay)
                   .withOptionsFrom (blendRelay)
                   .withOptionsFrom (levelRelay)
                   .withOptionsFrom (cabEnableRelay)
                   .withOptionsFrom (cabIRRelay)
                   .withOptionsFrom (sourceModeRelay)
                   .withOptionsFrom (clipShapeRelay)
                   .withNativeFunction (
                       juce::Identifier ("resizeTo"),
                       [this] (const juce::Array<juce::var>& args,
                               juce::WebBrowserComponent::NativeFunctionCompletion completion)
                       {
                           // The native WKWebView covers JUCE's corner resizer,
                           // so the web UI's drag grip drives resizing instead.
                           if (args.size() >= 2)
                               setSize (juce::jlimit (560, 1840, (int) args[0]),
                                        juce::jlimit (475, 1560, (int) args[1]));
                           completion ({});
                       })
                   .withNativeFunction (
                       juce::Identifier ("openIRFileDialog"),
                       [this] (const juce::Array<juce::var>&,
                               juce::WebBrowserComponent::NativeFunctionCompletion completion)
                       {
                           juce::MessageManager::callAsync (
                               [this, completion = std::move (completion)] () mutable
                               {
                                   launchIRFileChooser (std::move (completion));
                               });
                       })
                   .withNativeFunction (
                       juce::Identifier ("loadPreset"),
                       [this] (const juce::Array<juce::var>& args,
                               juce::WebBrowserComponent::NativeFunctionCompletion completion)
                       {
                           const juce::String name = args.size() > 0 ? args[0].toString() : juce::String();
                           const bool isFactory   = args.size() > 1 ? (bool) args[1] : true;
                           const bool ok = processorRef.getPresetManager().loadByName (name, isFactory);
                           if (ok) ++presetListVersion; // force re-emit so UI sees the new "current"
                           completion (juce::var (ok));
                       })
                   .withNativeFunction (
                       juce::Identifier ("saveUserPreset"),
                       [this] (const juce::Array<juce::var>& args,
                               juce::WebBrowserComponent::NativeFunctionCompletion completion)
                       {
                           const juce::String name = args.size() > 0 ? args[0].toString() : juce::String();
                           const bool ok = processorRef.getPresetManager().saveAsUser (name);
                           if (ok) ++presetListVersion;
                           completion (juce::var (ok));
                       })
                   .withNativeFunction (
                       juce::Identifier ("deleteUserPreset"),
                       [this] (const juce::Array<juce::var>& args,
                               juce::WebBrowserComponent::NativeFunctionCompletion completion)
                       {
                           const juce::String name = args.size() > 0 ? args[0].toString() : juce::String();
                           const bool ok = processorRef.getPresetManager().deleteUserPreset (name);
                           if (ok) ++presetListVersion;
                           completion (juce::var (ok));
                       })
                   .withNativeFunction (
                       juce::Identifier ("stepPreset"),
                       [this] (const juce::Array<juce::var>& args,
                               juce::WebBrowserComponent::NativeFunctionCompletion completion)
                       {
                           const int dir = args.size() > 0 ? (int) args[0] : 1;
                           const bool ok = (dir >= 0)
                               ? processorRef.getPresetManager().loadNext()
                               : processorRef.getPresetManager().loadPrevious();
                           if (ok) ++presetListVersion;
                           completion (juce::var (ok));
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
    setResizeLimits (560, 475, 1840, 1560);
    setSize (980, 780);

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
    // Audio meters
    const float newIn  = processorRef.fetchInputPeakAndReset();
    const float newOut = processorRef.fetchOutputPeakAndReset();

    constexpr float decay = 0.80f;
    displayedInputPeak  = juce::jmax (newIn,  displayedInputPeak  * decay);
    displayedOutputPeak = juce::jmax (newOut, displayedOutputPeak * decay);

    auto* obj = new juce::DynamicObject();
    obj->setProperty ("in",  displayedInputPeak);
    obj->setProperty ("out", displayedOutputPeak);
    webView.emitEventIfBrowserIsVisible ("audioLevels", juce::var (obj));

    // Preset state (only emitted if something changed since last tick).
    emitPresetStateIfChanged();
}
