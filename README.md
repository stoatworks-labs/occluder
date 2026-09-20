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

**Video:** [What it does, in 49 seconds](https://www.youtube.com/watch?v=g5Z5TZsy74k) —
everything you hear went through the plugin.

[![Occluder: the editor at 100 % beside the name and the line "one knob from open ears to
earplugs"](docs/video-thumb.png)](https://www.youtube.com/watch?v=g5Z5TZsy74k)

**Try it in the browser:** [occluder-demo.stoatworks-labs.com](https://occluder-demo.stoatworks-labs.com/) —
the plugin's own DSP compiled to WebAssembly, on your microphone or any audio file. Nothing
you play or say leaves your browser. See [`demo/`](demo/README.md).

![Occluder: the response curve at 60 % above one large Occlusion knob, labelled open at one
end and plugged at the other](docs/screenshots/plugin.png)

*Rendered from the real editor by `tools/ocshot.cpp`, at 60 %. The curve is the exact
response the audio is getting, trim included.*

<!-- downloads:start -->

## Download

**[v0.1.0](https://github.com/stoatworks-labs/occluder/releases/tag/v0.1.0)** — prebuilt for macOS, Windows and Linux. Pick your platform:

<details>
<summary><b>macOS</b> — Universal (Apple Silicon + Intel)</summary>

| Build | Download | Size |
| --- | --- | --- |
| Universal (Apple Silicon + Intel) · .dmg disk image | [`occluder-0.1.0-macos-universal.dmg`](https://github.com/stoatworks-labs/occluder/releases/download/v0.1.0/occluder-0.1.0-macos-universal.dmg) | 9.7 MB |
| Universal (Apple Silicon + Intel) · .pkg installer | [`occluder-0.1.0-macos-universal.pkg`](https://github.com/stoatworks-labs/occluder/releases/download/v0.1.0/occluder-0.1.0-macos-universal.pkg) | 9.7 MB |
| Universal (Apple Silicon + Intel) · .zip archive | [`occluder-macos-universal.zip`](https://github.com/stoatworks-labs/occluder/releases/latest/download/occluder-macos-universal.zip) | 9.7 MB |

</details>

<details>
<summary><b>Windows</b> — x64, ARM64</summary>

| Build | Download | Size |
| --- | --- | --- |
| x64 · .exe installer | [`occluder-0.1.0-windows-x86_64-setup.exe`](https://github.com/stoatworks-labs/occluder/releases/download/v0.1.0/occluder-0.1.0-windows-x86_64-setup.exe) | 2.8 MB |
| ARM64 · .exe installer | [`occluder-0.1.0-windows-aarch64-setup.exe`](https://github.com/stoatworks-labs/occluder/releases/download/v0.1.0/occluder-0.1.0-windows-aarch64-setup.exe) | 2.5 MB |
| x64 · .zip archive | [`occluder-windows-x86_64.zip`](https://github.com/stoatworks-labs/occluder/releases/latest/download/occluder-windows-x86_64.zip) | 5.0 MB |
| ARM64 · .zip archive | [`occluder-windows-aarch64.zip`](https://github.com/stoatworks-labs/occluder/releases/latest/download/occluder-windows-aarch64.zip) | 4.9 MB |

</details>

<details>
<summary><b>Linux</b> — x64, ARM64</summary>

| Build | Download | Size |
| --- | --- | --- |
| x64 · .deb package (Debian/Ubuntu) | [`occluder_0.1.0_amd64.deb`](https://github.com/stoatworks-labs/occluder/releases/download/v0.1.0/occluder_0.1.0_amd64.deb) | 2.1 MB |
| ARM64 · .deb package (Debian/Ubuntu) | [`occluder_0.1.0_arm64.deb`](https://github.com/stoatworks-labs/occluder/releases/download/v0.1.0/occluder_0.1.0_arm64.deb) | 2.1 MB |
| x64 · .rpm package (Fedora/RHEL) | [`occluder-0.1.0-1.x86_64.rpm`](https://github.com/stoatworks-labs/occluder/releases/download/v0.1.0/occluder-0.1.0-1.x86_64.rpm) | 2.2 MB |
| ARM64 · .rpm package (Fedora/RHEL) | [`occluder-0.1.0-1.aarch64.rpm`](https://github.com/stoatworks-labs/occluder/releases/download/v0.1.0/occluder-0.1.0-1.aarch64.rpm) | 2.2 MB |
| x64 · .zip archive | [`occluder-linux-x86_64.zip`](https://github.com/stoatworks-labs/occluder/releases/latest/download/occluder-linux-x86_64.zip) | 4.2 MB |
| ARM64 · .zip archive | [`occluder-linux-aarch64.zip`](https://github.com/stoatworks-labs/occluder/releases/latest/download/occluder-linux-aarch64.zip) | 4.2 MB |

</details>

All builds, checksums and release notes: [github.com/stoatworks-labs/occluder/releases](https://github.com/stoatworks-labs/occluder/releases).

macOS builds are signed and notarised and open normally. The Windows builds are unsigned, so SmartScreen warns once — see [Windows SmartScreen](#windows-smartscreen) for the one-time click-through.

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

`tools/ocfilm.cpp` is the project video's footage: frames of the real editor and the audio
the real processor made, from one knob automation, so what the video shows and what it
plays are the plugin doing the same thing at the same moment.

The browser demo is verified the same way: `tools/web/build.sh` compiles `Source/DSP/` to
WebAssembly and `node tools/web/harness.mjs` runs the compiled module through the same
checks in AudioWorklet-sized blocks.

## Windows SmartScreen

The download block above says whether this release's macOS builds are signed and
notarised — the fleet's signer does that after each release, and the block is regenerated
once it has. Before that, macOS asks once: right-click the installer, **Open**. Windows
builds are not code-signed: plugin files load normally, but the **installer** trips
SmartScreen — **More info → Run anyway**. Linux has no signing gate.

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
