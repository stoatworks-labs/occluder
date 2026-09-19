# Occluder — browser demo

Try it: **https://occluder-demo.stoatworks-labs.com/**

This is the plugin's own DSP — the unmodified sources in [`../Source/DSP`](../Source/DSP) —
compiled to WebAssembly and run inside an AudioWorklet, wrapped in a page with the same
curve display and knob as the editor. Sources are your microphone (raw: no echo
cancellation or noise suppression between you and the effect) or any audio file, which is
decoded in the browser and never uploaded. A web-only hard clip at −0.2 dBFS sits on the
output so a hot microphone cannot send full scale into headphones; the plugin has no such
stage.

## How it works

- [`../tools/web/occluder_web.cpp`](../tools/web/occluder_web.cpp) plays
  `PluginProcessor`'s part: one `Engine`, planar buffers for the worklet, and `oc_curve` /
  `oc_trim_db` for the display, all straight off `curve::design()`.
- [`../tools/web/build.sh`](../tools/web/build.sh) compiles it with Emscripten to
  [`occluder.js`](occluder.js), a single self-contained ES module (wasm embedded, compiled
  synchronously) so the one file loads in the AudioWorklet, on the main thread and in Node.
- Two instances run in the page: one in [`worklet.js`](worklet.js) for the audio, one in
  [`app.js`](app.js) for the curve, so the picture cannot drift from the sound.
- `Source/DSP/` depends on nothing — `Ramp.h` exists so that it does not need JUCE's
  `SmoothedValue` — which is what lets the same files build here unmodified.

## Build and verify

Requires Emscripten (`brew install emscripten`) and Node.

```bash
tools/web/build.sh          # -> demo/occluder.js
node tools/web/harness.mjs  # must pass before deploying
```

The harness is `tools/ocdsp.cpp`'s discipline applied to the compiled module: real signal
through the actual processor in 128-frame blocks, checked numerically — bit-exact
pass-through at 0, the measured response against `oc_curve` at ten frequencies and three
knob positions, the full-scale shape against the acoustic model, speech-shaped level across
the dial, and a click-free slam of the knob.

## Deploy

Static-assets-only Cloudflare Worker (`../wrangler.toml`), from the repo root:

```bash
cf-run npx wrangler deploy
```

`_headers` carries the CSP; `script-src` needs `'wasm-unsafe-eval'` for the module to
compile in production, and `connect-src` names the intake origin for the support footer's
feedback button. `support-footer.js` is vendored by
`stoatworks-backend/scripts/sync-support-footer.sh` — edit the master, not the copy.
