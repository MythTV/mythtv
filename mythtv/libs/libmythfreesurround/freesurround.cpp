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
#include <iostream>
#include <sstream>
#include "compat.h"
#include "freesurround.h"
#include "el_processor.h"
#include <vector>
#include <list>
#include <map>
#include <math.h>

#include <qstring.h>
#include <qdatetime.h>

using namespace std;

#if 0
#define VERBOSE(args...) \
    do { \
        QDateTime dtmp = QDateTime::currentDateTime(); \
        QString dtime = dtmp.toString("yyyy-MM-dd hh:mm:ss.zzz"); \
        ostringstream verbose_macro_tmp; \
        verbose_macro_tmp << dtime << " " << args; \
        cout << verbose_macro_tmp.str() << endl; \
    } while (0)
#else
#define VERBOSE(args...)
#endif
#if 0
#define VERBOSE1(args...) \
    do { \
        QDateTime dtmp = QDateTime::currentDateTime(); \
        QString dtime = dtmp.toString("yyyy-MM-dd hh:mm:ss.zzz"); \
        ostringstream verbose_macro_tmp; \
        verbose_macro_tmp << dtime << " " << args; \
        cout << verbose_macro_tmp.str() << endl; \
    } while (0)
#else
#define VERBOSE1(args...)
#endif

// our default internal block size, in floats
const unsigned default_block_size = 8192;
// there will be a slider for this in the future
//const float master_gain = 1.0;
//#define MASTER_GAIN * master_gain
#define MASTER_GAIN
//const float master_gain = 1.0/(1<<15);
//const float inv_master_gain = (1<<15);
//#define INV_MASTER_GAIN * inv_master_gain
#define INV_MASTER_GAIN

unsigned int block_size = default_block_size;

// stupidity countermeasure...
template<class T> T pop_back(std::list<T> &l)
{
    T result(l.back());
    l.pop_back();
    return result;
}

