/*
 * ocdsp - the DSP checks, run against the shipped OccluderAudioProcessor.
 *
 * No host, no audio device, no window. Everything here pushes real signal
 * through the real processor and measures what comes out. The stimuli are
 * chosen so that right and wrong are distinguishable: a filter is checked at
 * ten frequencies across the band rather than at one, and the level check uses
 * a speech-shaped spectrum with energy between the bands the trim is defined
 * at, not on them.
 *
 *   1. amount 0 is bit-exact pass-through
 *   2. the measured response matches the design's stated magnitude, at three
 *      knob positions and three sample rates
 *   3. the full-scale curve is the fit docs/DESIGN.md describes (guards the
 *      constants against a quiet edit)
 *   4. speech-weighted level holds across the dial
 *   5. a knob sweep is ramped and does not click
 *   6. nothing on the processing path allocates
 *   7. state survives a save/restore round trip
 *   8. mono, stereo and 5.1 all process, every channel identically
 *
 * Exit status is the number of failures.
 */

#include <JuceHeader.h>

#include <atomic>
#include <cmath>
#include <cstdio>
#include <new>
#include <random>
#include <vector>

#include "Source/PluginProcessor.h"

// ---------------------------------------------------------------- allocation counting
static std::atomic<long> g_allocations { 0 };

void* operator new(std::size_t n)
{
    g_allocations.fetch_add(1, std::memory_order_relaxed);
    if (void* p = std::malloc(n ? n : 1))
        return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t n)
{
    g_allocations.fetch_add(1, std::memory_order_relaxed);
    if (void* p = std::malloc(n ? n : 1))
        return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

// ------------------------------------------------------------------------ helpers
static int g_failures = 0;

static void check(bool ok, const char* what, const juce::String& detail = {})
{
    std::printf("  [%s] %s%s%s\n", ok ? "ok" : "FAIL", what,
                detail.isEmpty() ? "" : " - ", detail.toRawUTF8());
    if (! ok)
        ++g_failures;
}

static double toDb(double linear) { return 20.0 * std::log10(std::max(linear, 1e-12)); }

struct Rig
{
    OccluderAudioProcessor proc;
    juce::MidiBuffer midi;
    double fs;
    int block;

    Rig(double sampleRate, int blockSize, int channels = 2)
        : fs(sampleRate), block(blockSize)
    {
        const auto set = channels == 1 ? juce::AudioChannelSet::mono()
                       : channels == 6 ? juce::AudioChannelSet::create5point1()
                                       : juce::AudioChannelSet::stereo();
        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses.add(set);
        layout.outputBuses.add(set);
        proc.setBusesLayout(layout);
        proc.setRateAndBufferSizeDetails(sampleRate, blockSize);
        proc.prepareToPlay(sampleRate, blockSize);
    }

    void setAmount(double amount01)
    {
        auto* p = proc.apvts.getParameter(occluder::ParamIDs::amount);
        p->setValueNotifyingHost((float) amount01);
    }

    void run(juce::AudioBuffer<float>& audio)
    {
        proc.processBlock(audio, midi);
    }

    /** Run `seconds` of silence so a knob change has finished ramping. */
    void settle(double seconds = 0.1)
    {
        juce::AudioBuffer<float> silence(proc.getTotalNumOutputChannels(), block);
        for (long n = 0; n < (long) (seconds * fs); n += block)
        {
            silence.clear();
            run(silence);
        }
    }

    /** Process `seconds` of a tone and return the output RMS over the last half. */
    double toneGainDb(double freq, double seconds = 1.0)
    {
        const int total = (int) (seconds * fs);
        juce::AudioBuffer<float> buf(proc.getTotalNumOutputChannels(), block);
        double sumSq = 0.0;
        long count = 0;
        long n = 0;
        while (n < total)
        {
            for (int ch = 0; ch < buf.getNumChannels(); ++ch)
                for (int i = 0; i < block; ++i)
                    buf.setSample(ch, i, (float) (0.25 * std::sin(2.0 * juce::MathConstants<double>::pi * freq * (double) (n + i) / fs)));
            run(buf);
            if (n >= total / 2)
            {
                for (int i = 0; i < block; ++i)
                {
                    const double v = buf.getSample(0, i);
                    sumSq += v * v;
                    ++count;
                }
            }
            n += block;
        }
        const double rmsOut = std::sqrt(sumSq / (double) count);
        const double rmsIn = 0.25 / std::sqrt(2.0);
        return toDb(rmsOut / rmsIn);
    }
};

// ------------------------------------------------------------------------- tests
static void testPassThrough()
{
    std::printf("1. pass-through at 0\n");
    Rig rig(48000.0, 256);
    // The default is 50 %, so this is a real ramp down to zero, then the
    // identity sections flushing their state, then pass-through.
    rig.setAmount(0.0);
    rig.settle();
    check(rig.proc.isPassingThrough(), "the engine reports pass-through once the ramp to 0 has settled");
    std::mt19937 rng(1);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    juce::AudioBuffer<float> in(2, 256), out(2, 256);
    bool identical = true;
    for (int b = 0; b < 200 && identical; ++b)
    {
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 256; ++i)
                in.setSample(ch, i, dist(rng));
        out.makeCopyOf(in);
        rig.run(out);
        for (int ch = 0; ch < 2 && identical; ++ch)
            for (int i = 0; i < 256; ++i)
                if (std::memcmp(&in.getReadPointer(ch)[i], &out.getReadPointer(ch)[i], sizeof(float)) != 0)
                { identical = false; break; }
    }
    check(identical, "200 blocks of noise come out bit-identical at amount 0");
}

