#!/usr/bin/env python

#############################################################################
# Doc-extractor :
# Reads the Code source files and prepares the documentation file
# 'api-documentation'
#############################################################################

import sys
import os, glob
import re

#Destination file where documentation will be written.
docFile = "api-documentation"

"""
Function:
find_file(file_name)

Function to find the the relative path of the given file.
"""
def find_file(file_name):
    for root, dirs, files in os.walk("../", topdown=False):
        if file_name in files:
              return os.path.join(root, file_name)


"""
main function
"""
def main(argv):
    global docFile
    printMode = False

    docPtr = open(docFile, 'w+')

    #All files starting with .c or .h
    c_files = [f for f in os.listdir("../src/") if f.endswith('.c')]
    h_files = [f for f in os.listdir("../include/") if f.endswith('.h')]

    all_files = c_files + h_files

    # Scanning all the files to be documented
    for fileIndex in range(len(all_files)):

        fileName = all_files[fileIndex]
        relfileName = find_file(fileName)

        lines = open(relfileName).read().splitlines()

        headerMode = True
        header = "------------------ File : %s ------------------\n\n" % fileName

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
                #Write header only if there is atleast 1 doc tag
                if headerMode:
                    docPtr.write(header)
                    headerMode = False 
                docPtr.write(lines[index])
                docPtr.write("\n")

    docPtr.close()
    print "Documentation File Name: %s" % (docFile)


if __name__ == "__main__":
    main(sys.argv[1:])
