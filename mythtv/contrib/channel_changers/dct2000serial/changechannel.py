#!/usr/bin/env python
#
# Chris Griffiths, June 8th 2003 (dct2000@y-fronts.com)
#
# Simple program to send remote control sequences via serial port to a
# motorola dct2000
# It does not respect sequence numbers, nor check the returned codes for
# success or failure
#
# Modified by Ian Forde (ian@duckland.org) (Sometime in early 2003 - I don't
# remember when...)
#
# Modified by Lonny Selinger to support additional dct2000 functions
#
#	Version history
#		0.87: Added an EXIT_KEY code to make the dct2000 osd go away
#			as quickly as possible, since I don't know how to disable
#			it yet!
#		0.86: Removed the debugging flag.  That's my responsibility,
#			not everyone else's!
#		0.85: Added a debugging flag
#		      Cleaned up the channel code determination by removing
#			a whole bunch of 'if' statements and the corresponding
#			variable names.
#		      Eliminated the need for a separate shell script by
#			including channel number length normalizing into the
#			script.
#		      Turned off flow control
#		      Created a variable for setting the serial port
#		      Fixed a bug in the alternate button parsing
#		0.81: Experimental code added by Lonny Selinger for additional
#			functions such as last_channel, mute, favorite, chup,
#			chdown.  Not integrated into Myth yet!
#		0.8:  it works!

import serial		# pyserial (http://pyserial.sourceforge.net/)
import crcgen		# CRC generator - preset to generate CCITT polynomial crc's
import time			# std python lib
import sys

# define the serial port
SERIALPORT='/dev/ttyS1'

# define button codes:
ONOFF='\x0A'
CHAN_UP='\x0B'
CHAN_DOWN='\x0C'
VOLUME_UP='\x0D'
VOLUME_DOWN='\x0E'
MUTE='\x0F'
MUSIC='\x10'
SELECT_OK='\x11'
ENTER='\x11'
EXIT_KEY='\x12'
LAST_CHANNEL='\x13'
SWITCH_AB='\x14'
FAVORITE='\x15'
PPV_KEY='\x16'
A_KEY='\x17'
P_F='\x18'
MENU='\x19'
OUTPUT_CHAN='\x1C'
FAST_FORWARD='\x1D'
REVERSE='\x1E'
PAUSE='\x1F'
BROWSE='\x22'
CANCEL='\x23'
LOCK='\x24'
LIST='\x25'
THEME='\x26'
GUIDE='\x30'
RECORD='\x31'
HELP='\x32'
INFO='\x33'
UP='\x34'
DOWN='\x35'
LEFT='\x36'
RIGHT='\x37'
DAY_PLUS='\x38'
DAY_MINUS='\x39'
PAGE_PLUS='\x3A'
PAGE_MINUS='\x3B'
B_KEY='\x27'
C_KEY='\x28'

# This function below was blatantly lifted from:
#         http://aspn.activestate.com/ASPN/Cookbook/Python/Recipe/52660

def IsAllDigits( str ):
	import string
        """ Is the given string composed entirely of digits? """
        match = string.digits
        ok = 1
        for letter in str:
                if letter not in match:
                        ok = 0
                        break
        return ok

def debug(ctrl_str):
	for hexChar in ctrl_str:
		print "%#x" % ord(hexChar),
	print '\n'

def word2str(val):
	return chr((val & 0xFF00) / 256) + chr(val & 0x00FF)

def get_response(ser):
	#response = ''
	#byte = ser.read()
	#while byte:
	#	response = response + byte
	#	byte = ser.read()

	byte = ser.read()
	response = byte
	byte = ser.read()
	while byte:
		response = response + byte
		byte = ser.read(7)
	#print "length of response is ",len(response)
	return response

def calc_code(payload=""):
	#
	# code is 10 78 ll ll ss 40 pp pp cc 10 03
	# where ss = seq num, pp pp = payload (for a remote keypress first byte is 22), cc = crc
	# It seems the sequence number can be ignored - so long as we send this double sequence before it - 
	# this is what embleem was using with the last sequence being the on/off keypress
	#
	# It also looks like we can only send one keypress at a time - although correct use of sequence number
	# be get that working
	#
	code = '\x78' + word2str(len(payload)+2) + '\x10\x40' + payload
	crc = crcgen.calcrc(code)
	code = "\x10\x70\x00\x02\x03\x40\xc8\x27\x10\x03\x10\x78\x00\x03\x03\x40\x00\x68\x96\x10\x03" + '\x10' + code + word2str(crc) + '\x10\x03'
	return code

def send_to_unit(ser, key_presses):
	#
	# Send each keypress, then get the returned status from the box (and subsequently ignore it)
	# You have to read the return code, otherwise the next send with fail
	#
	for key in key_presses:
		ser.write(calc_code('\x22' + key))
		get_response(ser)

cmdarg = sys.argv[1]

# added an extra code block to handle a few more options that
# can be passed to remote.py.
# options: last_channel(), mute(), favorite(), chan_up(),
# chan_down() .... for now anyway
#
# Lonny Selinger

if (not IsAllDigits(cmdarg)):
	if (cmdarg == "l" or cmdarg == "m" or cmdarg == "f" or cmdarg == "u" or cmdarg == "d"):
		BUTNKEY=''
		if (cmdarg == "l" ):
			BUTNKEY=LAST_CHANNEL
		else:
			if (cmdarg == "m" ):
				BUTNKEY=MUTE
			else:
				if (cmdarg == "f" ):
					BUTNKEY=FAVORITE
				else:
					if (cmdarg == "u" ):
						BUTNKEY=CHAN_UP
					else:
						if (cmdarg == "d" ):
							BUTNKEY=CHAN_DOWN
		serCon = serial.Serial(SERIALPORT,baudrate=9600, bytesize=serial.EIGHTBITS,  parity=serial.PARITY_NONE, stopbits=serial.STOPBITS_ONE, timeout=1, xonxoff=0, rtscts=0,)
		send_to_unit(serCon, BUTNKEY)
else:
	CHANKEY=''
	CHANCODE=''
	if (len(cmdarg) > 3):
		print "too many chars!"
	else:
		lenc=len(cmdarg)
		if (lenc == 3):
			chanset=cmdarg
		else:
			if (lenc == 2):
				chanset="0"+cmdarg
			else:
				if (lenc == 1):
					chanset="00"+cmdarg

	for num in [1,2,3]:
		CHANKEY=chr(int(chanset[num-1:num]))
		CHANCODE=CHANCODE+CHANKEY

	serCon = serial.Serial(SERIALPORT,baudrate=9600, bytesize=serial.EIGHTBITS,  parity=serial.PARITY_NONE, stopbits=serial.STOPBITS_ONE, timeout=1, xonxoff=0, rtscts=0,)
	send_to_unit(serCon, CHANCODE)
	send_to_unit(serCon, EXIT_KEY)
