#!/usr/bin/env python3

import sys, os
import pywii as wii

if len(sys.argv) != 2:
	print("Usage: %s keyfile.priv"%sys.argv[0])
	sys.exit(1)

print("Generating private key...")

k = wii.ec.gen_priv_key()

print("Private key:")
pk = k.encode('hex')
print("K =",pk[:30])
print("   ",pk[30:])

print()
print("Corresponding public key:")
q = wii.ec.priv_to_pub(k)
pq = q.encode('hex')
print("X =",pq[:30])
print("   ",pq[30:60])
print("Y =",pq[60:90])
print("   ",pq[90:])

fd = open(sys.argv[1],"wb")
os.fchmod(fd.fileno(), 0o600)
fd.write(k)
fd.close()

print("Saved private key to %s"%sys.argv[1])
