import os, sys, struct, md5

output, datafile, iconarc, bannerarc, soundbns, namesfile = sys.argv[1:]

data = open(datafile,"r").read()

names={}

for i in open(namesfile,"r"):
	a,b = i.split("=")
	while b[-1] == "\n":
		b = b[:-1]
	b = b.replace("\\n","\n")
	names[a] = b.decode("utf-8")

def getsize(x):
	return os.stat(x).st_size

def pad(x,l):
	if len(x) > l:
		raise ValueError("%d > %d",len(x),l)
	n = l-len(x)
	return x + "\x00"*n

imet = "\x00"*0x40
imet += struct.pack(">4sIIIIII","IMET",0x600,3,getsize(iconarc),getsize(bannerarc),getsize(soundbns),1)

for i in ["jp", "en", "de", "fr", "sp", "it", "nl", "cn", None, "ko", "id"]:
	try:
		imet += pad(names[i].encode("UTF-16BE"),0x54)
	except KeyError:
		imet += "\x00"*0x54
imet += "\x00"*(0x600 - len(imet))

imet = imet[:-16] + md5.new(imet).digest()

open(output,"w").write(imet)

f = open(sys.argv[1],"w")
f.write(imet)
f.write(data)

fsize = f.tell()

if (fsize % 20) != 0:
	f.write("\x00"*(20-(fsize%20)))

f.close()
