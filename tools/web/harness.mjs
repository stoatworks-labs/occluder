// Verification harness for the Occluder wasm core. Node-only; not shipped.
//
// Same discipline as tools/ocdsp.cpp, against the compiled module the browser
// runs: real signal through the actual processor in 128-frame blocks (the
// AudioWorklet render quantum), checked numerically, with stimuli that can tell
// right from wrong - ten frequencies for the response, a speech-shaped spectrum
// with energy between the trim's bands for the level.
//
// Run: node tools/web/harness.mjs

import createOccluderModule from '../../demo/occluder.js';

const SR = 48000;
const BLOCK = 128;

const M = await createOccluderModule();
const C = {
  init: M.cwrap('oc_init', null, ['number', 'number']),
  setAmount: M.cwrap('oc_set_amount', null, ['number', 'number']),
  getAmount: M.cwrap('oc_get_amount', 'number', []),
  buffer: M.cwrap('oc_buffer', 'number', ['number']),
  process: M.cwrap('oc_process', null, ['number', 'number']),
  passthrough: M.cwrap('oc_is_passthrough', 'number', []),
  curve: M.cwrap('oc_curve', null, ['number', 'number', 'number', 'number', 'number']),
  trimDb: M.cwrap('oc_trim_db', 'number', ['number', 'number']),
};

let failures = 0;
function check(name, ok, detail = '') {
  console.log(`  [${ok ? 'ok' : 'FAIL'}] ${name}${detail ? ' - ' + detail : ''}`);
  if (!ok) failures++;
}
const db = (x) => 20 * Math.log10(Math.max(x, 1e-12));

/** Run mono samples through the engine in worklet-sized blocks; returns the output. */
function run(input, channels = 1) {
  const out = new Float32Array(input.length);
  const p0 = C.buffer(0) >> 2, p1 = C.buffer(1) >> 2;
  for (let start = 0; start < input.length; start += BLOCK) {
    const n = Math.min(BLOCK, input.length - start);
    for (let i = 0; i < n; i++) {
      M.HEAPF32[p0 + i] = input[start + i];
      if (channels > 1) M.HEAPF32[p1 + i] = input[start + i];
    }
    C.process(n, channels);
    for (let i = 0; i < n; i++) out[start + i] = M.HEAPF32[p0 + i];
  }
  return out;
}

function tone(freq, seconds, amp = 0.25) {
  const x = new Float32Array(Math.round(seconds * SR));
  for (let i = 0; i < x.length; i++) x[i] = amp * Math.sin(2 * Math.PI * freq * i / SR);
  return x;
}

function rmsTail(x) {
  let s = 0, n = 0;
  for (let i = x.length >> 1; i < x.length; i++) { s += x[i] * x[i]; n++; }
  return Math.sqrt(s / n);
}

function curveAt(amount, freqs) {
  const fp = M._malloc(freqs.length * 4), op = M._malloc(freqs.length * 4);
  M.HEAPF32.set(Float32Array.from(freqs), fp >> 2);
  C.curve(SR, amount, fp, op, freqs.length);
  const out = Array.from(M.HEAPF32.subarray(op >> 2, (op >> 2) + freqs.length));
  M._free(fp); M._free(op);
  return out;
}

// 1. pass-through at 0 -----------------------------------------------------------
console.log('1. pass-through at 0');
C.init(SR, 2);
C.setAmount(0.5, 1);
C.setAmount(0.0, 0);
run(new Float32Array(SR / 10), 2);           // the ramp down, then the flush
check('engine reports pass-through after the ramp to 0', C.passthrough() === 1);
{
  const noise = new Float32Array(BLOCK * 50);
  let seed = 1;
  for (let i = 0; i < noise.length; i++) { seed = (seed * 1103515245 + 12345) & 0x7fffffff; noise[i] = seed / 0x3fffffff - 1; }
  const out = run(noise, 2);
  let identical = true;
  for (let i = 0; i < noise.length; i++) if (out[i] !== noise[i]) { identical = false; break; }
  check('50 blocks of noise come out bit-identical', identical);
}

// 2. response vs the curve the page draws ------------------------------------------
console.log('2. measured response vs oc_curve');
const freqs = [50, 100, 200, 300, 500, 1000, 2000, 4000, 8000, 12000];
for (const amount of [0.25, 0.5, 1.0]) {
  const expected = curveAt(amount, freqs);
  let worst = 0, worstAt = '';
  freqs.forEach((f, i) => {
    C.init(SR, 1);
    C.setAmount(amount, 1);
    const measured = db(rmsTail(run(tone(f, 1.0))) / (0.25 / Math.SQRT2));
    const err = Math.abs(measured - expected[i]);
    if (err > worst) { worst = err; worstAt = `${f} Hz (${measured.toFixed(2)} vs ${expected[i].toFixed(2)})`; }
  });
  check(`amount ${amount.toFixed(2)}`, worst < 0.1, `worst ${worst.toFixed(3)} dB at ${worstAt}`);
}

