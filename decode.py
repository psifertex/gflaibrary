#!/usr/bin/which python3
import sys
from math import ceil

fn = sys.argv[1]
vals = []

with open(fn,'r') as f:
	data = f.readlines()
	for line in data:
		if line.startswith("RAW_Data"):
			line=line.split()[1:]
			line=map(int,line)
			vals+=list(line)

bs = ''
started = False
output = b''


pause = 0
for pair in [vals[i : i + 2] for i in range(0, len(vals), 2)]:
	if len(pair) < 2:
		break
	if abs(pair[1]) > 1000:
		# fill byte
		bs += '1'
		pad = len(bs) % 8
		if pad or len(bs) == 0:
			bs += '0' * (8 - pad)
		pause = pair[1]
	elif abs(pair[0]) > abs(pair[1]):
		bs += '0'
	else:
		bs += '1'
	if len(bs) % 8 == 0:
		intout = int(bs, 2)
		if pause:
			print(' ' + '/'*ceil(abs(pause)//1500) + ' ')
			pause = 0
		else:
			print(f'{intout:02x} ', end='')
		bs = ''

# pretty-print the hex output:
#s = ' '.join([f'{b:02x}' for b in output])
#print(s)




