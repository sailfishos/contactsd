#!/usr/bin/python

import sys

mode = 0

lines = sys.stdin.readlines()

for line in lines:

    if mode == 3:
       mode = 0
       continue

    if mode == 2:
        mode = 3
        continue

    if mode == 1:
        mode = 2
        if last.find('.moc') > 0:
            continue
        
        if last[-6:-2] != ".cpp":
            continue
        
        print last[:-1], ":", line[:-1]
        continue

    if mode == 0:
        mode = 1
        last = line
        continue
