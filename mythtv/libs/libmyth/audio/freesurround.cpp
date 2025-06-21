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
#include "freesurround.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <cmath>

#include <iostream>
#include <sstream>
#include <vector>
#include <list>
#include <map>

#include "libmythbase/mythlogging.h"

#include <QString>

#include "freesurround_decoder.h"

// our default internal block size, in floats
static const unsigned default_block_size = SURROUND_BUFSIZE;
// Gain of center and lfe channels in passive mode (sqrt 0.5)
//static const float center_level = 0.707107;
static const float m3db = 0.7071067811865476F;           // 3dB  = SQRT(2)
static const float m6db = 0.5;                           // 6dB  = SQRT(4)
//static const float m7db = 0.44721359549996;            // 7dB  = SQRT(5)

unsigned int block_size = default_block_size;

struct buffers
{
    explicit buffers(unsigned int s):
        m_l(s),m_r(s),m_c(s),m_ls(s),m_rs(s),m_lfe(s), m_rls(s), m_rrs(s) { }
    void resize(unsigned int s)
    {
        m_l.resize(s);   m_r.resize(s);  m_lfe.resize(s);
        m_ls.resize(s);  m_rs.resize(s); m_c.resize(s);
        m_rls.resize(s); m_rrs.resize(s);
    }
    void clear()
    {
        m_l.clear();   m_r.clear();  m_lfe.clear();
        m_ls.clear();  m_rs.clear(); m_c.clear();
        m_rls.clear(); m_rrs.clear();
    }
    std::vector<float> m_l,m_r,m_c,m_ls,m_rs,m_lfe,m_cs,m_lcs,m_rcs,
                       m_rls, m_rrs;       // for demultiplexing
};

//#define SPEAKERTEST
#ifdef SPEAKERTEST
int channel_select = -1;
#endif

FreeSurround::FreeSurround(uint srate, bool moviemode, SurroundMode smode) :
    m_srate(srate),
    m_surroundMode(smode)
{
    LOG(VB_AUDIO, LOG_DEBUG,
        QString("FreeSurround::FreeSurround rate %1 moviemode %2")
            .arg(srate).arg(moviemode));

    if (moviemode)
    {
        m_params.phasemode = 1;
        m_params.center_width = 25;
        m_params.dimension = 50;
    }
    else
    {
        m_params.center_width = 65;
        m_params.dimension = 30;
    }
    switch (m_surroundMode)
    {
        case SurroundModeActiveSimple:
            m_params.steering = 0;
            break;
        case SurroundModeActiveLinear:
            m_params.steering = 1;
            m_latencyFrames = block_size/2;
            break;
        default:
            break;
    }

    m_bufs = new buffers(block_size/2);
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
    if (m_decoder)
    {
        m_decoder->steering_mode(m_params.steering != 0);
        m_decoder->phase_mode(m_params.phasemode);
        m_decoder->surround_coefficients(m_params.coeff_a, m_params.coeff_b);
        m_decoder->separation(m_params.front_sep/100.0F,m_params.rear_sep/100.0F);
    }
}

FreeSurround::~FreeSurround()
{
    LOG(VB_AUDIO, LOG_DEBUG, QString("FreeSurround::~FreeSurround"));
    close();
    delete m_bufs;
    m_bufs = nullptr;
    LOG(VB_AUDIO, LOG_DEBUG, QString("FreeSurround::~FreeSurround done"));
}

