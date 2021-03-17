#!/usr/bin/env python3

import sys, os, os.path
import pywii as wii

args = sys.argv[1:]

if args[0] == "-dpki":
	wii.loadkeys_dpki()
	args.pop(0)
else:
	wii.loadkeys()

tmdfile = args.pop(0)

certs = None
if len(args) > 0:
	certs, certlist = wii.parse_certs(open(args.pop(0), "rb").read())

print("TMD file %s:"%tmdfile)
tmd = wii.WiiTmd(open(tmdfile, "rb").read())
tmd.showinfo(" ")
if certs is not None:
	tmd.showsig(certs," ")
	print("Certificates:")
	for cert in certlist:
		cert.showinfo(" - ")
		cert.showsig(certs,"    ")
