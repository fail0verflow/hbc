#!/usr/bin/env python3

import sys, os, os.path
import pywii as wii

wii.loadkeys()

wadfile = sys.argv[1]
indir = sys.argv[2]

tmd = wii.WiiTmd(open(indir+os.sep+"tmd", "rb").read())
tik = wii.WiiTik(open(indir+os.sep+"cetk", "rb").read())

certs, certlist = wii.parse_certs(open(os.path.join(indir, "certs"), "rb").read())

footer = open(indir+os.sep+"footer", "rb").read()

wad = wii.WiiWadMaker(wadfile, tmd, tik, certlist, footer, nandwad=True)

for i,ct in enumerate(tmd.get_content_records()):
	data = open(indir+os.sep+"%08X"%ct.cid, "rb").read()
	wad.adddata(data,ct.cid)

wad.finish()

if not wad.tik.signcheck(wad.certs):
	wad.tik.null_signature()
	wad.tik.brute_sha()
	wad.updatetik()

if not wad.tmd.signcheck(wad.certs):
	wad.tmd.null_signature()
	wad.tmd.brute_sha()
	wad.updatetmd()