uint FreeSurround::putFrames(void* buffer, uint numFrames, uint numChannels)
{
    uint i = 0;
    uint ic = m_inCount;
    uint bs = block_size/2;
    bool process = true;
    auto *samples = (float *)buffer;
    // demultiplex

    if ((m_surroundMode != SurroundModePassive) && (ic+numFrames > bs))
    {
        numFrames = bs - ic;
    }

    switch (numChannels)
    {
        case 1:
            switch (m_surroundMode)
            {
                case SurroundModePassive:
                case SurroundModePassiveHall:
                    for (i = 0; i < numFrames && ic < bs; i++,ic++)
                    {
                        // should be -7dB to keep power level the same
                        // but we bump the level a tad.
                        m_bufs->m_c[ic]  = m_bufs->m_l[ic]  = m_bufs->m_r[ic] = samples[i] * m6db;
                        m_bufs->m_ls[ic] = m_bufs->m_rs[ic] = m_bufs->m_c[ic];
                    }
                    process = false;
                    break;
                default:
                    float **inputs = m_decoder->getInputBuffers();
                    float *lt      = &inputs[0][ic];
                    float *rt      = &inputs[1][ic];
                    for (i=0; i<numFrames; i++)
                        *lt++ = *rt++ = *samples++;
                    process = true;
                    break;
            }
            m_channels = 6;
            break;

        case 2:
            switch (m_surroundMode)
            {
                case SurroundModePassive:
                    for (i = 0; i < numFrames && ic < bs; i++,ic++)
                    {
                        float lt      = *samples++;
                        float rt      = *samples++;
                        m_bufs->m_l[ic]   = lt;
                        m_bufs->m_lfe[ic] = m_bufs->m_c[ic] = (lt+rt) * m3db;
                        m_bufs->m_r[ic]   = rt;
                        // surround channels receive out-of-phase
                        m_bufs->m_ls[ic]  = (rt-lt) * 0.5F;
                        m_bufs->m_rs[ic]  = (lt-rt) * 0.5F;
                    }
                    process = false;
                    break;
                case SurroundModePassiveHall:
                    for (i = 0; i < numFrames && ic < bs; i++,ic++)
                    {
                        float lt      = *samples++;
                        float rt      = *samples++;
                        m_bufs->m_l[ic]   = lt * m3db;
                        m_bufs->m_lfe[ic] = m_bufs->m_c[ic] = (lt+rt) * m3db;
                        m_bufs->m_r[ic]   = rt * m3db;
                        m_bufs->m_ls[ic]  = m_bufs->m_l[ic];
                        m_bufs->m_rs[ic]  = m_bufs->m_r[ic];
                    }
                    process = false;
                    break;
                default:
                    float **inputs = m_decoder->getInputBuffers();
                    float *lt      = &inputs[0][ic];
                    float *rt      = &inputs[1][ic];
                    for (i=0; i<numFrames; i++)
                    {
                        *lt++ = *samples++;
                        *rt++ = *samples++;
                    }
                    process = true;
                    break;
            }
            m_channels = 6;
            break;

        case 5:
            for (i = 0; i < numFrames && ic < bs; i++,ic++)
            {
                float lt      = *samples++;
                float rt      = *samples++;
                float c       = *samples++;
                float ls      = *samples++;
                float rs      = *samples++;
                m_bufs->m_l[ic]   = lt;
                m_bufs->m_lfe[ic] = 0.0F;
                m_bufs->m_c[ic]   = c;
                m_bufs->m_r[ic]   = rt;
                m_bufs->m_ls[ic]  = ls;
                m_bufs->m_rs[ic]  = rs;
            }
            process = false;
            m_channels = 6;
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
                m_bufs->m_l[ic]   = lt;
                m_bufs->m_lfe[ic] = lfe;
                m_bufs->m_c[ic]   = c;
                m_bufs->m_r[ic]   = rt;
                m_bufs->m_ls[ic]  = ls;
                m_bufs->m_rs[ic]  = rs;
                m_bufs->m_rls[ic]  = m_bufs->m_rrs[ic]  = cs * m3db;
            }
            process = false;
            m_channels = 8;
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
            m_inCount = ic;
        }
        else
        {
            m_processed = process;
            // process_block takes some time so dont update in and out count
            // before its finished so that Audiotime is correctly calculated
            process_block();
            m_inCount = 0;
            m_outCount = bs;
            m_processedSize = bs;
            m_latencyFrames = block_size/2;
        }
    }
    else
    {
        m_inCount = 0;
        m_outCount = m_processedSize = ic;
        m_processed = false;
        m_latencyFrames = 0;
    }

    LOG(VB_AUDIO | VB_TIMESTAMP, LOG_DEBUG,
        QString("FreeSurround::putFrames %1 #ch %2 used %3 generated %4")
            .arg(numFrames).arg(numChannels).arg(i).arg(m_outCount));

    return i;
}

