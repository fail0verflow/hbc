#!/usr/bin/env python3

import sys, os, os.path
import pywii

pywii.loadkeys_dpki()

args = sys.argv[1:]
mode = args.pop(0)
infile = args.pop(0)
outfile = args.pop(0)
certfile = args.pop(0)
issuer = args.pop(0)

if sys.argv[1] == "-cetk":
	signed = pywii.WiiTik(open(infile, "rb").read())
elif sys.argv[1] == "-tmd":
	signed = pywii.WiiTmd(open(infile, "rb").read())
else:
	print("EYOUFAILIT")
	sys.exit(1)

certs, certlist = pywii.parse_certs(open(certfile).read())

signed.update_issuer(issuer)

if not signed.sign(certs):
	print("dpki signing failed")
	sys.exit(1)

open(outfile, "wb").write(signed.data)

print("successfully signed %s" % outfile)
sys.exit(0)

