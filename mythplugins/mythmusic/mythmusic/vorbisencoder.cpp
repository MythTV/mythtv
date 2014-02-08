// System
#include <vorbis/vorbisfile.h>

// POSIX
#include <time.h>

// ANSI C
#include <cstdlib>

// C++
#include <iostream>
using namespace std;

// Qt
#include <QApplication>
#include <QString>

// MythTV
#include <mythcontext.h>
#include <compat.h> // For random() on MINGW32
#include <musicmetadata.h>

// MythMusic
#include "encoder.h"
#include "vorbisencoder.h"
#include "metaiooggvorbis.h"


static int write_page(ogg_page *page, FILE *fp)
{
    int written = fwrite(page->header, 1, page->header_len, fp);
    written += fwrite(page->body, 1, page->body_len, fp);

    return written;
}

VorbisEncoder::VorbisEncoder(const QString &outfile, int qualitylevel,
                             MusicMetadata *metadata) :
    Encoder(outfile, qualitylevel, metadata),
    packetsdone(0),
    eos(0),
    bytes_written(0L),
    m_metadata(metadata)
{
    vorbis_comment_init(&vc);

    vorbis_info_init(&vi);

    float quality = 1.0;
    if (qualitylevel == 0)
        quality = 0.4;
    if (qualitylevel == 1)
        quality = 0.7;
  
    int ret = vorbis_encode_setup_vbr(&vi, 2, 44100, quality);
    if (ret)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Error initializing VORBIS encoder."
                                    " Got return code: %1").arg(ret));
        vorbis_info_clear(&vi);
        return;
    }

    vorbis_encode_ctl(&vi, OV_ECTL_RATEMANAGE_SET, NULL);
    vorbis_encode_setup_init(&vi);
    vorbis_analysis_init(&vd, &vi);
    vorbis_block_init(&vd, &vb);

    ogg_stream_init(&os, random());

    ogg_packet header_main;
    ogg_packet header_comments;
    ogg_packet header_codebooks;

    vorbis_analysis_headerout(&vd, &vc, &header_main, &header_comments, 
                              &header_codebooks);

    ogg_stream_packetin(&os, &header_main);
    ogg_stream_packetin(&os, &header_comments);
    ogg_stream_packetin(&os, &header_codebooks);

    int result;
    while ((result = ogg_stream_flush(&os, &og)))
    {
        if (!result || !m_out)
            break;
        int ret = write_page(&og, m_out);
        if (ret != og.header_len + og.body_len)
        {
            LOG(VB_GENERAL, LOG_ERR,
                "Failed to write header to output stream.");
        }
    }
}

VorbisEncoder::~VorbisEncoder()
{
    addSamples(0, 0); //flush
    ogg_stream_clear(&os);
    vorbis_block_clear(&vb);
    vorbis_dsp_clear(&vd);
    vorbis_comment_clear(&vc);
    vorbis_info_clear(&vi);

    // Now write the Metadata
    if (m_metadata)
        MetaIOOggVorbis().write(m_outfile, m_metadata);
}

int VorbisEncoder::addSamples(int16_t * bytes, unsigned int length)
{
    int i;
    long realsamples = 0;
    signed char *chars = (signed char *)bytes;

    realsamples = length / 4;

    if (!m_out)
        return 0;

    float** buffer = vorbis_analysis_buffer(&vd, realsamples);

    for (i = 0; i < realsamples; i++) 
    {
        buffer[0][i] = ((chars[i * 4 + 1] << 8) |
                        (chars[i * 4] & 0xff)) / 32768.0f;
        buffer[1][i] = ((chars[i * 4 + 3] << 8) |
                        (chars[i * 4 + 2] & 0xff)) / 32768.0f;
    }

    vorbis_analysis_wrote(&vd, realsamples);

    while (vorbis_analysis_blockout(&vd, &vb) == 1)
    {
        vorbis_analysis(&vb, NULL);
        vorbis_bitrate_addblock(&vb);
 
        while (vorbis_bitrate_flushpacket(&vd, &op))
        {
            ogg_stream_packetin(&os, &op);
            packetsdone++;

            int eos = 0;
            while (!eos)
            {
                int result = ogg_stream_pageout(&os, &og);
                if (!result)
                    break;

                int ret = write_page(&og, m_out);
                if (ret != og.header_len + og.body_len)
                {
                    LOG(VB_GENERAL, LOG_ERR,
                        QString("Failed to write ogg data. Aborting."));
                    return EENCODEERROR;
                }
                bytes_written += ret;

                if (ogg_page_eos(&og))
                    eos = 1;
            }
        }
    }

    return 0;
}
