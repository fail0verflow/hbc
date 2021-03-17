#!/usr/bin/env python3
import sys
import re
import pywii as wii

hash = wii.SHA.new(open(sys.argv[2]).read()).digest().encode("hex")
f = open(sys.argv[1], "rb")
data = f.read()
f.close()
data = re.sub('@SHA1SUM@', hash, data)
open(sys.argv[3], "wb").write(data)
