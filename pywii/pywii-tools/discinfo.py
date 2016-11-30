#!/usr/bin/env python2

import sys, os, os.path
import pywii as wii

wii.loadkeys()

isofile = sys.argv[1]
disc = wii.WiiDisc(isofile,readonly=True)
disc.showinfo()

partitions = disc.read_partitions()

parts = range(len(partitions))

try:
	pnum = int(sys.argv[2])
	partitions[pnum]
	parts = [pnum]
except:
	pass

for partno in parts:
	part = wii.WiiCachedPartition(disc,partno)
	part.showinfo()
	pdat = wii.WiiPartitionData(part)
	pdat.showinfo()

