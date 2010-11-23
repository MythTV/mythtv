/*
Copyright (C) 2007 Christian Kothe, Mark Spieth

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
#include "mythverbose.h"
#include "freesurround.h"
#include "el_processor.h"

#include <QString>
#include <QDateTime>

// our default internal block size, in floats
static const unsigned default_block_size = 8192;
// Gain of center and lfe channels in passive mode (sqrt 0.5)
static const float center_level = 0.707107; 

unsigned int block_size = default_block_size;

// stupidity countermeasure...
template<class T> T pop_back(std::list<T> &l)
{
    T result(l.back());
    l.pop_back();
    return result;
}

// a pool, where the DSP can throw its objects at after it got deleted and
// get them back when it is recreated...
class object_pool
{
public:
    typedef void* (*callback)();
    // initialize
    object_pool(callback cbf):construct(cbf) { }
    ~object_pool()
    {
        for (std::map<void*,void*>::iterator i=pool.begin(),e=pool.end();
             i != e; i++)
            delete i->second;
        for (std::list<void*>::iterator i=freelist.begin(),e=freelist.end();
             i != e; i++)
            delete *i;
    }
    // (re)acquire an object
    void *acquire(void *who)
    {
        std::map<void*,void*>::iterator i(pool.find(who));
        if (i != pool.end())
            return i->second;
        else
            if (!freelist.empty())
                return pool.insert(std::make_pair(who,pop_back(freelist)))
                                  .first->second;
            else
                return pool.insert(std::make_pair(who,construct()))
                                  .first->second;
    }
    // release an object into the wild
    void release(void *who)
    {
        std::map<void*,void*>::iterator i(pool.find(who));
        if (i != pool.end()) {
            freelist.push_back(i->second);
            pool.erase(i);
        }
    }    
public:
    callback construct;            // object constructor callback
    std::list<void*> freelist;     // list of available objects
    std::map<void*,void*> pool;    // pool of used objects, by class
};

// buffers which we usually need (and want to share between plugin lifespans)
struct buffers
{
    buffers(unsigned int s): 
    l(s),r(s),c(s),ls(s),rs(s),lfe(s) { }
    void resize(unsigned int s)
    {
        l.resize(s); r.resize(s); lfe.resize(s); 
        ls.resize(s); rs.resize(s); c.resize(s);
    }
    void clear()
    {
        l.clear(); r.clear(); lfe.clear();
        ls.clear(); rs.clear(); c.clear();
    }
    std::vector<float> l,r,c,ls,rs,lfe,cs,lcs,rcs;  // for demultiplexing
};

// construction methods
void *new_decoder() { return new fsurround_decoder(block_size); }
void *new_buffers() { return new buffers(block_size/2); }

object_pool dp(&new_decoder);
object_pool bp(&new_buffers);

//#define SPEAKERTEST
#ifdef SPEAKERTEST
int channel_select = -1;
#endif

FreeSurround::FreeSurround(uint srate, bool moviemode, SurroundMode smode) :
    srate(srate),
    open_(false),
    initialized_(false),
    bufs(NULL),
    decoder(0),
    in_count(0),
    out_count(0),
    processed(true),
    processed_size(0),
    surround_mode(smode)
{
    VERBOSE(VB_AUDIO+VB_EXTRA,
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
            break;
        default:
            break;
    }

    bufs = (buffers*)bp.acquire((void*)1);
    open();
#ifdef SPEAKERTEST
    channel_select++;
    if (channel_select>=6)
        channel_select = 0;
    VERBOSE(VB_AUDIO+VB_EXTRA,
            QString("FreeSurround::FreeSurround channel_select %1")
            .arg(channel_select));
#endif
    VERBOSE(VB_AUDIO+VB_EXTRA,
            QString("FreeSurround::FreeSurround done"));
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
    VERBOSE(VB_AUDIO+VB_EXTRA, QString("FreeSurround::~FreeSurround"));
    close();
    if (bufs)
    {
        bp.release((void*)1);
        bufs = NULL;
    }
    VERBOSE(VB_AUDIO+VB_EXTRA, QString("FreeSurround::~FreeSurround done"));
}

uint FreeSurround::putFrames(void* buffer, uint numFrames, uint numChannels)
{
    int i = 0;
    int ic = in_count;
    int bs = block_size/2;
    bool process = true;
    float *samples = (float *)buffer;
    // demultiplex
    switch (surround_mode)
    {
        case SurroundModePassive:
            switch (numChannels)
            {
                case 1:
                    for (i = 0; i < numFrames && ic < bs; i++,ic++)
                        bufs->l[ic] = bufs->c[ic] = bufs->r[ic] = samples[i];
                    break;
                case 2:
                    for (i = 0; i < numFrames && ic < bs; i++,ic++)
                    {
                        float lt      = *samples++;
                        float rt      = *samples++;
                        bufs->l[ic]   = lt;
                        bufs->lfe[ic] = bufs->c[ic]  = (lt+rt) * center_level;
                        bufs->r[ic]   = rt;
                        bufs->ls[ic]  = bufs->rs[ic] = (lt-rt) * center_level;
                    }
                    break;
            }
            in_count = 0;
            out_count = processed_size = ic;
            processed = false;
            break;

        default:
            float **inputs = decoder->getInputBuffers();
            float *lt      = &inputs[0][ic];
            float *rt      = &inputs[1][ic];
            if ((ic+numFrames) > bs)
                numFrames = bs - ic;
            switch (numChannels)
            {
                case 1:
                    for (i=0; i<numFrames; i++)
                        *lt++ = *rt++ = *samples++;
                    break;
                case 2:
                    for (i=0; i<numFrames; i++)
                    {
                        *lt++ = *samples++;
                        *rt++ = *samples++;
                    }
                    break;
            }
            ic += numFrames;
            processed = process;
            if (ic != bs)
            {
                // dont modify unless no processing is to be done
                // for audiotime consistency
                in_count = ic;
                break;
            }
            // process_block takes some time so dont update in and out count
            // before its finished so that Audiotime is correctly calculated
            if (process)
                process_block();
            in_count = 0;
            out_count = bs;
            processed_size = bs;
            break;
    }

    VERBOSE(VB_AUDIO+VB_TIMESTAMP+VB_EXTRA,
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

    switch (surround_mode)
    {
        case SurroundModePassive:
            for (i = 0; i < maxFrames; i++) 
            {
                *output++ = bufs->l[outindex];
                *output++ = bufs->r[outindex];
                *output++ = bufs->c[outindex];
                *output++ = bufs->lfe[outindex];
                *output++ = bufs->ls[outindex];
                *output++ = bufs->rs[outindex];
                oc--;
                outindex++;
            }
            break;

        default:
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
            break;
    }
    out_count = oc;
    VERBOSE(VB_AUDIO+VB_TIMESTAMP+VB_EXTRA, QString("FreeSurround::receiveFrames %1").arg(maxFrames));
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
    if (surround_mode == SurroundModePassive)
        return 0;
    return decoder ? ((block_size/2 + in_count)*1000000)/(2*srate) : 0;
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
        decoder = (fsurround_decoder*)dp.acquire((void*)1);
        decoder->flush();
        if (bufs)
            bufs->clear();
        decoder->sample_rate(srate);
    }
    SetParams();
}

void FreeSurround::close() 
{
    if (decoder)
    {
        dp.release(this);
        decoder = 0;
    }
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

