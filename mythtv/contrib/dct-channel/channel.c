/*
 * dct-channel
 * Copyright (c) 2003 Jim Paris <jim@jtan.com>
 *
 * This is free software; you can redistribute it and/or modify it and
 * it is provided under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation; see COPYING.
 */

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "crc.h"
#include "serial.h"
#include "packet.h"
#include "debug.h"
#include "remote.h"
#include "opt.h"
#include "version.h"

struct options opt[] = {
	{ 'p', "port", "port", "serial port (default: /dev/ttyS0)" },
	{ 'f', "force", NULL, "keep going after communication errors" },
	{ 'o', "ok", NULL, "send OK after channel number" },
	{ 'b', "blind", NULL, "send keys blindly and ignore returned data" },
	{ 'n', "nopower", NULL, "never attempt to turn box on" },
	{ 't', "timeout", "scale", "scale all timeouts by this much "
	                           "(default 1.0)" },
	{ 'q', "quiet", NULL, "suppress printing of channel on exit" },
	{ 'v', "verbose", NULL, "be verbose (use twice for extra debugging)" },
	{ 'h', "help", NULL, "this help" },
	{ 'V', "version", NULL, "show program version and exit" },
	{ 0, NULL, NULL, NULL }
};	

int verb_count = 0;
int send_ok = 0;

/* Timeouts, in msec. */
double timeout_scale = 1.0;


#define TIMEOUT_TINY (int)(timeout_scale*250)	/* Messages that may not
						   some (status, etc) */
#define TIMEOUT_SHORT (int)(timeout_scale*1500)	/* For most messages */
#define TIMEOUT_LONG (int)(timeout_scale*5000) 	/* Waiting for chan. change */
#define KEY_SLEEP (int)(timeout_scale*300)	/* After sending keypress */
#define POWER_SLEEP (int)(timeout_scale*6000)	/* For box to power on */
#define BLIND_SLEEP (int)(timeout_scale*1000)	/* Blind mode delay between 
						   keypresses */

int send_and_get_ack(packet *p, int timeout)
{
	packet s;
	int done=0;
	serial_sendpacket(p);
	while(!done) {
		if (serial_getpacket(&s,timeout)<0) {
			verb("No response to packet\n");
			return -1;
		}
		if (packet_is_ack(p,&s))
			done = 1;
		else 
			verb("Got response, but it's not an ACK; "
			     "waiting for more.\n");
	}
	return 0;
}

int initialize(void)
{
	packet p;
	verb("Attempting to initialize DCT\n");

	serial_flush();

	/* Send initialize_1 and get ack.  Sometimes the box
	   doesn't respond to this one. */
	packet_build_initialize_1(&p);
	if (send_and_get_ack(&p,TIMEOUT_TINY)<0)
		verb("No response to init_1; trying to continue\n");

	/* We should always get a response to initialize_2 */
	packet_build_initialize_2(&p);
	if (send_and_get_ack(&p,TIMEOUT_SHORT)<0) {
		verb("No response to initialize_2\n");
		return -1;
	}

	/* The DCT might now send us a status message, but 
	   not always.  Ignore it if it comes. */
	if (serial_getpacket(&p,TIMEOUT_TINY)<0)
		verb("No initial status message\n");
	       
	return 0;
}

int get_channel(void)
{
	int chan;
	packet p;

	verb("Reading current channel\n");

	/* Send channelquery and get ack */
	packet_build_channelquery(&p);
	if (send_and_get_ack(&p,TIMEOUT_SHORT)<0)
		return -1;
	
	/* Now we expect one more packet with the channel status */
	if (serial_getpacket(&p,TIMEOUT_SHORT)<0) {
		verb("Didn't get channel status message\n");
		return -1;
	}

	if ((chan=packet_extract_channel(&p))<0) {
		verb("Returned packet isn't channel status!\n");
		return -1;
	}

	return chan;
}

int send_keypress(int key)
{
	packet p;

	packet_build_keypress(&p,key);
	if (send_and_get_ack(&p,TIMEOUT_TINY)<0)
		return -1;

	/* After sending a keypress, we have to wait. 
	   0.1 sec is not quite enough; 0.2 sec seems to be */
	usleep(KEY_SLEEP * 1000);
	return 0;
}

int set_channel(int chan)
{
	packet p;
	int cur;

	if ((send_keypress(KEY_0+((chan/100)%10)))<0 ||
	   (send_keypress(KEY_0+((chan/10)%10)))<0 ||
	   (send_keypress(KEY_0+((chan/1)%10)))<0 ||
	   (send_ok && send_keypress(KEY_OK)<0)) {
		verb("Error sending channel keypresses\n");
		return -1;
	}

	/* Now the DCT should send us channel status */
	if (serial_getpacket(&p,TIMEOUT_LONG)<0) {
		verb("Didn't get channel status message\n");
		return -1;
	}
	if ((cur=packet_extract_channel(&p))<0) {
		verb("Returned packet isn't channel status!\n");
		return -1;
	}

	/* The DCT may send more channel status messages, if it
	   figures out something new about the signal.  Wait
	   for these, but don't wait long. */
	while(serial_getpacket(&p,TIMEOUT_TINY)>=0) {
		int newcur;
		if ((newcur=packet_extract_channel(&p))>=0) {
			debug("Got extra status back\n");
			cur = newcur;
		}
	}

	/* Send EXIT to clear the OSD, but don't
	   care if this doesn't work */
	send_keypress(KEY_EXIT);

	return cur;
}

/* Do everything blindly and slowly.
   Should work exactly like the Python script. */
