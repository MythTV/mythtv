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

#ifndef WIN32
#include <unistd.h>
#include <fcntl.h>                                     
#include <sys/ioctl.h>

#include <mythtv/mythcontext.h>
#include "config.h"
#endif

#include "rtp.h"
#include "g711.h"

extern "C" {
#include "gsm/gsm.h"
}

using namespace std;



gsmCodec::gsmCodec() : codec()
{
    gsmEncData = gsm_create();
    gsmDecData = gsm_create();
    gsmMicrosoftCompatability = false;
}

gsmCodec::~gsmCodec()
{
    gsm_destroy(gsmEncData);
    gsm_destroy(gsmDecData);
}

int gsmCodec::Encode(short *In, unsigned char *Out, int Samples, short &maxPower, int gain)
{
    (void)gain;
    if (Samples != 160)
        VERBOSE(VB_IMPORTANT, QString("GSM Encode unsupported length %1").arg(Samples));
    gsm_encode(gsmEncData, In, Out);
    maxPower = 0;
    for (int i=0;i<Samples;i++)
        maxPower = QMAX(maxPower, *In++);
    return 33; // Fixed 33 bytes per 20ms samples
}

int gsmCodec::Decode(unsigned char *In, short *Out, int Len, short &maxPower)
{
    if (Len == 65)
    {
        // Microsoft chose an alternative coding method which creates 40ms samples
        // of 2x 32.5 bytes each.  We need to configure the codec to handle this then
        // pass data in as a 33byte then a 32byte sample
        if (!gsmMicrosoftCompatability)
        {
            VERBOSE(VB_IMPORTANT, "SIP: Switching GSM decoder to Microsoft Compatability mode");
            gsmMicrosoftCompatability = true;
            int opt=1;
            gsm_option(gsmDecData, GSM_OPT_WAV49, &opt);
        }
        gsm_decode(gsmDecData, In, Out);
        gsm_decode(gsmDecData, In+33, Out+160);
        maxPower = 0;
        for (int i=0;i<320;i++)
            maxPower = QMAX(maxPower, *Out++);
        return (320*sizeof(short)); 
    }    
    
    if (Len != 33)
        VERBOSE(VB_IMPORTANT, QString("GSM Invalid receive length %1").arg(Len));
    gsm_decode(gsmDecData, In, Out);
    maxPower = 0;
    for (int i=0;i<160;i++)
        maxPower = QMAX(maxPower, *Out++);
    return (160*sizeof(short)); // One packet of 33 bytes translates into 20ms of data
}

int gsmCodec::Silence(uchar *out, int ms)
{
    if (ms != 20)
        VERBOSE(VB_IMPORTANT, QString("GSM Silence unsupported length %1").arg(ms));

    short pcmSilence[160];
    memset(pcmSilence, 0, 160*sizeof(short));
    gsm_encode(gsmEncData, pcmSilence, out);
    return 33;
}

