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
    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#ifndef LAMEENCODER_H_
#define LAMEENCODER_H_

class QString;
class MusicMetadata;
class Encoder;

#ifdef MMX
#define LAME_WORKAROUND 1
#undef MMX
#endif

#include <lame/lame.h>

#ifdef LAME_WORKAROUND
#define MMX 1
#endif

#include "encoder.h"

class LameEncoder : public Encoder
{
  public:
    LameEncoder(const QString &outfile, int qualitylevel, MusicMetadata *metadata,
                bool vbr = false);
   ~LameEncoder();
    int addSamples(int16_t *bytes, unsigned int len);

  private:
    int init_encoder(lame_global_flags *gf, int quality, bool vbr);
    void init_id3tags(lame_global_flags *gf);

    int bits;
    int channels;
    int samplerate;
    int bytes_per_sample;
    int samples_per_channel; 

    int mp3buf_size;
    char *mp3buf;

    int mp3bytes;

    lame_global_flags *gf;
};

#endif

