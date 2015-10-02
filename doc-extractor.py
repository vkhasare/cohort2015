#!/usr/bin/env python

#############################################################################
# Doc-extractor :
# Reads the Code source files and prepares the documentation file
# 'code-documentation'
#############################################################################

import sys
import os
import re

#List of all files which need to be read for documentation purpose.
D_Files = ["client.c", "common.h"]
docFile = "code-documentation"
printMode = False

docPtr = open(docFile, 'w+')

for fileName in D_Files:
    lines = open(fileName).read().splitlines()

    header = "------------------ File : %s ------------------\n\n" % fileName
    docPtr.write(header)
    print header

    for index in range(len(lines)):
        #if <doc> is hit, then start recording
        if re.search("<doc>", lines[index]):
            printMode = True
            continue
        #if </doc> is hit, then stop recording
        elif re.search("</doc>", lines[index]):
            printMode = False
            print "\n"
            docPtr.write("\n")

        if printMode:
            print lines[index]
            docPtr.write(lines[index])
            docPtr.write("\n")



