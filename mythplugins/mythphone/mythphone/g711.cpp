/*
	g711.cpp

	(c) 2004 Paul Volkaerts
	Part of the mythTV project
	
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

#include "g711.h"
#ifdef VA_G729
#include "g729/va_g729a.h"
#endif

#define PCM2G711U(p)	(ulaw_comp_table[((p)/4)&0x3FFF])    
#define G711U2PCM(g)	(ulaw_exp_table[g])
#define PCM2G711A(p)	(alaw_comp_table[((p)/4)&0x3FFF])
#define G711A2PCM(g)	(alaw_exp_table[g])



int g711ulaw::Encode(short *In, unsigned char *Out, int Samples, short &maxPower, int gain)
{
    (void)gain;
    int Count = Samples;
    while (Count-- > 0)
    {
        maxPower = QMAX(maxPower, *In);
        *Out++ = PCM2G711U(*In++);
    }
    return Samples;
}

int g711ulaw::Decode(unsigned char *In, short *Out, int Len, short &maxPower)
{
    int Count = Len;
    while (Count-- > 0)
    {
        *Out = G711U2PCM(*In++);
        maxPower = QMAX(maxPower, *Out);
        *Out++;
    }
    return (Len*sizeof(short));
}

int g711ulaw::Silence(uchar *out, int ms)
{
    int len = ms * PCM_SAMPLES_PER_MS;
    memset(out, PCM2G711U(0), len);
    return len;
}

int g711alaw::Encode(short *In, unsigned char *Out, int Samples, short &maxPower, int gain)
{
    (void)gain;
    int Count = Samples;
    while (Count-- > 0)
    {
       maxPower = QMAX(maxPower, *In);
        *Out++ = PCM2G711A(*In++);
    }
    return Samples;
}

int g711alaw::Decode(unsigned char *In, short *Out, int Len, short &maxPower)
{
    int Count = Len;
    while (Count-- > 0)
    {
        *Out = G711A2PCM(*In++);
        maxPower = QMAX(maxPower, *Out);
        Out++;
    }
    return (Len*sizeof(short));
}

int g711alaw::Silence(uchar *out, int ms)
{
    int len = ms * PCM_SAMPLES_PER_MS;
    memset(out, PCM2G711A(0), len);
    return len;
}

#ifdef VA_G729

g729::g729() :codec()
{
    va_g729a_init_decoder();
    va_g729a_init_encoder();
}

int g729::Encode(short *In, unsigned char *Out, int Samples, short &maxPower, int gain)
{
    int Len = 0;
    while (Samples>0)
    {
        va_g729a_encoder(In, Out);
        In+=L_FRAME;
        Out+=L_FRAME_COMPRESSED;
        Len += L_FRAME_COMPRESSED;
        Samples -= L_FRAME;
    }
    return Len;
}

int g729::Decode(unsigned char *In, short *Out, int Len, int &maxPower)
{
    int bfi=0;
    int OutLen=0;
    while (Len>0)
    {
        va_g729a_decoder(In, Out, bfi);
        In+=L_FRAME_COMPRESSED;
        Out+=L_FRAME;
        Len -= L_FRAME_COMPRESSED;
        OutLen += L_FRAME;
    }
    return (OutLen);
}

int g729::Silence(uchar *out, int ms)
{
    // TODO !!!
    return 20;
}

#endif