uint FreeSurround::receiveFrames(void *buffer, uint maxFrames)
{
    uint oc = m_outCount;
    maxFrames = std::min(maxFrames, oc);
    uint outindex = m_processedSize - oc;
    auto *output = (float *)buffer;
    if (m_channels == 8)
    {
        float *l   = &m_bufs->m_l[outindex];
        float *c   = &m_bufs->m_c[outindex];
        float *r   = &m_bufs->m_r[outindex];
        float *ls  = &m_bufs->m_ls[outindex];
        float *rs  = &m_bufs->m_rs[outindex];
        float *lfe = &m_bufs->m_lfe[outindex];
        float *rls = &m_bufs->m_rls[outindex];
        float *rrs = &m_bufs->m_rrs[outindex];
        for (uint i = 0; i < maxFrames; i++)
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
    }
    else        // channels == 6
    {
        if (m_processed)
        {
            float** outputs = m_decoder->getOutputBuffers();
            float *l   = &outputs[0][outindex];
            float *c   = &outputs[1][outindex];
            float *r   = &outputs[2][outindex];
            float *ls  = &outputs[3][outindex];
            float *rs  = &outputs[4][outindex];
            float *lfe = &outputs[5][outindex];
            for (uint i = 0; i < maxFrames; i++)
            {
                *output++ = *l++;
                *output++ = *r++;
                *output++ = *c++;
                *output++ = *lfe++;
                *output++ = *ls++;
                *output++ = *rs++;
            }
            oc -= maxFrames;
        }
        else
        {
            float *l   = &m_bufs->m_l[outindex];
            float *c   = &m_bufs->m_c[outindex];
            float *r   = &m_bufs->m_r[outindex];
            float *ls  = &m_bufs->m_ls[outindex];
            float *rs  = &m_bufs->m_rs[outindex];
            float *lfe = &m_bufs->m_lfe[outindex];
            for (uint i = 0; i < maxFrames; i++)
            {
                *output++ = *l++;
                *output++ = *r++;
                *output++ = *c++;
                *output++ = *lfe++;
                *output++ = *ls++;
                *output++ = *rs++;
            }
            oc -= maxFrames;
        }
    }
    m_outCount = oc;
    LOG(VB_AUDIO | VB_TIMESTAMP, LOG_DEBUG,
        QString("FreeSurround::receiveFrames %1").arg(maxFrames));
    return maxFrames;
}

void FreeSurround::process_block()
{
    // process the data
    try
    {
        if (m_decoder)
        {
            m_decoder->decode(m_params.center_width/100.0F,m_params.dimension/100.0F);
        }
    }
    catch(const std::exception& ex)
    {
        LOG(VB_AUDIO, LOG_DEBUG,
            QString("FreeSurround::process_block exception: %1").arg(ex.what()));
    }
    catch(...)
    {
        LOG(VB_AUDIO, LOG_DEBUG,
            QString("FreeSurround::process_block exception: unknown"));
    }
}

long long FreeSurround::getLatency()
{
    // returns in usec
    if (m_latencyFrames == 0)
        return 0;
    if (m_decoder == nullptr)
        return 0;
    return ((m_latencyFrames + m_inCount) * 1000000LL) / (2LL * m_srate);
}

void FreeSurround::flush()
{
    if (m_decoder)
        m_decoder->flush();
    m_bufs->clear();
}

// load the lib and initialize the interface
void FreeSurround::open()
{
    if (!m_decoder)
    {
        m_decoder = new fsurround_decoder(block_size);
        m_decoder->flush();
        if (m_bufs)
            m_bufs->clear();
        m_decoder->sample_rate(m_srate);
    }
    SetParams();
}

void FreeSurround::close()
{
    delete m_decoder;
    m_decoder = nullptr;
}

uint FreeSurround::numUnprocessedFrames() const
{
    return m_inCount;
}

uint FreeSurround::numFrames() const
{
    return m_outCount;
}

uint FreeSurround::frameLatency() const
{
    if (m_processed)
        return m_inCount + m_outCount + (block_size/2);
    return m_inCount + m_outCount;
}

uint FreeSurround::framesPerBlock()
{
    return block_size/2;
}

