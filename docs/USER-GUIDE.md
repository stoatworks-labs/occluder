# Occluder user guide

Occluder is a one-knob EQ (VST3 / AU) that makes a recorded voice sound the way
you hear your own voice — from your ears as they are, up to foam earplugs in.

> **Status:** the curve has been checked numerically against the acoustic model it
> comes from, the plugin passes `pluginval` at its strictest level on both formats
> and Apple's `auval`, and it adds no latency. It has been listened to on this
> machine and nowhere else yet.

---

## What it does

When you speak, you hear yourself two ways: through the air, like everyone else
does, and through your skull. Block your ears and the skull path is trapped in the
ear canal while the air path is shut out. Your voice goes boomy and muffled at
once — the "talking into a bucket" sound of wearing earplugs, in-ear monitors, or
a finger in each ear.

Occluder applies that change to a recording. One knob, **Occlusion**, from 0 to 100:

- **0 — open.** Nothing happens. The audio passes through bit for bit.
- **A touch (10–20).** Roughly the difference between a recording of you and how
  you hear yourself with open ears: a little fuller, a little less air.
- **Half way.** One plug in, or hands cupped over the ears.
- **100 — plugged.** Foam earplugs, properly inserted. The low mids stay, the
  consonants are ~30 dB down, everything above 4 kHz is gone.

The display shows the exact response the knob is asking for, at your session's
sample rate, and the **trim** it is applying — see below.

## Installing

Take a release build, or build it yourself (see the README). On macOS the VST3
goes in `/Library/Audio/Plug-Ins/VST3/` and the Audio Unit in
`/Library/Audio/Plug-Ins/Components/`; the installer puts them there. Rescan
plugins in your host. It appears under **Allan Sargeant › Occluder**, category
EQ.

Any channel layout works, mono to surround, as long as the output matches the
input. Every channel gets the same curve.

## Using it

Insert it on the voice, turn the knob until it sounds like what you are after.
Double-click the knob to return to 0. The number under the knob can be typed into.

It is an ordinary parameter, so it can be automated: fade it in as a character
puts their ear defenders on, ride it for a swimmer surfacing, snap it for a cut to
a point-of-view shot. The knob is smoothed over 50 ms, so automation moves are
click-free however fast they are drawn.

**Where it fits.** Post-production perspective shifts (a scene heard from inside
ear protection, under a helmet, from the speaker's own head); podcast and radio
drama "inner voice"; demonstrating to a performer what an in-ear monitor block or
a badly fitted earplug does to their own voice; hearing-care demos of the
occlusion effect; and, at low settings, the small amount of body that makes a
voice-over sound like the person rather than the microphone.

## The trim

A curve that lifts 300 Hz and drops 4 kHz by 30 dB would also change how loud the
voice is, and then the knob would be a volume control too. So Occluder trims each
setting to keep the long-term level of *speech* where it was: at 100 it is taking
about 3.5 dB off, and the display says so.

Two things follow. A fully plugged voice can *seem* a little quieter than the dry
one even though its level has not moved, because its energy has shifted to where
the ear is less sensitive — real plugs make your own voice louder, and if you want
that impression, add a few dB after it. And the trim is worked out for speech; on
a full mix the level will drop as you turn the knob up.

## What it is not

It is not a low-pass filter with a name. The shape is the plugged ear's — the
hump is at 300 Hz because that is where the ear canal traps bone-conducted sound,
and the tilt is the plug's attenuation curve. `docs/DESIGN.md` shows the working.

It is also not adjustable beyond the one knob, on purpose. If you need to move the
hump or change the slope, a parametric EQ is the right tool.

## Known quirks — not bugs

**One `pluginval` warning on the AU build** is expected: *"Current program is
-1"* — the AU wrapper reports no preset selected until a host picks one. Apple's
own `auval` passes clean.

**The standalone app asks for the microphone** the first time it runs. It is
for talking into, so it needs one; nothing is recorded or sent anywhere.
