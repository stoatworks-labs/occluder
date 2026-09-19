/*
 * The audio path: one CurveDesign shared by every channel, four biquad states
 * per channel, and a smoothed knob.
 *
 * Nothing in process() allocates or locks. The channel states are sized in
 * prepare(); the design is a value type rebuilt in place. The knob is ramped
 * over `rampSeconds` and the sections are redesigned at sub-block boundaries
 * while it moves, so an automation sweep from 0 to 100 is 75-odd small steps
 * rather than one jump - tools/ocdsp.cpp measures that nothing clicks.
 *
 * Zero is a true pass-through, but not by switching the filters out: their
 * state carries the tail of whatever the knob was just doing, and dropping it
 * would click. At zero every section is the identity, which flushes its state
 * within two samples; the filters keep running until every state is nothing,
 * and only then do blocks go untouched.
 */
#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <vector>

#include "OcclusionCurve.h"

namespace occluder
{

class Engine
{
public:
    static constexpr double rampSeconds = 0.05;
    static constexpr int subBlock = 32;

    void prepare(double sampleRate, int numChannels);
    void reset();

    /** The knob, 0..1. Ramped unless `immediate`. */
    void setAmount(double amount, bool immediate = false);
    double getAmount() const { return smoother.getTargetValue(); }

    /** In place, `numChannels` <= the count given to prepare(). */
    void process(float* const* channels, int numChannels, int numSamples);

    /** The design the audio is currently running through. */
    const CurveDesign& currentDesign() const { return design; }
    double getSampleRate() const { return sampleRate; }

    /** True while blocks are going through untouched. For the checks. */
    bool isPassingThrough() const { return passThrough; }

private:
    struct ChannelState
    {
        BiquadState boom, muffle, top, rumble;
        void reset() { boom.reset(); muffle.reset(); top.reset(); rumble.reset(); }
        bool isQuiet(double threshold) const;
    };

    void redesign(double amount);
    void processRun(float* const* channels, int numChannels, int start, int numSamples);
    bool statesAreQuiet() const;

    // Below this, a section's state is inaudible by any measure (-200 dB) and
    // zeroing it changes nothing anyone could hear or meter.
    static constexpr double quietThreshold = 1e-10;

    double sampleRate = 48000.0;
    juce::SmoothedValue<double, juce::ValueSmoothingTypes::Linear> smoother { 0.0 };
    CurveDesign design;
    std::vector<ChannelState> states;
    bool passThrough = true;
};

} // namespace occluder
