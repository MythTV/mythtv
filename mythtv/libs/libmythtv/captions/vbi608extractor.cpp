/*
  VBI 608 Extractor, extracts CEA-608 VBI from a line of raw data.
  Copyright (C) 2010  Digital Nirvana, Inc.

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

#include <algorithm>
#include <cfloat>
#include <cstdint>

#include "libmythbase/mythlogging.h"
#include "captions/vbi608extractor.h"

#define LOC QString("VBI608Extractor: ")

static void print(
    const QList<uint>  &raw_minimas, const QList<uint>  &raw_maximas,
    const QList<float> &minimas,     const QList<float> &maximas)
{
    QString raw_mins;
    QString raw_maxs;
    for (uint minima : std::as_const(raw_minimas))
        raw_mins += QString("%1,").arg(minima);
    for (uint maxima : std::as_const(raw_maximas))
        raw_maxs += QString("%1,").arg(maxima);
    LOG(VB_VBI, LOG_DEBUG, QString("raw mins: %1").arg(raw_mins));
    LOG(VB_VBI, LOG_DEBUG, QString("raw maxs: %1").arg(raw_maxs));

    QString mins;
    QString maxs;
    for (float minima : std::as_const(minimas))
        mins += QString("%1,").arg(minima);
    for (float maxima : std::as_const(maximas))
        maxs += QString("%1,").arg(maxima);
    LOG(VB_VBI, LOG_DEBUG, QString("mins: %1 maxs: %2")
            .arg(mins, maxs));
}

static float find_clock_diff(const QList<float> &list)
{
    float min_diff = FLT_MAX;
    float max_diff = 0.0F;
    float avg_diff = 0.0F;
    for (uint i = 1; i < uint(list.size()); i++)
    {
        float diff = list[i] - list[i-1];
        min_diff = std::min(diff, min_diff);
        max_diff = std::max(diff, max_diff);
        avg_diff += diff;
    }
    if (list.size() >= 2)
        avg_diff /= (list.size() - 1);
    if (avg_diff * 1.15F < max_diff)
    {
        LOG(VB_VBI, LOG_DEBUG, "max_diff too big");
        return 0.0F;
    }
    if (avg_diff * 0.85F > max_diff)
    {
        LOG(VB_VBI, LOG_DEBUG, "min_diff too small");
        return 0.0F;
    }

    return avg_diff;
}

bool VBI608Extractor::FindClocks(const unsigned char *buf, uint width)
{
    m_rawMinimas.clear();
    m_rawMaximas.clear();
    m_maximas.clear();
    m_minimas.clear();

    // find our threshold
    uint minv = 255;
    for (uint j = width / 8; j < width / 4; j++)
        minv = std::min(uint(buf[j]), minv);
    uint maxv = 0;
    for (uint j = width / 8; j < width / 4; j++)
        maxv = std::max(uint(buf[j]), maxv);
    uint avgv = (maxv<minv) ? 0 : minv + ((maxv-minv) / 2);
    if (avgv <= 11)
    {
        LOG(VB_VBI, LOG_DEBUG, QString("FindClocks: avgv(%1) <= 11").arg(avgv));
        return false;
    }

    // get the raw minima and maxima list
    uint noise_flr_sm = std::max(uint(0.003 * width), 2U);
    uint noise_flr_lg = std::max(uint(0.007 * width), noise_flr_sm+1);
    int last_max = -1;
    int last_min = -1;
    for (uint i = 0; i < (width/3); i++)
    {
        if (buf[i] > avgv+10)
            m_rawMaximas.push_back(last_max=i);
        else if (last_max>=0 && (i - last_max) <= noise_flr_sm)
            m_rawMaximas.push_back(i);
        else if (buf[i] < avgv-10)
            m_rawMinimas.push_back(last_min=i);
        else if (last_min>=0 && (i - last_min) <= noise_flr_lg)
            m_rawMinimas.push_back(i);
    }

    for (uint i = 0; i < uint(m_rawMaximas.size()); i++)
    {
        uint start_idx = m_rawMaximas[i];
        while ((i+1) < uint(m_rawMaximas.size()) &&
               (m_rawMaximas[i+1] == m_rawMaximas[i] + 1)) i++;
        uint end_idx = m_rawMaximas[i];
        if (end_idx - start_idx > noise_flr_lg)
            m_maximas.push_back((start_idx + end_idx) * 0.5F);
    }

    if (m_maximas.size() < 7)
    {
        LOG(VB_VBI, LOG_DEBUG, LOC +
            QString("FindClocks: maximas %1 < 7").arg(m_maximas.size()));
        print(m_rawMinimas, m_rawMaximas, m_minimas, m_maximas);
        return false;
    }

    // drop outliers on edges
    bool dropped = true;
    while (m_maximas.size() > 7 && dropped)
    {
        float min_diff = width * 8;
        float max_diff = 0.0F;
        float avg_diff = 0.0F;
        for (uint i = 1; i < uint(m_maximas.size()); i++)
        {
            float diff = m_maximas[i] - m_maximas[i-1];
            min_diff = std::min(diff, min_diff);
            max_diff = std::max(diff, max_diff);
            avg_diff += diff;
        }
        avg_diff -= min_diff;
        avg_diff -= max_diff;
        avg_diff /= (m_maximas.size() - 3);

        dropped = false;
        if (avg_diff * 1.1F < max_diff)
        {
            float last_diff = m_maximas.back() -
                              m_maximas[(uint)(m_maximas.size())-2];
            if (last_diff*1.01F >= max_diff || last_diff > avg_diff * 1.2F)
            {
                m_maximas.pop_back();
                dropped = true;
            }
            float first_diff = m_maximas[1] - m_maximas[0];
            if ((m_maximas.size() > 7) && (first_diff*1.01F >= max_diff))
            {
                m_maximas.pop_front();
                dropped = true;
            }
        }

        if (avg_diff * 0.9F > min_diff)
        {
            float last_diff = m_maximas.back() -
                              m_maximas[(uint)(m_maximas.size())-2];
            if ((m_maximas.size() > 7) &&
                (last_diff*0.99F <= min_diff || last_diff < avg_diff * 0.80F))
            {
                m_maximas.pop_back();
                dropped = true;
            }
            float first_diff = m_maximas[1] - m_maximas[0];
            if ((m_maximas.size() > 7) && (first_diff*0.99F <= min_diff))
            {
                m_maximas.pop_front();
                dropped = true;
            }
        }
    }

    if (m_maximas.size() != 7)
    {
        LOG(VB_VBI, LOG_DEBUG, LOC + QString("FindClocks: maximas: %1 != 7")
                .arg(m_maximas.size()));
        print(m_rawMinimas, m_rawMaximas, m_minimas, m_maximas);
        return false;
    }

    // find the minimas
    for (uint i = 0; i < uint(m_rawMinimas.size()); i++)
    {
        uint start_idx = m_rawMinimas[i];
        while ((i+1) < uint(m_rawMinimas.size()) &&
               (m_rawMinimas[i+1] == m_rawMinimas[i] + 1)) i++;
        uint end_idx = m_rawMinimas[i];
        float center = (start_idx + end_idx) * 0.5F;
        if (end_idx - start_idx > noise_flr_lg &&
            center > m_maximas[0] && center < m_maximas.back())
        {
            m_minimas.push_back(center);
        }
    }

    if (m_minimas.size() != 6)
    {
        LOG(VB_VBI, LOG_DEBUG, LOC + QString("FindClocks: minimas: %1 != 6")
                .arg(m_minimas.size()));
        print(m_rawMinimas, m_rawMaximas, m_minimas, m_maximas);
        return false;
    }

    // get the average clock rate in samples
    float maxima_avg_diff = find_clock_diff(m_maximas);
    float minima_avg_diff = find_clock_diff(m_minimas);
    m_rate = (maxima_avg_diff * 7 + minima_avg_diff * 6) / 13.0F;
    if (maxima_avg_diff == 0.0F || minima_avg_diff == 0.0F)
        return false;

    // get the estimated location of the first maxima
    // based on the rate and location of all maximas
    m_start = m_maximas[0];
    for (uint i = 1; i < uint(m_maximas.size()); i++)
        m_start += m_maximas[i] - (i * m_rate);
    m_start /= m_maximas.size();
    // then move it back by a third to make each sample
    // more or less in the center of each encoded byte.
    m_start -= m_rate * 0.33F;

    // if the last bit is after the last sample...
    // 7 clocks + 3 bits run in + 16 bits data
    if (m_start+((7+3+8+8-1) * m_rate) > width)
    {
        LOG(VB_VBI, LOG_DEBUG, LOC + QString("FindClocks: end %1 > width %2")
                .arg(m_start+((7+3+8+8-1) * m_rate)).arg(width));

        return false;
    }

#if 0
    LOG(VB_VBI, LOG_DEBUG, LOC + QString("FindClocks: Clock start %1, rate %2")
            .arg(m_start).arg(m_rate));
#endif

    return true;
}

bool VBI608Extractor::ExtractCC(const MythVideoFrame *picframe, uint max_lines)
{
    int ypitch = picframe->m_pitches[0];
    int ywidth = picframe->m_width;

    m_code[0] = UINT16_MAX;
    m_code[1] = UINT16_MAX;

    // find CC
    uint found_cnt = 0;
    for (uint i = 0; i < max_lines; i++)
    {
        const unsigned char *y = picframe->m_buffer +
            picframe->m_offsets[0] + (i * static_cast<ptrdiff_t>(ypitch));
        if (FindClocks(y, ywidth))
        {
            uint maxv = 0;
            for (uint j = 0; j < m_start + (8 * m_rate); j++)
                maxv = std::max(uint((y+(i * static_cast<ptrdiff_t>(ypitch)))[j]), maxv);
            uint avgv = maxv / 2;

            if (y[uint(m_start + ((0+7) * m_rate))] > avgv ||
                y[uint(m_start + ((1+7) * m_rate))] > avgv ||
                y[uint(m_start + ((2+7) * m_rate))] < avgv)
            {
                continue; // need 001 at run in..
            }

            m_code[found_cnt] = 0;
            for (uint j = 0; j < 8+8; j++)
            {
                bool bit = y[uint(m_start + ((j+7+3) * m_rate))] > avgv;
                m_code[found_cnt] =
                    (m_code[found_cnt]>>1) | (bit?(1<<15):0);
            }

            found_cnt++;
            if (found_cnt>=2)
                break;
#if 0
            unsigned char *Y = const_cast<unsigned char*>(y);
            unsigned char *u = const_cast<unsigned char*>
                (picframe->buf + picframe->offsets[1] +
                 (i*picframe->pitches[1]));
            unsigned char *v = const_cast<unsigned char*>
                (picframe->buf + picframe->offsets[2] +
                 (i*picframe->pitches[2]));
            unsigned uwidth = picframe->pitches[1];
            v[uwidth / 3] = 0x40;
            for (uint j = 0; j < 7+3+8+8; j++)
            {
                uint yloc = uint (m_start + j * m_rate + 0.5);
                Y[yloc] = 0xFF;
                uint uloc = uint (uwidth * (m_start + j * m_rate + 0.5) / ywidth);
                u[uloc] = 0x40;
            }
#endif
        }
    }

    return found_cnt > 0;
}

bool VBI608Extractor::ExtractCC12(const unsigned char *buf, uint width)
{
    m_code[0] = UINT16_MAX;
    if (FindClocks(buf, width))
    {
        uint maxv = 0;
        for (uint j = 0; j < m_start + (8 * m_rate); j++)
            maxv = std::max(uint(buf[j]), maxv);
        uint avgv = maxv / 2;

        if (buf[uint(m_start + ((0+7) * m_rate))] > avgv ||
            buf[uint(m_start + ((1+7) * m_rate))] > avgv ||
            buf[uint(m_start + ((2+7) * m_rate))] < avgv)
        {
            LOG(VB_VBI, LOG_DEBUG, LOC + "did not find VBI 608 header");
            return false;
        }

        m_code[0] = 0;
        for (uint j = 0; j < 8+8; j++)
        {
            bool bit = buf[uint(m_start + ((j+7+3) * m_rate))] > avgv;
            m_code[0] = (m_code[0]>>1) | (bit?(1<<15):0);
        }

        return true;
    }
    return false;
}

bool VBI608Extractor::ExtractCC34(const unsigned char *buf, uint width)
{
    m_code[1] = UINT16_MAX;
    if (FindClocks(buf, width))
    {
        uint maxv = 0;
        for (uint j = 0; j < m_start + (8 * m_rate); j++)
            maxv = std::max(uint(buf[j]), maxv);
        uint avgv = maxv / 2;

        if (buf[uint(m_start + ((0+7) * m_rate))] > avgv ||
            buf[uint(m_start + ((1+7) * m_rate))] > avgv ||
            buf[uint(m_start + ((2+7) * m_rate))] < avgv)
        {
            return false;
        }

        m_code[1] = 0;
        for (uint j = 0; j < 8+8; j++)
        {
            bool bit = buf[uint(m_start + ((j+7+3) * m_rate))] > avgv;
            m_code[1] = (m_code[1]>>1) | (bit?(1<<15):0);
        }
        return true;
    }
    return false;
}

uint VBI608Extractor::FillCCData(cc608_data &cc_data) const
{
    uint cc_count = 0;
    if (m_code[0] != UINT16_MAX)
    {
        cc_data[2] = 0x04;
        cc_data[3] = (m_code[0])    & 0xff;
        cc_data[4] = (m_code[0]>>8) & 0xff;
        cc_count++;
    }

    if (m_code[1] != UINT16_MAX)
    {
        cc_data[2+(3*cc_count)] = 0x04|0x01;
        cc_data[3+(3*cc_count)] = (m_code[1])    & 0xff;
        cc_data[4+(3*cc_count)] = (m_code[1]>>8) & 0xff;
        cc_count++;
    }

    if (cc_count)
    {
        cc_data[0] = 0x40 | cc_count;
        cc_data[1] = 0x00;
        return 2+(3*cc_count);
    }
    return 0;
}
