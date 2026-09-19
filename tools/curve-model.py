#!/usr/bin/env python3
"""
curve-model.py - where Occluder's curve comes from.

Builds the acoustic target for a fully plugged ear from published numbers,
then evaluates the constants shipped in Source/DSP/OcclusionCurve.h against
it and prints the net response (trim included) across the dial. Nothing here
is used by the build; it is the working shown. Needs numpy (scipy only for
--refit).

    tools/curve-model.py            the target, the shipped fit, the dial
    tools/curve-model.py --refit    re-run the least-squares fit that chose
                                    the constants

Sources
  Stenfelt & Reinfeldt, "A model of the occlusion effect with bone-conducted
  stimulation", Int J Audiol 46 (2007)              occlusion effect, shallow plug
  Porschmann, "Influences of bone conduction and air conduction on the sound
  of one's own voice", Acustica 86 (2000)           bone/air ratio of own voice
  Reinfeldt et al., "Hearing one's own voice during phoneme vocalization",
  J Acoust Soc Am 128 (2010)                        same, measured
  3M E-A-R Classic / 1100 attenuation data           foam plug attenuation
  Byrne et al., "An international comparison of long-term average speech
  spectra", J Acoust Soc Am 96 (1994)               LTASS, for the trim
"""
import argparse
import sys

import numpy as np

FS = 48000.0

# ------------------------------------------------------------------ the target
# Octave-band values, dB. Power ratios throughout.
F = np.array([100, 125, 250, 500, 1000, 2000, 4000, 8000], float)
# Bone-conducted own voice relative to air-conducted, open ear.
BC_REL = np.array([0, 0, 0, 0, 0, -5, -12, -20], float)
# Occlusion effect for a shallowly inserted foam plug.
OCCLUSION = np.array([18, 20, 24, 22, 12, 3, 0, 0], float)
# Mean attenuation of a foam plug (air path).
PLUG = np.array([-27, -28, -30, -33, -35, -34, -40, -44], float)


def db(x):
    return 10.0 * np.log10(x)


def lin(d):
    return 10.0 ** (d / 10.0)


def target():
    """Plugged own voice relative to open own voice, dB, at F."""
    ac = np.ones_like(F)
    bc = lin(BC_REL)
    open_ear = ac + bc
    plugged = ac * lin(PLUG) + bc * lin(OCCLUSION)
    return db(plugged / open_ear)


# ---------------------------------------------------------------- the filters
# RBJ cookbook, matching Source/DSP/Biquad.h.
def _norm(b, a):
    b = np.array(b, float) / a[0]
    a = np.array(a, float) / a[0]
    return b, a


def peak(f0, q, gain_db):
    A = 10 ** (gain_db / 40)
    w0 = 2 * np.pi * f0 / FS
    al = np.sin(w0) / (2 * q)
    c = np.cos(w0)
    return _norm([1 + al * A, -2 * c, 1 - al * A], [1 + al / A, -2 * c, 1 - al / A])


def high_shelf(f0, gain_db):
    A = 10 ** (gain_db / 40)
    w0 = 2 * np.pi * f0 / FS
    c = np.cos(w0)
    al = np.sin(w0) / 2 * np.sqrt(2.0)
    sq = 2 * np.sqrt(A) * al
    return _norm([A * ((A + 1) + (A - 1) * c + sq), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sq)],
                 [(A + 1) - (A - 1) * c + sq, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sq])


def low_shelf(f0, gain_db):
    A = 10 ** (gain_db / 40)
    w0 = 2 * np.pi * f0 / FS
    c = np.cos(w0)
    al = np.sin(w0) / 2 * np.sqrt(2.0)
    sq = 2 * np.sqrt(A) * al
    return _norm([A * ((A + 1) - (A - 1) * c + sq), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sq)],
                 [(A + 1) + (A - 1) * c + sq, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sq])


def mag_db(b, a, freqs):
    z = np.exp(-1j * 2 * np.pi * freqs / FS)
    return 20 * np.log10(np.abs((b[0] + b[1] * z + b[2] * z ** 2) / (a[0] + a[1] * z + a[2] * z ** 2)))


