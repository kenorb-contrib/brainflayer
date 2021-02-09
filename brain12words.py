#!/usr/bin/env python
# encoding: utf-8
"""


#  Bip39 by TheRealLordFractal 2019
#  Updated by OguzDelioglu 2021

"""

import sys
import getopt
import random

help_message = '''
Bip39 12 Word Random Generator By TheRealLordFractal
Output File Saved as 12words.txt
Command Exntensions:
-n <x> or --number <x>: Number of sample code phrases given. (Default is 5)
-w <file> or --wordlist <file>: Uses another wordlist to generate code phrases from.
'''

CODEWORDS = open('wordlist.txt', 'r').readlines()


class Usage(Exception):
    def __init__(self, msg):
        self.msg = msg


def generate(prefix=False, number=5, lengthCount=12, isSpace=True):
    while number > 0:
        laststring = list()
        if prefix == 'TRUE':
            print("Not Supported.")
            # word1 = PREFIXES[int(random.uniform(0,len(PREFIXES)))]
        elif prefix:
            laststring = prefix
        else:
            for x in range(lengthCount):
                laststring.append(
                    CODEWORDS[int(random.uniform(0, len(CODEWORDS)))].rstrip())

            if isSpace:
                # print(" " + " ".join(laststring) + " ")
                print(" ".join(laststring))
            else:
                # print(" " + "".join(laststring) + " ")
                print("".join(laststring))

        number -= 1


def main(argv=None):
    isSpace = True
    number = 5
    prefix = False
    lengthCount = 1

    if argv is None:
        argv = sys.argv
    try:
        try:
            opts, args = getopt.getopt(argv[1:], "hn:p:i:vw:l:s", [
                                       "help", "number=", "prefix=", "wordlist=", "length=", "nospace"])
        except getopt.error, msg:
            raise Usage(msg)

        # option processing
        for option, value in opts:
            if option == "-v":
                verbose = True
            if option in ("-h", "--help"):
                raise Usage(help_message)
            if option in ("-n", "--number"):
                number = int(value)
            if option in ("-w", "--wordlist"):
                global CODEWORDS
                print "Importing: %s" % value
                CODEWORDS = open(value, 'r').readlines()
            if option in ("-s", "--nospace"):
                isSpace = False
            if option in ("-l", "--length"):
                lengthCount = int(value)
            if option in ("-p", "--prefixe"):

                print value

                if (value):
                    prefix = value
                else:
                    prefix = 'TRUE'

        generate(prefix, number, lengthCount, isSpace)

    except Usage, err:
        print >> sys.stderr, sys.argv[0].split("/")[-1] + ": " + str(err.msg)
        print >> sys.stderr, "\t for help use --help"
        return 2


if __name__ == "__main__":
    sys.exit(main())
