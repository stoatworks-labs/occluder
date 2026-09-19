#include "Engine.h"

#include <algorithm>
#include <cmath>

namespace occluder
{

bool Engine::ChannelState::isQuiet(double threshold) const
{
    for (const auto* s : { &boom, &muffle, &top, &rumble })
        if (std::abs(s->s1) > threshold || std::abs(s->s2) > threshold)
            return false;
    return true;
}

void Engine::prepare(double newSampleRate, int numChannels)
{
    sampleRate = newSampleRate;
    states.assign((size_t) std::max(1, numChannels), ChannelState{});
    smoother.reset(sampleRate, rampSeconds);
    // A new sample rate means new coefficients for the same knob position.
    redesign(smoother.getTargetValue());
    passThrough = design.unity;
}

void Engine::reset()
{
    for (auto& s : states)
        s.reset();
}

void Engine::setAmount(double amount, bool immediate)
{
    amount = std::clamp(amount, 0.0, 1.0);
    if (immediate)
    {
        smoother.setCurrentAndTargetValue(amount);
        redesign(amount);
        passThrough = design.unity;
        if (passThrough)
            reset();
        return;
    }
    smoother.setTargetValue(amount);
}

void Engine::redesign(double amount)
{
    design = curve::design(sampleRate, amount);
}

bool Engine::statesAreQuiet() const
{
    for (const auto& s : states)
        if (! s.isQuiet(quietThreshold))
            return false;
    return true;
}

void Engine::process(float* const* channels, int numChannels, int numSamples)
{
    numChannels = std::min(numChannels, (int) states.size());

    if (! smoother.isSmoothing())
    {
        if (passThrough)
            return;

        processRun(channels, numChannels, 0, numSamples);

        // Parked at zero: the sections are the identity, so once their state
        // has died away there is nothing left for them to do.
        if (design.unity && statesAreQuiet())
        {
            reset();
            passThrough = true;
        }
        return;
    }

    // The knob is moving: step the design every sub-block.
    passThrough = false;
    for (int start = 0; start < numSamples; start += subBlock)
    {
        const int n = std::min(subBlock, numSamples - start);
        smoother.skip(n);
        redesign(smoother.getCurrentValue());
        processRun(channels, numChannels, start, n);
    }
}

void Engine::processRun(float* const* channels, int numChannels, int start, int numSamples)
{
    const auto& d = design;
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto& st = states[(size_t) ch];
        float* x = channels[ch] + start;
        for (int i = 0; i < numSamples; ++i)
        {
            double v = (double) x[i];
            v = st.rumble.process(d.rumble, v);
            v = st.boom.process(d.boom, v);
            v = st.muffle.process(d.muffle, v);
            v = st.top.process(d.top, v);
            x[i] = (float) (v * d.trimLinear);
        }
    }
}

} // namespace occluder
