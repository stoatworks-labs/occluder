// Occluder browser demo - the page around the wasm core.
//
// Two instances of the same module run here: one inside the AudioWorklet
// (worklet.js) for the audio, and this one on the main thread for the curve,
// so the picture cannot drift from the sound. Everything else in this file
// is a knob, a canvas, and the plumbing that gets audio to the worklet.

import createOccluderModule from './occluder.js';

const M = await createOccluderModule();
const C = {
  curve: M.cwrap('oc_curve', null, ['number', 'number', 'number', 'number', 'number']),
  trimDb: M.cwrap('oc_trim_db', 'number', ['number', 'number']),
};

const $ = (id) => document.getElementById(id);
const knobCanvas = $('knob');
const curveCanvas = $('curve');
const valueOut = $('value');
const statusEl = $('status');
const compareBtn = $('compare');
const micBtn = $('mic');
const fileBtn = $('file');
const playBtn = $('play');
const stopBtn = $('stop');
const fileInput = $('fileinput');
const meterIn = $('meter-in');
const meterOut = $('meter-out');

const palette = {
  bg: '#0b1420', panel: '#101d2e', grid: '#1d3048', gridBright: '#2b4566', zero: '#35557a',
  accent: '#4cc9f0', text: '#e8eef5', dim: '#93a8bd',
};

// ------------------------------------------------------------------ state
let amount = 50;                 // the knob, 0..100, what the plugin calls "Occlusion"
let comparing = false;           // Hold-to-compare: the audio runs at 0 while true
let sampleRate = 48000;          // the curve is drawn at the context's rate once there is one
let ctx = null, node = null, source = null, stream = null, buffer = null, playing = false;
let workletReady = false, lastMeter = { in: 0, out: 0 };

function setStatus(text) { statusEl.textContent = text; }

// ------------------------------------------------------------------ the knob
// The plugin's rotary sweep, measured clockwise from twelve o'clock as JUCE
// does; a canvas arc measures from three, hence the quarter turn below.
const START = Math.PI * 1.25, END = Math.PI * 2.75;
const QUARTER = Math.PI / 2;

function drawKnob() {
  const dpr = window.devicePixelRatio || 1;
  const size = knobCanvas.clientWidth;
  if (knobCanvas.width !== size * dpr) { knobCanvas.width = size * dpr; knobCanvas.height = size * dpr; }
  const g = knobCanvas.getContext('2d');
  g.setTransform(dpr, 0, 0, dpr, 0, 0);
  g.clearRect(0, 0, size, size);

  const cx = size / 2, cy = size / 2;
  const radius = size / 2 - 8;
  const arcWidth = 6, arcRadius = radius - arcWidth / 2;
  const angle = START + (amount / 100) * (END - START);

  g.lineCap = 'round';
  g.lineWidth = arcWidth;
  g.strokeStyle = palette.grid;
  g.beginPath(); g.arc(cx, cy, arcRadius, START - QUARTER, END - QUARTER); g.stroke();
  if (amount > 0) {
    g.strokeStyle = palette.accent;
    g.beginPath(); g.arc(cx, cy, arcRadius, START - QUARTER, angle - QUARTER); g.stroke();
  }

  const body = radius - arcWidth - 8;
  g.fillStyle = palette.panel;
  g.beginPath(); g.arc(cx, cy, body, 0, Math.PI * 2); g.fill();
  g.lineWidth = 1.5; g.strokeStyle = palette.grid;
  g.beginPath(); g.arc(cx, cy, body, 0, Math.PI * 2); g.stroke();

  // The pointer: rotate the frame by the knob's angle and draw straight up.
  g.save();
  g.translate(cx, cy);
  g.rotate(angle);
  g.fillStyle = palette.text;
  const len = body * 0.55;
  roundRect(g, -2, -body + 6, 4, len, 2);
  g.fill();
  g.restore();
}

function roundRect(g, x, y, w, h, r) {
  g.beginPath();
  g.moveTo(x + r, y);
  g.arcTo(x + w, y, x + w, y + h, r);
  g.arcTo(x + w, y + h, x, y + h, r);
  g.arcTo(x, y + h, x, y, r);
  g.arcTo(x, y, x + w, y, r);
  g.closePath();
}

