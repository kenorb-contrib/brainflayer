#!/usr/bin/env python
# encoding: utf-8
import line

ids = {}
found = []
with open("wordlist.txt") as file1:
    for line in file1.lines():
        id_ = line.split()[0]
        ids[id_] = line

with open("base") as file2:
    for line in file2.lines():
        id_ = line.split()[0]
        if id_ in ids:
            found.append(f"{ids[id_]} {line}")
