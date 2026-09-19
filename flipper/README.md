# GFLAI Neon Drive

A Flipper Zero app that drives GFLAI RGB wristbands directly, with a palette
and a set of patterns borrowed from the [cyberputer](../../cyberputer) scenes so
the wristband matches the rest of the costume.

No `.sub` files, no pre-generation: the app synthesises the OOK pulse train in
memory and hands it straight to the radio, so scenes animate live and colour and
brightness respond immediately.

## Controls

| Key | Action |
| --- | --- |
| Left / Right | Previous / next scene (wraps, so Left from the first scene lands on Blackout) |
| Up / Down | Master brightness, 0-15 |
| OK | Start / stop transmitting |
| OK (hold) | Blackout — jumps to the last scene and keeps sending until the band goes dark |
| Back | Exit, leaving the band on whatever it was last told |

The Flipper's own RGB LED mirrors the colour being sent, so you can tell the app
is alive without looking at the band. `RUN` in the header means it is on air.

## Scenes

Thirteen, deliberately. Dim solids are left out because the brightness control
already covers them, and scenes that were variations on a neighbour were cut
rather than kept to pad the list.

| Scene | What it does |
| --- | --- |
| Neon City | Crossfades violet → pink → amber → cyan, the skyline sign cycle |
| Neon Odyssey | Three detuned sines drifting against each other, the plasma scene |
| Night Drift | The long-evening scene: a 32s hue drift under a 17s swell, staying at the cool end |
| Signal Rain | Steel drizzle with bright cyan glyphs falling through it |
| Neon Radar | Amber ping, then a decaying cyan trail behind the sweep |
| Glitch | Violet hold punched through by corrupted frames |
| Strobe | 5 Hz hard white strobe |
| Violet, Cyan, Pink, Amber, White | Solid sign colours |
| Blackout | Off |

Most patterns run at about 10 frames per second, which is as fast as the
protocol goes — one block occupies roughly 102 ms of air time. Night Drift
runs at half that, since its fades are slow enough not to need the frames and
it halves the air time and battery draw. Solid colours re-send twice a second
so a band that was out of range picks the colour up when it comes back.

### About the colours

The palette starts from the cyberputer renderer and is then **saturated**. That
palette targets an emissive screen on a near-black background, where the
surrounding black supplies the contrast, so every accent has all three channels
lit — violet `0xb080ff` is `(11,8,15)`. A wristband LED has no dark surround,
and those values just read as washed out on it. Pulling the white floor out
while keeping the hue and the peak level gives the colour the palette was
reaching for:

| Scene | Palette | On the band |
| --- | --- | --- |
| Violet | `0xb080ff` → 11,8,15 | 6,0,15 |
| Cyan | `0x65ffe0` → 6,15,14 | 0,15,13 |
| Pink | `0xff59cc` → 15,5,12 | 15,0,10 |
| Amber | `0xffca73` → 15,12,7 | 15,9,0 |
| White | `0xe4fff9` → 14,15,15 | unchanged — a near-grey has no hue to saturate toward |

## Building and installing

Built and tested against the Momentum SDK (`mntm-012`, API 87.1) on hardware.

```sh
# once, to fetch the SDK for your firmware
ufbt update --index-url=https://up.momentum-fw.dev/firmware/directory.json

cd flipper
ufbt              # build -> dist/gflai_neon.fap
ufbt launch       # build, upload and run on a connected Flipper
```

For stock firmware use plain `ufbt update`; for Unleashed or RogueMaster point
`--index-url` at their SDK index. The app only uses documented `subghz_devices`
and `gui` APIs, so it should rebuild against any of them, but a `.fap` built for
one firmware will not load on another — rebuild rather than copying the binary.

To install without `ufbt`, copy `dist/gflai_neon.fap` to `/ext/apps/Sub-GHz/`
on the SD card.

## Protocol

Documented in the [top-level README](../README.md). Two details there are worth
repeating because the app depends on them and `generate.py` does not quite agree
with the captures:

- The preamble on the wire is the **nibble** `0101`, not a full `0x50` byte.
  `generate.py` pads it to eight bits and appends a separate short blip;
  `gflai_tx.c` emits what the captures actually contain.
- Every burst ends with a lone 200 µs mark followed by a long gap — 1600 µs
  between bursts within a block, 5600 µs between blocks. One block is the
  preamble burst plus two identical payload repeats.

A payload is `AA FF R G B CK 00`, where each colour byte carries its level in
the upper nibble and `CK` is `((R ^ G ^ B ^ 5) << 4) | 5`.

## Tests

`test/test_encoder.py` compiles the real `gflai_tx.c` on the host against small
stub headers and diffs the pulses it produces against the captures in
`../captures`:

```sh
python3 flipper/test/test_encoder.py
```

It checks a whole block symbol-for-symbol against `01_red.sub`, the payload
bytes against each hand-trimmed single-packet capture, the checksum against
every distinct packet in `07_crossfade.sub`, and each timing constant against
the median of the same class across all captures.

## Notes

- Transmits on 433.92 MHz. The app asks the firmware whether TX is permitted for
  your region and shows an error instead of transmitting if it is not.
- A running pattern keeps the radio busy more or less continuously, which is
  what the original GFLAI transmitter does, but it is a lot of air time and a
  lot of battery. Stop it with OK when you are not using it.
- Strobe flashes at 5 Hz. Skip it around anyone photosensitive.
