/*
 * The browser demo's view of the plugin: a C API over the shipped DSP.
 *
 * This plays PluginProcessor's part - it owns one Engine and hands the
 * AudioWorklet planar buffers to fill - and nothing else. Every number the
 * page shows comes from the same Source/DSP files the plugin is built from,
 * compiled here unmodified (see build.sh beside this file), so the demo cannot drift from the
 * release: the curve it draws is curve::design(), the trim it prints is the
 * trim the audio is getting, and the audio path is Engine::process().
 *
 * Two instances of the module run in the page: one in the AudioWorklet for
 * the audio, one on the main thread for the curve. The frame buffers are
 * static because a worklet's render quantum is 128 frames and the demo is
 * stereo at most; asking for more is a programming error, not a runtime one.
 */

#include <algorithm>
#include <cmath>

#include "DSP/Engine.h"

extern "C"
{

static occluder::Engine engine;
static double engineRate = 48000.0;
static constexpr int maxFrames = 128;
static constexpr int maxChannels = 2;
static float frames[maxChannels][maxFrames];

/** Prepare for a sample rate and channel count. The knob starts at 0. */
void oc_init(double sampleRate, int channels)
{
    engineRate = sampleRate;
    engine.prepare(sampleRate, std::clamp(channels, 1, maxChannels));
    engine.setAmount(0.0, true);
}

/** The knob, 0..1; ramped over 50 ms unless immediate. */
void oc_set_amount(double amount, int immediate)
{
    engine.setAmount(amount, immediate != 0);
}

double oc_get_amount() { return engine.getAmount(); }

/** Where to put channel `ch`'s samples before oc_process, and read them after. */
float* oc_buffer(int ch)
{
    return frames[std::clamp(ch, 0, maxChannels - 1)];
}

void oc_process(int numFrames, int numChannels)
{
    float* ptrs[maxChannels] = { frames[0], frames[1] };
    engine.process(ptrs, std::clamp(numChannels, 1, maxChannels), std::clamp(numFrames, 0, maxFrames));
}

int oc_is_passthrough() { return engine.isPassingThrough() ? 1 : 0; }

/** The net response, trim included, at n frequencies, for the curve display. */
void oc_curve(double sampleRate, double amount, const float* freqs, float* outDb, int n)
{
    const auto d = occluder::curve::design(sampleRate, amount);
    for (int i = 0; i < n; ++i)
        outDb[i] = (float) d.magnitudeDb(sampleRate, (double) freqs[i]);
}

/** The trim the design carries at this knob position, in dB. */
double oc_trim_db(double sampleRate, double amount)
{
    return occluder::curve::design(sampleRate, amount).trimDb;
}

} // extern "C"