# The shipped constants: keep in step with Source/DSP/OcclusionCurve.h.
SHIPPED = dict(boom=(6.0, 320.0, 0.5), muffle=(-25.0, 1300.0), top=(-12.0, 5000.0), rumble=(-12.0, 70.0))


def cascade_db(freqs, amount=1.0, k=SHIPPED):
    g, f0, q = k["boom"]
    out = mag_db(*peak(f0, q, g * amount), freqs)
    g, f0 = k["muffle"]
    out += mag_db(*high_shelf(f0, g * amount), freqs)
    g, f0 = k["top"]
    out += mag_db(*high_shelf(f0, g * amount), freqs)
    g, f0 = k["rumble"]
    out += mag_db(*low_shelf(f0, g * amount), freqs)
    return out


# -------------------------------------------------------------------- the trim
LTASS_F = np.array([63, 80, 100, 125, 160, 200, 250, 315, 400, 500, 630, 800, 1000, 1250, 1600,
                    2000, 2500, 3150, 4000, 5000, 6300, 8000, 10000, 12500, 16000], float)
LTASS = np.array([38.6, 43.5, 54.4, 57.7, 56.8, 60.2, 60.3, 59.0, 62.1, 62.1, 60.5, 56.8, 53.7, 53.0, 52.0,
                  48.7, 48.1, 46.8, 45.6, 44.6, 44.9, 44.4, 42.4, 40.6, 35.9])


def trim_db(amount):
    w = lin(LTASS)
    w /= w.sum()
    return -db(np.sum(w * lin(cascade_db(LTASS_F, amount))))


# ------------------------------------------------------------------------ main
def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[1])
    ap.add_argument("--refit", action="store_true", help="re-run the least-squares fit")
    args = ap.parse_args()

    t = target()
    print("Target: plugged own voice relative to open, dB")
    for f, v in zip(F, t):
        print(f"  {f:6.0f} Hz  {v:+6.1f}")

    fit = cascade_db(F)
    off = np.mean(t - fit)   # the shape is what matters; level is the trim's job
    err = fit + off - t
    print(f"\nShipped constants vs target (shape, offset {off:+.1f} dB removed): rms {np.sqrt(np.mean(err ** 2)):.2f} dB")
    for f, v, e in zip(F, fit + off, err):
        print(f"  {f:6.0f} Hz  {v:+6.1f}  ({e:+.1f})")

    dense = np.array([30, 50, 100, 200, 300, 500, 1000, 2000, 4000, 8000, 16000], float)
    print("\nNet response with the trim, dB, across the dial")
    print("  amount   trim  " + "".join(f"{int(f):>7d}" for f in dense))
    for a in (0.1, 0.25, 0.5, 0.75, 1.0):
        tr = trim_db(a)
        print(f"  {a:6.2f} {tr:+6.2f}  " + "".join(f"{v:+7.1f}" for v in cascade_db(dense, a) + tr))

    if args.refit:
        from scipy.optimize import least_squares

        def resid(p):
            k = dict(boom=(p[0], p[1], 0.5), muffle=(p[3], p[2]), top=(p[5], p[4]), rumble=(-12.0, 70.0))
            return cascade_db(F, 1.0, k) + p[6] - t

        r = least_squares(resid, [13, 290, 1500, -18, 6000, -14, 0],
                          bounds=([0, 150, 600, -30, 2000, -40, -60], [30, 600, 4000, 0, 16000, 0, 60]))
        p = r.x
        print("\nRefit (peak Q and the rumble shelf fixed):")
        for n, v in zip(["boom gain", "boom f", "muffle f", "muffle gain", "top f", "top gain", "offset"], p):
            print(f"  {n:12s} {v:9.2f}")
        print(f"  rms {np.sqrt(np.mean(resid(p) ** 2)):.3f} dB")
    return 0


if __name__ == "__main__":
    sys.exit(main())
