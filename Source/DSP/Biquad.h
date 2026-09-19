/*
 * A biquad that owns nothing.
 *
 * juce::dsp::IIR::Coefficients hands back a reference-counted object, so
 * designing a filter with it allocates - and this plugin redesigns its four
 * filters every time the knob moves, on the audio thread. These are the RBJ
 * cookbook forms written straight into a struct, with the design and the
 * running state kept apart so one design can drive every channel. Only the
 * three shapes the curve uses are here: at 0 dB each of them is exactly
 * unity, which is what lets the knob's zero be a true pass-through.
 *
 * Double throughout. The rumble shelf sits at 70 Hz, which at 96 kHz puts its
 * poles close enough to 1 that a float transposed-direct-form-II biquad does
 * not hold its shape.
 */
#pragma once

#include <cmath>

namespace occluder
{

struct BiquadCoefficients
{
    // Normalised so a0 == 1.
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;

    static BiquadCoefficients identity() { return {}; }

    /** RBJ peaking EQ. */
    static BiquadCoefficients peak(double sampleRate, double freq, double q, double gainDb)
    {
        const double A = std::pow(10.0, gainDb / 40.0);
        const double w0 = 2.0 * pi * freq / sampleRate;
        const double alpha = std::sin(w0) / (2.0 * q);
        const double c = std::cos(w0);
        return normalise(1.0 + alpha * A, -2.0 * c, 1.0 - alpha * A,
                         1.0 + alpha / A, -2.0 * c, 1.0 - alpha / A);
    }

    /** RBJ high shelf, shelf slope S = 1 (the gentlest monotonic shelf). */
    static BiquadCoefficients highShelf(double sampleRate, double freq, double gainDb)
    {
        const double A = std::pow(10.0, gainDb / 40.0);
        const double w0 = 2.0 * pi * freq / sampleRate;
        const double c = std::cos(w0);
        // alpha = sin(w0)/2 * sqrt((A + 1/A)(1/S - 1) + 2), which for S = 1 is just sqrt(2).
        const double alpha = std::sin(w0) / 2.0 * std::sqrt(2.0);
        const double sq = 2.0 * std::sqrt(A) * alpha;
        return normalise(A * ((A + 1.0) + (A - 1.0) * c + sq),
                         -2.0 * A * ((A - 1.0) + (A + 1.0) * c),
                         A * ((A + 1.0) + (A - 1.0) * c - sq),
                         (A + 1.0) - (A - 1.0) * c + sq,
                         2.0 * ((A - 1.0) - (A + 1.0) * c),
                         (A + 1.0) - (A - 1.0) * c - sq);
    }

    /** RBJ low shelf, shelf slope S = 1. */
    static BiquadCoefficients lowShelf(double sampleRate, double freq, double gainDb)
    {
        const double A = std::pow(10.0, gainDb / 40.0);
        const double w0 = 2.0 * pi * freq / sampleRate;
        const double c = std::cos(w0);
        const double alpha = std::sin(w0) / 2.0 * std::sqrt(2.0);
        const double sq = 2.0 * std::sqrt(A) * alpha;
        return normalise(A * ((A + 1.0) - (A - 1.0) * c + sq),
                         2.0 * A * ((A - 1.0) - (A + 1.0) * c),
                         A * ((A + 1.0) - (A - 1.0) * c - sq),
                         (A + 1.0) + (A - 1.0) * c + sq,
                         -2.0 * ((A - 1.0) + (A + 1.0) * c),
                         (A + 1.0) + (A - 1.0) * c - sq);
    }

    /** |H| in dB at one frequency. Used by the curve display and by the loudness trim. */
    double magnitudeDb(double sampleRate, double freq) const
    {
        const double w = 2.0 * pi * freq / sampleRate;
        const double c1 = std::cos(w), s1 = std::sin(w);
        const double c2 = std::cos(2.0 * w), s2 = std::sin(2.0 * w);
        // H(z) with z^-1 = e^{-jw}: (b0 + b1 z^-1 + b2 z^-2) / (1 + a1 z^-1 + a2 z^-2)
        const double nr = b0 + b1 * c1 + b2 * c2, ni = -(b1 * s1 + b2 * s2);
        const double dr = 1.0 + a1 * c1 + a2 * c2, di = -(a1 * s1 + a2 * s2);
        const double num = nr * nr + ni * ni, den = dr * dr + di * di;
        return 10.0 * std::log10(num / den);
    }

private:
    static constexpr double pi = 3.14159265358979323846;

    static BiquadCoefficients normalise(double b0, double b1, double b2, double a0, double a1, double a2)
    {
        BiquadCoefficients k;
        k.b0 = b0 / a0; k.b1 = b1 / a0; k.b2 = b2 / a0;
        k.a1 = a1 / a0; k.a2 = a2 / a0;
        return k;
    }
};

/** The running state of one biquad on one channel. Transposed direct form II. */
struct BiquadState
{
    double s1 = 0.0, s2 = 0.0;

    void reset() { s1 = s2 = 0.0; }

    inline double process(const BiquadCoefficients& k, double x)
    {
        const double y = k.b0 * x + s1;
        s1 = k.b1 * x - k.a1 * y + s2;
        s2 = k.b2 * x - k.a2 * y;
        return y;
    }
};

} // namespace occluder
