#!/bin/bash
# Builds the plugin's own DSP (Source/DSP, unmodified) plus the C API in
# occluder_web.cpp to WebAssembly, as a single self-contained ES module: the
# wasm is embedded and compiled synchronously, so the one file loads in an
# AudioWorklet, on the main thread, and in Node for the harness.
#
#   tools/web/build.sh          -> demo/occluder.js
#   node tools/web/harness.mjs  verification; must pass before deploying
#
# Requires Emscripten (brew install emscripten).
set -euo pipefail
cd "$(dirname "$0")"

SRC=../../Source
OUT=../../demo/occluder.js

emcc -O3 -std=c++20 \
  -I "$SRC" \
  occluder_web.cpp \
  "$SRC/DSP/OcclusionCurve.cpp" \
  "$SRC/DSP/Engine.cpp" \
  -sMODULARIZE=1 \
  -sEXPORT_ES6=1 \
  -sEXPORT_NAME=createOccluderModule \
  -sSINGLE_FILE=1 \
  -sWASM_ASYNC_COMPILATION=0 \
  -sALLOW_MEMORY_GROWTH=0 \
  -sINITIAL_MEMORY=4194304 \
  -sENVIRONMENT=web,worker,node \
  -sEXPORTED_FUNCTIONS=_oc_init,_oc_set_amount,_oc_get_amount,_oc_buffer,_oc_process,_oc_is_passthrough,_oc_curve,_oc_trim_db,_malloc,_free \
  -sEXPORTED_RUNTIME_METHODS=cwrap,HEAPF32 \
  -sINCOMING_MODULE_JS_API=instantiateWasm,locateFile \
  -o "$OUT"

echo "Built $OUT ($(du -h "$OUT" | cut -f1))"