void blind_key(int key)
{
	packet p;
	verb("Sending key %d\n",key);
	serial_flush();
	packet_build_initialize_1(&p);
	serial_sendpacket(&p);
	packet_build_initialize_2(&p);
	serial_sendpacket(&p);
	packet_build_keypress(&p,key);
	serial_sendpacket(&p);
	serial_flush();
}

void blind_channel(int chan)
{
	verb("Sending keys blindly.\n");
	blind_key(KEY_0+((chan/100)%10));
	usleep(BLIND_SLEEP * 1000);
	blind_key(KEY_0+((chan/10)%10));
	usleep(BLIND_SLEEP * 1000);
	blind_key(KEY_0+((chan/1)%10));
	if (send_ok) {
		usleep(BLIND_SLEEP * 1000);
		blind_key(KEY_OK);
	}
	usleep(BLIND_SLEEP * 1000 * 2);
	blind_key(KEY_EXIT);
	usleep(BLIND_SLEEP * 1000);
	serial_flush();	
}

int main(int argc, char *argv[])
{
	int optind;
	char *optarg;
	char c;
	FILE *help = stderr;
	int tries=0;
	int chan=0;
	int cur=0;
	int newcur;
	int quiet=0;
	int force=0;
	int nopower=0;
	int blind=0;
	char *t;
	char *port = "/dev/ttyS0";

	opt_init(&optind);
	while((c=opt_parse(argc,argv,&optind,&optarg,opt))!=0) {
		switch(c) {
		case 'v':
		        verb_count++;
			break;
		case 'f':
			force++;
			break;
		case 'n':
			nopower++;
			break;
		case 'o':
			send_ok++;
			break;
		case 'b':
			blind++;
			break;
		case 'q':
			quiet++;
			break;
		case 't':
			timeout_scale = strtod(optarg, &t);
			if (timeout_scale<0 || *t!=0 ||
			   optarg[0]<'0' || optarg[0]>'9') {
				fprintf(stderr,"Invalid time scale: %s\n",
					optarg);
				goto printhelp;
			}
			verb("Scaling timeouts by %f\n",timeout_scale);
			break;				
		case 'p':
			port = optarg;
			break;
		case 'V':
			printf("dct-channel " VERSION "\n");
			printf("Written by Jim Paris <jim@jtan.com>\n");
			printf("This program comes with no warranty and is "
			       "provided under the GPLv2.\n");
			return 0;
			break;
		case 'h':
			help=stdout;
		default:
		printhelp:
			fprintf(help,"Usage: %s [options] [channel]\n",*argv);
			opt_help(opt,help);
			fprintf(help,"Interfaces with a DCT-2000 series cable "
				     "box over RS232.\n");
			fprintf(help,"Changes channel, if one is supplied, "
				     "and prints it.\n");
			fprintf(help,"With no arguments, prints current "
				     "channel.\n");
			fprintf(help,"See README for more details.\n");
			return (help==stdout)?0:1;
		}
	}

	if ((optind+1)<argc) {
		fprintf(stderr,"Error: too many arguments (%s)\n\n",
			argv[optind+1]);
		goto printhelp;
	}

	if (optind<argc) {
		char *end;
		chan = strtol(argv[optind],&end,10);
		if (*end!='\0' || chan<1 || chan>999) {
			fprintf(stderr,"Invalid channel: %s\n",argv[optind]);
			fprintf(stderr,"valid channels are 1 - 999\n");
			goto printhelp;
		}
	}

	if (blind && !chan) {
		fprintf(stderr,"Can't read channel number in blind mode;\n");
		fprintf(stderr,"turning off blind mode.\n");
		blind=0;
	}

	if ((serial_init(port))<0) 
		err(1,port);

	atexit(serial_close);

	if (blind) {
		blind_channel(chan);
		if (!quiet) printf("%d\n",chan);
		return 0;
	}

	while(tries < 5) {
		tries++;
		if (initialize()<0) {
			verb("Initialization failed\n");
			if (!force) continue;
		}

		if ((newcur=get_channel())<0) {
			verb("Failed to read current channel\n");
			if (!force) continue;
			newcur = 0;
		}
		cur = newcur;

		if (chan!=0 && chan!=cur) {
			verb("Changing from channel %d to channel %d\n",cur,chan);
			/* Switch channel. */
			if ((newcur=set_channel(chan))<0 &&
			   (newcur=get_channel())!=chan) {
				int i;

				verb("Failed to set new channel\n");
				switch(tries) {
				case 1:
				case 2:
					/* Try increasing timeouts by 50%,
					   which will also increase the
					   delay between keypresses */
					timeout_scale *= 1.5;
					verb("Increased timeout_scale to %f\n",
					     timeout_scale);
					break;
				case 3:
					/* Try sending exit key a few
					   times to make sure we're
					   not in a menu or something */
					verb("Attempting to exit menus\n");
					for(i=0;i<4;i++)
						if (send_keypress(KEY_EXIT)<0)
							break;
					break;
				case 4:
					/* After 4 unsuccessful tries,
					   try sending power button,
					   in case the box is turned
					   off.  Need to wait for it
					   to actually power on, and
					   flush buffers since it will
					   probably send garbage. */
					if (nopower) 
						break;
					verb("Attempting to turn box on\n");
					if (send_keypress(KEY_POWER)>=0) {
						usleep(POWER_SLEEP*1000);
					}
 					serial_flush();
					break;
				default:
					break;
				}
				continue;
			}
			cur = newcur;

			if (cur != chan) {
				debug("channel is wrong; didn't switch?\n");
				continue;
			}

			verb("Success!\n");
		}
		if (!quiet) printf("%d\n",cur);
		return 0;
	}

	if (!quiet) {
		fprintf(stderr,"Communication failed after %d tries\n", tries);
		printf("%d\n",cur);
	}
	return 1;
}
