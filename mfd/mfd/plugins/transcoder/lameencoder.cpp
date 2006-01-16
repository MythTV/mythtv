/*
    MP3 encoding support using liblame for MythMusic

    (c) 2003 Stefan Frank
    
    Please send an e-mail to sfr@gmx.net if you have
    questions or comments.

    Project Website: out http://www.mythtv.org/

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

#include <qstring.h>
#include <qcstring.h>
#include <qapplication.h>
#include <qprogressbar.h>

#include "metadata.h"
#include "lameencoder.h"

#include <iostream>

#include <mythtv/mythcontext.h>

using namespace std;

int write_buffer(char *buf, int bufsize, FILE *fp)
{
    return fwrite(buf, 1, bufsize, fp);
}

void LameEncoder::init_id3tags(lame_global_flags *gf)
{
    id3tag_init(gf);
    
    QString tagstr = metadata->getArtist();
    id3tag_set_artist(gf, tagstr);    
             
    tagstr = metadata->getTitle();
    id3tag_set_title(gf, tagstr);
                     
    tagstr = metadata->getAlbum();
    id3tag_set_album(gf, tagstr);
                             
    tagstr = metadata->getGenre();
    id3tag_set_genre(gf, tagstr);
                                    
    tagstr = QString::number(metadata->getTrack(), 10);
    id3tag_set_track(gf, tagstr);
                                             
    tagstr = QString::number(metadata->getYear(), 10);
    id3tag_set_year(gf, tagstr);
                                                     
    // write v2 tags.
    id3tag_v2_only(gf);
}

int LameEncoder::init_encoder(lame_global_flags *gf, int quality, bool vbr)
{
    int lameret = 0;
    int meanbitrate = 128;
    int preset = STANDARD;

    switch (quality)
    {
        case 0:                         // low, always use CBR
            meanbitrate = 128;
            vbr = false;
            break;
        case 1:                         // medium
            meanbitrate = 192;
            break;
        case 2:                         // high
            meanbitrate = 256;
            preset = EXTREME;
            break;
    }

    if (vbr)
        lame_set_preset(gf, preset);
    else
    {
        lame_set_preset(gf, meanbitrate);
        lame_set_VBR(gf, vbr_off);
    }

    if (channels == 1)
    {
        lame_set_mode(gf, MONO);
    }

    lameret = lame_init_params(gf);

    return lameret;
}

LameEncoder::LameEncoder(const QString &l_outfile, int qualitylevel,
                         AudioMetadata *l_metadata, bool vbr)
           : Encoder(l_outfile, qualitylevel, l_metadata)
{ 
    channels = 2;
    bits = 16;
    samplerate = 44100;

    bytes_per_sample = channels * bits / 8;
    samples_per_channel = 0; 

    mp3buf_size = (int)(1.25 * 16384 + 7200); // worst-case estimate
    mp3buf = new char[mp3buf_size];

    gf = lame_init();

    init_id3tags(gf);

    int lameret = init_encoder(gf, qualitylevel, vbr);
    if (lameret < 0)
    {
        VERBOSE(VB_GENERAL, QString("Error initializing LAME encoder. "
                                    "Got return code: %1").arg(lameret));
        return;
    }
}

int LameEncoder::addSamples(int16_t * bytes, unsigned int length)
{
    int lameret = 0;

    samples_per_channel = length / bytes_per_sample;

    if (length > 0)
    {
        lameret = lame_encode_buffer_interleaved(gf, (short int *)bytes,
                                                 samples_per_channel,
                                                 (unsigned char *)mp3buf,
                                                 mp3buf_size);
    }
    else
    {
        lameret = lame_encode_flush(gf, (unsigned char *)mp3buf,
                                          mp3buf_size);
    }

    if (lameret < 0)
    {
        VERBOSE(VB_IMPORTANT, QString("LAME encoder error."));
    } 
    else if (lameret > 0 && out)
    {
        if (write_buffer(mp3buf, lameret, out) != lameret)
        {
            VERBOSE(VB_GENERAL, QString("Failed to write mp3 data."
                                        " Aborting."));
            return EENCODEERROR;
        }
    }

    return 0;
}



LameEncoder::~LameEncoder()
{
    addSamples(0, 0); //flush

    if (gf)
        lame_close(gf);
    if (mp3buf)
        delete[] mp3buf;
        
    // Need to close the file here.
    if (out)
    {
        fclose(out);
        
        // Make sure the base class doesn't do a double clear.
        out = NULL;
    }
}

