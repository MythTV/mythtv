/*
	gsm.cpp

	(c) 2004 Paul Volkaerts
	Part of the mythTV project
        
        This is a wrapper class for the GSM TOAST codec.
        See http://kbs.cs.tu-berlin.de/~jutta/toast.html
	
*/

#include <qapplication.h>

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <iostream>
#include <unistd.h>
#include <fcntl.h>                                     
#include <sys/ioctl.h>

#include <mythtv/mythcontext.h>
#include "config.h"

#include "rtp.h"
#include "g711.h"

extern "C" {
#include "gsm/gsm.h"
}


gsmCodec::gsmCodec() : codec()
{
    gsmEncData = gsm_create();
    gsmDecData = gsm_create();
}

gsmCodec::~gsmCodec()
{
    gsm_destroy(gsmEncData);
    gsm_destroy(gsmDecData);
}

int gsmCodec::Encode(short *In, unsigned char *Out, int Samples, short &maxPower, int gain)
{
    (void)gain;
    gsm_encode(gsmEncData, In, Out);
    maxPower = 0;
    for (int i=0;i<Samples;i++)
        maxPower = QMAX(maxPower, *In++);
    return 33; // Fixed 33 bytes per 20ms samples
}

int gsmCodec::Decode(unsigned char *In, short *Out, int Len, short &maxPower)
{
    if (Len != 33)
        cout << "GSM Invalid receive length " << Len << endl;
    gsm_decode(gsmDecData, In, Out);
    for (int i=0;i<160;i++)
        maxPower = QMAX(maxPower, *Out++);
    maxPower = 0;
    return (160*sizeof(short)); // One packet of 33 bytes translates into 20ms of data
}

int gsmCodec::Silence(uchar *out, int ms)
{
    if (ms != 20)
        cout << "GSM Silence unsupported length " << ms << endl;

    short pcmSilence[160];
    memset(pcmSilence, 0, 160*sizeof(short));
    gsm_encode(gsmEncData, pcmSilence, out);
    return 33;
}

