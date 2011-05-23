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

#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS
#endif

#include <stdint.h>

#include <algorithm>
using namespace std;

#include "vbi608extractor.h"
#include "mythverbose.h"

#define LOC      QString("VBI608Extractor: ")
#define LOC_WARN QString("VBI608Extractor, Warning: ")
#define LOC_ERR  QString("VBI608Extractor, Error: ")

static void print(
    const QList<uint>  &raw_minimas, const QList<uint>  &raw_maximas,
    const QList<float> &minimas,     const QList<float> &maximas)
{
    QString raw_mins, raw_maxs;
    for (uint i = 0; i < uint(raw_minimas.size()); i++)
        raw_mins += QString("%1,").arg(raw_minimas[i]);
    for (uint i = 0; i < uint(raw_maximas.size()); i++)
        raw_maxs += QString("%1,").arg(raw_maximas[i]);
    VERBOSE(VB_VBI|VB_EXTRA, QString("raw mins: %1").arg(raw_mins));
    VERBOSE(VB_VBI|VB_EXTRA, QString("raw maxs: %1").arg(raw_maxs));

    QString mins, maxs;
    for (uint i = 0; i < uint(minimas.size()); i++)
        mins += QString("%1,").arg(minimas[i]);
    for (uint i = 0; i < uint(maximas.size()); i++)
        maxs += QString("%1,").arg(maximas[i]);
    VERBOSE(VB_VBI|VB_EXTRA, QString("mins: %1 maxs: %2")
            .arg(mins).arg(maxs));
}

static float find_clock_diff(const QList<float> &list)
{
    float min_diff = INT32_MAX;
    float max_diff = 0.0f;
    float avg_diff = 0.0f;
    for (uint i = 1; i < uint(list.size()); i++)
    {
        float diff = list[i] - list[i-1];
        min_diff = min(diff, min_diff);
        max_diff = max(diff, max_diff);
        avg_diff += diff;
    }
    if (list.size() >= 2)
        avg_diff /= (list.size() - 1);
    if (avg_diff * 1.15 < max_diff)
    {
        VERBOSE(VB_VBI|VB_EXTRA, "max_diff too big");
        return 0.0f;
    }
    if (avg_diff * 0.85 > max_diff)
    {
        VERBOSE(VB_VBI|VB_EXTRA, "min_diff too small");
        return 0.0f;
    }

    return avg_diff;
}

VBI608Extractor::VBI608Extractor() : start(0.0f), rate(0.0f)
{
    code[0] = UINT16_MAX;
    code[1] = UINT16_MAX;
}

