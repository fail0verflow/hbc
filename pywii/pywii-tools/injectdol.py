#!/usr/bin/env python3

import sys, os, os.path
import pywii as wii

wii.loadkeys()

if len(sys.argv) != 4:
	print("Usage:")
	print(" python %s <encrypted ISO> <partition number> <dol to inject>"%sys.argv[0])
	sys.exit(1)

iso_name, partno, dol_name = sys.argv[1:4]
partno = int(partno)

doldata = open(dol_name, "rb").read()

disc = wii.WiiDisc(iso_name)
disc.showinfo()
part = wii.WiiCachedPartition(disc, partno, cachesize=32, debug=False)
partdata = wii.WiiPartitionData(part)
partdata.replacedol(doldata)

part.flush()
part.update()
part.tmd.null_signature()
part.tmd.brute_sha()
part.updatetmd()
part.showinfo()


