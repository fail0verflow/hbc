#!/usr/bin/env python3

import sys, os, os.path
import pywii as wii

def hexdump(s):
	return ' '.join(["%02x"%x for x in list(map(ord,s))])

isofile = sys.argv[1]
disc = WiiDisc(isofile)
disc.showinfo()
part = WiiPartition(disc,int(sys.argv[2]))
part.showinfo()

part.tmd.update_signature(file("signthree.bin").read())
part.tmd.brute_sha()
part.updatetmd()

part.showinfo()

