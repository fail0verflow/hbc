#!/usr/bin/env python

import sys, os, os.path
import pywii as wii

wii.loadkeys()

savefile = sys.argv[1]
save = wii.WiiSave(savefile)
save.showcerts()
