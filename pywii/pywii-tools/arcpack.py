#!/usr/bin/env python3

import sys, os, os.path, struct
import pywii as wii

fstb = wii.WiiFSTBuilder(0x20)

fstb.addfrom(sys.argv[2])

try:
	arc = open(sys.argv[1],"wb")
	# dummy generate to get length
	fstlen = len(fstb.fst.generate())
	dataoff = wii.align(0x20+fstlen,0x20)
	fst = fstb.fst.generate(dataoff)

	hdr = struct.pack(">IIII16x",0x55AA382d,0x20,fstlen,dataoff)
	arc.write(hdr)

	arc.write(fst)
	wii.falign(arc,0x20)
	for f in fstb.files:
		data = open(f, "rb").read()
		arc.write(data)
		wii.falign(arc,0x20)

	arc.close()
except:
	os.remove(sys.argv[1])
	raise
