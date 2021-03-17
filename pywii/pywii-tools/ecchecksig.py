#!/usr/bin/env python3

import sys
import pywii as wii

if len(sys.argv) != 3:
	print("Usage: %s keyfile.[priv|pub] infile"%sys.argv[0])
	sys.exit(1)

if sys.argv[1] == "-":
	k = sys.stdin.read()
else:
	k = open(sys.argv[1],"rb").read()
if len(k) not in (30,60):
	print("Failed to read key")
	sys.exit(2)

if len(k) == 30:
	print("Key is a private key, generating public key...")
	q = wii.ec.priv_to_pub(k)
else:
	q = k

print("Public key:")
pq = q.encode('hex')
print("X =",pq[:30])
print("   ",pq[30:60])
print("Y =",pq[60:90])
print("   ",pq[90:])
print()

indata = open(sys.argv[2],"rb").read()

if len(indata) < 64 or indata[:4] != "SIG0":
	print("Invalid header")
	sys.exit(3)

r = indata[4:34]
s = indata[34:64]

sha = wii.SHA.new(indata[64:]).digest()
print("SHA1: %s"%sha.encode('hex'))

print()
print("Signature:")
print("R =",r[:15].encode('hex'))
print("   ",r[15:].encode('hex'))
print("S =",s[:15].encode('hex'))
print("   ",s[15:].encode('hex'))
print()

if wii.ec.check_ecdsa(q,r,s,sha):
	print("Signature is VALID")
else:
	print("Signature is INVALID")
	sys.exit(4)

