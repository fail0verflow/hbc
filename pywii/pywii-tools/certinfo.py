#!/usr/bin/env python3

import sys, os, os.path
import pywii as wii

args = sys.argv[1:]

if args[0] == "-dpki":
	wii.loadkeys_dpki()
	args.pop(0)
else:
	wii.loadkeys()

certfile = args.pop(0)

certs, certlist = wii.parse_certs(open(args.pop(0), "rb").read())

print("Certification file %s: " % certfile)
cert = wii.WiiCert(open(certfile, "rb").read())
cert.showinfo(" ")
cert.showsig(certs," ")

print("Certificates:")
for cert in certlist:
	cert.showinfo(" - ")
	cert.showsig(certs,"    ")
