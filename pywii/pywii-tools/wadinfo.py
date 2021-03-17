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
wad = wii.WiiWad(wadfile)
wad.showinfo()