static void testResponseMatchesDesign()
{
    std::printf("2. measured response vs design\n");
    const double freqs[] = { 50, 100, 200, 300, 500, 1000, 2000, 4000, 8000, 12000 };
    for (double fs : { 44100.0, 48000.0, 96000.0 })
    {
        for (double amount : { 0.25, 0.5, 1.0 })
        {
            Rig rig(fs, 512);
            rig.setAmount(amount);
            const auto design = occluder::curve::design(fs, amount);
            double worst = 0.0;
            juce::String worstAt;
            for (double f : freqs)
            {
                const double measured = rig.toneGainDb(f, 1.0);
                const double expected = design.magnitudeDb(fs, f);
                const double err = std::abs(measured - expected);
                if (err > worst) { worst = err; worstAt = juce::String(f) + " Hz (" + juce::String(measured, 2) + " vs " + juce::String(expected, 2) + ")"; }
            }
            check(worst < 0.1, juce::String("fs " + juce::String(fs / 1000.0, 1) + "k amount " + juce::String(amount, 2)).toRawUTF8(),
                  "worst " + juce::String(worst, 3) + " dB at " + worstAt);
        }
    }
}

static void testFullScaleCurveIsTheFit()
{
    std::printf("3. full-scale curve is the documented fit\n");
    // docs/DESIGN.md, the acoustic target at amount 1 relative to the open ear,
    // with the fit's constant offset removed and the trim added back so it can be
    // compared with the design's net response. The tolerance is the fit's own
    // error plus a little: the point is to notice a changed constant, not to
    // re-derive the physics on every build.
    struct Pt { double f, targetDb; };
    const Pt target[] = { { 100, 15.0 }, { 125, 17.0 }, { 250, 21.0 }, { 500, 19.0 },
                          { 1000, 9.0 }, { 2000, -3.2 }, { 4000, -12.3 }, { 8000, -20.0 } };
    const double fs = 48000.0;
    const auto d = occluder::curve::design(fs, 1.0);
    // Shape only: compare after removing each side's mean.
    double meanT = 0.0, meanD = 0.0;
    for (const auto& p : target) { meanT += p.targetDb; meanD += d.magnitudeDb(fs, p.f); }
    meanT /= 8.0; meanD /= 8.0;
    double worst = 0.0;
    for (const auto& p : target)
        worst = std::max(worst, std::abs((d.magnitudeDb(fs, p.f) - meanD) - (p.targetDb - meanT)));
    check(worst < 1.5, "shape within 1.5 dB of the acoustic target at the eight model points",
          "worst " + juce::String(worst, 2) + " dB");

    const double weighted = occluder::curve::speechWeightedGainDb(fs, d) + d.trimDb;
    check(std::abs(weighted) < 1e-9, "trim zeroes the speech-weighted gain exactly",
          juce::String(weighted, 12) + " dB");
    check(d.trimDb < -2.0 && d.trimDb > -5.0, "full-scale trim is in the expected range",
          juce::String(d.trimDb, 2) + " dB");
}

