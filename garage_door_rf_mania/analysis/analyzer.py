#!/usr/bin/env python

from math import pow
import sys

filePath = sys.argv[1]

def tern2bin(ternary, opt):

    decimal = 0
    for i in range (0, len(ternary)):
        decimal += int(ternary[i]) * pow(3, len(ternary) - i - 1)
    decimal = int(decimal)

    if opt == 'dec':
        return '{:010d}'.format(decimal)        #decimal
    elif opt == 'bin':
        return '{:032b}'.format(decimal)       #binary
    elif opt == 'hex':
        return '{:08x}'.format(decimal)        #hex
    else:
        raise Exception('invalid option')

def bitstream():

    f = open(filePath)
    lines = f.readlines()
    bitstream = ""
    op = 'bin'

    for i in range(0, len(lines)):
        frames = lines[i].split()

        f1 = tern2bin(frames[0], op)
        f2 = tern2bin(frames[1], op)

        print f1, f2

        #bitstream += (f1 + f2)

    #print bitstream

def extract(f1, f2):

    bits = f1 + f2
    fixed = ""
    rolling = ""

    for i in range (0, len(bits)):
        if i % 2 == 0:
            fixed += bits[i]
        else:
            rolling += bits[i]
    return fixed, rolling

def interleaf():

    f = open(filePath)
    lines = f.readlines()
    op = 'hex'
    reverse_roll = True

    for i in range(0, len(lines)):
        frames = lines[i].split()
        fixed, rolling = extract(frames[0], frames[1])

        if reverse_roll:
            rolling = rolling[::-1]

        fixed = tern2bin(fixed, op)
        rolling = tern2bin(rolling, op)

        print i, "   ", fixed[0], fixed[1:4], fixed[4:8], rolling

interleaf()
#bitstream()
