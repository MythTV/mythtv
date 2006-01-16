#include <qstring.h>
#include <qcstring.h>
#include <qapplication.h>
#include <qprogressbar.h>

#include "metadata.h"
#include "vorbisencoder.h"


#include <vorbis/vorbisfile.h>

#include <iostream>

#include <mythtv/mythcontext.h>

using namespace std;

int write_page(ogg_page *page, FILE *fp)
{
    int written = fwrite(page->header, 1, page->header_len, fp);
    written += fwrite(page->body, 1, page->body_len, fp);

    return written;
}

VorbisEncoder::VorbisEncoder(const QString &l_outfile, int qualitylevel,
                             AudioMetadata *l_metadata)
             : Encoder(l_outfile, qualitylevel, l_metadata)
{ 
    int result;

    vorbis_comment_init(&vc);

    QCString utf8str = metadata->getArtist().utf8();
    char *artist = utf8str.data();
    vorbis_comment_add_tag(&vc, (char *)"ARTIST", artist);    
             
    utf8str = metadata->getTitle().utf8();
    char *title = utf8str.data();
    vorbis_comment_add_tag(&vc, (char *)"TITLE", title);
                         
    utf8str = metadata->getAlbum().utf8();
    char *album = utf8str.data();
    vorbis_comment_add_tag(&vc, (char *)"ALBUM", album);
                                     
    utf8str = metadata->getGenre().utf8();
    char *genre = utf8str.data();
    vorbis_comment_add_tag(&vc, (char *)"GENRE", genre);
                                                 
    char tracknum[10];
    sprintf(tracknum, "%d", metadata->getTrack());
    vorbis_comment_add_tag(&vc, (char *)"TRACKNUMBER", tracknum);

    char year[10];
    if(metadata->getYear() > 0)
    {
        snprintf(year, 9, "%d", metadata->getYear());
        vorbis_comment_add_tag(&vc, (char *)"DATE", year);
    }

    if(metadata->getCompilation())
    {
        //
        //  Add a tag flag that says this is a compilation (we use the
        //  MUSICBRAINZ tag and flag value only because we have to use
        //  something, and it makes slightly more sense to use their
        //  "standard" than it does to just completely invent one ourselves)
        //

        vorbis_comment_add_tag(&vc, (char *)"MUSICBRAINZ_ALBUMARTISTID",
                                "89ad4ac3-39f7-470e-963a-56509c546377");

        //
        //  The tag above says this is a compilation. Any software that
        //  parses the resulting file can just assume "Various Artists",
        //  unless we specifically have an "Album Artist" tag (which would
        //  get used for things like a DJ re-mix album)
        //

        if(metadata->getCompilationArtist().length() > 0)
        {
            QString compilation_artist = metadata->getCompilationArtist();
            char *cartist = compilation_artist.utf8().data();
            vorbis_comment_add_tag(&vc, (char *)"ALBUM ARTIST", cartist);
        }
    }


    packetsdone = 0;
    bytes_written = 0;

    vorbis_info_init(&vi);

    float quality = 1.0;
    if (qualitylevel == 0)
        quality = 0.4;
    if (qualitylevel == 1)
        quality = 0.7;
  
    int ret = vorbis_encode_setup_vbr(&vi, 2, 44100, quality);
    if (ret)
    {
        VERBOSE(VB_GENERAL, QString("Error initializing VORBIS encoder."
                                    " Got return code: %1").arg(ret));
        vorbis_info_clear(&vi);
        return;
    }

    vorbis_encode_ctl(&vi, OV_ECTL_RATEMANAGE_SET, NULL);
    vorbis_encode_setup_init(&vi);
    vorbis_analysis_init(&vd, &vi);
    vorbis_block_init(&vd, &vb);
 
    ogg_stream_init(&os, 0);

    ogg_packet header_main;
    ogg_packet header_comments;
    ogg_packet header_codebooks;

    vorbis_analysis_headerout(&vd, &vc, &header_main, &header_comments, 
                              &header_codebooks);

    ogg_stream_packetin(&os, &header_main);
    ogg_stream_packetin(&os, &header_comments);
    ogg_stream_packetin(&os, &header_codebooks);

    while ((result = ogg_stream_flush(&os, &og)))
    {
        if (!result || !out)
            break;
        int ret = write_page(&og, out);
        if (ret != og.header_len + og.body_len)
        {
            VERBOSE(VB_IMPORTANT, QString("Failed to write header"
                                    " to output stream."));
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
}

int VorbisEncoder::addSamples(int16_t * bytes, unsigned int length)
{
    int i;
    long realsamples = 0;
    signed char *chars = (signed char *)bytes;

    realsamples = length / 4;

    if (!out)
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

                int ret = write_page(&og, out);
                if (ret != og.header_len + og.body_len)
                {
                    VERBOSE(VB_GENERAL, QString("Failed to write ogg data."
                                            " Aborting."));
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
