#!/usr/bin/env python

# Chris Griffiths, June 8th 2003 (dct2000@y-fronts.com)
#
# CRC calculator preset for CRC-CCIT suitable for a Motorola DCT2000

# Please see http://rcswww.urz.tu-dresden.de/~sr21/crctester.c
# and http://rcswww.urz.tu-dresden.de/~sr21/crc.html for the 
# original c code to this, a web based crc generator, and
# a description of what these settings do

order = 16;
polynom = 0x1021;
direct = 1;
crcinit = 0x0000;
crcxor = 0x0000;
refin = 1;
refout = 1;


def reflect (crc, bitnum):
	i = 1 << (bitnum-1)
	j = 1
	crcout=0
	while i:
		if (crc & i):
			crcout = crcout | j
		j = j << 1
		i = i >> 1
	
	return (crcout)

def calcrc(data_bytes):

	crcmask = (((1<<(order-1))-1)<<1)|1;
	crchighbit = 1<<(order-1);

	# compute missing initial CRC value
	if direct:
		crcinit_direct = crcinit;
		crc = crcinit;
		for i in range (order):
			bit = crc & 1;
			if bit:
				crc = crc ^ polynom;
			crc >>= 1;
			if bit:
				 crc = crc | crchighbit;
		crcinit_nondirect = crc;
	else:
		crcinit_nondirect = crcinit
		crc = crcinit
		for i in range (order):
			bit = crc & crchighbit
			crc = crc << 1;
			if bit: 
				crc= crc ^ polynom;
		crc = crc & crcmask;
		crcinit_direct = crc;

	# now compute the crc
	crc = crcinit_direct

	for i in range( len(data_bytes) ):

		c = ord(data_bytes[i])
		if refin:
			c = reflect(c, 8)

		j=0x80;
		while j:
			bit = crc & crchighbit
			crc = crc << 1
			if (c & j):
				bit = bit ^ crchighbit
			if bit:
				crc = crc ^ polynom
			j = j >> 1


	if refout:
		crc=reflect(crc, order)
	crc = crc ^ crcxor
	crc = crc & crcmask

	return(crc)
