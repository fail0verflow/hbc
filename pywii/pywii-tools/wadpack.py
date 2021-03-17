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
indir = args.pop(0)

tmd = wii.WiiTmd(open(os.path.join(indir, "tmd"), "rb").read())
tik = wii.WiiTik(open(os.path.join(indir, "cetk"), "rb").read())

certs, certlist = wii.parse_certs(open(os.path.join(indir, "certs"), "rb").read())

footer = open(os.path.join(indir, "footer"), "rb").read()

wad = wii.WiiWadMaker(wadfile, tmd, tik, certlist, footer)

for i,ct in enumerate(tmd.get_content_records()):
	data = open(os.path.join(indir, "%08X" % ct.cid), "rb").read()
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
