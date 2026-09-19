# Flipper Zero Light Show

Trigger GFLAI lights via your Flipper Zero

# Overview

This project was created during [ShmooCon 2025](https://shmoocon.org/) in response to a [challenge](https://lightsatshmoo.free.nf/) by [Rob Joyce](https://x.com/RGB_Lights) during his presentation. All research and code done by [Jordan](https://github.com/psifertex) and [Josh](https://github.com/negasora).

The goal was to first decode and analyze the RF used in the control over the RGB wands, wrist-bands, hats, and other devices. The second was to generate a [Flipper Zero](https://flipperzero.one/) application to allow arbitrary control.

RF captures were provided for replay on both Flipper Zeros and HackRFs.

# Analysis

## Decoding RF

The first step toward decoding the protocol was minimizing the existing captures. Noting that patterns generally repeated and long delays (larger values) in the [sub](https://developer.flipper.net/flipperzero/doxygen/subghz_file_format.html) file format likely indicated a repeating pattern, the RAW_Data field was truncated manually. 

Next, we had to figure out which encoding mechanism was being used. For this, we originally tried [URH](https://github.com/jopohl/urh) but without a solid understanding of the application (or even much about RF protocols themselves!) we decided it was a bit overkill. Thankfully, the [Flipper Zero Lab](https://lab.flipper.net/pulse-plotter) has an excellent Pulse-Plotter utility that can automatically predict the encoding, but heuristically guess the proper pulse length to decode the data. After trying many different settings we found that the encoding was PWM, with a short/long of ~200/600. In our basic understanding you could see that this meant each bit was encoded in a pair of values. An on value and an off value and the numbers in the sub file were simple the transmit time and then delay time between transmit. 

Pauses indicate breaks in data and the on and off values could otherwise be treated as simple zeros and ones!

At this point we decided to just write our own [decoder](./decode.py) for the plots since the format was so simple. This let us begin to dig into the actual data so that we could better understand the format.

## Provided Captures

In the `captures` subfolder, you can see the 1-8 capture files that were provided demonstrating different patterns of lights. The additional files were created either manually by trimming the large files or by the generator python script we wrote later.

## GFLAI RF Protocol

The transmissions were captures from a device that took [DMX](https://en.wikipedia.org/wiki/DMX512) packets from a tool like [xlights](https://www.xlights.org/) and broadcast them on a custom RF format to available devices.

Once we'd figured out the RF encoding we began to exampel the minimize files. The data wasn't much. Here's the single red/green "packets":

```
$ python3 decode.py captures/11_green_min.sub
 /
aa ff 00 f0 00 a5 00

$ python3 decode.py captures/12_red_min.sub
 /
aa ff f0 00 00 a5 00
```

Cleaned up, it's pretty obvious the difference! The 3 bytes in the middle clearly represent RGB values, at least in their upper nibbles. It didn't look like the lower nibbles did anything in our testing, so we think there's only 16 levels of brightness for each color that can be mixed.

At this point we thought we were basically done and could easily replay any color we wanted in any pattern with the data we had. At least, we thought that right up until we tried to display purple by maxing both red and blue... nothing happened.

## The Checksum

At this point we realized something else was getting in our way. Thanfully the "off" file gave us a hint:

```
$ python3 decode.py captures/09_off_min.sub
 /
aa ff 00 00 00 55 00
```

We noticed that the upper nibble of the byte following the colors was changing when the values of the colors changed, but only sometimes. For example, white had the same value as a single RGB value:

```
$ python3 decode.py captures/04_white.sub|head
 /
aa ff f0 f0 f0 a5 00  /
aa ff f0 f0 f0 a5 00  ///
 /
aa ff f0 f0 f0 a5 00  /
aa ff f0 f0 f0 a5 00  ///
```

Eventually we realized this must be some checksum value. In the end, we determined it's a [simple XOR](https://github.com/psifertex/gflaibrary/blob/912388f73ca02a90e1e277012e6e7aa4f29fc40e/generate.py#L35-L38) that includes the values of the RGB as well as the lower libble of the byte that follows it. This `5` value never changed in all of the captures we received, and changing it to any of the other 15 possible values resulted in our devices ignoring the packets so if that field can be used to do anything else, we haven't figured out what it is yet!

Now we could finally produce arbitrary colors and generate specific packets and solve the first challenge!

The file packet breakdown is:

```
50      # wakeup byte/initialization
aa ff   # fixed byte sequence as far as we can tell, could also be group code related
00      # Upper nibble contains 0-15 hex for RED
00      # Upper nibble contains 0-15 hex for GREEN
00      # Upper nibble contains 0-15 hex for BLUE
55      # Checksum and fixed value of `5`, unknown
00      # Unknown, but potentially contains a group code? Need devices in different groups to confirm.
XX      # Either contains an 80 or 50 depending on whether the packet was a 
```

## Refinements from re-reading the captures

Writing the [Flipper app](./flipper) meant going back to the raw pulses, and two
details turned out slightly different from what the generator emits. Both are
cosmetic in the sense that the original approach worked, but the app reproduces
what the captures actually contain:

- That leading `50` is a **nibble** on the wire, not a byte. The preamble burst
  is three bits `010`, then a lone 200us mark and a 1600us gap. `decode.py`
  zero-pads the trailing partial byte, which is what turns it into `0x50`.
- Every burst ends the same way: one short 200us mark followed by a long
  silence, 1600us between bursts inside a block and 5600us between blocks. A
  block is the preamble burst plus **two** identical payload repeats, and takes
  about 102ms, which is why the captured crossfade steps once per ~101ms.

`flipper/test/test_encoder.py` pins all of this down by diffing the app's
generated pulses against the captures.

# Transmit Script

We made a lot of other random utilities along the way most of which eventually got removed or replaced, but the `transmit.py` script we found useful for quickly iterating/triggering the flipper zero to upload .sub files and trigger them without having to manually interact with it each time. We also used the same script to upload the javascript sample app later. Note that you'll need to adjust the default path on a platform where the flipper zero shows up as a different usb serial device and put the name of your flipper into the 

# Flipper App

[`flipper/`](./flipper) holds **GFLAI Neon Drive**, a native Flipper Zero app
that controls the wristbands directly. It builds the OOK pulse train in memory
and feeds it to the radio, so there are no `.sub` files to generate or upload
and patterns animate live.

It ships a palette and a set of scenes taken from
[cyberputer](https://github.com/psifertex/cyberputer) — Neon City, Neon Odyssey,
Night Drift, Signal Rain, Neon Radar and friends — for costumes that need the
wristband to match everything else. The palette is saturated on the way across:
cyberputer's colours are tuned for an emissive screen on black, and need the
white floor pulled out of them before they look right on an LED.

```sh
cd flipper
ufbt launch
```

See [flipper/README.md](./flipper/README.md) for controls, the scene list, and
build instructions for firmware other than Momentum.

# Todo

The javascript application in `crowdled.js` was never finished — the native app
took its place. You can still load it onto your flipper by putting it in the
`/ext/apps/Scripts` folder and running it via the UI, but it does not work yet.

 - Add more patterns
 - Work out what the trailing `00` byte does; a group code would let one Flipper
   drive several bands independently
 - Find out whether the checksum nibble `5` means anything

# Credits

Thanks to [Rob Joyce](https://x.com/RGB_Lights) for the inspiration during ShmooCon 2025. See his [details page](https://lightsatshmoo.free.nf/) for more information.

While we never ended up using it, another partial attempt was based on the [flipper-zero-tutorials](https://github.com/jamisonderek/flipper-zero-tutorials), specifically the [signal send demo](https://github.com/jamisonderek/flipper-zero-tutorials/tree/main/subghz/apps/signal_send_demo) application from [Derek Jamison](https://github.com/jamisonderek) who has many great resources on flipper zero app development!
