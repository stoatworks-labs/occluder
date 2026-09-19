# How the curve was chosen

Occluder has one control, so the whole product is the shape that control
dials in. This is where that shape comes from. `tools/curve-model.py` reproduces
every number here.

## The effect being imitated

You hear your own voice two ways at once:

- **through the air** — mouth → air → ear canal. This is the path a microphone
  also hears, so a recording of you *is* this path;
- **through bone** — vocal tract → skull → the walls of the ear canal, which
  radiate sound into the canal from the inside.

With the canal open, the low-frequency sound the canal walls radiate mostly
escapes out of the ear. Block the canal and it is trapped: ear-canal sound
pressure from bone conduction rises by **20–25 dB around 200–500 Hz**, fading
to nothing by about 2 kHz. That is the *occlusion effect*, the reason your
voice booms when you put your fingers in your ears, and the reason hearing-aid
fitters chase deep, vented fits. At the same time the plug attenuates the air
path — roughly 28 dB at 125 Hz rising to 44 dB at 8 kHz for a foam plug — so
the consonants, which travel almost entirely by air, all but disappear.

The two paths sum at the eardrum. Relative to the open ear:

```
plugged / open  =  (AC · P + BC · O) / (AC + BC)
```

with, per octave band (power ratios in dB):

| Hz | BC/AC, open (dB) | Occlusion O (dB) | Plug P (dB) | **Target (dB)** |
|---:|---:|---:|---:|---:|
| 100 | 0 | +18 | −27 | **+15.0** |
| 125 | 0 | +20 | −28 | **+17.0** |
| 250 | 0 | +24 | −30 | **+21.0** |
| 500 | 0 | +22 | −33 | **+19.0** |
| 1000 | 0 | +12 | −35 | **+9.0** |
| 2000 | −5 | +3 | −34 | **−3.2** |
| 4000 | −12 | 0 | −40 | **−12.3** |
| 8000 | −20 | 0 | −44 | **−20.0** |

A 41 dB tilt from 250 Hz to 8 kHz with a hump on top. The absolute level
is not the point — a plugged ear really is ~20 dB louder in the hump, which is
why people with earplugs in talk quietly — the shape is.

Inputs: Stenfelt & Reinfeldt 2007 for the occlusion effect (shallow foam
insertion); Pörschmann 2000 and Reinfeldt et al. 2010 for the bone/air ratio
of the open-ear own voice (comparable below 1 kHz, falling above it); 3M
attenuation data for the plug. Each is an approximation of a spread of
measurements; the target is the shape they agree on, not a claim about any one
ear.

## The fit

Four RBJ second-order sections, chosen so that every one of them is **exactly
the identity at 0 dB**:

| Section | Type | Full scale |
|---|---|---|
| boom | peak, Q 0.5 | +6 dB @ 320 Hz |
| muffle | high shelf | −25 dB @ 1.3 kHz |
| top | high shelf | −12 dB @ 5 kHz |
| rumble | low shelf | −12 dB @ 70 Hz |

Least squares on the eight target points, with the peak's Q and the rumble
shelf held fixed, lands on +5.9 dB @ 320 Hz, −25.5 dB @ 1.30 kHz, −12.0 dB @
5.16 kHz (0.025 dB rms). Rounded as above the shape is within 0.09 dB rms of
the target — the residual is smaller than the spread in the source data.

The rumble shelf is not part of the model. It is there because the hump would
otherwise lift room rumble and handling noise along with the voice, and
because the bone path has nothing below the voice's fundamentals anyway.

**Why shelves and not a low-pass.** The first draft used a second-order
low-pass sliding from 18 kHz to 5 kHz for the top end. It fit the target as
well, and the DSP check caught it clicking: a low-pass parked at 18 kHz is flat
to the ear but not to the sample — its `b0` is about 0.49 — so starting it from
a zeroed state on the way out of bypass put a one-sample dent in the audio.
A shelf at 0 dB has `b == a`; it is the identity transfer function, its state
flushes in two samples, and the knob's zero can be a real pass-through with
nothing to switch.

## The knob

`amount` in 0..1 scales all four gains linearly in decibels. Half way is half
the decibels everywhere, which is not a physical state (there is no
half-plugged ear) but is the family of shapes that reads as *one plug in* or
*hands over the ears* on the way to *foam plugs*. Corner frequencies do not
move.

The parameter is ramped over 50 ms and the sections are redesigned every 32
samples while it moves, so a sweep is ~75 small steps. `tools/ocdsp.cpp` slams
the knob from 0 to 100 under a 1 kHz tone and checks the largest
sample-to-sample step against the tone's own.

## The trim

Tilting speech by 40 dB moves its level, and a one-knob plugin whose knob is
also a volume control is two knobs. So each design carries a trim: the
cascade's response is evaluated at the 25 third-octave bands of the Byrne et
al. (1994) long-term average speech spectrum, power-summed with those band
levels as weights, and the result is subtracted. For speech the knob then
changes only the shape; the check measures a speech-shaped stimulus holding
within 0.01 dB across the dial.

It is computed from the coefficients the audio actually runs through, at the
actual sample rate, so it cannot drift from them. The display shows it.

Net response, trim included, at 48 kHz:

| amount | trim | 30 | 50 | 100 | 200 | 300 | 500 | 1k | 2k | 4k | 8k | 16k |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 0.10 | −0.3 | −1.4 | −1.2 | −0.3 | +0.2 | +0.3 | +0.2 | −0.7 | −2.4 | −3.1 | −3.8 | −4.0 |
| 0.25 | −0.8 | −3.6 | −3.0 | −0.9 | +0.4 | +0.7 | +0.3 | −1.9 | −5.9 | −7.7 | −9.7 | −10.0 |
| 0.50 | −1.7 | −7.4 | −6.1 | −1.9 | +0.6 | +1.2 | +0.4 | −4.3 | −11.8 | −15.7 | −19.5 | −20.2 |
| 0.75 | −2.6 | −11.1 | −9.2 | −3.0 | +0.8 | +1.7 | +0.3 | −7.0 | −17.2 | −23.5 | −29.2 | −30.3 |
| 1.00 | −3.5 | −14.7 | −12.0 | −4.1 | +1.1 | +2.3 | +0.0 | −9.9 | −22.1 | −31.2 | −38.8 | −40.4 |

Two consequences worth knowing. Equal speech power is not equal loudness: at
full scale the energy sits where the ear is least sensitive, so a plugged
voice will *sound* a touch quieter than the dry one even though its RMS has
not moved — turn it up if you want the "louder in the lows" impression a real
plug gives. And the trim is speech-weighted; on material that is not speech
(a full mix, say) the level will move with the knob, downwards.

## What is not modelled

- The open ear canal's quarter-wave resonance near 2.7 kHz, which a plug
  removes. It sits in the air path, which the plug has already taken 35 dB out
  of, so it changes nothing audible.
- Insertion depth. A deep plug (bony canal) has a much smaller occlusion effect
  than a shallow one; the target is the shallow, everyday case.
- The plugged canal's own resonance (a closed cavity, 6–8 kHz). Inaudible under
  40 dB of attenuation.
- Anything time-varying. Occlusion is linear and static; so is this.
