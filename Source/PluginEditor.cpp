#include "PluginEditor.h"

namespace
{
    constexpr int titleHeight = 36;
    constexpr int curveHeight = 170;
    constexpr int knobSize = 200;
    constexpr int margin = 14;
}

OccluderAudioProcessorEditor::OccluderAudioProcessorEditor(OccluderAudioProcessor& p)
    : AudioProcessorEditor(&p),
      audioProcessor(p),
      curve(p)
{
    addAndMakeVisible(curve);

    knob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    knob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 90, 26);
    knob.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f,
                             juce::MathConstants<float>::pi * 2.75f, true);
    knob.setDoubleClickReturnValue(true, 0.0);
    knob.setTooltip("How plugged the ears are. 0 is your voice as a microphone hears it; 100 is foam earplugs in.");
    addAndMakeVisible(knob);
    knobAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, occluder::ParamIDs::amount, knob);

    addAndMakeVisible(aboutButton);
    aboutButton.setTooltip("About Occluder");
    aboutButton.onClick = [this] { aboutPanel.setVisible(true); };

    // Hidden until asked for, and on top of everything when it is shown.
    addChildComponent(aboutPanel);
    aboutPanel.setAlwaysOnTop(true);

    // After the children exist: a Slider builds its text box with whatever
    // look-and-feel it can see at the time, and only rebuilds it when told the
    // look-and-feel changed - which setting it here, last, does for all of them.
    setLookAndFeel(&lookAndFeel);

    setSize(2 * margin + 332, titleHeight + curveHeight + knobSize + 26 + 3 * margin + 30);
}

OccluderAudioProcessorEditor::~OccluderAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

void OccluderAudioProcessorEditor::paint(juce::Graphics& g)
{
    using LF = occluder::OccluderLookAndFeel;
    g.fillAll(LF::background);

    auto titleArea = getLocalBounds().removeFromTop(titleHeight).reduced(margin, 0);
    g.setColour(LF::text);
    g.setFont(juce::Font(juce::FontOptions(17.0f, juce::Font::bold)));
    g.drawText("Occluder", titleArea, juce::Justification::centredLeft);

    g.setColour(LF::textDim);
    g.setFont(juce::Font(juce::FontOptions(11.0f)));
    // Right-aligned, but clear of the About button that resized() puts in the
    // last 30 px of this strip.
    g.drawText("own-voice occlusion", titleArea.withTrimmedRight(34), juce::Justification::centredRight);

    // The ends of the dial, named. Everything in between is the knob's business.
    const auto knobBounds = knob.getBounds();
    const auto labelY = knobBounds.getBottom() - 30 - 26;
    g.setFont(juce::Font(juce::FontOptions(11.0f)));
    g.setColour(LF::textDim);
    g.drawText("open", juce::Rectangle<int>(knobBounds.getX() - 8, labelY, 60, 16), juce::Justification::centredLeft);
    g.drawText("plugged", juce::Rectangle<int>(knobBounds.getRight() - 52, labelY, 60, 16), juce::Justification::centredRight);

    g.setColour(LF::textDim);
    g.setFont(juce::Font(juce::FontOptions(10.0f)));
    g.drawText("OCCLUSION", knobBounds.withHeight(14).translated(0, knobBounds.getHeight() + 2),
               juce::Justification::centred);
}

void OccluderAudioProcessorEditor::resized()
{
    aboutPanel.setBounds(getLocalBounds());

    auto area = getLocalBounds();
    auto titleArea = area.removeFromTop(titleHeight);
    aboutButton.setBounds(titleArea.removeFromRight(margin + 30).withTrimmedRight(margin).reduced(0, 7));

    area.reduce(margin, 0);
    curve.setBounds(area.removeFromTop(curveHeight));
    area.removeFromTop(margin);

    auto knobArea = area.removeFromTop(knobSize + 26);
    knob.setBounds(knobArea.withSizeKeepingCentre(knobSize, knobSize + 26));
}
