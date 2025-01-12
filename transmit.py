#!/usr/bin/which python3

from pyflipper.pyflipper import PyFlipper
import argparse
import time
from pathlib import Path

with open('.flipper', 'r') as file:
    flippername = file.read().strip()

modem = f"/dev/tty.usbmodemflip_{flippername}1"

parser = argparse.ArgumentParser(description='Transmit file using Flipper Zero.')
parser.add_argument('--modem', type=str, default=modem, help='Modem device path')
parser.add_argument('--path', type=str, default="/ext/subghz/rgb/", help='Output file path')
parser.add_argument('--file', type=str, default="output.sub", help='Output file name')
parser.add_argument('--count', type=int, default=2, help='Number of times to transmit')
parser.add_argument('--upload', action='store_true', help='Flag to upload the file')
parser.add_argument('--notransmit', action='store_false', help='Disable transmitting')
args = parser.parse_args()

modem = args.modem
outputpath = args.path + args.file
count = args.count
transmit = not args.notransmit
upload = args.upload

#helper to make working with captures mildly easier
if (Path("captures") / args.file).is_file():
    prefix = "captures/"
else:
    prefix = ""

with open(prefix + args.file, 'r') as file:
    header = file.read()

flipper = PyFlipper(com=modem)

if upload:
    try:
        # In case the file doesn't exist
        print("Removing...")
        flipper.storage.remove(file=outputpath)
    except:
        pass
    print(f"Uploading to {outputpath}...")
    flipper.storage.write.start(outputpath)
    time.sleep(1)
    flipper.storage.write.send(header)
    time.sleep(1)
    flipper.storage.write.stop()
    time.sleep(1)

if transmit and ".sub" in args.file:
    print(f"Transmitting {outputpath}...")
    flipper._serial_wrapper.send(f"subghz tx_from_file {outputpath} {count} 0")
