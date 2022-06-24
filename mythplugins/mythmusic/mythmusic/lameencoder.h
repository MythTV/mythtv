/*
    MP3 encoding support using liblame for MythMusic

    (c) 2003 Stefan Frank
    
    Please send an e-mail to sfr@gmx.net if you have
    questions or comments.

    Project Website:  http://www.mythtv.org/

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
*/

#ifndef LAMEENCODER_H_
#define LAMEENCODER_H_

class QString;
class MusicMetadata;
class Encoder;

#ifdef MMX
#define LAME_WORKAROUND 1 // NOLINT(cppcoreguidelines-macro-usage)
#undef MMX
#endif

#include <lame/lame.h>

#ifdef LAME_WORKAROUND
#define MMX 1 // NOLINT(cppcoreguidelines-macro-usage)
#endif

#include "encoder.h"

class LameEncoder : public Encoder
{
  public:
    LameEncoder(const QString &outfile, int qualitylevel, MusicMetadata *metadata,
                bool vbr = false);
   ~LameEncoder() override;
    int addSamples(int16_t *bytes, unsigned int len) override; // Encoder

  private:
    int init_encoder(lame_global_flags *gf, int quality, bool vbr) const;
    static void init_id3tags(lame_global_flags *gf);

    int m_bits                {16};
    int m_channels            {2};
    int m_bytesPerSample      {m_channels * m_bits / 8};
    int m_samplesPerChannel   {0};

                              // worst-case estimate
    int   m_mp3BufSize        {(int)(1.25 * 16384 + 7200)};
    char *m_mp3Buf            {nullptr};

    lame_global_flags *m_gf   {nullptr};
};

#endif

