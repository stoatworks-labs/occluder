// AudioWorklet host for the Occluder wasm core. All of the DSP happens inside
// the module - the plugin's own Source/DSP code - and this file only shuttles
// samples and the knob across. One thing is web-only and marked as such: a hard
// clip at -0.2 dBFS on the way out, because a hot microphone into headphones is
// not a place to find out what full scale plus feels like. The plugin has no
// such stage.

import createOccluderModule from './occluder.js';

const CEILING = 0.977;            // -0.2 dBFS
const METER_INTERVAL_BLOCKS = 4;  // ~10 ms at 48 kHz

class OccluderProcessor extends AudioWorkletProcessor {
  constructor() {
    super();
    this.ready = false;
    this.pending = null;
    this.blocks = 0;
    this.peakIn = 0;
    this.peakOut = 0;

    createOccluderModule().then((M) => {
      this.M = M;
      this.C = {
        init: M.cwrap('oc_init', null, ['number', 'number']),
        setAmount: M.cwrap('oc_set_amount', null, ['number', 'number']),
        buffer: M.cwrap('oc_buffer', 'number', ['number']),
        process: M.cwrap('oc_process', null, ['number', 'number']),
      };
      this.C.init(sampleRate, 2);
      this.p0 = this.C.buffer(0) >> 2;
      this.p1 = this.C.buffer(1) >> 2;
      if (this.pending) this.C.setAmount(this.pending.value, 1);
      this.ready = true;
      this.port.postMessage({ type: 'ready', sampleRate });
    });

    this.port.onmessage = (e) => {
      const msg = e.data;
      if (msg.type === 'amount') {
        if (this.ready) this.C.setAmount(msg.value, msg.immediate ? 1 : 0);
        else this.pending = msg;
      }
    };
  }

  process(inputs, outputs) {
    const out = outputs[0];
    if (out.length === 0) return true;
    const input = inputs[0];
    const n = out[0].length;

    if (!this.ready || input.length === 0) {
      for (const ch of out) ch.fill(0);
      return true;
    }

    const H = this.M.HEAPF32;
    const inL = input[0];
    const inR = input.length > 1 ? input[1] : input[0];
    const channels = out.length > 1 ? 2 : 1;
    let pin = this.peakIn;
    for (let i = 0; i < n; i++) {
      H[this.p0 + i] = inL[i];
      H[this.p1 + i] = inR[i];
      const a = Math.abs(inL[i]);
      if (a > pin) pin = a;
    }
    this.C.process(n, channels);

    let pout = this.peakOut;
    const o0 = out[0];
    const o1 = out.length > 1 ? out[1] : null;
    for (let i = 0; i < n; i++) {
      const l = H[this.p0 + i];
      const cl = l > CEILING ? CEILING : (l < -CEILING ? -CEILING : l);
      o0[i] = cl;
      if (o1) {
        const r = H[this.p1 + i];
        o1[i] = r > CEILING ? CEILING : (r < -CEILING ? -CEILING : r);
      }
      const a = Math.abs(l);
      if (a > pout) pout = a;
    }
    this.peakIn = pin;
    this.peakOut = pout;

    if (++this.blocks >= METER_INTERVAL_BLOCKS) {
      this.blocks = 0;
      this.port.postMessage({ type: 'meter', in: this.peakIn, out: this.peakOut });
      this.peakIn = 0;
      this.peakOut = 0;
    }
    return true;
  }
}

registerProcessor('occluder', OccluderProcessor);
