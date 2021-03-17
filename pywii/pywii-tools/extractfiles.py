#!/usr/bin/env python3

import sys, os, os.path
import pywii as wii

wii.loadkeys()

def parseint(d):
	if len(d) > 2 and d[0:2].lower()=='0x':
		return int(d,16)
	return int(d)

if len(sys.argv) < 4 or len(sys.argv) > 7:
	print("Usage:")
	print(" python %s <encrypted ISO> <partition number> <root path to extract to> "%sys.argv[0])
	sys.exit(1)

iso_name, partno, data_name = sys.argv[1:4]
partno = int(partno)
part_offset = 0
data_offset = 0
copy_length = None

if len(sys.argv) >= 5:
	part_offset = parseint(sys.argv[4])
if len(sys.argv) == 6:
	copy_length = parseint(sys.argv[5])

if copy_length is not None and copy_length < 0:
	print("Error: negative copy length")
	sys.exit(1)

disc = wii.WiiDisc(iso_name)
disc.showinfo()
part = wii.WiiCachedPartition(disc, partno, cachesize=32, debug=False, checkhash=False)
if part_offset >= part.data_bytes:
	print("Error: Offset past end of partition")
	sys.exit(1)
if copy_length is None:
	copy_length = part.data_bytes - part_offset
if copy_length > (part.data_bytes - part_offset):
	print("Error: Length too large")
	sys.exit(1)
	
dataf = open(data_name, "wb")
left = copy_length
offset = part_offset
while left > 0:
	blocklen = min(left, 4*1024*1024)
	d = part.read(offset, blocklen)
	if len(d) != blocklen:
		print("Part EOF reached!")
		sys.exit(1)
	dataf.write(d)
	offset += blocklen
	left -= blocklen

dataf.close()

