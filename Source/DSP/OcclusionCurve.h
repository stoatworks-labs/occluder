/*
 * The curve. Everything audible about Occluder is decided in this file.
 *
 * -------------------------------------------------------------- the model
 *
 * Your own voice reaches your eardrum two ways: through the air (mouth -> air
 * -> ear canal, which is also what a microphone hears) and through bone (vocal
 * tract -> skull -> the walls of the ear canal). Blocking the canal changes
 * both at once:
 *
 *   - the plug attenuates the air path, and more at high frequencies than
 *     low (a foam plug is roughly -28 dB at 125 Hz and -44 dB at 8 kHz);
 *   - the bone path's low-frequency energy, which normally leaks out of the
 *     open canal, is trapped. That is the occlusion effect: +20..25 dB
 *     around 200..500 Hz, fading out by about 2 kHz.
 *
 * Summing the two paths for a plugged ear and dividing by the open-ear sum
 * gives the target: a broad hump peaking near 300 Hz, roughly flat at 1.5 kHz,
 * and 20 dB down by 8 kHz - about 40 dB of tilt. That is the "booming, muffled,
 * talking into a bucket" sound of your own voice with earplugs in.
 *
 * Sources: Stenfelt & Reinfeldt, "A model of the occlusion effect with
 * bone-conducted stimulation" (Int J Audiol 2007); Porschmann, "Influences of
 * bone conduction and air conduction on the sound of one's own voice"
 * (Acustica 2000); Dillon, Hearing Aids, ch. 5; 3M foam-plug attenuation data.
 * The fit, with the numbers, is docs/DESIGN.md.
 *
 * ------------------------------------------------------------ the filters
 *
 * Four second-order sections, each an RBJ shape that is EXACTLY unity at
 * 0 dB. That is the reason there is no low-pass here: a low-pass parked at
 * 18 kHz is flat to the ear but not to the sample (its b0 is about 0.5), so
 * bringing it in from a zeroed state clicked. A shelf at 0 dB is the identity
 * transfer function, so the knob's zero is a true pass-through and leaving it
 * is seamless.
 *
 *   boom     peak       +6 dB @ 320 Hz, Q 0.5      the occlusion hump
 *   muffle   high shelf -25 dB @ 1.3 kHz            the plug's tilt
 *   top      high shelf -12 dB @ 5 kHz              the last of the treble
 *   rumble   low shelf  -12 dB @ 70 Hz              keeps the hump off room
 *                                                   rumble and handling noise
 *
 * `amount` (0..1) scales every gain linearly in dB. The full-scale values are
 * the fit (0.1 dB rms against the model's eight points); the mid-dial values
 * are a design choice - "half plugged" is not a physical state, and halving
 * the decibels is what sounds like one plug in, or hands over the ears.
 *
 * ------------------------------------------------------------- the trim
 *
 * Tilting speech by 40 dB moves its level, so each design carries a trim that
 * holds the long-term speech-weighted level constant: the curve is evaluated
 * at the 25 third-octave bands of the Byrne et al. (1994) long-term average
 * speech spectrum, power-summed with those weights, and the result is taken
 * back off. The knob then changes only the shape. It is computed from the
 * same coefficients the audio runs through, so it cannot disagree with them.
 */
#pragma once

#include "Biquad.h"

namespace occluder
{

/** The four sections plus the trim, for one knob position at one sample rate. */
struct CurveDesign
{
    BiquadCoefficients boom, muffle, top, rumble;
    double trimDb = 0.0;
    double trimLinear = 1.0;
    // amount == 0: every section is the identity and the trim is 1.
    bool unity = true;

    /** The net response, trim included, in dB at one frequency. */
    double magnitudeDb(double sampleRate, double freq) const
    {
        if (unity)
            return 0.0;
        return boom.magnitudeDb(sampleRate, freq) + muffle.magnitudeDb(sampleRate, freq)
             + top.magnitudeDb(sampleRate, freq) + rumble.magnitudeDb(sampleRate, freq)
             + trimDb;
    }
};

namespace curve
{
    // Full-scale constants: what the knob does at 100%.
    inline constexpr double boomGainDb   = 6.0;
    inline constexpr double boomFreq     = 320.0;
    inline constexpr double boomQ        = 0.5;
    inline constexpr double muffleGainDb = -25.0;
    inline constexpr double muffleFreq   = 1300.0;
    inline constexpr double topGainDb    = -12.0;
    inline constexpr double topFreq      = 5000.0;
    inline constexpr double rumbleGainDb = -12.0;
    inline constexpr double rumbleFreq   = 70.0;

    /** Design the four sections and the trim for `amount` in 0..1. Allocation-free. */
    CurveDesign design(double sampleRate, double amount);

    /** The speech-weighted power gain of a design (before its trim), in dB.
        Exposed so the DSP check can verify that design() zeroes it. */
    double speechWeightedGainDb(double sampleRate, const CurveDesign&);
}

} // namespace occluder
