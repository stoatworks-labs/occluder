cmake_minimum_required(VERSION 3.22)

project(Occluder VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS deployment target")

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

# A dev build goes straight into this Mac's system plug-in folders, which is
# where every host looks (SuperRack, for one, never scans ~/Library) and where
# the shipped .pkg installs to - so a dev build and a release never sit side by
# side as duplicates. Not on CI: the runner has no such folders.
if(NOT DEFINED ENV{CI})
    set(OCCLUDER_COPY_AFTER_BUILD TRUE)
else()
    set(OCCLUDER_COPY_AFTER_BUILD FALSE)
endif()

juce_add_plugin(Occluder
    COMPANY_NAME "Allan Sargeant"
    BUNDLE_ID "com.allansargeant.occluder"
    PLUGIN_MANUFACTURER_CODE Alsg
    PLUGIN_CODE Occl
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Occluder"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD ${OCCLUDER_COPY_AFTER_BUILD}
    VST3_COPY_DIR "/Library/Audio/Plug-Ins/VST3"
    AU_COPY_DIR "/Library/Audio/Plug-Ins/Components"
    VST3_CATEGORIES "Fx" "EQ"
    AU_MAIN_TYPE "kAudioUnitType_Effect"
    # The standalone is for talking into. Without the usage string macOS kills
    # the app the moment it opens an input, and the fleet's signer keys the
    # audio-input entitlement off this same string.
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Occluder listens to a microphone so you can hear your own voice through it."
)

juce_generate_juce_header(Occluder)

target_sources(Occluder
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/DSP/OcclusionCurve.cpp
        Source/DSP/Engine.cpp
        Source/GUI/OccluderLookAndFeel.cpp
        Source/GUI/CurveDisplay.cpp
)

target_compile_definitions(Occluder
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(Occluder
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# --------------------------------------------------------------------------------------
# DSP checks. No host, no audio device, no window. It pushes real signals through the
# shipped processor class and measures what comes out: bit-exact pass-through at 0,
# the response against the design at several knob positions, the speech-weighted level
# holding steady across the dial, no clicks while the knob sweeps, and no allocation on
# the processing path. See the comment at the top of tools/ocdsp.cpp.
# --------------------------------------------------------------------------------------
juce_add_console_app(ocdsp PRODUCT_NAME "ocdsp")
juce_generate_juce_header(ocdsp)
target_sources(ocdsp PRIVATE tools/ocdsp.cpp)
target_compile_definitions(ocdsp PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)
target_include_directories(ocdsp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
# Linking the plugin target rather than re-listing its sources: that is what carries the
# JucePlugin_* defines the processor header needs, which a console app does not get on
# its own.
target_link_libraries(ocdsp PRIVATE
    Occluder
    juce::juce_audio_utils
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

enable_testing()
add_test(NAME dsp COMMAND ocdsp)

# --------------------------------------------------------------------------------------
# ocshot - renders the real editor to a PNG, offscreen, for the README and the guide.
# See the comment at the top of tools/ocshot.cpp.
# --------------------------------------------------------------------------------------
juce_add_console_app(ocshot PRODUCT_NAME "ocshot")
juce_generate_juce_header(ocshot)
target_sources(ocshot PRIVATE tools/ocshot.cpp)
target_compile_definitions(ocshot PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    # runDispatchLoopUntil, which lets the editor's timer see the knob before the snapshot.
    JUCE_MODAL_LOOPS_PERMITTED=1)
target_include_directories(ocshot PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ocshot PRIVATE
    Occluder
    juce::juce_audio_utils
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
