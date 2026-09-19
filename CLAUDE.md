# occluder (Occluder)

One-knob own-voice occlusion EQ. VST3/AU/Standalone, JUCE 8 / C++20, CMake. Not yet
released; not yet a fleet repo.

## Commands (CMake)
- Configure: `cmake -B build -DCMAKE_BUILD_TYPE=Release`
- Build: `cmake --build build`
- DSP checks: `./build/ocdsp_artefacts/Release/ocdsp` (also `ctest --test-dir build`)
- Screenshots: `./build/ocshot_artefacts/Release/ocshot docs/screenshots/plugin.png 60`
- Hosts: `pluginval --strictness-level 10 --validate <bundle>`, `auval -v aufx Occl Alsg`

## Notes
- The curve is `Source/DSP/OcclusionCurve.{h,cpp}` and nothing else. Its constants are a fit
  to an acoustic model (`docs/DESIGN.md`, `tools/curve-model.py`); `ocdsp` fails if the
  full-scale shape drifts from that model's eight points. Change the constants and the
  document together, or not at all.
- Audio thread is allocation-free and lock-free; `ocdsp` counts allocations. Coefficients are
  designed in place (`Source/DSP/Biquad.h`), never via `juce::dsp::IIR::Coefficients`.
- Every section must be exactly unity at 0 dB (peak, shelves). No low-pass or high-pass: a
  "parked" one is not unity at the sample level and clicks on the way out of bypass.
- A local macOS build copies the plug-ins into `/Library/Audio/Plug-Ins/` (system domain).
  Move, don't copy, if you relocate them, or hosts show duplicates.
- Public-repo posture: ships the AI-assisted disclaimer in the README. "Commit" = commit
  **and** push. Never leave test scaffolding in `CMakeLists.txt`: `ocdsp` and `ocshot` are the
  two permanent tools.

## Verifying DSP changes
`tools/ocdsp.cpp` is the harness; extend it rather than writing a throwaway. The stimulus must
be able to distinguish right from wrong: the response check uses ten frequencies across the
band, and the level check uses a speech-shaped spectrum with energy *between* the bands the
trim is computed at, so it cannot pass by restating the trim's own definition.