function setAmount(v, { immediate = false } = {}) {
  amount = Math.max(0, Math.min(100, v));
  valueOut.textContent = `${Math.round(amount)} %`;
  knobCanvas.setAttribute('aria-valuenow', String(Math.round(amount)));
  knobCanvas.setAttribute('aria-valuetext', `${Math.round(amount)} %`);
  drawKnob();
  drawCurve();
  if (!comparing) sendAmount(immediate);
}

function sendAmount(immediate = false) {
  if (node) node.port.postMessage({ type: 'amount', value: (comparing ? 0 : amount) / 100, immediate });
}

// Drag: up or right raises it, 200 px of travel is the whole range - the same
// feel as the plugin's RotaryHorizontalVerticalDrag.
let drag = null;
knobCanvas.addEventListener('pointerdown', (e) => {
  knobCanvas.setPointerCapture(e.pointerId);
  drag = { x: e.clientX, y: e.clientY, start: amount };
  knobCanvas.focus();
  e.preventDefault();
});
knobCanvas.addEventListener('pointermove', (e) => {
  if (!drag) return;
  const delta = (drag.x - e.clientX) * -1 + (drag.y - e.clientY);
  setAmount(drag.start + delta * 0.5);
});
const endDrag = () => { drag = null; };
knobCanvas.addEventListener('pointerup', endDrag);
knobCanvas.addEventListener('pointercancel', endDrag);
knobCanvas.addEventListener('dblclick', () => setAmount(0));
knobCanvas.addEventListener('wheel', (e) => {
  e.preventDefault();
  setAmount(amount - Math.sign(e.deltaY) * (e.shiftKey ? 5 : 1));
}, { passive: false });
knobCanvas.addEventListener('keydown', (e) => {
  const step = e.shiftKey ? 10 : 1;
  const map = { ArrowUp: step, ArrowRight: step, ArrowDown: -step, ArrowLeft: -step, PageUp: 10, PageDown: -10 };
  if (e.key in map) { setAmount(amount + map[e.key]); e.preventDefault(); }
  else if (e.key === 'Home') { setAmount(0); e.preventDefault(); }
  else if (e.key === 'End') { setAmount(100); e.preventDefault(); }
});

// ------------------------------------------------------------------ the curve
const MIN_F = 20, MAX_F = 20000, MIN_DB = -42, MAX_DB = 12;
let freqScratch = null, dbScratch = null, scratchN = 0;

