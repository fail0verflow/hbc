import os, sys, struct, hashlib

output, datafile, iconarc, bannerarc, soundbns, namesfile = sys.argv[1:]

data = open(datafile,"rb").read()

names={}

for i in open(namesfile,"r"):
	a,b = i.split("=")
	while b[-1] == "\n":
		b = b[:-1]
	b = b.replace("\\n","\n")
	names[a] = b

def getsize(x):
	return os.stat(x).st_size

def pad(x,l):
	if len(x) > l:
		raise ValueError("%d > %d",len(x),l)
	n = l-len(x)
	return x + b"\x00"*n

imet = b"\x00"*0x40
imet += struct.pack(">4sIIIIII",b"IMET",0x600,3,getsize(iconarc),getsize(bannerarc),getsize(soundbns),1)

for i in ["jp", "en", "de", "fr", "sp", "it", "nl", "cn", None, "ko"]:
	try:
		imet += pad(names[i].encode("UTF-16BE"),0x54)
	except KeyError:
		imet += b"\x00"*0x54
imet += b"\x00"*(0x600 - len(imet))

imet = imet[:-16] + hashlib.md5(imet).digest()

open(output,"wb").write(imet)

f = open(sys.argv[1],"wb")
f.write(imet)
f.write(data)

fsize = f.tell()

if (fsize % 20) != 0:
	f.write(b"\x00"*(20-(fsize%20)))

f.close()