static void testLevelHoldsAcrossTheDial()
{
    std::printf("4. speech-weighted level across the dial\n");
    // A speech-shaped stimulus: tones every 1/12 octave from 63 Hz to 16 kHz with
    // levels interpolated from the LTASS, random phases. Most of its energy sits
    // BETWEEN the third-octave centres the trim is computed at, so this measures
    // whether the compensation holds for speech-like material rather than
    // restating its own definition.
    const double fs = 48000.0;
    const int block = 512;
    struct Band { double f, db; };
    const Band ltass[] = { { 63, 38.6 }, { 80, 43.5 }, { 100, 54.4 }, { 125, 57.7 }, { 160, 56.8 }, { 200, 60.2 },
                           { 250, 60.3 }, { 315, 59.0 }, { 400, 62.1 }, { 500, 62.1 }, { 630, 60.5 }, { 800, 56.8 },
                           { 1000, 53.7 }, { 1250, 53.0 }, { 1600, 52.0 }, { 2000, 48.7 }, { 2500, 48.1 }, { 3150, 46.8 },
                           { 4000, 45.6 }, { 5000, 44.6 }, { 6300, 44.9 }, { 8000, 44.4 }, { 10000, 42.4 }, { 12500, 40.6 },
                           { 16000, 35.9 } };
    struct Tone { double f, amp, phase; };
    std::vector<Tone> tones;
    std::mt19937 rng(7);
    std::uniform_real_distribution<double> ph(0.0, 2.0 * juce::MathConstants<double>::pi);
    for (double f = 63.0; f <= 16000.0; f *= std::pow(2.0, 1.0 / 12.0))
    {
        // Interpolate the LTASS level in log-frequency.
        double db = ltass[0].db;
        for (size_t i = 0; i + 1 < std::size(ltass); ++i)
            if (f >= ltass[i].f && f <= ltass[i + 1].f)
            {
                const double t = std::log(f / ltass[i].f) / std::log(ltass[i + 1].f / ltass[i].f);
                db = ltass[i].db + t * (ltass[i + 1].db - ltass[i].db);
            }
        // Third-octave band level -> per-tone level at 4 tones per band.
        tones.push_back({ f, std::pow(10.0, (db - 70.0) / 20.0) / 2.0, ph(rng) });
    }

    auto rmsFor = [&](double amount)
    {
        Rig rig(fs, block);
        rig.setAmount(amount);
        juce::AudioBuffer<float> buf(2, block);
        const int total = (int) fs;   // one second, measure the second half
        double sumSq = 0.0; long count = 0;
        for (int n = 0; n < total; n += block)
        {
            for (int i = 0; i < block; ++i)
            {
                double v = 0.0;
                for (const auto& t : tones)
                    v += t.amp * std::sin(2.0 * juce::MathConstants<double>::pi * t.f * (double) (n + i) / fs + t.phase);
                buf.setSample(0, i, (float) v);
                buf.setSample(1, i, (float) v);
            }
            rig.run(buf);
            if (n >= total / 2)
                for (int i = 0; i < block; ++i) { const double v = buf.getSample(0, i); sumSq += v * v; ++count; }
        }
        return std::sqrt(sumSq / (double) count);
    };

    const double reference = rmsFor(0.0);
    double worst = 0.0; juce::String worstAt;
    for (double amount : { 0.1, 0.25, 0.5, 0.75, 1.0 })
    {
        const double delta = toDb(rmsFor(amount) / reference);
        std::printf("     amount %.2f: %+.2f dB\n", amount, delta);
        if (std::abs(delta) > worst) { worst = std::abs(delta); worstAt = juce::String(amount, 2); }
    }
    check(worst < 0.75, "speech-shaped level within 0.75 dB of unity across the dial",
          "worst " + juce::String(worst, 2) + " dB at amount " + worstAt);
}

