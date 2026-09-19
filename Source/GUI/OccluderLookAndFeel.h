#pragma once

#include <JuceHeader.h>

namespace occluder
{

/* One palette for the editor. Navy like the Stoatworks About card, one cyan
   accent for anything live (the arc, the curve), nothing else competing. */
class OccluderLookAndFeel : public juce::LookAndFeel_V4
{
public:
    OccluderLookAndFeel();

    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider&) override;

    juce::Label* createSliderTextBox(juce::Slider&) override;

    static const juce::Colour background;
    static const juce::Colour panel;
    static const juce::Colour grid;
    static const juce::Colour accent;
    static const juce::Colour accentDim;
    static const juce::Colour text;
    static const juce::Colour textDim;
};

} // namespace occluder
