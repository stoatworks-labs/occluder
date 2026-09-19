#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace occluder
{

namespace ParamIDs
{
    inline constexpr const char* amount = "amount";
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // The one knob. 0 is bit-exact pass-through; 100 is foam plugs in. It
    // defaults to the middle so that inserting the plugin is audible without
    // touching anything.
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamIDs::amount, 1 },
        "Occlusion",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        50.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel("%")
            .withStringFromValueFunction([](float v, int) { return juce::String(v, 0) + " %"; })
            .withValueFromStringFunction([](const juce::String& s) { return s.getFloatValue(); })));

    return layout;
}

} // namespace occluder
