#!/usr/bin/env python3
import sys
import pywii as wii

if len(sys.argv) != 4:
	print("Usage: %s keyfile.priv infile outfile"%sys.argv[0])
	sys.exit(1)

if sys.argv[1] == "-":
	k = sys.stdin.read()
else:
	k = open(sys.argv[1],"rb").read()

if len(k) != 30:
	print("Failed to read private key")
	sys.exit(2)

indata = open(sys.argv[2],"rb").read()
sha = wii.SHA.new(indata).digest()

print("SHA1: %s"%sha.encode('hex'))
print()
print("Signature:")
r,s = wii.ec.generate_ecdsa(k,sha)
print("R =",r[:15].encode('hex'))
print("   ",r[15:].encode('hex'))
print("S =",s[:15].encode('hex'))
print("   ",s[15:].encode('hex'))

outdata = "SIG0" + r + s + indata

fd = open(sys.argv[3],"wb")
fd.write(outdata)
fd.close()
