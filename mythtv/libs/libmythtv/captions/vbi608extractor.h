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

#ifndef VBI_608_EXTRACTOR_H
#define VBI_608_EXTRACTOR_H

#include <array>
#include <cstdint>

#include <QList>

#include "mythframe.h"

using cc608_data = std::array<uint8_t,8>;

class VBI608Extractor
{
  public:
    VBI608Extractor() = default;

    uint16_t GetCode1(void) const { return m_code[0]; }
    uint16_t GetCode2(void) const { return m_code[1]; }

    bool ExtractCC(const MythVideoFrame *picframe, uint max_lines = 4);
    bool ExtractCC12(const unsigned char *buf, uint width);
    bool ExtractCC34(const unsigned char *buf, uint width);

    uint FillCCData(cc608_data &cc_data) const;

  private:
    float    GetClockStart(void) const { return m_start; }
    float    GetClockRate(void)  const { return m_rate;  }
    bool     FindClocks(const unsigned char *buf, uint width);

    QList<uint>  m_rawMinimas;
    QList<uint>  m_rawMaximas;
    QList<float> m_maximas;
    QList<float> m_minimas;
    float        m_start       {0.0F};
    float        m_rate        {0.0F};
    std::array<uint16_t,2> m_code {UINT16_MAX, UINT16_MAX};
};

#endif // VBI_608_EXTRACTOR_H
