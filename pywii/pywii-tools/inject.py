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
	print(" python %s <encrypted ISO> <partition number> <file to inject> [Partition offset] [data offset] [length]"%sys.argv[0])
	sys.exit(1)

iso_name, partno, data_name = sys.argv[1:4]
partno = int(partno)
part_offset = 0
data_offset = 0
copy_length = None

if len(sys.argv) >= 5:
	part_offset = parseint(sys.argv[4])
if len(sys.argv) >= 6:
	data_offset = parseint(sys.argv[5])
if len(sys.argv) == 7:
	copy_length = parseint(sys.argv[6])

data_len = os.stat(data_name).st_size

if copy_length == None:
	copy_length = data_len - data_offset
copy_end = data_offset + copy_length
if copy_length < 0:
	print("Error: negative copy length")
	sys.exit(1)
if copy_end > data_len:
	print("Error: data file is too small")
	sys.exit(1)

disc = wii.WiiDisc(iso_name)
disc.showinfo()
part = wii.WiiCachedPartition(disc, partno, cachesize=32, debug=False, checkhash=False)

dataf = open(data_name, "rb")
dataf.seek(data_offset)
left = copy_length
offset = part_offset
while left > 0:
	blocklen = min(left, 4*1024*1024)
	d = dataf.read(blocklen)
	if len(d) != blocklen:
		print("File EOF reached!")
		sys.exit(1)
	part.write(offset, d)
	offset += blocklen
	left -= blocklen

part.flush()
part.update()
part.tmd.null_signature()
part.tmd.brute_sha()
part.updatetmd()