// 3. the full-scale shape is the plugin's ----------------------------------------
console.log('3. full-scale shape');
{
  // docs/DESIGN.md: the acoustic target at the eight model points; shape only.
  const target = { 100: 15.0, 125: 17.0, 250: 21.0, 500: 19.0, 1000: 9.0, 2000: -3.2, 4000: -12.3, 8000: -20.0 };
  const fs = Object.keys(target).map(Number);
  const got = curveAt(1.0, fs);
  const meanT = fs.reduce((a, f) => a + target[f], 0) / fs.length;
  const meanG = got.reduce((a, v) => a + v, 0) / fs.length;
  let worst = 0;
  fs.forEach((f, i) => { worst = Math.max(worst, Math.abs((got[i] - meanG) - (target[f] - meanT))); });
  check('within 1.5 dB of the acoustic target at the eight model points', worst < 1.5, `worst ${worst.toFixed(2)} dB`);
  const trim = C.trimDb(SR, 1.0);
  check('full-scale trim in range', trim < -2.0 && trim > -5.0, `${trim.toFixed(2)} dB`);
}

// 4. speech-shaped level across the dial --------------------------------------------
console.log('4. speech-shaped level across the dial');
{
  const ltass = [[63, 38.6], [80, 43.5], [100, 54.4], [125, 57.7], [160, 56.8], [200, 60.2], [250, 60.3], [315, 59.0],
    [400, 62.1], [500, 62.1], [630, 60.5], [800, 56.8], [1000, 53.7], [1250, 53.0], [1600, 52.0], [2000, 48.7],
    [2500, 48.1], [3150, 46.8], [4000, 45.6], [5000, 44.6], [6300, 44.9], [8000, 44.4], [10000, 42.4], [12500, 40.6], [16000, 35.9]];
  const tones = [];
  let seed = 7;
  for (let f = 63; f <= 16000; f *= Math.pow(2, 1 / 12)) {
    let level = ltass[0][1];
    for (let i = 0; i + 1 < ltass.length; i++) {
      if (f >= ltass[i][0] && f <= ltass[i + 1][0]) {
        const t = Math.log(f / ltass[i][0]) / Math.log(ltass[i + 1][0] / ltass[i][0]);
        level = ltass[i][1] + t * (ltass[i + 1][1] - ltass[i][1]);
      }
    }
    seed = (seed * 1103515245 + 12345) & 0x7fffffff;
    tones.push([f, Math.pow(10, (level - 70) / 20) / 2, (seed / 0x7fffffff) * 2 * Math.PI]);
  }
  const x = new Float32Array(SR);
  for (let i = 0; i < x.length; i++) {
    let v = 0;
    for (const [f, a, ph] of tones) v += a * Math.sin(2 * Math.PI * f * i / SR + ph);
    x[i] = v;
  }
  const level = (amount) => { C.init(SR, 1); C.setAmount(amount, 1); return rmsTail(run(x)); };
  const ref = level(0);
  let worst = 0, worstAt = 0;
  for (const a of [0.1, 0.25, 0.5, 0.75, 1.0]) {
    const d = db(level(a) / ref);
    console.log(`     amount ${a.toFixed(2)}: ${d >= 0 ? '+' : ''}${d.toFixed(2)} dB`);
    if (Math.abs(d) > worst) { worst = Math.abs(d); worstAt = a; }
  }
  check('within 0.75 dB of unity across the dial', worst < 0.75, `worst ${worst.toFixed(2)} dB at ${worstAt}`);
}

// 5. a slam of the knob is ramped and click-free ------------------------------------
console.log('5. knob sweep');
{
  C.init(SR, 1);
  C.setAmount(0.0, 1);
  const f = 1000;
  run(tone(f, 0.5));
  C.setAmount(1.0, 0);
  const x = new Float32Array(BLOCK * 80);
  const t0 = Math.round(0.5 * SR);
  for (let i = 0; i < x.length; i++) x[i] = 0.25 * Math.sin(2 * Math.PI * f * (t0 + i) / SR);
  const y = run(x);
  let maxDelta = 0, early = 0;
  for (let i = 1; i < y.length; i++) maxDelta = Math.max(maxDelta, Math.abs(y[i] - y[i - 1]));
  for (let i = BLOCK * 2; i < BLOCK * 4; i++) early = Math.max(early, Math.abs(y[i]));
  const finalAmp = 0.25 * Math.pow(10, curveAt(1.0, [f])[0] / 20);
  let late = 0;
  for (let i = y.length - BLOCK * 4; i < y.length; i++) late = Math.max(late, Math.abs(y[i]));
  const steady = 0.25 * 2 * Math.PI * f / SR;
  check('part way down the ramp after 256 samples', early > 0.55 * 0.25 && early < 0.98 * 0.25, `peak ${early.toFixed(4)} (start 0.25, final ${finalAmp.toFixed(4)})`);
  check('settled at the design amplitude', Math.abs(late - finalAmp) < 0.02 * finalAmp, `${late.toFixed(4)} vs ${finalAmp.toFixed(4)}`);
  check('largest step within 1.5x the tone\'s own', maxDelta < 1.5 * steady, `${maxDelta.toFixed(5)} vs steady ${steady.toFixed(5)}`);
}

console.log(`${failures === 0 ? 'PASS' : 'FAIL'}: ${failures} failure(s)`);
process.exit(failures);
