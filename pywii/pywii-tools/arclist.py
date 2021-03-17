#!/usr/bin/env python3

import sys, os, os.path, struct
import pywii as wii

arc = open(sys.argv[1], "rb")

tag, fstoff, fstsize, dataoff = struct.unpack(">IIII16x",arc.read(0x20))

arc.seek(fstoff)
fst = arc.read(fstsize)

fst = wii.WiiFST(fst)

fst.show()
