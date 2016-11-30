#!/usr/bin/env python2
import sys
import re
import pywii as wii

hash = wii.SHA.new(open(sys.argv[2]).read()).digest().encode("hex")
f = open(sys.argv[1], "r")
data = f.read()
f.close()
data = re.sub('@SHA1SUM@', hash, data)
open(sys.argv[3], "w").write(data)
