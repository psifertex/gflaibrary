#!/usr/bin/which python3

from pyflipper.pyflipper import PyFlipper
import time

with open('.flipper', 'r') as file:
    flippername = file.read().strip()

modem = f"/dev/tty.usbmodemflip_{flippername}1"
outputpath = "/ext/subghz/rgb/output.sub"

flipper = PyFlipper(com=modem)

