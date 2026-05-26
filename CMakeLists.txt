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
)

target_include_directories(VROOM PRIVATE Source)

# JUCE_WEB_BROWSER must stay enabled (default) — the editor uses
# juce::WebBrowserComponent to host the React UI.
target_compile_definitions(VROOM PRIVATE
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_REPORT_APP_USAGE=0
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
