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

#ifndef _VBI_608_EXTRACTOR_H_
#define _VBI_608_EXTRACTOR_H_

#include <stdint.h>

#include <QList>

#include "mythframe.h"

class VBI608Extractor
{
  public:
    VBI608Extractor();

    uint16_t GetCode1(void) const { return code[0]; }
    uint16_t GetCode2(void) const { return code[1]; }

    bool ExtractCC(const VideoFrame*, uint max_lines = 4);
    bool ExtractCC12(const unsigned char *buf, uint width);
    bool ExtractCC34(const unsigned char *buf, uint width);

    uint FillCCData(uint8_t cc_data[8]) const;

  private:
    float    GetClockStart(void) const { return start; }
    float    GetClockRate(void)  const { return rate;  }
    bool     FindClocks(const unsigned char *buf, uint width);

    QList<uint>  raw_minimas;
    QList<uint>  raw_maximas;
    QList<float> maximas;
    QList<float> minimas;
    float        start;
    float        rate;
    uint16_t     code[2];
};

#endif // _VBI_608_EXTRACTOR_H_