bool VBI608Extractor::FindClocks(const unsigned char *buf, uint width)
{
    raw_minimas.clear();
    raw_maximas.clear();
    maximas.clear();
    minimas.clear();

    // find our threshold
    uint minv = 255;
    for (uint j = width / 8; j < width / 4; j++)
        minv = min(uint(buf[j]), minv);
    uint maxv = 0;
    for (uint j = width / 8; j < width / 4; j++)
        maxv = max(uint(buf[j]), maxv);
    uint avgv = (maxv<minv) ? 0 : minv + ((maxv-minv) / 2);
    if (avgv <= 11)
    {
        VERBOSE(VB_VBI|VB_EXTRA, QString("FindClocks: avgv(%1) <= 11").arg(avgv));
        return false;
    }

    // get the raw minima and maxima list
    uint noise_flr_sm = max(uint(0.003 * width), 2U);
    uint noise_flr_lg = max(uint(0.007 * width), noise_flr_sm+1);
    int last_max = -1, last_min = -1;
    for (uint i = 0; i < (width/3); i++)
    {
        if (buf[i] > avgv+10)
            raw_maximas.push_back(last_max=i);
        else if (last_max>=0 && (i - last_max) <= noise_flr_sm)
            raw_maximas.push_back(i);
        else if (buf[i] < avgv-10)
            raw_minimas.push_back(last_min=i);
        else if (last_min>=0 && (i - last_min) <= noise_flr_lg)
            raw_minimas.push_back(i);
    }

    for (uint i = 0; i < uint(raw_maximas.size()); i++)
    {
        uint start_idx = raw_maximas[i];
        while ((i+1) < uint(raw_maximas.size()) &&
               (raw_maximas[i+1] == raw_maximas[i] + 1)) i++;
        uint end_idx = raw_maximas[i];
        if (end_idx - start_idx > noise_flr_lg)
            maximas.push_back((start_idx + end_idx) * 0.5f);
    }

    if (maximas.size() < 7)
    {
        VERBOSE(VB_VBI|VB_EXTRA, LOC +
                QString("FindClocks: maximas %1 < 7").arg(maximas.size()));
        print(raw_minimas, raw_maximas, minimas, maximas);
        return false;
    }

    // drop outliers on edges
    bool dropped = true;
    while (maximas.size() > 7 && dropped)
    {
        float min_diff = width * 8;
        float max_diff = 0.0f;
        float avg_diff = 0.0f;
        for (uint i = 1; i < uint(maximas.size()); i++)
        {
            float diff = maximas[i] - maximas[i-1];
            min_diff = min(diff, min_diff);
            max_diff = max(diff, max_diff);
            avg_diff += diff;
        }
        avg_diff -= min_diff;
        avg_diff -= max_diff;
        avg_diff /= (maximas.size() - 3);

        dropped = false;
        if (avg_diff * 1.1f < max_diff)
        {
            float last_diff = maximas.back() - maximas[maximas.size()-2];
            if (last_diff*1.01f >= max_diff || last_diff > avg_diff * 1.2f)
            {
                maximas.pop_back();
                dropped = true;
            }
            float first_diff = maximas[1] - maximas[0];
            if ((maximas.size() > 7) && (first_diff*1.01f >= max_diff))
            {
                maximas.pop_front();
                dropped = true;
            }
        }

        if (avg_diff * 0.9f > min_diff)
        {
            float last_diff = maximas.back() - maximas[maximas.size()-2];
            if ((maximas.size() > 7) &&
                (last_diff*0.99f <= min_diff || last_diff < avg_diff * 0.80f))
            {
                maximas.pop_back();
                dropped = true;
            } 
            float first_diff = maximas[1] - maximas[0];
            if ((maximas.size() > 7) && (first_diff*0.99f <= min_diff))
            {
                maximas.pop_front();
                dropped = true;
            }
        }
    }

    if (maximas.size() != 7)
    {
        VERBOSE(VB_VBI|VB_EXTRA, LOC + QString("FindClocks: maximas: %1 != 7")
                .arg(maximas.size()));
        print(raw_minimas, raw_maximas, minimas, maximas);
        return false;
    }

    // find the minimas
    for (uint i = 0; i < uint(raw_minimas.size()); i++)
    {
        uint start_idx = raw_minimas[i];
        while ((i+1) < uint(raw_minimas.size()) &&
               (raw_minimas[i+1] == raw_minimas[i] + 1)) i++;
        uint end_idx = raw_minimas[i];
        float center = (start_idx + end_idx) * 0.5f;
        if (end_idx - start_idx > noise_flr_lg &&
            center > maximas[0] && center < maximas.back())
        {
            minimas.push_back(center);
        }
    }

    if (minimas.size() != 6)
    {
        VERBOSE(VB_VBI|VB_EXTRA, LOC + QString("FindClocks: minimas: %1 != 6")
                .arg(minimas.size()));
        print(raw_minimas, raw_maximas, minimas, maximas);
        return false;
    }

    // get the average clock rate in samples
    float maxima_avg_diff = find_clock_diff(maximas);
    float minima_avg_diff = find_clock_diff(minimas);
    rate = (maxima_avg_diff * 7 + minima_avg_diff * 6) / 13.0f;
    if (maxima_avg_diff == 0.0f || minima_avg_diff == 0.0f)
        return false;

    // get the estimated location of the first maxima
    // based on the rate and location of all maximas
    start = maximas[0];
    for (uint i = 1; i < uint(maximas.size()); i++)
        start += maximas[i] - i * rate;
    start /= maximas.size();
    // then move it back by a third to make each sample
    // more or less in the center of each encoded byte.
    start -= rate * 0.33f;

    // if the last bit is after the last sample...
    // 7 clocks + 3 bits run in + 16 bits data
    if (start+((7+3+8+8-1) * rate) > width)
    {
        VERBOSE(VB_VBI|VB_EXTRA, LOC + QString("FindClocks: end %1 > width %2")
                .arg(start+((7+3+8+8-1) * rate)).arg(width));

        return false;
    }

    //VERBOSE(VB_VBI|VB_EXTRA, LOC + QString("FindClocks: Clock start %1, rate %2")
    //        .arg(start).arg(rate));

    return true;
}

