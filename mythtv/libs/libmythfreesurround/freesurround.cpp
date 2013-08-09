/*
Copyright (C) 2007 Christian Kothe, Mark Spieth
Copyright (C) 2010-2011 Jean-Yves Avenard

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <cmath>

#include <iostream>
#include <sstream>
#include <vector>
#include <list>
#include <map>
using namespace std;

#include "compat.h"
#include "mythlogging.h"
#include "freesurround.h"
#include "el_processor.h"

#include <QString>
#include <QDateTime>

// our default internal block size, in floats
static const unsigned default_block_size = SURROUND_BUFSIZE;
// Gain of center and lfe channels in passive mode (sqrt 0.5)
static const float center_level = 0.707107;
static const float m3db = 0.7071067811865476f;           // 3dB  = SQRT(2)
static const float m6db = 0.5;                           // 6dB  = SQRT(4)
static const float m7db = 0.44721359549996;              // 7dB  = SQRT(5)

unsigned int block_size = default_block_size;

struct buffers
{
    buffers(unsigned int s):
        l(s),r(s),c(s),ls(s),rs(s),lfe(s), rls(s), rrs(s) { }
    void resize(unsigned int s)
    {
        l.resize(s); r.resize(s); lfe.resize(s);
        ls.resize(s); rs.resize(s); c.resize(s);
        rls.resize(s); rrs.resize(s);
    }
    void clear()
    {
        l.clear(); r.clear(); lfe.clear();
        ls.clear(); rs.clear(); c.clear();
        rls.clear(); rrs.clear();
    }
    std::vector<float> l,r,c,ls,rs,lfe,cs,lcs,rcs,
                       rls, rrs;       // for demultiplexing
};

//#define SPEAKERTEST
#ifdef SPEAKERTEST
int channel_select = -1;
#endif

FreeSurround::FreeSurround(uint srate, bool moviemode, SurroundMode smode) :
    srate(srate),
    bufs(NULL),
    decoder(0),
    in_count(0),
    out_count(0),
    processed(true),
    processed_size(0),
    surround_mode(smode),
    latency_frames(0),
    channels(0)
{
    LOG(VB_AUDIO, LOG_DEBUG,
        QString("FreeSurround::FreeSurround rate %1 moviemode %2")
            .arg(srate).arg(moviemode));

    if (moviemode)
    {
        params.phasemode = 1;
        params.center_width = 25;
        params.dimension = 0.5;
    }
    else
    {
        params.center_width = 65;
        params.dimension = 0.3;
    }
    switch (surround_mode)
    {
        case SurroundModeActiveSimple:
            params.steering = 0;
            break;
        case SurroundModeActiveLinear:
            params.steering = 1;
            latency_frames = block_size/2;
            break;
        default:
            break;
    }

    bufs = new buffers(block_size/2);
    open();
#ifdef SPEAKERTEST
    channel_select++;
    if (channel_select>=6)
        channel_select = 0;
    LOG(VB_AUDIO, LOG_DEBUG,
        QString("FreeSurround::FreeSurround channel_select %1")
            .arg(channel_select));
#endif
    LOG(VB_AUDIO, LOG_DEBUG, QString("FreeSurround::FreeSurround done"));
}

void FreeSurround::SetParams()
{
    if (decoder)
    {
        decoder->steering_mode(params.steering);
        decoder->phase_mode(params.phasemode);
        decoder->surround_coefficients(params.coeff_a, params.coeff_b);
        decoder->separation(params.front_sep/100.0,params.rear_sep/100.0);
    }
}

FreeSurround::fsurround_params::fsurround_params(int32_t center_width,
                                                 int32_t dimension) :
    center_width(center_width),
    dimension(dimension),
    coeff_a(0.8165),coeff_b(0.5774),
    phasemode(0),
    steering(1),
    front_sep(100),
    rear_sep(100)
{
}

FreeSurround::~FreeSurround()
{
    LOG(VB_AUDIO, LOG_DEBUG, QString("FreeSurround::~FreeSurround"));
    close();
    delete bufs;
    bufs = NULL;
    LOG(VB_AUDIO, LOG_DEBUG, QString("FreeSurround::~FreeSurround done"));
}

uint FreeSurround::putFrames(void* buffer, uint numFrames, uint numChannels)
{
    int i = 0;
    int ic = in_count;
    int bs = block_size/2;
    bool process = true;
    float *samples = (float *)buffer;
    // demultiplex

    float **inputs = decoder->getInputBuffers();
    float *lt      = &inputs[0][ic];
    float *rt      = &inputs[1][ic];

    if ((surround_mode != SurroundModePassive) && (ic+numFrames > bs))
    {
        numFrames = bs - ic;
    }

    switch (numChannels)
    {
        case 1:
            switch (surround_mode)
            {
                case SurroundModePassive:
                case SurroundModePassiveHall:
                    for (i = 0; i < numFrames && ic < bs; i++,ic++)
                    {
                        // should be -7dB to keep power level the same
                        // but we bump the level a tad.
                        bufs->c[ic] = bufs->l[ic] = bufs->r[ic] = samples[i] * m6db;
                        bufs->ls[ic] = bufs->rs[ic] = bufs->c[ic];
                    }
                    process = false;
                    break;
                default:
                    for (i=0; i<numFrames; i++)
                        *lt++ = *rt++ = *samples++;
                    process = true;
                    break;
            }
            channels = 6;
            break;

        case 2:
            switch (surround_mode)
            {
                case SurroundModePassive:
                    for (i = 0; i < numFrames && ic < bs; i++,ic++)
                    {
                        float lt      = *samples++;
                        float rt      = *samples++;
                        bufs->l[ic]   = lt;
                        bufs->lfe[ic] = bufs->c[ic] = (lt+rt) * m3db;
                        bufs->r[ic]   = rt;
                        // surround channels receive out-of-phase
                        bufs->ls[ic]  = (rt-lt) * 0.5;
                        bufs->rs[ic]  = (lt-rt) * 0.5;
                    }
                    process = false;
                    break;
                case SurroundModePassiveHall:
                    for (i = 0; i < numFrames && ic < bs; i++,ic++)
                    {
                        float lt      = *samples++;
                        float rt      = *samples++;
                        bufs->l[ic]   = lt * m3db;
                        bufs->lfe[ic] = bufs->c[ic] = (lt+rt) * m3db;
                        bufs->r[ic]   = rt * m3db;
                        bufs->ls[ic]  = bufs->l[ic];
                        bufs->rs[ic]  = bufs->r[ic];
                    }
                    process = false;
                    break;
                default:
                    for (i=0; i<numFrames; i++)
                    {
                        *lt++ = *samples++;
                        *rt++ = *samples++;
                    }
                    process = true;
                    break;
            }
            channels = 6;
            break;

        case 5:
            for (i = 0; i < numFrames && ic < bs; i++,ic++)
            {
                float lt      = *samples++;
                float rt      = *samples++;
                float c       = *samples++;
                float ls      = *samples++;
                float rs      = *samples++;
                bufs->l[ic]   = lt;
                bufs->lfe[ic] = 0.0f;
                bufs->c[ic]   = c;
                bufs->r[ic]   = rt;
                bufs->ls[ic]  = ls;
                bufs->rs[ic]  = rs;
            }
            process = false;
            channels = 6;
            break;

        case 7:
            for (i = 0; i < numFrames && ic < bs; i++,ic++)
            {
                // 3F3R-LFE  L  R  C  LFE  BC  LS   RS
                float lt      = *samples++;
                float rt      = *samples++;
                float c       = *samples++;
                float lfe     = *samples++;
                float cs      = *samples++;
                float ls      = *samples++;
                float rs      = *samples++;
                bufs->l[ic]   = lt;
                bufs->lfe[ic] = lfe;
                bufs->c[ic]   = c;
                bufs->r[ic]   = rt;
                bufs->ls[ic]  = ls;
                bufs->rs[ic]  = rs;
                bufs->rls[ic]  = bufs->rrs[ic]  = cs * m3db;
            }
            process = false;
            channels = 8;
            break;
        default:
            break;
    }
    if (process)
    {
        ic += numFrames;
        if (ic != bs)
        {
            // dont modify unless no processing is to be done
            // for audiotime consistency
            in_count = ic;
        }
        else
        {
            processed = process;
            // process_block takes some time so dont update in and out count
            // before its finished so that Audiotime is correctly calculated
            process_block();
            in_count = 0;
            out_count = bs;
            processed_size = bs;
            latency_frames = block_size/2;
        }
    }
    else
    {
        in_count = 0;
        out_count = processed_size = ic;
        processed = false;
        latency_frames = 0;
    }

    LOG(VB_AUDIO | VB_TIMESTAMP, LOG_DEBUG,
        QString("FreeSurround::putFrames %1 #ch %2 used %3 generated %4")
            .arg(numFrames).arg(numChannels).arg(i).arg(out_count));

    return i;
}

uint FreeSurround::receiveFrames(void *buffer, uint maxFrames)
{
    uint i;
    uint oc = out_count;
    if (maxFrames > oc) maxFrames = oc;
    uint outindex = processed_size - oc;
    float *output = (float *)buffer;
    if (channels == 8)
    {
        float *l   = &bufs->l[outindex];
        float *c   = &bufs->c[outindex];
        float *r   = &bufs->r[outindex];
        float *ls  = &bufs->ls[outindex];
        float *rs  = &bufs->rs[outindex];
        float *lfe = &bufs->lfe[outindex];
        float *rls = &bufs->rls[outindex];
        float *rrs = &bufs->rrs[outindex];
        for (i = 0; i < maxFrames; i++)
        {
//            printf("1:%f 2:%f 3:%f 4:%f 5:%f 6:%f 7:%f 8:%f\n",
//                   *l, *r, *c, *lfe, *rls, *rrs, *ls, *rs);

            // 3F4-LFE   L   R   C    LFE  Rls  Rrs  LS   RS
            *output++ = *l++;
            *output++ = *r++;
            *output++ = *c++;
            *output++ = *lfe++;
            *output++ = *rls++;
            *output++ = *rrs++;
            *output++ = *ls++;
            *output++ = *rs++;
        }
        oc -= maxFrames;
        outindex += maxFrames;
    }
    else        // channels == 6
    {
        if (processed)
        {
            float** outputs = decoder->getOutputBuffers();
            float *l   = &outputs[0][outindex];
            float *c   = &outputs[1][outindex];
            float *r   = &outputs[2][outindex];
            float *ls  = &outputs[3][outindex];
            float *rs  = &outputs[4][outindex];
            float *lfe = &outputs[5][outindex];
            for (i = 0; i < maxFrames; i++)
            {
                *output++ = *l++;
                *output++ = *r++;
                *output++ = *c++;
                *output++ = *lfe++;
                *output++ = *ls++;
                *output++ = *rs++;
            }
            oc -= maxFrames;
            outindex += maxFrames;
        }
        else
        {
            float *l   = &bufs->l[outindex];
            float *c   = &bufs->c[outindex];
            float *r   = &bufs->r[outindex];
            float *ls  = &bufs->ls[outindex];
            float *rs  = &bufs->rs[outindex];
            float *lfe = &bufs->lfe[outindex];
            for (i = 0; i < maxFrames; i++)
            {
                *output++ = *l++;
                *output++ = *r++;
                *output++ = *c++;
                *output++ = *lfe++;
                *output++ = *ls++;
                *output++ = *rs++;
            }
            oc -= maxFrames;
            outindex += maxFrames;
        }
    }
    out_count = oc;
    LOG(VB_AUDIO | VB_TIMESTAMP, LOG_DEBUG,
        QString("FreeSurround::receiveFrames %1").arg(maxFrames));
    return maxFrames;
}

void FreeSurround::process_block()
{
    // process the data
    try
    {
        if (decoder)
        {
            decoder->decode(params.center_width/100.0,params.dimension/100.0);
        }
    }
    catch(...)
    {
    }
}

long long FreeSurround::getLatency()
{
    // returns in usec
    if (latency_frames == 0)
        return 0;
    return decoder ? ((long long)(latency_frames + in_count)*1000000)/(2*srate) : 0;
}

void FreeSurround::flush()
{
    if (decoder)
        decoder->flush();
    bufs->clear();
}

// load the lib and initialize the interface
void FreeSurround::open()
{
    if (!decoder)
    {
        decoder = new fsurround_decoder(block_size);
        decoder->flush();
        if (bufs)
            bufs->clear();
        decoder->sample_rate(srate);
    }
    SetParams();
}

void FreeSurround::close()
{
    delete decoder;
    decoder = NULL;
}

uint FreeSurround::numUnprocessedFrames()
{
    return in_count;
}

uint FreeSurround::numFrames()
{
    return out_count;
}

uint FreeSurround::frameLatency()
{
    if (processed)
        return in_count + out_count + (block_size/2);
    else
        return in_count + out_count;
}

uint FreeSurround::framesPerBlock()
{
    return block_size/2;
}

