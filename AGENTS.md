# AGENTS.md — bringing an LLM up to speed on Occluder

Orientation for an AI assistant (or a new human) picking this project up cold. Read this
before proposing changes. `CLAUDE.md` holds the short command reference; this file explains
the *why*, and the traps.

---

## 1. What this is

A **one-knob EQ** that imitates the occlusion effect — how your own voice sounds with
your ears blocked. JUCE 8, C++20, CMake; ships as VST3, AU and a Standalone app. One
parameter, `amount` (shown as *Occlusion*, 0–100 %). Built and released (v0.1.0) 2026-09-19.

The audible content of the plugin is a single curve, and that curve is derived rather
than tuned: an acoustic model of a plugged ear (bone-conducted own voice trapped in the
canal, air-conducted voice attenuated by the plug, both relative to the open ear) fitted
with four second-order sections. `docs/DESIGN.md` is the working; `tools/curve-model.py`
reproduces it.

## 2. The rules that matter most

1. **The constants and the document move together.** `Source/DSP/OcclusionCurve.h` holds the
   full-scale values; `docs/DESIGN.md` and `tools/curve-model.py` hold the same numbers with
   their justification; `tools/ocdsp.cpp` fails if the full-scale shape drifts more than
   1.5 dB from the model's eight points. Retuning by ear without updating the model is how
   a derived curve quietly becomes an arbitrary one.
2. **Every section is exactly unity at 0 dB.** Peak and shelves only. A low-pass "parked"
   at 18 kHz is flat to the ear but has `b0 ≈ 0.49`, and bringing it in from a zeroed state
   clicked — the check caught it on the first run. The bypass design depends on the identity
   property: at 0 the sections run as identities until their state has flushed, then blocks
   pass through untouched, bit-exact.
3. **Nothing on the audio thread allocates or locks.** `Biquad.h` exists because
   `juce::dsp::IIR::Coefficients` allocates on design. `ocdsp` counts `operator new` calls
   over a hundred blocks with the knob moving; the number is zero and must stay zero.
4. **One knob.** Requests for a mix control, a separate output trim, or movable corners are
   requests for a different product. The trim is automatic and displayed, not adjustable.

## 3. Layout

```
Source/
  PluginProcessor.{h,cpp}     Host-facing entry point; owns the Engine, one APVTS param
  PluginEditor.{h,cpp}        Curve display + knob + About "i" button
  PluginParameters.h          The one parameter
  DSP/
    Biquad.h                  Allocation-free RBJ peak / high shelf / low shelf, TDF2 state
    OcclusionCurve.{h,cpp}    THE CURVE: constants, design(amount) -> four sections + trim
    Engine.{h,cpp}            Smoothed knob, sub-block redesign, per-channel state, bypass
    Ramp.h                    The knob's linear ramp; here so DSP/ needs no JUCE (the web build)
  GUI/
    CurveDisplay.{h,cpp}      Draws design(amount) at the session rate, trim included
    OccluderLookAndFeel.*     Palette, rotary knob, text box
  StoatworksAbout*.h          Vendored About panel (backend master) + hand-written data
tools/
  ocdsp.cpp                   The DSP checks (permanent; `ctest` runs it)
  ocshot.cpp                  Renders the real editor to PNG for the docs
  ocrender.cpp                File in, file out, at a knob position (listening, A/B)
  ocfilm.cpp                  The video's footage: editor frames + processed audio, one automation
  curve-model.py              The acoustic model and the fit, reproducible
  web/                        The browser demo's C API, Emscripten build and Node harness
demo/                         The browser demo as served: page, worklet, built occluder.js, CSP
docs/
  DESIGN.md                   Why the curve is what it is
  USER-GUIDE.md               The guide (the PDF and site page are generated from it)
  screenshots/                Output of ocshot, never hand-made
```

## 4. How it works, per block

`processBlock` reads the parameter (an atomic), hands it to `Engine::setAmount` (ramped,
50 ms), and calls `Engine::process`:

