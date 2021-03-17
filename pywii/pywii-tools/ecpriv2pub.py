#!/usr/bin/env python3

import sys
import pywii as wii

if len(sys.argv) not in (2,3):
	print("Usage: %s keyfile.priv [keyfile.pub]"%sys.argv[0])
	sys.exit(1)

if sys.argv[1] == "-":
	k = sys.stdin.read()
else:
	k = open(sys.argv[1],"rb").read()
if len(k) != 30:
	print("Failed to read private key")
	sys.exit(2)

print("Public key:")
q = wii.ec.priv_to_pub(k)
pq = q.encode('hex')
print("X =",pq[:30])
print("   ",pq[30:60])
print("Y =",pq[60:90])
print("   ",pq[90:])

if len(sys.argv) == 3:
	fd = open(sys.argv[2],"wb")
	fd.write(q)
	fd.close()
	print("Saved public key to %s"%sys.argv[2])
