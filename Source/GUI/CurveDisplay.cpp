#include "CurveDisplay.h"

#include "OccluderLookAndFeel.h"

namespace occluder
{

CurveDisplay::CurveDisplay(OccluderAudioProcessor& p)
    : processor(p)
{
    setInterceptsMouseClicks(false, false);
    startTimerHz(30);
}

CurveDisplay::~CurveDisplay()
{
    stopTimer();
}

void CurveDisplay::timerCallback()
{
    const auto amount = processor.getAmount();
    const auto sampleRate = processor.getCurrentSampleRate();
    if (! juce::exactlyEqual(amount, drawnAmount) || ! juce::exactlyEqual(sampleRate, drawnSampleRate))
        repaint();
}

float CurveDisplay::xForFrequency(double freq, juce::Rectangle<float> plot) const
{
    const auto t = std::log(freq / minFreq) / std::log(maxFreq / minFreq);
    return plot.getX() + (float) t * plot.getWidth();
}

float CurveDisplay::yForDb(double db, juce::Rectangle<float> plot) const
{
    const auto t = (db - minDb) / (maxDb - minDb);
    return plot.getBottom() - (float) t * plot.getHeight();
}

void CurveDisplay::paint(juce::Graphics& g)
{
    const auto area = getLocalBounds().toFloat();
    g.setColour(OccluderLookAndFeel::panel);
    g.fillRoundedRectangle(area, 6.0f);

    // Room for the frequency labels along the bottom and dB labels at the left.
    auto plot = area.reduced(1.0f);
    plot.removeFromBottom(16.0f);
    plot.removeFromLeft(30.0f);
    plot.removeFromRight(8.0f);
    plot.removeFromTop(8.0f);

    g.setFont(juce::Font(juce::FontOptions(10.0f)));

    // Frequency grid: decades labelled, the 2s and 5s faint.
    for (double f : { 20.0, 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0, 20000.0 })
    {
        const float x = xForFrequency(f, plot);
        const bool major = juce::exactlyEqual(f, 100.0) || juce::exactlyEqual(f, 1000.0) || juce::exactlyEqual(f, 10000.0);
        g.setColour(major ? OccluderLookAndFeel::grid.brighter(0.4f) : OccluderLookAndFeel::grid);
        g.drawVerticalLine((int) x, plot.getY(), plot.getBottom());
        if (major)
        {
            g.setColour(OccluderLookAndFeel::textDim);
            const auto label = f >= 1000.0 ? juce::String(f / 1000.0, 0) + "k" : juce::String(f, 0);
            g.drawText(label, juce::Rectangle<float>(x - 20.0f, plot.getBottom() + 2.0f, 40.0f, 12.0f),
                       juce::Justification::centred);
        }
    }

    // Gain grid every 12 dB, the zero line brighter.
    for (double db = minDb + 6.0; db <= maxDb; db += 12.0)
    {
        const float y = yForDb(db, plot);
        g.setColour(juce::exactlyEqual(db, 0.0) ? OccluderLookAndFeel::grid.brighter(0.6f) : OccluderLookAndFeel::grid);
        g.drawHorizontalLine((int) y, plot.getX(), plot.getRight());
        g.setColour(OccluderLookAndFeel::textDim);
        g.drawText(juce::String((int) db), juce::Rectangle<float>(area.getX() + 2.0f, y - 6.0f, 26.0f, 12.0f),
                   juce::Justification::centredRight);
    }

    drawnAmount = processor.getAmount();
    drawnSampleRate = processor.getCurrentSampleRate();
    const auto design = curve::design(drawnSampleRate, drawnAmount);

    // The curve, one point per pixel column, and the area between it and 0 dB.
    juce::Path curvePath, fill;
    const int columns = juce::jmax(2, (int) plot.getWidth());
    const float zeroY = yForDb(0.0, plot);
    for (int i = 0; i < columns; ++i)
    {
        const double t = (double) i / (double) (columns - 1);
        const double f = minFreq * std::pow(maxFreq / minFreq, t);
        const double db = juce::jlimit(minDb, maxDb, design.magnitudeDb(drawnSampleRate, f));
        const float x = plot.getX() + (float) i;
        const float y = yForDb(db, plot);
        if (i == 0)
        {
            curvePath.startNewSubPath(x, y);
            fill.startNewSubPath(x, zeroY);
        }
        curvePath.lineTo(x, y);
        fill.lineTo(x, y);
    }
    fill.lineTo(plot.getX() + (float) (columns - 1), zeroY);
    fill.closeSubPath();

    g.setColour(OccluderLookAndFeel::accent.withAlpha(0.18f));
    g.fillPath(fill);
    g.setColour(OccluderLookAndFeel::accent);
    g.strokePath(curvePath, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // The trim, so the loudness compensation is visible rather than mysterious.
    if (! design.unity)
    {
        g.setColour(OccluderLookAndFeel::textDim);
        g.drawText("trim " + juce::String(design.trimDb, 1) + " dB",
                   plot.withTrimmedTop(2.0f).withTrimmedRight(4.0f), juce::Justification::topRight);
    }
}

} // namespace occluder