- If the smoother is idle and the engine is passing through, return — the buffer is untouched.
- If idle and active, run the four sections with the current design.
- If the knob is moving, step 32 samples at a time: advance the smoother, rebuild the design
  from the new value (four RBJ designs plus the 25-band trim, ~2 k flops), process the
  sub-block. ~75 steps for a full sweep.
- When the smoother lands on 0 the design is the identity; the sections run until every
  state variable is below 1e-10 (two samples, in practice), then the engine flips to
  pass-through and zeroes them.

The trim is the negative of the design's LTASS-weighted power gain (Byrne et al. 1994
third-octave levels as weights). It is computed from the coefficients themselves, so the
display, the audio and the check all agree by construction.

## 5. Verifying

`ocdsp` is the harness. Eight groups; each stimulus was chosen so that right and wrong are
distinguishable (single tones at ten frequencies for a linear filter are fine; a single tone
would not be for anything frequency-selective and nonlinear). The level check deliberately
uses tones *between* the third-octave centres the trim is defined at.

Then `pluginval --strictness-level 10` on the VST3 and AU, and `auval -v aufx Occl Alsg`. The
AU logs one benign pluginval warning, *"Current program is -1"*.

For the editor, `ocshot` renders offscreen: `createComponentSnapshot` on the real editor
after a 100 ms dispatch loop so the display's timer has seen the knob. No window is needed.

The browser demo (`demo/`, `occluder-demo.stoatworks-labs.com`) is the same `Source/DSP/`
compiled to WebAssembly by `tools/web/build.sh`; `tools/web/harness.mjs` runs the compiled
module through ocdsp's checks in 128-frame blocks, and an OfflineAudioContext test of the
worklet in Chromium measured the design's response to 0.01 dB. A push to main that touches
`demo/` or `wrangler.toml` deploys it (`.github/workflows/deploy.yml`, wrangler pinned, live
`<head>` hash check); the first deploy was by hand, as a brand-new custom domain resolves too
slowly for a workflow's live check.

## 6. Traps

- **`juce::FileOutputStream` appends.** `ocshot` deletes the target first; before it did, every
  re-render was appended after the first PNG and every viewer showed the stale first image.
  Half an hour went into "fixing" a text-box outline that had been gone since the first fix.
- **A `Slider` builds its text box with whatever look-and-feel it can see at the time**, and
  only rebuilds on `lookAndFeelChanged`. `setLookAndFeel` is called last in the editor
  constructor, after the children exist, and the text-box label's colours are set on the
  label itself.
- **The Standalone needs `MICROPHONE_PERMISSION_ENABLED`** or macOS kills it when it opens an
  input. The fleet's post-hoc signer grants the audio-input entitlement by looking for that
  same `NSMicrophoneUsageDescription` string, so removing it would also ship a deaf app.
- **The demo's CSP needs `'wasm-unsafe-eval'`** in `script-src`, or Chrome refuses to compile the
  embedded module in production only — a local server applies no `_headers` at all.
- **A canvas arc measures from three o'clock; JUCE's rotary from twelve.** The demo's knob was a
  quarter turn out until `app.js` subtracted it. Compare the two at 50 %: both point straight up.
- **`COPY_PLUGIN_AFTER_BUILD` goes to `/Library`, not `~/Library`**, and is off when `CI` is
  set. The system domain is where every host scans and where the `.pkg` installs; a dev
  build there and a release there never sit side by side as duplicates.
- **`Source/StoatworksAbout.h` is generated** by `stoatworks-backend/scripts/sync-about.py`
  from the website's `projects.json`; so are the issue templates and the README's
  attributions block. Edit the masters, re-run the sync, never the copies.

## 7. Not yet done

- Listened to only as `ocrender` output of a synthesised voice; not used on a show, and it
  has met no DAW but pluginval's and auval's hosts.
- The demo video is rendered by `ocfilm` through `stoatworks-backend/video/projects/occluder/`;
  its YouTube ID lands in `projects.json` and the README once uploaded.
- `scripts/release-local.sh` is zero-eq's with the names swapped and has not been run; the
  release path is the tag → `release.yml` → autosign one.
