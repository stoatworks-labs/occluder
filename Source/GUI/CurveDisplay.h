#pragma once

#include <JuceHeader.h>

#include "../PluginProcessor.h"

namespace occluder
{

/* The response the knob is asking for, drawn from the same design() the audio
   runs through - trim included, so what is shown is what the output does
   relative to the input. Polls the parameter at 30 Hz and repaints when it
   has moved. */
class CurveDisplay : public juce::Component,
                     private juce::Timer
{
public:
    explicit CurveDisplay(OccluderAudioProcessor&);
    ~CurveDisplay() override;

    void paint(juce::Graphics&) override;

private:
    void timerCallback() override;

    float xForFrequency(double freq, juce::Rectangle<float> plot) const;
    float yForDb(double db, juce::Rectangle<float> plot) const;

    static constexpr double minFreq = 20.0, maxFreq = 20000.0;
    static constexpr double minDb = -42.0, maxDb = 12.0;

    OccluderAudioProcessor& processor;
    double drawnAmount = -1.0;
    double drawnSampleRate = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CurveDisplay)
};

} // namespace occluder
