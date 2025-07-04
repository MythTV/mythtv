// System
#include <vorbis/vorbisfile.h>

// C++
#include <cstdio>
#include <iostream>

// Qt
#include <QApplication>
#include <QString>

// MythTV
#include <libmythbase/mythlogging.h>
#include <libmythbase/mythrandom.h>
#include <libmythmetadata/metaiooggvorbis.h>
#include <libmythmetadata/musicmetadata.h>

// MythMusic
#include "encoder.h"
#include "vorbisencoder.h"


static int write_page(ogg_page *page, FILE *fp)
{
    int written = fwrite(page->header, 1, page->header_len, fp);
    written += fwrite(page->body, 1, page->body_len, fp);

    return written;
}

VorbisEncoder::VorbisEncoder(const QString &outfile, int qualitylevel,
                             MusicMetadata *metadata) :
    Encoder(outfile, qualitylevel, metadata)
{
    vorbis_comment_init(&m_vc);

    vorbis_info_init(&m_vi);

    ogg_packet_clear(&m_op);

    float quality = 1.0;
    if (qualitylevel == 0)
        quality = 0.4;
    if (qualitylevel == 1)
        quality = 0.7;
  
    int ret = vorbis_encode_setup_vbr(&m_vi, 2, 44100, quality);
    if (ret)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Error initializing VORBIS encoder."
                                    " Got return code: %1").arg(ret));
        vorbis_info_clear(&m_vi);
        return;
    }

    vorbis_encode_ctl(&m_vi, OV_ECTL_RATEMANAGE_SET, nullptr);
    vorbis_encode_setup_init(&m_vi);
    vorbis_analysis_init(&m_vd, &m_vi);
    vorbis_block_init(&m_vd, &m_vb);

    ogg_stream_init(&m_os, MythRandom());

    ogg_packet header_main;
    ogg_packet header_comments;
    ogg_packet header_codebooks;

    vorbis_analysis_headerout(&m_vd, &m_vc, &header_main, &header_comments,
                              &header_codebooks);

    ogg_stream_packetin(&m_os, &header_main);
    ogg_stream_packetin(&m_os, &header_comments);
    ogg_stream_packetin(&m_os, &header_codebooks);

    int result = 0;
    while ((result = ogg_stream_flush(&m_os, &m_og)))
    {
        if (!result || !m_out)
            break;
        int ret2 = write_page(&m_og, m_out);
        if (ret2 != m_og.header_len + m_og.body_len)
        {
            LOG(VB_GENERAL, LOG_ERR,
                "Failed to write header to output stream.");
        }
    }
}

VorbisEncoder::~VorbisEncoder()
{
    VorbisEncoder::addSamples(nullptr, 0); //flush
    ogg_stream_clear(&m_os);
    vorbis_block_clear(&m_vb);
    vorbis_dsp_clear(&m_vd);
    vorbis_comment_clear(&m_vc);
    vorbis_info_clear(&m_vi);

    // Now write the Metadata
    if (m_metadata)
        MetaIOOggVorbis().write(m_outfile, m_metadata);
}

int VorbisEncoder::addSamples(int16_t * bytes, unsigned int length)
{
    long realsamples = 0;
    auto *chars = (signed char *)bytes;

    realsamples = length / 4;

    if (!m_out)
        return 0;

    float** buffer = vorbis_analysis_buffer(&m_vd, realsamples);

    for (long i = 0; i < realsamples; i++)
    {
        buffer[0][i] = ((chars[(i * 4) + 1] << 8) |
                        (chars[i * 4] & 0xff)) / 32768.0F;
        buffer[1][i] = ((chars[(i * 4) + 3] << 8) |
                        (chars[(i * 4) + 2] & 0xff)) / 32768.0F;
    }

    vorbis_analysis_wrote(&m_vd, realsamples);

    while (vorbis_analysis_blockout(&m_vd, &m_vb) == 1)
    {
        vorbis_analysis(&m_vb, nullptr);
        vorbis_bitrate_addblock(&m_vb);
 
        while (vorbis_bitrate_flushpacket(&m_vd, &m_op))
        {
            ogg_stream_packetin(&m_os, &m_op);
            m_packetsDone++;

            int eos = 0;
            while (!eos)
            {
                int result = ogg_stream_pageout(&m_os, &m_og);
                if (!result)
                    break;

                int ret = write_page(&m_og, m_out);
                if (ret != m_og.header_len + m_og.body_len)
                {
                    LOG(VB_GENERAL, LOG_ERR,
                        QString("Failed to write ogg data. Aborting."));
                    return EENCODEERROR;
                }
                m_bytesWritten += ret;

                if (ogg_page_eos(&m_og))
                    eos = 1;
            }
        }
    }

    return 0;
}