function drawCurve() {
  const dpr = window.devicePixelRatio || 1;
  const w = curveCanvas.clientWidth, h = Math.round(w * 240 / 704);
  if (curveCanvas.width !== Math.round(w * dpr)) { curveCanvas.width = Math.round(w * dpr); curveCanvas.height = Math.round(h * dpr); }
  const g = curveCanvas.getContext('2d');
  g.setTransform(dpr, 0, 0, dpr, 0, 0);
  g.fillStyle = palette.panel;
  g.fillRect(0, 0, w, h);

  // The plot, with room for the labels along the bottom and the left.
  const plot = { x: 31, y: 9, w: w - 31 - 9, h: h - 9 - 17 };
  const xFor = (f) => plot.x + Math.log(f / MIN_F) / Math.log(MAX_F / MIN_F) * plot.w;
  const yFor = (db) => plot.y + plot.h - (db - MIN_DB) / (MAX_DB - MIN_DB) * plot.h;

  g.font = '10px -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif';
  g.textBaseline = 'middle';
  for (const f of [20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000]) {
    const major = f === 100 || f === 1000 || f === 10000;
    const x = Math.round(xFor(f)) + 0.5;
    g.strokeStyle = major ? palette.gridBright : palette.grid;
    g.beginPath(); g.moveTo(x, plot.y); g.lineTo(x, plot.y + plot.h); g.stroke();
    if (major) {
      g.fillStyle = palette.dim; g.textAlign = 'center';
      g.fillText(f >= 1000 ? `${f / 1000}k` : String(f), x, plot.y + plot.h + 9);
    }
  }
  for (let db = MIN_DB + 6; db <= MAX_DB; db += 12) {
    const y = Math.round(yFor(db)) + 0.5;
    g.strokeStyle = db === 0 ? palette.zero : palette.grid;
    g.beginPath(); g.moveTo(plot.x, y); g.lineTo(plot.x + plot.w, y); g.stroke();
    g.fillStyle = palette.dim; g.textAlign = 'right';
    g.fillText(String(db), plot.x - 4, y);
  }

  // One point per pixel column, from the same design() the audio runs through.
  const n = Math.max(2, Math.floor(plot.w));
  if (scratchN !== n) {
    if (freqScratch) { M._free(freqScratch); M._free(dbScratch); }
    freqScratch = M._malloc(n * 4); dbScratch = M._malloc(n * 4); scratchN = n;
  }
  const fp = freqScratch >> 2;
  for (let i = 0; i < n; i++) M.HEAPF32[fp + i] = MIN_F * Math.pow(MAX_F / MIN_F, i / (n - 1));
  C.curve(sampleRate, amount / 100, freqScratch, dbScratch, n);
  const dbs = M.HEAPF32.subarray(dbScratch >> 2, (dbScratch >> 2) + n);

  const zeroY = yFor(0);
  g.beginPath();
  g.moveTo(plot.x, zeroY);
  for (let i = 0; i < n; i++) g.lineTo(plot.x + i, yFor(Math.max(MIN_DB, Math.min(MAX_DB, dbs[i]))));
  g.lineTo(plot.x + n - 1, zeroY);
  g.closePath();
  g.fillStyle = 'rgba(76, 201, 240, 0.18)';
  g.fill();

  g.beginPath();
  for (let i = 0; i < n; i++) {
    const y = yFor(Math.max(MIN_DB, Math.min(MAX_DB, dbs[i])));
    if (i === 0) g.moveTo(plot.x, y); else g.lineTo(plot.x + i, y);
  }
  g.lineWidth = 2; g.lineJoin = 'round'; g.strokeStyle = palette.accent;
  g.stroke();

  if (amount > 0) {
    const trim = C.trimDb(sampleRate, amount / 100);
    g.fillStyle = palette.dim; g.textAlign = 'right'; g.textBaseline = 'top';
    g.fillText(`trim ${trim.toFixed(1)} dB`, plot.x + plot.w - 4, plot.y + 2);
  }
  if (comparing) {
    g.fillStyle = palette.accent; g.textAlign = 'left'; g.textBaseline = 'top';
    g.fillText('comparing: hearing the input untouched', plot.x + 4, plot.y + 2);
  }
}

// ------------------------------------------------------------------ compare
function startCompare(e) {
  if (comparing) return;
  comparing = true;
  compareBtn.classList.add('active');
  sendAmount();
  drawCurve();
  if (e) e.preventDefault();
}
function endCompare() {
  if (!comparing) return;
  comparing = false;
  compareBtn.classList.remove('active');
  sendAmount();
  drawCurve();
}
compareBtn.addEventListener('pointerdown', startCompare);
compareBtn.addEventListener('pointerup', endCompare);
compareBtn.addEventListener('pointercancel', endCompare);
compareBtn.addEventListener('pointerleave', endCompare);
compareBtn.addEventListener('keydown', (e) => { if (e.key === ' ' || e.key === 'Enter') startCompare(e); });
compareBtn.addEventListener('keyup', (e) => { if (e.key === ' ' || e.key === 'Enter') endCompare(); });
window.addEventListener('blur', endCompare);

// ------------------------------------------------------------------ audio
async function ensureContext() {
  if (ctx) { if (ctx.state === 'suspended') await ctx.resume(); return; }
  ctx = new AudioContext({ latencyHint: 'interactive' });
  await ctx.audioWorklet.addModule('./worklet.js');
  node = new AudioWorkletNode(ctx, 'occluder', { numberOfInputs: 1, numberOfOutputs: 1, outputChannelCount: [2] });
  node.port.onmessage = (e) => {
    const msg = e.data;
    if (msg.type === 'ready') {
      workletReady = true;
      sampleRate = msg.sampleRate;
      drawCurve();
      sendAmount(true);
    } else if (msg.type === 'meter') {
      lastMeter = msg;
      meter(meterIn, msg.in);
      meter(meterOut, msg.out);
    }
  };
  node.connect(ctx.destination);
  sampleRate = ctx.sampleRate;
  drawCurve();
}

function meter(el, peak) {
  const db = 20 * Math.log10(Math.max(peak, 1e-6));
  const pct = Math.max(0, Math.min(100, (db + 60) / 60 * 100));
  el.style.width = `${pct}%`;
  el.classList.toggle('hot', peak > 0.97);
}

