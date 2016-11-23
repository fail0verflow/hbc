import md5, sys, struct

data= open(sys.argv[1]).read()

digest = md5.new(data).digest()

hdr = struct.pack(">4sI8x","IMD5",len(data))

f2 = open(sys.argv[2],"w")
f2.write(hdr)
f2.write(digest)
f2.write(data)
f2.close()
