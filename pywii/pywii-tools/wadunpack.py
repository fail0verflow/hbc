#!/usr/bin/env python3

import sys, os, os.path
import pywii as wii

args = sys.argv[1:]

if args[0] == "-dpki":
	wii.loadkeys_dpki()
	args.pop(0)
else:
	wii.loadkeys()

wadfile = args.pop(0)
outdir = args.pop(0)
wad = wii.WiiWad(wadfile)
wad.showinfo()

if not os.path.isdir(outdir):
	os.mkdir(outdir)

for ct in wad.tmd.get_content_records():
	data = wad.getcontent(ct.index)
	f = open(os.path.join(outdir, "%08X" % ct.cid),"wb")
	f.write(data)
	f.close()

f = open(os.path.join(outdir, "cetk"),"wb")
f.write(wad.tik.data)
f.close()

f = open(os.path.join(outdir, "tmd"),"wb")
f.write(wad.tmd.data)
f.close()

f = open(os.path.join(outdir, "certs"),"wb")
for cert in wad.certlist:
	f.write(cert.data)
	wii.falign(f,0x40)
f.close()

f = open(os.path.join(outdir, "footer"),"wb")
f.write(wad.footer)
f.close()