function disconnectSource() {
  if (source) { try { source.disconnect(); } catch (_) { /* already gone */ } }
  if (source && source.stop && playing) { try { source.stop(); } catch (_) { /* not started */ } }
  source = null;
  playing = false;
  if (stream) { for (const t of stream.getTracks()) t.stop(); stream = null; }
  meter(meterIn, 0); meter(meterOut, 0);
}

async function useMic() {
  try {
    await ensureContext();
    disconnectSource();
    // Raw: the echo canceller and noise suppressor are exactly the kind of
    // processing that would sit between your voice and the effect.
    stream = await navigator.mediaDevices.getUserMedia({
      audio: { echoCancellation: false, noiseSuppression: false, autoGainControl: false },
    });
    source = ctx.createMediaStreamSource(stream);
    source.connect(node);
    playing = true;
    playBtn.disabled = true;
    stopBtn.disabled = false;
    const latency = Math.round(((ctx.baseLatency || 0) + (ctx.outputLatency || 0)) * 1000);
    setStatus(`Microphone through Occluder at ${(ctx.sampleRate / 1000).toFixed(1)} kHz` +
      (latency ? `, about ${latency} ms behind you` : '') + '. Talk, and turn the knob.');
  } catch (err) {
    setStatus(err && err.name === 'NotAllowedError'
      ? 'Microphone access was refused. Open a file instead, or allow the microphone and try again.'
      : `Could not open the microphone: ${err && err.message ? err.message : err}`);
  }
}

async function loadFile(file) {
  try {
    await ensureContext();
    setStatus(`Decoding ${file.name}…`);
    const data = await file.arrayBuffer();
    buffer = await ctx.decodeAudioData(data);
    playBtn.disabled = false;
    startFile(file.name);
  } catch (err) {
    setStatus(`Could not decode ${file.name}: ${err && err.message ? err.message : err}`);
  }
}

function startFile(name) {
  if (!buffer) return;
  disconnectSource();
  source = ctx.createBufferSource();
  source.buffer = buffer;
  source.loop = true;
  source.connect(node);
  source.start();
  playing = true;
  playBtn.textContent = 'Restart';
  stopBtn.disabled = false;
  const secs = buffer.duration;
  setStatus(`Playing ${name || 'the file'} (${secs.toFixed(1)} s, ${buffer.numberOfChannels === 1 ? 'mono' : buffer.numberOfChannels + ' ch'}) on a loop through Occluder. Nothing was uploaded.`);
}

function stopAll() {
  disconnectSource();
  stopBtn.disabled = true;
  playBtn.textContent = 'Play';
  setStatus('Stopped.');
}

micBtn.addEventListener('click', useMic);
fileBtn.addEventListener('click', () => fileInput.click());
fileInput.addEventListener('change', () => { if (fileInput.files[0]) loadFile(fileInput.files[0]); fileInput.value = ''; });
playBtn.addEventListener('click', () => startFile());
stopBtn.addEventListener('click', stopAll);

// Drop a file anywhere.
let dragDepth = 0;
document.addEventListener('dragenter', (e) => { e.preventDefault(); if (++dragDepth === 1) document.body.classList.add('dragging'); });
document.addEventListener('dragleave', () => { if (--dragDepth <= 0) { dragDepth = 0; document.body.classList.remove('dragging'); } });
document.addEventListener('dragover', (e) => e.preventDefault());
document.addEventListener('drop', (e) => {
  e.preventDefault();
  dragDepth = 0;
  document.body.classList.remove('dragging');
  const file = e.dataTransfer && e.dataTransfer.files && e.dataTransfer.files[0];
  if (file) loadFile(file);
});

if (!navigator.mediaDevices || !navigator.mediaDevices.getUserMedia) {
  micBtn.disabled = true;
  micBtn.title = 'This browser does not offer microphone capture here.';
}

// ------------------------------------------------------------------ go
window.addEventListener('resize', () => { drawKnob(); drawCurve(); });
setAmount(50);

// For the fleet's screenshot and verification scripts: the page's state, read-only.
window.__occluder = {
  get amount() { return amount; },
  set amount(v) { setAmount(v); },
  get ready() { return workletReady; },
  get meter() { return lastMeter; },
  get contextState() { return ctx ? ctx.state : 'none'; },
  get playing() { return playing; },
  loadFile,
};
