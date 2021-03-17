#!/usr/bin/env python3

import sys, os, os.path
import pywii as wii

wii.loadkeys()

if len(sys.argv) != 4:
	print("Usage:")
	print(" python %s <encrypted ISO> <partition number> <IOS version>"%sys.argv[0])
	print(" IOS version should be just the minor number (16, 33, etc) in decimal")
	sys.exit(1)

iso_name, partno, ios = sys.argv[1:4]
partno = int(partno)
iosno = int(ios)

disc = wii.WiiDisc(iso_name)
disc.showinfo()
part = wii.WiiCachedPartition(disc, partno, cachesize=32, debug=False)
part.tmd.sys_version = 0x100000000 + iosno
part.tmd.update()
part.tmd.null_signature()
part.tmd.brute_sha()
part.updatetmd()
part.showinfo()


