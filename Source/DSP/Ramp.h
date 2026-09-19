/*
 * A linear ramp between two values over a fixed number of samples.
 *
 * The one thing the DSP core used from JUCE was juce::SmoothedValue, and it is
 * the reason this file exists: without it Source/DSP/ depends on nothing, so
 * the same three files compile to WebAssembly for the browser demo unchanged.
 * Same behaviour as the JUCE class for the calls the engine makes: a new
 * target starts a ramp of the configured length from wherever the value is,
 * skip() advances it, and it lands exactly on the target.
 */
#pragma once

#include <algorithm>
#include <cmath>

namespace occluder
{

class Ramp
{
public:
    void reset(double sampleRate, double seconds)
    {
        rampSamples = std::max(1, (int) std::lround(sampleRate * seconds));
        countdown = 0;
        current = target;
    }

    void setCurrentAndTargetValue(double v)
    {
        current = target = v;
        countdown = 0;
    }

    void setTargetValue(double v)
    {
        // An exact comparison on purpose: the host hands the same float back
        // every block, and a ramp restarted on each of them would never land.
        if (std::abs(v - target) == 0.0)
            return;
        target = v;
        countdown = rampSamples;
        step = (target - current) / (double) countdown;
    }

    /** Advance by n samples. Lands exactly on the target when the ramp ends. */
    void skip(int n)
    {
        if (countdown <= 0)
            return;
        if (n >= countdown)
        {
            current = target;
            countdown = 0;
            return;
        }
        current += step * (double) n;
        countdown -= n;
    }

    bool isSmoothing() const { return countdown > 0; }
    double getCurrentValue() const { return current; }
    double getTargetValue() const { return target; }

private:
    double current = 0.0, target = 0.0, step = 0.0;
    int rampSamples = 1, countdown = 0;
};

} // namespace occluder
