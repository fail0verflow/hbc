#!/usr/bin/env python3

import sys, os, os.path
import pywii as wii

wii.loadkeys()

tmdfile = sys.argv[1]
print("TMD file %s:"%tmdfile)
tmd = wii.WiiTmd(open(tmdfile, "rb").read())
tmd.null_signature()
tmd.brute_sha()
tmd.update()
tmd.parse()
tmd.showinfo(" ")
f = open(tmdfile,"wb")
f.write(tmd.data)
f.close()
