#!/usr/bin/env python3

import sys, os, os.path
import pywii as wii

wii.loadkeys()

tikfile = sys.argv[1]
print("fixing Tik file %s " % tikfile)
tik = wii.WiiTik(open(tikfile, "rb").read())
tik.null_signature()
tik.brute_sha()
tik.update()
f = open(tikfile,"wb")
f.write(tik.data)
f.close()

