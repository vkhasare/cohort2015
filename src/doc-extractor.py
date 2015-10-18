#!/usr/bin/env python

#############################################################################
# Doc-extractor :
# Reads the Code source files and prepares the documentation file
# 'code-documentation'
#############################################################################

import sys
import os, glob
import re

#List of all files which need to be read for documentation purpose.
D_Files = ["client.c", "common.h", "client_ll.h", "print.h"]
docFile = "code-documentation"

"""
Function:
find_file(file_name)

Function to find the the relative path of the given file.
"""
def find_file(file_name):
    for root, dirs, files in os.walk(".", topdown=False):
        if file_name in files:
              return os.path.join(root, file_name)

"""
main function
"""
def main(argv):
    global D_Files
    global docFile
    printMode = False

    docPtr = open(docFile, 'w+')

    # Scanning all the files to be documented
    for fileName in D_Files:
        relfileName = find_file(fileName)
        lines = open(relfileName).read().splitlines()

        header = "------------------ File : %s ------------------\n\n" % fileName
        docPtr.write(header)

        for index in range(len(lines)):
            #if <doc> is hit, then start recording
            if re.search("<doc>", lines[index]):
                printMode = True
                continue
            #if </doc> is hit, then stop recording
            elif re.search("</doc>", lines[index]):
                printMode = False
                docPtr.write("\n")

            if printMode:
                docPtr.write(lines[index])
                docPtr.write("\n")

    docPtr.close()
    print "Documentation File Name: %s" % (docFile)


if __name__ == "__main__":
    main(sys.argv[1:])