bool VBI608Extractor::ExtractCC(const VideoFrame *picframe, uint max_lines)
{
    int ypitch = picframe->pitches[0];
    int ywidth = picframe->width;

    code[0] = UINT16_MAX;
    code[1] = UINT16_MAX;

    // find CC
    uint found_cnt = 0;
    for (uint i = 0; i < max_lines; i++)
    {
        const unsigned char *y = picframe->buf +
            picframe->offsets[0] + (i * ypitch);
        if (FindClocks(y, ywidth))
        {
            uint maxv = 0;
            for (uint j = 0; j < start + 8 * rate; j++)
                maxv = max(uint((y+(i * ypitch))[j]), maxv);
            uint avgv = maxv / 2;

            if (y[uint(start + (0+7) * rate)] > avgv ||
                y[uint(start + (1+7) * rate)] > avgv ||
                y[uint(start + (2+7) * rate)] < avgv)
            {
                continue; // need 001 at run in..
            }

            code[found_cnt] = 0;
            for (uint j = 0; j < 8+8; j++)
            {
                bool bit = y[uint(start + (j+7+3) * rate)] > avgv;
                code[found_cnt] =
                    (code[found_cnt]>>1) | (bit?(1<<15):0);
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
                uint yloc = uint (start + j * rate + 0.5);
                Y[yloc] = 0xFF;
                uint uloc = uint (uwidth * (start + j * rate + 0.5) / ywidth);
                u[uloc] = 0x40;
            }
#endif
        }
    }

    return found_cnt;
}

bool VBI608Extractor::ExtractCC12(const unsigned char *buf, uint width)
{
    code[0] = UINT16_MAX;
    if (FindClocks(buf, width))
    {
        uint maxv = 0;
        for (uint j = 0; j < start + 8 * rate; j++)
            maxv = max(uint(buf[j]), maxv);
        uint avgv = maxv / 2;

        if (buf[uint(start + (0+7) * rate)] > avgv ||
            buf[uint(start + (1+7) * rate)] > avgv ||
            buf[uint(start + (2+7) * rate)] < avgv)
        {
            VERBOSE(VB_VBI|VB_EXTRA, LOC + "did not find VBI 608 header");
            return false;
        }

        code[0] = 0;
        for (uint j = 0; j < 8+8; j++)
        {
            bool bit = buf[uint(start + (j+7+3) * rate)] > avgv;
            code[0] = (code[0]>>1) | (bit?(1<<15):0);
        }

        return true;
    }
    return false;
}

bool VBI608Extractor::ExtractCC34(const unsigned char *buf, uint width)
{
    code[1] = UINT16_MAX;
    if (FindClocks(buf, width))
    {
        uint maxv = 0;
        for (uint j = 0; j < start + 8 * rate; j++)
            maxv = max(uint(buf[j]), maxv);
        uint avgv = maxv / 2;

        if (buf[uint(start + (0+7) * rate)] > avgv ||
            buf[uint(start + (1+7) * rate)] > avgv ||
            buf[uint(start + (2+7) * rate)] < avgv)
        {
            return false;
        }

        code[1] = 0;
        for (uint j = 0; j < 8+8; j++)
        {
            bool bit = buf[uint(start + (j+7+3) * rate)] > avgv;
            code[1] = (code[1]>>1) | (bit?(1<<15):0);
        }
        return true;
    }
    return false;
}

uint VBI608Extractor::FillCCData(uint8_t cc_data[8])
{
    uint cc_count = 0;
    if (code[0] != UINT16_MAX)
    {
        cc_data[2] = 0x04;
        cc_data[3] = (code[0])    & 0xff;
        cc_data[4] = (code[0]>>8) & 0xff;
        cc_count++;
    }

    if (code[1] != UINT16_MAX)
    {
        cc_data[2+3*cc_count] = 0x04|0x01;
        cc_data[3+3*cc_count] = (code[1])    & 0xff;
        cc_data[4+3*cc_count] = (code[1]>>8) & 0xff;
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