// a pool, where the DSP can throw its objects at after it got deleted and get them back when it is recreated...
class object_pool
{
public:
	typedef void* (*callback)();
	// initialize
	object_pool(callback cbf):construct(cbf) { }
	~object_pool()
    {
		for (std::map<void*,void*>::iterator i=pool.begin(),e=pool.end();i!=e;i++)
			delete i->second;
		for (std::list<void*>::iterator i=freelist.begin(),e=freelist.end();i!=e;i++)
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
				return pool.insert(std::make_pair(who,pop_back(freelist))).first->second;
			else
				return pool.insert(std::make_pair(who,construct())).first->second;
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
	callback construct;			// object constructor callback
	std::list<void*> freelist;		// list of available objects
	std::map<void*,void*> pool;	// pool of used objects, by class
};

// buffers which we usually need (and want to share between plugin lifespans)
struct buffers
{
	buffers(unsigned int s): 
        //lt(s),rt(s),
        l(s),r(s),c(s),ls(s),rs(s),lfe(s) { }
	void resize(unsigned int s)
    {
		//lt.resize(s); rt.resize(s);
        l.resize(s); r.resize(s); lfe.resize(s); 
		ls.resize(s); rs.resize(s); c.resize(s);
	}
	void clear()
    {
		//lt.clear(); rt.clear();
        l.clear(); r.clear(); lfe.clear();
		ls.clear(); rs.clear(); c.clear();
	}
	//std::vector<float> lt,rt;						// for multiplexing
	std::vector<float> l,r,c,ls,rs,lfe,cs,lcs,rcs;	// for demultiplexing
};

struct int16buffers
{
	int16buffers(unsigned int s): 
        l(s),r(s),c(s),ls(s),rs(s),lfe(s) { }
	void resize(unsigned int s)
    {
		l.resize(s); r.resize(s); lfe.resize(s); 
		ls.resize(s); rs.resize(s); c.resize(s);
	}
	void clear()
    {
		l.clear(); r.clear();
		ls.clear(); rs.clear(); c.clear();
        lfe.clear();
	}
	std::vector<short> l,r,c,ls,rs,lfe;	// for demultiplexing
};

// construction methods
void *new_decoder() { return new fsurround_decoder(block_size); }
void *new_buffers() { return new buffers(block_size/2); }
void *new_int16buffers() { return new int16buffers(block_size/2); }

object_pool dp(&new_decoder);
//object_pool bp(&new_buffers);
object_pool bp16(&new_int16buffers);

//#define SPEAKERTEST
#ifdef SPEAKERTEST
int channel_select = -1;
#endif

FreeSurround::FreeSurround(uint srate, bool moviemode, SurroundMode smode) :
        srate(srate),
        open_(false),
        initialized_(false),
        //bufs(NULL),
        int16bufs(NULL),
        decoder(0),
        in_count(0),
        out_count(0),
        processed(true),
        surround_mode(smode)
{
    VERBOSE(QString("FreeSurround::FreeSurround rate %1 moviemode %2").arg(srate).arg(moviemode));
    if (moviemode)
    {
        params.phasemode = 1;
        params.center_width = 0;
        params.gain = 1.0;
    }
    else
    {
        params.center_width = 70;
        // for 50, gain should be about 1.9, c/lr about 2.7
        // for 70, gain should be about 3.1, c/lr about 1.5
        params.gain = 3.1;
    }
    switch (surround_mode)
    {
        case SurroundModeActiveSimple:
            params.steering = 0;
            //bufs = (buffers*)bp.acquire((void*)1);
            break;
        case SurroundModeActiveLinear:
            params.steering = 1;
            //bufs = (buffers*)bp.acquire((void*)1);
            break;
        default:
            //int16bufs = (int16buffers*)bp16.acquire((void*)1);
            break;
    }
    int16bufs = (int16buffers*)bp16.acquire((void*)1);
    open();
#ifdef SPEAKERTEST
    channel_select++;
    if (channel_select>=6)
        channel_select = 0;
    VERBOSE(QString("FreeSurround::FreeSurround channel_select %1").arg(channel_select));
#endif

    VERBOSE(QString("FreeSurround::FreeSurround done"));
}

void FreeSurround::SetParams()
{
    if (decoder)
    {
        decoder->steering_mode(params.steering);
        decoder->phase_mode(params.phasemode);
        decoder->surround_coefficients(params.coeff_a, params.coeff_b);				
        decoder->separation(params.front_sep/100.0,params.rear_sep/100.0);
        decoder->gain(params.gain);
    }
}

FreeSurround::fsurround_params::fsurround_params(
        int32_t center_width, 
        int32_t dimension
    ) : 
    center_width(center_width), 
    dimension(dimension),
    coeff_a(0.8165),coeff_b(0.5774),
    phasemode(0),
    steering(1),
    front_sep(100),
    rear_sep(100), 
    gain(1.0)
{
}

FreeSurround::~FreeSurround()
{
    VERBOSE(QString("FreeSurround::~FreeSurround"));
    close();
    /*
    if (bufs)
    {
        bp.release((void*)1);
        bufs = NULL;
    }
    */
    if (int16bufs)
    {
        bp16.release((void*)1);
        int16bufs = NULL;
    }
    VERBOSE(QString("FreeSurround::~FreeSurround done"));
}

uint FreeSurround::putSamples(short* samples, uint numSamples, uint numChannels, int step)
{
    int i;
    int ic = in_count;
    int bs = block_size/2;
    bool process = true;
    // demultiplex
    switch (surround_mode)
    {
        case SurroundModePassive:
            switch (numChannels)
            {
                case 1:
                    for (i=0;(i<numSamples) && (ic < bs);i++,ic++)
                    {
                        int16bufs->l[ic] = 
                        int16bufs->c[ic] = 
                        int16bufs->r[ic] = 
                            samples[i] >> 1;
                    }
                    break;
                case 2:
                    if (step>0)
                    {
                        for (i=0;(i<numSamples) && (ic < bs);i++,ic++)
                        {
                            short lt = samples[i] >> 1;
                            short rt = samples[i+step] >> 1;
                            int16bufs->l[ic] = lt;
                            int16bufs->lfe[ic] = 
                            int16bufs->c[ic] = ((lt+rt)*23)>>5; // about sqrt(0.5)
                            int16bufs->r[ic] = rt;
                            int16bufs->ls[ic] = 
                            int16bufs->rs[ic] = ((lt-rt)*23)>>5;
                        }
                    }
                    else
                    {
                        for (i=0;(i<numSamples) && (ic < bs);i++,ic++)
                        {
                            short lt = samples[i*2] >> 1;
                            short rt = samples[i*2+1] >> 1;
                            int16bufs->l[ic] = lt;
                            int16bufs->lfe[ic] = 
                            int16bufs->c[ic] = ((lt+rt)*23)>>5; // about sqrt(0.5)
                            int16bufs->r[ic] = rt;
                            int16bufs->ls[ic] = 
                            int16bufs->rs[ic] = ((lt-rt)*23)>>5;
                        }
                    }
                    break;
                case 6:
                    for (i=0;(i<numSamples) && (ic < bs);i++,ic++)
                    {
                        int16bufs->l[ic] = *samples++ >> 1;
                        int16bufs->c[ic] = *samples++ >> 1;
                        int16bufs->r[ic] = *samples++ >> 1;
                        int16bufs->ls[ic] = *samples++ >> 1;
                        int16bufs->rs[ic] = *samples++ >> 1;
                        int16bufs->lfe[ic] = *samples++ >> 1;
                    }
                    break;
            }
            in_count = 0;
            out_count = ic;
            processed_size = ic;
            processed = false;
            break;

        default:
            {
                float** inputs = decoder->getInputBuffers();
                float * lt = &inputs[0][ic];
                float * rt = &inputs[1][ic];
                if ((ic+numSamples) > bs)
                    numSamples = bs - ic;
                switch (numChannels)
                {
                    case 1:
                        for (i=0;i<numSamples;i++)
                        {
                            *lt++ = 
                            *rt++ = 
                                *samples++ MASTER_GAIN;
                        }
                        break;
                    case 2:
                        if (step>0)
                        {
                            for (i=0;i<numSamples;i++)
                            {
                                *lt++ = samples[0] MASTER_GAIN;
                                *rt++ = samples[step] MASTER_GAIN;
                                samples++;
                            }
                        }
                        else
                        {
                            for (i=0;i<numSamples;i++)
                            {
                                *lt++ = *samples++ MASTER_GAIN;
                                *rt++ = *samples++ MASTER_GAIN;
                            }
                        }
                        break;
                    case 6:
                        {
                        process = false;
                        short * l = &int16bufs->l[ic];
                        short * c = &int16bufs->c[ic];
                        short * r = &int16bufs->r[ic];
                        short * ls = &int16bufs->ls[ic];
                        short * rs = &int16bufs->rs[ic];
                        short * lfe = &int16bufs->lfe[ic];
                        for (i=0;i<numSamples;i++)
                        {
                            *l++ = *samples++ >> 1;
                            *c++ = *samples++ >> 1;
                            *r++ = *samples++ >> 1;
                            *ls++ = *samples++ >> 1;
                            *rs++ = *samples++ >> 1;
                            *lfe++ = *samples++ >> 1;
                        }
                        } break;
                }
                ic += numSamples;
                in_count = ic;
                processed = process;
                if (ic == bs)
                {
                    in_count = 0;
                    if (process)
                    {
                        process_block();
                    }
                    out_count = bs;
                    processed_size = bs;
                }
            } break;
    }
    VERBOSE1(QString("FreeSurround::putSamples %1 %2 %3 used %4 generated %5")
            .arg(numSamples)
            .arg(numChannels)
            .arg(step)
            .arg(i)
            .arg(out_count)
           );
    return i;
}

uint FreeSurround::putSamples(char* samples, uint numSamples, uint numChannels, int step)
{
    int i;
    int ic = in_count;
    int bs = block_size/2;
    bool process = true;
    // demultiplex
    switch (surround_mode)
    {
        case SurroundModePassive:
            switch (numChannels)
            {
                case 1:
                    for (i=0;(i<numSamples) && (ic < bs);i++,ic++)
                    {
                        int16bufs->l[ic] = 
                        int16bufs->c[ic] = 
                        int16bufs->r[ic] = 
                            samples[i] << 7;
                    }
                    break;
                case 2:
                    if (step>0)
                    {
                        for (i=0;(i<numSamples) && (ic < bs);i++,ic++)
                        {
                            short lt = samples[i] << 7;
                            short rt = samples[i+step] << 7;
                            int16bufs->l[ic] = lt;
                            int16bufs->lfe[ic] = 
                            int16bufs->c[ic] = ((lt+rt)*23)>>5; // about sqrt(0.5)
                            int16bufs->r[ic] = rt;
                            int16bufs->ls[ic] = 
                            int16bufs->rs[ic] = ((lt-rt)*23)>>5;
                        }
                    }
                    else
                    {
                        for (i=0;(i<numSamples) && (ic < bs);i++,ic++)
                        {
                            short lt = samples[i*2] << 7;
                            short rt = samples[i*2+1] << 7;
                            int16bufs->l[ic] = lt;
                            int16bufs->lfe[ic] = 
                            int16bufs->c[ic] = ((lt+rt)*23)>>5; // about sqrt(0.5)
                            int16bufs->r[ic] = rt;
                            int16bufs->ls[ic] = 
                            int16bufs->rs[ic] = ((lt-rt)*23)>>5;
                        }
                    }
                    break;
                case 6:
                    for (i=0;(i<numSamples) && (ic < bs);i++,ic++)
                    {
                        int16bufs->l[ic] = *samples++ << 7;
                        int16bufs->c[ic] = *samples++ << 7;
                        int16bufs->r[ic] = *samples++ << 7;
                        int16bufs->ls[ic] = *samples++ << 7;
                        int16bufs->rs[ic] = *samples++ << 7;
                        int16bufs->lfe[ic] = *samples++ << 7;
                    }
                    break;
            }
            in_count = 0;
            out_count = ic;
            processed_size = ic;
            processed = false;
            break;

        default:
            {
                float** inputs = decoder->getInputBuffers();
                float * lt = &inputs[0][ic];
                float * rt = &inputs[1][ic];
                if ((ic+numSamples) > bs)
                    numSamples = bs - ic;
                switch (numChannels)
                {
                    case 1:
                        for (i=0;i<numSamples;i++)
                        {
                            *lt++ = 
                            *rt++ = 
                                *samples++ MASTER_GAIN;
                        }
                        break;
                    case 2:
                        if (step>0)
                        {
                            for (i=0;i<numSamples;i++)
                            {
                                *lt++ = samples[0] MASTER_GAIN;
                                *rt++ = samples[step] MASTER_GAIN;
                                samples++;
                            }
                        }
                        else
                        {
                            for (i=0;i<numSamples;i++)
                            {
                                *lt++ = *samples++ MASTER_GAIN;
                                *rt++ = *samples++ MASTER_GAIN;
                            }
                        }
                        break;
                    case 6:
                        {
                        process = false;
                        short * l = &int16bufs->l[ic];
                        short * c = &int16bufs->c[ic];
                        short * r = &int16bufs->r[ic];
                        short * ls = &int16bufs->ls[ic];
                        short * rs = &int16bufs->rs[ic];
                        short * lfe = &int16bufs->lfe[ic];
                        for (i=0;i<numSamples;i++)
                        {
                            *l++ = *samples++ << 7;
                            *c++ = *samples++ << 7;
                            *r++ = *samples++ << 7;
                            *ls++ = *samples++ << 7;
                            *rs++ = *samples++ << 7;
                            *lfe++ = *samples++ << 7;
                        }
                        } break;
                }
                ic += numSamples;
                in_count = ic;
                processed = process;
                if (ic == bs)
                {
                    in_count = 0;
                    if (process)
                        process_block();
                    out_count = bs;
                    processed_size = bs;
                }
            } break;
    }
    VERBOSE1(QString("FreeSurround::putSamples %1 %2 %3 used %4 generated %5")
            .arg(numSamples)
            .arg(numChannels)
            .arg(step)
            .arg(i)
            .arg(out_count)
           );
    return i;
}

uint FreeSurround::receiveSamples(
        short *output, 
        uint maxSamples
        )
{
    uint i;
    uint oc = out_count;
    if (maxSamples>oc) maxSamples = oc;
    uint outindex = processed_size - oc;
    switch (surround_mode)
    {
        case SurroundModePassive:
            for (unsigned int i=0;i<maxSamples;i++) 
            {
                *output++ = int16bufs->l[outindex];
                *output++ = int16bufs->r[outindex];
                *output++ = int16bufs->ls[outindex];
                *output++ = int16bufs->rs[outindex];
                *output++ = int16bufs->c[outindex];
                *output++ = int16bufs->lfe[outindex];
                oc--;
                outindex++;
            }
            break;

        default:
            if (processed)
            {
                float** outputs = decoder->getOutputBuffers();
                float * l = &outputs[0][outindex];
                float * c = &outputs[1][outindex];
                float * r = &outputs[2][outindex];
                float * ls = &outputs[3][outindex];
                float * rs = &outputs[4][outindex];
                float * lfe = &outputs[5][outindex];
                for (unsigned int i=0;i<maxSamples;i++) 
                {
                    *output++ = lrintf(*l++ INV_MASTER_GAIN);
                    *output++ = lrintf(*r++ INV_MASTER_GAIN);
                    *output++ = lrintf(*ls++ INV_MASTER_GAIN);
                    *output++ = lrintf(*rs++ INV_MASTER_GAIN);
                    *output++ = lrintf(*c++ INV_MASTER_GAIN);
                    *output++ = lrintf(*lfe++ INV_MASTER_GAIN);
                }
                oc -= maxSamples;
                outindex += maxSamples;
            }
            else
            {
                short * l = &int16bufs->l[outindex];
                short * c = &int16bufs->c[outindex];
                short * r = &int16bufs->r[outindex];
                short * ls = &int16bufs->ls[outindex];
                short * rs = &int16bufs->rs[outindex];
                short * lfe = &int16bufs->lfe[outindex];
                for (unsigned int i=0;i<maxSamples;i++) 
                {
                    *output++ = *l++;
                    *output++ = *r++;
                    *output++ = *ls++;
                    *output++ = *rs++;
                    *output++ = *c++;
                    *output++ = *lfe++;
                }
                oc -= maxSamples;
                outindex += maxSamples;
            }
            break;
    }
    out_count = oc;
    VERBOSE1(QString("FreeSurround::receiveSamples %1")
            .arg(maxSamples)
           );
    return maxSamples;
}

void FreeSurround::process_block()
{
    // process the data
    try 
    {
        if (decoder) 
        {
            // actually these params need only be set when they change... but it doesn't hurt
#if 0
            decoder->steering_mode(params.steering);
            decoder->phase_mode(params.phasemode);
            decoder->surround_coefficients(params.coeff_a, params.coeff_b);				
            decoder->separation(params.front_sep/100.0,params.rear_sep/100.0);
#endif
            // decode the bufs->block
            //decoder->decode(input,output,params.center_width/100.0,params.dimension/100.0);
            //decoder->decode(output,params.center_width/100.0,params.dimension/100.0);
            decoder->decode(params.center_width/100.0,params.dimension/100.0);
        }
    }
    catch(...)
    {
        //throw(std::runtime_error(std::string("error during processing (unsupported input format?)")));
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
    int16bufs->clear();
}

// load the lib and initialize the interface
void FreeSurround::open() 
{		
    if (!decoder)
    {
        decoder = (fsurround_decoder*)dp.acquire((void*)1);
        decoder->flush();
        //if (bufs)
        //    bufs->clear();
        if (int16bufs)
            int16bufs->clear();
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

uint FreeSurround::numUnprocessedSamples()
{
    return in_count;
}

uint FreeSurround::numSamples()
{
    return out_count;
}

uint FreeSurround::sampleLatency()
{
    if (processed)
        return in_count + out_count + (block_size/2);
    else
        return in_count + out_count;
}

uint FreeSurround::samplesPerBlock()
{
    return block_size/2;
}

