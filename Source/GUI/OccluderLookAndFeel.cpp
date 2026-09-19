#include "OccluderLookAndFeel.h"

namespace occluder
{

const juce::Colour OccluderLookAndFeel::background { 0xff0b1420 };
const juce::Colour OccluderLookAndFeel::panel      { 0xff101d2e };
const juce::Colour OccluderLookAndFeel::grid       { 0xff1d3048 };
const juce::Colour OccluderLookAndFeel::accent     { 0xff4cc9f0 };
const juce::Colour OccluderLookAndFeel::accentDim  { 0x554cc9f0 };
const juce::Colour OccluderLookAndFeel::text       { 0xffe8eef5 };
const juce::Colour OccluderLookAndFeel::textDim    { 0xff93a8bd };

OccluderLookAndFeel::OccluderLookAndFeel()
{
    setColour(juce::ResizableWindow::backgroundColourId, background);
    setColour(juce::Slider::rotarySliderFillColourId, accent);
    setColour(juce::Slider::rotarySliderOutlineColourId, grid);
    setColour(juce::Slider::thumbColourId, text);
    setColour(juce::Slider::textBoxTextColourId, text);
    setColour(juce::Slider::textBoxBackgroundColourId, background);
    setColour(juce::Slider::textBoxOutlineColourId, background);
    setColour(juce::Slider::textBoxHighlightColourId, accentDim);
    setColour(juce::Label::textColourId, textDim);
    setColour(juce::TextButton::buttonColourId, panel);
    setColour(juce::TextButton::buttonOnColourId, accent);
    setColour(juce::TextButton::textColourOffId, textDim);
    setColour(juce::TextButton::textColourOnId, background);
    setColour(juce::TextEditor::highlightColourId, accentDim);
    setColour(juce::TextEditor::focusedOutlineColourId, accent);
    setColour(juce::TooltipWindow::backgroundColourId, panel);
    setColour(juce::TooltipWindow::textColourId, text);
    setColour(juce::TooltipWindow::outlineColourId, grid);
}

void OccluderLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                                           juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<float>((float) x, (float) y, (float) width, (float) height).reduced(8.0f);
    const auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    const float arcWidth = 6.0f;
    const float arcRadius = radius - arcWidth * 0.5f;

    // The track, then the part of it the knob has covered.
    juce::Path track;
    track.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(grid);
    g.strokePath(track, juce::PathStrokeType(arcWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    if (sliderPos > 0.0f)
    {
        juce::Path value;
        value.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, angle, true);
        g.setColour(slider.isEnabled() ? accent : textDim);
        g.strokePath(value, juce::PathStrokeType(arcWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // The body: a flat disc with a faint rim, and a pointer.
    const float bodyRadius = radius - arcWidth - 8.0f;
    g.setColour(panel);
    g.fillEllipse(centre.x - bodyRadius, centre.y - bodyRadius, bodyRadius * 2.0f, bodyRadius * 2.0f);
    g.setColour(grid);
    g.drawEllipse(centre.x - bodyRadius, centre.y - bodyRadius, bodyRadius * 2.0f, bodyRadius * 2.0f, 1.5f);

    juce::Path pointer;
    const float pointerLength = bodyRadius * 0.55f;
    pointer.addRoundedRectangle(-2.0f, -bodyRadius + 6.0f, 4.0f, pointerLength, 2.0f);
    pointer.applyTransform(juce::AffineTransform::rotation(angle).translated(centre));
    g.setColour(slider.isEnabled() ? text : textDim);
    g.fillPath(pointer);
}

juce::Label* OccluderLookAndFeel::createSliderTextBox(juce::Slider& slider)
{
    auto* label = LookAndFeel_V4::createSliderTextBox(slider);
    label->setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 18.0f, juce::Font::plain)));
    label->setJustificationType(juce::Justification::centred);
    label->setColour(juce::Label::textColourId, text);
    // Set on the label itself: the base class copies the slider's outline colour
    // in at creation, and which look-and-feel answers that lookup depends on
    // construction order. No box around the number, whatever the order.
    label->setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);
    label->setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    return label;
}

} // namespace occluder
