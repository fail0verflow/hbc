import hashlib, sys, struct

data= open(sys.argv[1], "rb").read()

digest = hashlib.md5(data).digest()

hdr = struct.pack(">4sI8x",b"IMD5",len(data))

f2 = open(sys.argv[2],"wb")
f2.write(hdr)
f2.write(digest)
f2.write(data)
f2.close()
