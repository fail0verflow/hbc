#!/usr/bin/env python3

import sys, os, os.path
import pywii as wii

wii.loadkeys()

if len(sys.argv) != 5:
	print("Usage:")
	print(" python %s <encrypted ISO> <partition number> <apploader text> <apploader trailer>"%sys.argv[0])
	sys.exit(1)

iso_name, partno, app_name, trail_name = sys.argv[1:5]
partno = int(partno)

disc = wii.WiiDisc(iso_name)
disc.showinfo()
part = wii.WiiCachedPartition(disc, partno, cachesize=32, debug=False)
partdata = wii.WiiPartitionData(part)

apploader = partdata.apploader
apploader.showinfo()
open(app_name,"wb").write(apploader.text)
open(trail_name,"wb").write(apploader.trailer)
