cmake_minimum_required(VERSION 3.22)

project(VROOM VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "macOS deployment target")

if(APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS architectures")
endif()

include(cmake/CPM.cmake)

CPMAddPackage(
    NAME JUCE
    GITHUB_REPOSITORY juce-framework/JUCE
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE
)

juce_add_plugin(VROOM
    COMPANY_NAME              "Sweet Papa Technologies"
    PLUGIN_MANUFACTURER_CODE  SwPa
    PLUGIN_CODE               Vrm1
    FORMATS                   VST3 AU Standalone
    PRODUCT_NAME              "VROOM"
    BUNDLE_ID                 "com.sweetpapa.vroom"
    DESCRIPTION               "Harmonic Saturator / Dirt Pedal"
    IS_SYNTH                  FALSE
    NEEDS_MIDI_INPUT          FALSE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD   TRUE
    PLUGIN_NAME               "VROOM"
    AU_MAIN_TYPE              kAudioUnitType_Effect
    VST3_CATEGORIES           Fx Distortion
    NEEDS_WEBVIEW2            TRUE
    # macOS Standalone needs explicit microphone entitlement or the OS
    # silently delivers zero-filled input buffers — that's why the early
    # standalone builds appeared to have "no sound" even with audio routed.
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT    "VROOM needs audio input to process your instrument signal."
    HARDENED_RUNTIME_ENABLED      TRUE
    # Hardened runtime blocks mic access by default — must explicitly grant the
    # audio-input entitlement. Without this the Standalone receives zero input
    # buffers even after the user accepts the TCC permission prompt.
    HARDENED_RUNTIME_OPTIONS      "com.apple.security.device.audio-input"
)

juce_generate_juce_header(VROOM)

# Bundle the Vite-built single-file UI into BinaryData. The placeholder dist/
# index.html exists so configure succeeds even before `npm run build`.
juce_add_binary_data(VroomBinaryData
    HEADER_NAME BinaryData.h
    NAMESPACE BinaryData
    SOURCES
        ui/dist/index.html
)

set_target_properties(VroomBinaryData PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

target_sources(VROOM PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/dsp/Saturator.cpp
    Source/dsp/ToneStack.cpp
    Source/dsp/Sag.cpp
    Source/dsp/CabSim.cpp
    Source/dsp/BandSplit.cpp
    Source/presets/PresetManager.cpp
)

target_include_directories(VROOM PRIVATE Source common)

# JUCE_WEB_BROWSER must stay enabled (default) — the editor uses
# juce::WebBrowserComponent to host the React UI.
target_compile_definitions(VROOM PRIVATE
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_REPORT_APP_USAGE=0
    # Windows webview UIs: enables Options::withResourceProvider (links the
    # static WebView2 loader found via JUCE_WEBVIEW2_PACKAGE_LOCATION)
    $<$<PLATFORM_ID:Windows>:JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1>
)

target_link_libraries(VROOM PRIVATE
    VroomBinaryData
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_gui_extra
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

# ─── DAYDREAM ──────────────────────────────────────────────────────────────
# Sibling plugin in the same project. One-knob indie/lo-fi character pedal:
# tape saturation → pitch wow/flutter → chorus → slap delay → shimmer reverb.
juce_add_plugin(DAYDREAM
    COMPANY_NAME              "Sweet Papa Technologies"
    PLUGIN_MANUFACTURER_CODE  SwPa
    PLUGIN_CODE               Dydr
    FORMATS                   VST3 AU Standalone
    PRODUCT_NAME              "DAYDREAM"
    BUNDLE_ID                 "com.sweetpapa.daydream"
    DESCRIPTION               "One-Knob Indie Lo-Fi Character"
    IS_SYNTH                  FALSE
    NEEDS_MIDI_INPUT          FALSE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD   TRUE
    PLUGIN_NAME               "DAYDREAM"
    AU_MAIN_TYPE              kAudioUnitType_Effect
    VST3_CATEGORIES           Fx Modulation
    NEEDS_WEBVIEW2            TRUE
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT    "DAYDREAM needs audio input to process your signal."
    HARDENED_RUNTIME_ENABLED      TRUE
    HARDENED_RUNTIME_OPTIONS      "com.apple.security.device.audio-input"
)

juce_generate_juce_header(DAYDREAM)

juce_add_binary_data(DaydreamBinaryData
    HEADER_NAME BinaryData.h
    NAMESPACE BinaryData
    SOURCES daydream/ui/dist/index.html
)

set_target_properties(DaydreamBinaryData PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

target_sources(DAYDREAM PRIVATE
    daydream/Source/PluginProcessor.cpp
    daydream/Source/PluginEditor.cpp
    daydream/Source/dsp/TapeSat.cpp
    daydream/Source/dsp/PitchDrift.cpp
    daydream/Source/dsp/ShimmerReverb.cpp
    daydream/Source/dsp/DaydreamEngine.cpp
)

target_include_directories(DAYDREAM PRIVATE daydream/Source common)

target_compile_definitions(DAYDREAM PRIVATE
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_REPORT_APP_USAGE=0
    # Windows webview UIs: enables Options::withResourceProvider (links the
    # static WebView2 loader found via JUCE_WEBVIEW2_PACKAGE_LOCATION)
    $<$<PLATFORM_ID:Windows>:JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1>
)

target_link_libraries(DAYDREAM PRIVATE
    DaydreamBinaryData
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_gui_extra
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

# ─── FOFOPEDAL ─────────────────────────────────────────────────────────────
# Flagship multi-effect. Six serial blocks (Character → Drive → Mod ↔ Pitch →
# Delay ↔ Space) with hidden Output Glue, 4-way source voicing, 12 curated
# named characters, MIDI Learn, tap tempo + host sync, A/B compare.
juce_add_plugin(FOFOPEDAL
    COMPANY_NAME              "Sweet Papa Technologies"
    PLUGIN_MANUFACTURER_CODE  SwPa
    PLUGIN_CODE               FoFo
    FORMATS                   VST3 AU Standalone
    PRODUCT_NAME              "FOFOPEDAL"
    BUNDLE_ID                 "com.sweetpapa.fofopedal"
    DESCRIPTION               "Six-block Multi-Effect (Series A)"
    IS_SYNTH                  FALSE
    NEEDS_MIDI_INPUT          TRUE   # MIDI Learn + CC mapping + tap tempo
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD   TRUE
    PLUGIN_NAME               "FOFOPEDAL"
    AU_MAIN_TYPE              kAudioUnitType_Effect
    VST3_CATEGORIES           Fx
    NEEDS_WEBVIEW2            TRUE
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT    "FOFOPEDAL needs audio input to process your signal."
    HARDENED_RUNTIME_ENABLED      TRUE
    HARDENED_RUNTIME_OPTIONS      "com.apple.security.device.audio-input"
)

juce_generate_juce_header(FOFOPEDAL)

juce_add_binary_data(FofopedalBinaryData
    HEADER_NAME BinaryData.h
    NAMESPACE BinaryData
    SOURCES fofopedal/ui/dist/index.html
)

set_target_properties(FofopedalBinaryData PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

target_sources(FOFOPEDAL PRIVATE
    fofopedal/Source/PluginProcessor.cpp
    fofopedal/Source/PluginEditor.cpp
    fofopedal/Source/dsp/Character.cpp
    fofopedal/Source/dsp/Drive.cpp
    fofopedal/Source/dsp/Mod.cpp
    fofopedal/Source/dsp/Pitch.cpp
    fofopedal/Source/dsp/Delay.cpp
    fofopedal/Source/dsp/Space.cpp
    fofopedal/Source/dsp/OutputGlue.cpp
    fofopedal/Source/dsp/FofoEngine.cpp
    fofopedal/Source/presets/CharacterBank.cpp
)

target_include_directories(FOFOPEDAL PRIVATE fofopedal/Source common)

target_compile_definitions(FOFOPEDAL PRIVATE
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_REPORT_APP_USAGE=0
    # Windows webview UIs: enables Options::withResourceProvider (links the
    # static WebView2 loader found via JUCE_WEBVIEW2_PACKAGE_LOCATION)
    $<$<PLATFORM_ID:Windows>:JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1>
)

target_link_libraries(FOFOPEDAL PRIVATE
    FofopedalBinaryData
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_gui_extra
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

# ─── DOUBLE (Series B) ─────────────────────────────────────────────────────
juce_add_plugin(DOUBLE
    COMPANY_NAME              "Sweet Papa Technologies"
    PLUGIN_MANUFACTURER_CODE  SwPa
    PLUGIN_CODE               Dbl1
    FORMATS                   VST3 AU Standalone
    PRODUCT_NAME              "DOUBLE"
    BUNDLE_ID                 "com.sweetpapa.double"
    DESCRIPTION               "Instant Doubled-Take Thickener"
    IS_SYNTH                  FALSE
    NEEDS_MIDI_INPUT          FALSE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD   TRUE
    PLUGIN_NAME               "DOUBLE"
    AU_MAIN_TYPE              kAudioUnitType_Effect
    VST3_CATEGORIES           Fx Pitch
    NEEDS_WEBVIEW2            TRUE
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT    "DOUBLE needs audio input to process your signal."
    HARDENED_RUNTIME_ENABLED      TRUE
    HARDENED_RUNTIME_OPTIONS      "com.apple.security.device.audio-input"
)

juce_generate_juce_header(DOUBLE)

juce_add_binary_data(DoubleBinaryData
    HEADER_NAME BinaryData.h
    NAMESPACE BinaryData
    SOURCES double/ui/dist/index.html
)

set_target_properties(DoubleBinaryData PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

target_sources(DOUBLE PRIVATE
    double/Source/PluginProcessor.cpp
    double/Source/PluginEditor.cpp
    double/Source/dsp/DoubleEngine.cpp
)

target_include_directories(DOUBLE PRIVATE double/Source common)

target_compile_definitions(DOUBLE PRIVATE
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_REPORT_APP_USAGE=0
    # Windows webview UIs: enables Options::withResourceProvider (links the
    # static WebView2 loader found via JUCE_WEBVIEW2_PACKAGE_LOCATION)
    $<$<PLATFORM_ID:Windows>:JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1>
)

target_link_libraries(DOUBLE PRIVATE
    DoubleBinaryData
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_gui_extra
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

# ─── BACKPORCH (Series B) ─────────────────────────────────────────────────────
juce_add_plugin(BACKPORCH
    COMPANY_NAME              "Sweet Papa Technologies"
    PLUGIN_MANUFACTURER_CODE  SwPa
    PLUGIN_CODE               Bkpr
    FORMATS                   VST3 AU Standalone
    PRODUCT_NAME              "BACKPORCH"
    BUNDLE_ID                 "com.sweetpapa.backporch"
    DESCRIPTION               "Ducking-First Production Space"
    IS_SYNTH                  FALSE
    NEEDS_MIDI_INPUT          FALSE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD   TRUE
    PLUGIN_NAME               "BACKPORCH"
    AU_MAIN_TYPE              kAudioUnitType_Effect
    VST3_CATEGORIES           Fx Reverb
    NEEDS_WEBVIEW2            TRUE
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT    "BACKPORCH needs audio input to process your signal."
    HARDENED_RUNTIME_ENABLED      TRUE
    HARDENED_RUNTIME_OPTIONS      "com.apple.security.device.audio-input"
)

juce_generate_juce_header(BACKPORCH)

juce_add_binary_data(BackporchBinaryData
    HEADER_NAME BinaryData.h
    NAMESPACE BinaryData
    SOURCES backporch/ui/dist/index.html
)

set_target_properties(BackporchBinaryData PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

target_sources(BACKPORCH PRIVATE
    backporch/Source/PluginProcessor.cpp
    backporch/Source/PluginEditor.cpp
    backporch/Source/dsp/BackporchEngine.cpp
)

target_include_directories(BACKPORCH PRIVATE backporch/Source common)

target_compile_definitions(BACKPORCH PRIVATE
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_REPORT_APP_USAGE=0
    # Windows webview UIs: enables Options::withResourceProvider (links the
    # static WebView2 loader found via JUCE_WEBVIEW2_PACKAGE_LOCATION)
    $<$<PLATFORM_ID:Windows>:JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1>
)

target_link_libraries(BACKPORCH PRIVATE
    BackporchBinaryData
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_gui_extra
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

# ─── SWAY (Series B) ─────────────────────────────────────────────────────
juce_add_plugin(SWAY
    COMPANY_NAME              "Sweet Papa Technologies"
    PLUGIN_MANUFACTURER_CODE  SwPa
    PLUGIN_CODE               Sway
    FORMATS                   VST3 AU Standalone
    PRODUCT_NAME              "SWAY"
    BUNDLE_ID                 "com.sweetpapa.sway"
    DESCRIPTION               "Subtle Movement Engine"
    IS_SYNTH                  FALSE
    NEEDS_MIDI_INPUT          FALSE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD   TRUE
    PLUGIN_NAME               "SWAY"
    AU_MAIN_TYPE              kAudioUnitType_Effect
    VST3_CATEGORIES           Fx Modulation
    NEEDS_WEBVIEW2            TRUE
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT    "SWAY needs audio input to process your signal."
    HARDENED_RUNTIME_ENABLED      TRUE
    HARDENED_RUNTIME_OPTIONS      "com.apple.security.device.audio-input"
)

juce_generate_juce_header(SWAY)

juce_add_binary_data(SwayBinaryData
    HEADER_NAME BinaryData.h
    NAMESPACE BinaryData
    SOURCES sway/ui/dist/index.html
)

set_target_properties(SwayBinaryData PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

target_sources(SWAY PRIVATE
    sway/Source/PluginProcessor.cpp
    sway/Source/PluginEditor.cpp
    sway/Source/dsp/SwayEngine.cpp
)

target_include_directories(SWAY PRIVATE sway/Source common)

target_compile_definitions(SWAY PRIVATE
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_REPORT_APP_USAGE=0
    # Windows webview UIs: enables Options::withResourceProvider (links the
    # static WebView2 loader found via JUCE_WEBVIEW2_PACKAGE_LOCATION)
    $<$<PLATFORM_ID:Windows>:JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1>
)

target_link_libraries(SWAY PRIVATE
    SwayBinaryData
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_gui_extra
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

# ─── FoFoDriver test suite ────────────────────────────────────────────────────
# Kernel behaviour plus permanent regression guards for the DSP defects fixed
# in the Step 0 pass. Console app; returns non-zero on any failure so CI gates
# on it. Run with:  cmake --build build --target FOFO_TESTS && <artefact>/fofotests
juce_add_console_app(FOFO_TESTS PRODUCT_NAME "fofotests")

target_sources(FOFO_TESTS PRIVATE
    tests/Main.cpp
    tests/KernelTests.cpp
    tests/PedalRegressionTests.cpp
    tests/SwayTests.cpp
    # Real pedal DSP, so the regression guards test what actually ships.
    double/Source/dsp/DoubleEngine.cpp
    fofopedal/Source/dsp/Character.cpp
    fofopedal/Source/dsp/OutputGlue.cpp
    fofopedal/Source/dsp/Drive.cpp
    sway/Source/dsp/SwayEngine.cpp
)

target_include_directories(FOFO_TESTS PRIVATE common tests .)

target_compile_definitions(FOFO_TESTS PRIVATE
    JUCE_USE_CURL=0
    JUCE_STANDALONE_APPLICATION=1
    JUCE_REPORT_APP_USAGE=0
    JUCE_WEB_BROWSER=0
)

target_link_libraries(FOFO_TESTS PRIVATE
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)
