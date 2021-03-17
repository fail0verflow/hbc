#!/usr/bin/env python3

import sys, os, os.path
import pywii as wii

wii.loadkeys()

savefile = sys.argv[1]
save = wii.WiiSave(savefile)
save.showcerts()
