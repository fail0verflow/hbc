#!/usr/bin/env python3

import sys, os, os.path
import pywii as wii
try:
    from Cryptodome.Hash import SHA
except ImportError:
    from Crypto.Hash import SHA

wii.loadkeys()

args = sys.argv[1:]

tmdfile = args.pop(0)
indir = args.pop(0)

print("updating content records of TMD file %s" % tmdfile)
tmd = wii.WiiTmd(open(tmdfile, "rb").read())

for i, cr in enumerate(tmd.get_content_records()):
	data = open(os.path.join(indir, "%08X" % cr.cid), "rb").read()
	cr.sha = SHA.new(data).digest()
	cr.size = len(data)
	tmd.update_content_record(i, cr)

tmd.null_signature()
tmd.brute_sha()
tmd.update()
f = open(tmdfile, "wb")
f.write(tmd.data)
f.close()

