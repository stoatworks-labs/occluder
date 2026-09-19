# Occluder

> **AI-assisted project.** This codebase was created with [Claude](https://claude.com/claude-code)
> (Anthropic), directed and reviewed by a human author. The curve is derived from the
> published acoustics of the occlusion effect and checked numerically against that model;
> the shipped processor is verified by a throwaway-free test harness (real signals through
> the real plugin class: response, level, click-free sweeps, zero allocations); `pluginval`
> passes at strictness 10 on VST3 and AU and Apple's `auval` passes. It has **not** been
> used on a show yet. Listen before you trust it.

A one-knob EQ that makes a recorded voice sound the way you hear your own — from open
ears to foam earplugs in. VST3 / AU / standalone, built with JUCE.

![Occluder: the response curve at 60 % above one large Occlusion knob, labelled open at one
end and plugged at the other](docs/screenshots/plugin.png)

*Rendered from the real editor by `tools/ocshot.cpp`, at 60 %. The curve is the exact
response the audio is getting, trim included.*

<!-- downloads:start -->

## Download

Not released yet. Build it (below), or watch this space.

<!-- downloads:end -->

## What it does

When you talk, you hear yourself through the air and through your skull. Block your ears
and the skull path is trapped in the ear canal while the air path is shut out: your voice
goes boomy and muffled at once. That is the **occlusion effect** — the reason your own
voice booms with a finger in each ear, and the thing hearing-aid fitters spend their
lives minimising.

Occluder does that to a recording. **0** passes the audio through untouched. **100** is a
pair of foam plugs: the low mids stay, the consonants are 30 dB down, nothing above
4 kHz survives. In between is one plug, or hands over the ears. A touch of it is roughly
the difference between a recording of you and you.

Each setting is trimmed so the long-term level of speech does not move — the knob changes
the shape, not the volume — and the trim is shown.

The shape is not a low-pass with a name. It is a fit to the plugged ear: bone-conducted
own voice trapped in the canal (+20–25 dB around 200–500 Hz, Stenfelt & Reinfeldt 2007)
plus a foam plug's attenuation of the air path (−28 dB at 125 Hz to −44 dB at 8 kHz),
divided by the open ear. [docs/DESIGN.md](docs/DESIGN.md) shows the working;
[`tools/curve-model.py`](tools/curve-model.py) reproduces the numbers.

## Goals

- **One knob.** Everything else is decided. If you need to move the hump, use an EQ.
- **Zero is zero.** At 0 the output is the input, bit for bit, and leaving 0 is seamless.
- **No latency, no allocation.** Four second-order sections in double precision, redesigned
  in place while the knob moves; nothing on the audio thread allocates or locks.
- **The display tells the truth.** The curve drawn is computed from the same coefficients
  the audio runs through, at the session's sample rate, trim included.
- **Click-free automation.** The knob is ramped over 50 ms and the filters step every 32
  samples; a slam from 0 to 100 is measured, not assumed, to make no step larger than the
  signal's own.

## Building

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

JUCE 8.0.6 is fetched at configure time. On macOS a local build copies the VST3 and AU into
`/Library/Audio/Plug-Ins/` (not `~/Library`, which some hosts never scan); on CI it copies
nothing. Bundles land under `build/Occluder_artefacts/Release/`.

## Verifying

```bash
./build/ocdsp_artefacts/Release/ocdsp       # or: ctest --test-dir build
```

No host, no window. It pushes real signal through the shipped `OccluderAudioProcessor` and
checks: bit-exact pass-through at 0; the measured response against the design's stated
magnitude at ten frequencies, three knob positions and three sample rates (within 0.01 dB);
the full-scale shape against the acoustic model's eight points; speech-shaped level within
0.01 dB across the dial; a 0→100 slam ramped and click-free; zero allocations over a hundred
blocks with the knob moving; state round-trip; mono, stereo and 5.1.

Then the hosts' own checks:

```bash
/Applications/pluginval.app/Contents/MacOS/pluginval --strictness-level 10 --validate "/Library/Audio/Plug-Ins/VST3/Occluder.vst3"
auval -v aufx Occl Alsg
```

`tools/ocshot.cpp` renders the editor offscreen for the screenshots above, so they are the
plugin rather than a picture of it. `tools/ocrender.cpp` runs a file through the shipped
processor at a knob position, for listening and A/B sets without a host:

```bash
./build/ocrender_artefacts/Release/ocrender voice.wav plugged.wav 100
```

## Windows SmartScreen

Released macOS builds are Developer ID-signed and notarised. Windows builds are not
code-signed: plugin files load normally, but the **installer** trips SmartScreen — **More
info → Run anyway**. Linux has no signing gate.

## Status

- **DSP:** verified as above. The curve's constants live in one place,
  `Source/DSP/OcclusionCurve.h`, and the check fails if they drift from the documented fit.
- **Hosts:** `pluginval` strictness 10 passes on VST3 and AU; `auval` passes. The AU build
  logs one benign `pluginval` warning, *"Current program is -1"* (the JUCE AU wrapper reports
  no program until a host picks one).
- **Listened to** on this machine only. Not yet used in anger.
- **Standalone:** builds and runs; asks for the microphone the first time, because it is for
  talking into.

<!-- attributions:start -->
This project is built on other people's work — see [ATTRIBUTIONS.md](ATTRIBUTIONS.md).

**Licensing:** the source here is MIT, but the released binaries link JUCE 8 and are conveyed under the **AGPLv3** — see [ATTRIBUTIONS.md](ATTRIBUTIONS.md) before redistributing them.
<!-- attributions:end -->

## Licence

MIT. See [LICENSE](LICENSE). The released binaries are combined works with JUCE and are
conveyed under the AGPLv3 — [ATTRIBUTIONS.md](ATTRIBUTIONS.md) explains what that does and
does not change.