static void testSweepIsRampedAndClickFree()
{
    std::printf("5. knob sweep\n");
    const double fs = 48000.0;
    const int block = 256;
    // 1 kHz: the net change from 0 to 1 there is about -14 dB, big enough for
    // the ramp to be measurable, at a frequency where a click stands out
    // against the tone's own sample-to-sample movement.
    const double f = 1000.0;
    Rig rig(fs, block);
    rig.setAmount(0.0);
    juce::AudioBuffer<float> buf(2, block);

    auto fill = [&](long n)
    {
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < block; ++i)
                buf.setSample(ch, i, (float) (0.25 * std::sin(2.0 * juce::MathConstants<double>::pi * f * (double) (n + i) / fs)));
    };

    // Settle at 0, then slam the knob to 1 and watch the output envelope.
    long n = 0;
    for (; n < 24000; n += block) { fill(n); rig.run(buf); }

    rig.setAmount(1.0);
    double maxDelta = 0.0, prev = 0.0;
    std::vector<double> peakPerBlock;
    for (int b = 0; b < 40; ++b, n += block)
    {
        fill(n);
        rig.run(buf);
        double peak = 0.0;
        for (int i = 0; i < block; ++i)
        {
            const double v = buf.getSample(0, i);
            if (b > 0 || i > 0) maxDelta = std::max(maxDelta, std::abs(v - prev));
            prev = v;
            peak = std::max(peak, std::abs(v));
        }
        peakPerBlock.push_back(peak);
    }
    const auto d = occluder::curve::design(fs, 1.0);
    const double finalAmp = 0.25 * std::pow(10.0, d.magnitudeDb(fs, f) / 20.0);
    const double steadyDelta = 0.25 * 2.0 * juce::MathConstants<double>::pi * f / fs;

    check(finalAmp < 0.6 * 0.25, "the net change at 1 kHz is large enough to measure a ramp",
          "final " + juce::String(finalAmp, 4) + " from 0.25");
    // Two blocks in (10.7 ms of a 50 ms ramp) the level has started to move but
    // is nowhere near the end. Anything unramped is at finalAmp already.
    check(peakPerBlock[1] > 0.55 * 0.25 && peakPerBlock[1] < 0.98 * 0.25,
          "two blocks after the jump the level is part way down the ramp",
          "peak " + juce::String(peakPerBlock[1], 4) + " (start 0.25, final " + juce::String(finalAmp, 4) + ")");
    check(std::abs(peakPerBlock.back() - finalAmp) < 0.02 * finalAmp,
          "settled at the design's amplitude within 40 blocks",
          juce::String(peakPerBlock.back(), 4) + " vs " + juce::String(finalAmp, 4));
    check(maxDelta < 1.5 * steadyDelta,
          "largest sample-to-sample step during the sweep is within 1.5x the tone's own",
          juce::String(maxDelta, 5) + " vs steady " + juce::String(steadyDelta, 5));
}

static void testNoAllocations()
{
    std::printf("6. allocations on the processing path\n");
    Rig rig(48000.0, 512);
    juce::AudioBuffer<float> buf(2, 512);
    buf.clear();
    rig.setAmount(0.3);
    rig.run(buf);   // first block after a parameter change, outside the count

    const long before = g_allocations.load();
    for (int b = 0; b < 100; ++b)
    {
        // Move the knob every block so the redesign path is exercised too.
        rig.setAmount(0.2 + 0.6 * (double) (b % 10) / 10.0);
        rig.run(buf);
    }
    const long after = g_allocations.load();
    check(after == before, "100 blocks with the knob moving: zero allocations", juce::String(after - before) + " allocations");
}

static void testStateRoundTrip()
{
    std::printf("7. state round trip\n");
    Rig a(48000.0, 256);
    a.setAmount(0.735);
    juce::MemoryBlock state;
    a.proc.getStateInformation(state);

    Rig b(48000.0, 256);
    b.proc.setStateInformation(state.getData(), (int) state.getSize());
    check(std::abs(b.proc.getAmount() - 0.735) < 1e-4, "73.5 % survives save and restore",
          juce::String(b.proc.getAmount() * 100.0, 2) + " %");
}

static void testLayouts()
{
    std::printf("8. channel layouts\n");
    for (int channels : { 1, 2, 6 })
    {
        Rig rig(48000.0, 256, channels);
        check(rig.proc.getTotalNumOutputChannels() == channels,
              juce::String(juce::String(channels) + " channel layout accepted").toRawUTF8(),
              "got " + juce::String(rig.proc.getTotalNumOutputChannels()));
        rig.setAmount(0.8);
        juce::AudioBuffer<float> buf(channels, 256);
        std::mt19937 rng(3);
        std::uniform_real_distribution<float> dist(-0.5f, 0.5f);
        bool identical = true;
        for (int b = 0; b < 50; ++b)
        {
            for (int i = 0; i < 256; ++i)
            {
                const float v = dist(rng);
                for (int ch = 0; ch < channels; ++ch) buf.setSample(ch, i, v);
            }
            rig.run(buf);
            for (int ch = 1; ch < channels; ++ch)
                for (int i = 0; i < 256; ++i)
                    if (! juce::exactlyEqual(buf.getSample(ch, i), buf.getSample(0, i))) identical = false;
        }
        check(identical, "every channel is processed identically");
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    std::printf("ocdsp - Occluder DSP checks\n");
    testPassThrough();
    testResponseMatchesDesign();
    testFullScaleCurveIsTheFit();
    testLevelHoldsAcrossTheDial();
    testSweepIsRampedAndClickFree();
    testNoAllocations();
    testStateRoundTrip();
    testLayouts();
    std::printf("%s: %d failure(s)\n", g_failures == 0 ? "PASS" : "FAIL", g_failures);
    return g_failures;
}
