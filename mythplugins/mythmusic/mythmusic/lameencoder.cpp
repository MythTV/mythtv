#include <qstring.h>
#include <qcstring.h>
#include <qapplication.h>
#include <qprogressbar.h>

#include "metadata.h"
#include "encoder.h"
#include "lameencoder.h"

#include <iostream>
using namespace std;

int write_buffer(char *buf, int bufsize, FILE *fp)
{
    return fwrite(buf, 1, bufsize, fp);
}

void LameEncoder::init_id3tags(lame_global_flags *gf, Metadata *metadata)
{
    id3tag_init(gf);

    QString tagstr = metadata->Artist();
    id3tag_set_artist(gf, tagstr);    
 
    tagstr = metadata->Title();
    id3tag_set_title(gf, tagstr);

    tagstr = metadata->Album();
    id3tag_set_album(gf, tagstr);

    tagstr = metadata->Genre();
    id3tag_set_genre(gf, tagstr);

    tagstr = QString::number(metadata->Track(), 10);
    id3tag_set_track(gf, tagstr);

    tagstr = QString::number(metadata->Year(), 10);
    id3tag_set_year(gf, tagstr);

    // write v2 tags.
    id3tag_v2_only(gf);
}

int LameEncoder::init_encoder(lame_global_flags *gf, int quality, int vbr)
{
    int lameret = 0;
    int algoqual = 2, vbrqual = 0;
    int meanbitrate = 128;

    switch (quality)
    {
        case 0:                         // low
            //algoqual = 7;
            meanbitrate = 128;
            break;
        case 1:                         // medium
            //algoqual = 5;
            meanbitrate = 192;
            break;
        case 2:                         // high
            //algoqual = 2;
            meanbitrate = 256;
            break;
    }

    lame_set_preset(gf, STANDARD);
    lame_set_num_channels(gf, channels);
    lame_set_mode(gf, channels == 2 ? STEREO : MONO);
    lame_set_in_samplerate(gf, samplerate);
    lame_set_out_samplerate(gf, samplerate);

    if (vbr)
    {
        lame_set_VBR(gf, vbr_abr);
        lame_set_VBR_mean_bitrate_kbps(gf, meanbitrate);
    } 
    else
    {
        lame_set_VBR(gf, vbr_off);
        lame_set_brate(gf, meanbitrate);
        lame_set_VBR_min_bitrate_kbps(gf, lame_get_brate(gf));
    }

    lame_set_VBR_q(gf, vbrqual);
    lame_set_bWriteVbrTag(gf, 0);
    lame_set_quality(gf, algoqual);

    lameret = lame_init_params(gf);

    return lameret;
}

LameEncoder::LameEncoder(const QString &outfile, int qualitylevel,
                         Metadata *metadata)
           : Encoder(outfile, qualitylevel, metadata)
{ 
    channels = 2;
    bits = 16;
    samplerate = 44100;
    vbr = 0;           // use constant bitrate for now

    bytes_per_sample = channels * bits / 8;
    samples_per_channel = 0; 

    mp3buf_size = (int)(1.25 * 16384 + 7200); // worst-case estimate
    mp3buf = new char[mp3buf_size];

    gf = lame_init();

    init_id3tags(gf, metadata);

    if (init_encoder(gf, qualitylevel, vbr) < 0)
    {
        cout << "Couldn't initialize lame encoder\n";
        return;
    }
}

LameEncoder::~LameEncoder()
{
    addSamples(0, 0); //flush

    if (gf)
        lame_close(gf);
    if (mp3buf)
        delete[] mp3buf;
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
        cout << "Lame encoder error '" << lameret << "', aborting.\n";
        return EENCODEERROR;
    } 
    else if (lameret > 0)
    {
        if (write_buffer(mp3buf, lameret, out) != lameret)
            cout << "Failed to write mp3 data to output file\n";
    }

    return 0;
}

