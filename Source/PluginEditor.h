#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"
#include "GUI/CurveDisplay.h"
#include "GUI/OccluderLookAndFeel.h"
#include "StoatworksAboutPanel.h"

class OccluderAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit OccluderAudioProcessorEditor(OccluderAudioProcessor&);
    ~OccluderAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    /** What the "i" button does. Public for tools/ocshot.cpp. */
    void showAbout() { aboutPanel.setVisible(true); }

private:
    OccluderAudioProcessor& audioProcessor;
    occluder::OccluderLookAndFeel lookAndFeel;

    occluder::CurveDisplay curve;
    juce::Slider knob;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> knobAttachment;

    /* Vendored from stoatworks-backend/about - see StoatworksAboutPanel.h.
       A child of the editor rather than a window of its own: a plugin must not
       put a second top-level window on a host's screen. */
    juce::TextButton aboutButton { "i" };
    stoatworks::AboutPanel aboutPanel;
    juce::TooltipWindow tooltips { this };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OccluderAudioProcessorEditor)
};
