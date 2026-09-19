#include "OcclusionCurve.h"

#include <algorithm>
#include <cmath>

namespace occluder::curve
{

namespace
{
    // Long-term average speech spectrum, Byrne et al. 1994, male and female
    // combined, one-third-octave band levels in dB SPL. Only the shape matters
    // here: the levels become power weights that sum to one.
    struct Band { double freq, levelDb; };
    constexpr Band ltass[] = {
        {    63.0, 38.6 }, {    80.0, 43.5 }, {   100.0, 54.4 }, {   125.0, 57.7 },
        {   160.0, 56.8 }, {   200.0, 60.2 }, {   250.0, 60.3 }, {   315.0, 59.0 },
        {   400.0, 62.1 }, {   500.0, 62.1 }, {   630.0, 60.5 }, {   800.0, 56.8 },
        {  1000.0, 53.7 }, {  1250.0, 53.0 }, {  1600.0, 52.0 }, {  2000.0, 48.7 },
        {  2500.0, 48.1 }, {  3150.0, 46.8 }, {  4000.0, 45.6 }, {  5000.0, 44.6 },
        {  6300.0, 44.9 }, {  8000.0, 44.4 }, { 10000.0, 42.4 }, { 12500.0, 40.6 },
        { 16000.0, 35.9 },
    };

}

double speechWeightedGainDb(double sampleRate, const CurveDesign& d)
{
    double weightSum = 0.0, powerSum = 0.0;
    for (const auto& band : ltass)
    {
        // Bands above Nyquist do not exist at this sample rate; leave them out
        // rather than evaluating a response that is not there.
        if (band.freq >= 0.5 * sampleRate)
            continue;
        const double w = std::pow(10.0, band.levelDb / 10.0);
        const double gainDb = d.boom.magnitudeDb(sampleRate, band.freq)
                            + d.muffle.magnitudeDb(sampleRate, band.freq)
                            + d.top.magnitudeDb(sampleRate, band.freq)
                            + d.rumble.magnitudeDb(sampleRate, band.freq);
        weightSum += w;
        powerSum += w * std::pow(10.0, gainDb / 10.0);
    }
    return 10.0 * std::log10(powerSum / weightSum);
}

CurveDesign design(double sampleRate, double amount)
{
    CurveDesign d;
    amount = std::clamp(amount, 0.0, 1.0);
    if (amount <= 0.0)
        return d;   // identity sections, unity == true

    // Every corner is well inside Nyquist at any rate a host will ask for, but
    // the bilinear transform folds a corner past it, so keep them below 0.45 fs.
    const auto corner = [sampleRate](double freq) { return std::min(freq, 0.45 * sampleRate); };

    d.unity  = false;
    d.boom   = BiquadCoefficients::peak(sampleRate, corner(boomFreq), boomQ, boomGainDb * amount);
    d.muffle = BiquadCoefficients::highShelf(sampleRate, corner(muffleFreq), muffleGainDb * amount);
    d.top    = BiquadCoefficients::highShelf(sampleRate, corner(topFreq), topGainDb * amount);
    d.rumble = BiquadCoefficients::lowShelf(sampleRate, corner(rumbleFreq), rumbleGainDb * amount);

    d.trimDb = -speechWeightedGainDb(sampleRate, d);
    d.trimLinear = std::pow(10.0, d.trimDb / 20.0);
    return d;
}

} // namespace occluder::curve
