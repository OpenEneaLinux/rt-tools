bitcalc: New tool for bit calculations
======================================

This is a very simple tool for performing bit calculations. It is intended as a
first step towards getting rid of bit calculations in the partrt script, since
this part of the partrt script is pretty messy.

It uses a postfix notation, and supports the following data formats:
 - Hex bit mask of arbitrary length, with optional "," for each 32-bit word,
   optionally starting with 0x. E.g.: "fe", "00001000,88880000", "0x5f00221223"
 - Comma separated list of bit ranges, e.g.: "#1-2,6,9-10"
 - Number of bits to set starting from 0, e.g. "&5".

It supports the following operators:

and             Pop two values, perform <oldest> bitwise-and <newer> on them and
                push the result back
xor             As and, but bitwise-xor
print-bit-count Pop one value, count the number of bits, and print the result
                to stdout

For installation instructions, read the INSTALL file.

