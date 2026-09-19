#!/usr/bin/env python3
"""Checks the C encoder against the original ShmooCon captures.

Builds test/dump_frame.c on the host, which pulls in the real gflai_tx.c, then
compares the pulses it produces with the ones recorded off the air.  Run from
anywhere:  python3 flipper/test/test_encoder.py
"""

import pathlib
import subprocess
import sys
import tempfile

HERE = pathlib.Path(__file__).resolve().parent
FLIPPER = HERE.parent
CAPTURES = FLIPPER.parent / "captures"

# A mark or gap is "long" above this, in microseconds.
SYMBOL_THRESHOLD = 400
# Above this a gap is a burst separator rather than part of a bit.
FRAME_THRESHOLD = 1000
# Above this it separates whole blocks rather than bursts within one.
BLOCK_THRESHOLD = 3000

failures = []


def check(name, ok, detail=""):
    print(f"{'PASS' if ok else 'FAIL'}  {name}")
    if not ok:
        if detail:
            print("        " + detail.replace("\n", "\n        "))
        failures.append(name)


def load_pairs(path):
    values = []
    for line in path.read_text().splitlines():
        if line.startswith("RAW_Data"):
            values += [int(v) for v in line.split()[1:]]
    return [(values[i], values[i + 1]) for i in range(0, len(values) - 1, 2)]


def symbolise(pairs):
    """Maps each mark/gap pair onto a timing-independent symbol.

    Ls = zero bit, Sl = one bit, SF = burst separator, SB = block separator.
    """
    out = []
    for mark, gap in pairs:
        gap = abs(gap)
        m = "S" if mark < SYMBOL_THRESHOLD else "L"
        if gap < SYMBOL_THRESHOLD:
            g = "s"
        elif gap < FRAME_THRESHOLD:
            g = "l"
        elif gap < BLOCK_THRESHOLD:
            g = "F"
        else:
            g = "B"
        out.append(m + g)
    return out


def bits_of(symbols):
    return "".join("1" if s == "Sl" else "0" for s in symbols if s in ("Sl", "Ls"))


def payload_bytes(bits):
    return " ".join(f"{int(bits[i:i + 8], 2):02x}" for i in range(0, len(bits) - 7, 8))


def build():
    binary = pathlib.Path(tempfile.mkdtemp()) / "dump_frame"
    subprocess.run(
        ["cc", "-std=gnu11", "-Wall", "-Wextra", "-Werror",
         "-I", str(HERE / "stub"), "-o", str(binary), str(HERE / "dump_frame.c")],
        check=True,
    )
    return binary


def generate(binary, r, g, b):
    result = subprocess.run(
        [str(binary), str(r), str(g), str(b)], capture_output=True, text=True, check=True
    )
    values = [int(line) for line in result.stdout.splitlines() if not line.startswith("#")]
    return [(values[i], values[i + 1]) for i in range(0, len(values) - 1, 2)]


def main():
    if not CAPTURES.is_dir():
        print(f"captures not found at {CAPTURES}", file=sys.stderr)
        return 2

    binary = build()

    # 1. A whole block, against a block lifted out of the middle of a real capture.
    captured = symbolise(load_pairs(CAPTURES / "01_red.sub"))
    starts = [i for i, s in enumerate(captured) if s == "SB"]
    reference = captured[starts[0] + 1 : starts[1] + 1]
    generated = symbolise(generate(binary, 15, 0, 0))
    check(
        "block structure matches captures/01_red.sub",
        generated == reference,
        f"generated {len(generated)} symbols, captured {len(reference)}\n"
        f"generated: {' '.join(generated[:8])} ...\n"
        f"captured:  {' '.join(reference[:8])} ...",
    )

    # 2. Payload bytes, against the hand-trimmed single-packet captures.
    for name, (r, g, b) in {
        "09_off_min.sub": (0, 0, 0),
        "11_green_min.sub": (0, 15, 0),
        "12_red_min.sub": (15, 0, 0),
        "10_white_min.sub": (15, 15, 15),
    }.items():
        expected = payload_bytes(bits_of(symbolise(load_pairs(CAPTURES / name)))[3:59])
        actual = payload_bytes(bits_of(symbolise(generate(binary, r, g, b)))[3:59])
        check(f"payload matches captures/{name}", actual == expected,
              f"generated {actual!r}, captured {expected!r}")

    # 3. Checksums, against every distinct packet in the fade capture.
    fade = symbolise(load_pairs(CAPTURES / "07_crossfade.sub"))
    seen, mismatched = set(), []
    run = []
    for symbol in fade:
        if symbol in ("Sl", "Ls"):
            run.append("1" if symbol == "Sl" else "0")
            continue
        if len(run) == 56:
            packet = [int("".join(run)[i:i + 8], 2) for i in range(0, 56, 8)]
            if packet[0] == 0xAA and tuple(packet) not in seen:
                seen.add(tuple(packet))
                r, g, b = packet[2] >> 4, packet[3] >> 4, packet[4] >> 4
                expected = packet[5]
                actual = (((r ^ g ^ b ^ 5) << 4) | 5) & 0xFF
                if actual != expected:
                    mismatched.append(f"{r:x}{g:x}{b:x}: got {actual:02x} want {expected:02x}")
        run = []
    check(
        f"checksum agrees on all {len(seen)} packets in captures/07_crossfade.sub",
        seen and not mismatched,
        "\n".join(mismatched[:5]),
    )

    # 4. The constants the encoder emits sit inside the spread the captures show.
    marks_short, marks_long, gaps_short, gaps_long, gaps_frame, gaps_block = ([] for _ in range(6))
    for capture in sorted(CAPTURES.glob("0*.sub")) + sorted(CAPTURES.glob("1*.sub")):
        for mark, gap in load_pairs(capture):
            gap = abs(gap)
            (marks_short if mark < SYMBOL_THRESHOLD else marks_long).append(mark)
            if gap < SYMBOL_THRESHOLD:
                gaps_short.append(gap)
            elif gap < FRAME_THRESHOLD:
                gaps_long.append(gap)
            elif gap < BLOCK_THRESHOLD:
                gaps_frame.append(gap)
            else:
                gaps_block.append(gap)

    # Outliers here are real: the fade captures carry multi-millisecond idle
    # stretches that land in the same buckets, so compare against the median.
    for label, nominal, observed in (
        ("short mark 200us", 200, marks_short),
        ("long mark 600us", 600, marks_long),
        ("short gap 200us", 200, gaps_short),
        ("long gap 600us", 600, gaps_long),
        ("frame gap 1600us", 1600, gaps_frame),
        ("block gap 5600us", 5600, gaps_block),
    ):
        observed.sort()
        median = observed[len(observed) // 2]
        drift = abs(nominal - median) / median
        check(
            f"{label} within 10% of captured median {median}us (n={len(observed)})",
            drift < 0.10,
            f"nominal {nominal}us is {drift:.0%} off the captured median",
        )

    print()
    if failures:
        print(f"{len(failures)} check(s) failed")
        return 1
    print("all checks passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
