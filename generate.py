#!/usr/bin/which python3

try:
    from pyflipper.pyflipper import PyFlipper
except:
    PyFlipper = None
    print("no flipping library")

import time
from dataclasses import dataclass

# Number of times to broadcast a color change
BROADCAST_TIMES = 5

@dataclass
class ColorCommand:
    red: int
    green: int
    blue: int
    brightness: int = 15
    delay_time: int = 0

    def __post_init__(self):
        assert 0 <= self.red <= 15
        assert 0 <= self.green <= 15
        assert 0 <= self.blue <= 15
        assert 0 <= self.brightness <= 15
        assert 0 <= self.delay_time <= 0xffffffff

        self.red = int(self.red * (self.brightness/15.0))
        self.green = int(self.green * (self.brightness/15.0))
        self.blue = int(self.blue * (self.brightness/15.0))


    def _calc_checksum(self) -> bytes:
        checksum_nibble = 5 # has to be 5
        assert 0 <= checksum_nibble <= 15
        return bytes([((self.red ^ self.green ^ self.blue ^ checksum_nibble) << 4) | checksum_nibble])

    def _encode_body(self) -> bytes:
        return b'\xaa\xff' + bytes([self.red << 4, self.green << 4, self.blue << 4]) + self._calc_checksum() + b'\x00'

    def encode(self) -> bytes:
        start_delay = b''
        if self.delay_time:
            start_delay = b'\xcd' + self.delay_time.to_bytes(4) + b'\xcd'
        return start_delay + b'\x80\xcc'.join(self._encode_body() for _ in range(BROADCAST_TIMES))


@dataclass
class ColorCommands:
    colors: list[ColorCommand]

    def encode(self):
        return b'\x50\xcc' + b'\x80\xcc'.join(i.encode() for i in self.colors)


header = '''Filetype: Flipper SubGhz RAW File
Version: 1
Frequency: 433920000
Preset: FuriHalSubGhzPresetOok650Async
Protocol: RAW
'''

input = ColorCommands(
    colors=[ColorCommand(red=15, green=0, blue=0, brightness=i, delay_time=250) for i in range(15, 0, -1)] + [ColorCommand(red=15, green=0, blue=0, brightness=i, delay_time=250) for i in range(1, 16)]
).encode()


print(input.hex(' '))

current_byte_is_delay = False
delay = 0

pairs: list[tuple[int, int]] = []

for b in input:
    if b == 0xcc:
        pairs.append((60, -1600))
        continue
    elif b == 0xcd:
        if current_byte_is_delay:
            pairs.append((200, -650*delay))
            current_byte_is_delay = False
        else:
            current_byte_is_delay = True
            delay = 0
        continue
    elif current_byte_is_delay:
        delay = (delay << 8) | b
        continue

    for bit in f'{b:08b}':
        if bit == '1':
            pairs.append((200, -600))
        else:
            pairs.append((600, -200))

# 256 pairs per line
for i in range(0, len(pairs), 256):
    header += 'RAW_Data: ' + ' '.join(f'{a} {b}' for a,b in pairs[i:i+256]) + '\n'

header += f"# {input.hex(' ')}"

if PyFlipper:
    with open('.flipper', 'r') as file:
        flippername = file.read().strip()
    modem = f"/dev/tty.usbmodemflip_{flippername}1"

    flipper = PyFlipper(com=modem)
    outputpath = "/ext/subghz/rgb/output.sub"
    try:
        # In case the file doesn't exist
        flipper.storage.remove(file=outputpath)
    except:
        pass
    flipper.storage.write.start(outputpath)
    time.sleep(1)
    flipper.storage.write.send(header)
    time.sleep(1)
    flipper.storage.write.stop()
    time.sleep(1)
    flipper._serial_wrapper.send(f"subghz tx_from_file /ext/subghz/rgb/output.sub 2 0")

with open('captures/output.sub', 'w') as f:
    f.write(header)
