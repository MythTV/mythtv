#include <qstring.h>
#include <qcstring.h>
#include <qapplication.h>
#include <qprogressbar.h>

#include "metadata.h"
#include "vorbisencoder.h"

#include <vorbis/vorbisfile.h>
#include <vorbis/vorbisenc.h>

int write_page(ogg_page *page, FILE *fp)
{
    int written = fwrite(page->header, 1, page->header_len, fp);
    written += fwrite(page->body, 1, page->body_len, fp);

    return written;
}

long read_samples(float **buffer, FILE *in, int samples, long int totalsamples,
                  long &totalread)
{
    int samplebyte = 2;
    int channels = 2;

    signed char *buf = new signed char[samples * samplebyte * channels];
    long bytes_read = fread(buf, 1, samples * samplebyte * channels, in);

    int i, j;
    long realsamples = 0;

    if (totalread + (bytes_read / 4) > totalsamples)
        bytes_read = 4 * (totalsamples - totalread);

    realsamples = bytes_read / 4;
    totalread += realsamples;
 
    for (i = 0; i < realsamples; i++)
    {
        for (j = 0; j < channels; j++)
        {
            buffer[j][i] = ((buf[i * 2 * channels + 2 * j + 1] << 8) |
                            (buf[i * 2 * channels + 2 * j] & 0xff)) / 
                            32768.0f;
        }
    }

    delete [] buf;

    return realsamples;
}

void VorbisEncode(const char *infile, const char *outfile, 
                  int qualitylevel, Metadata *metadata, int totalbytes,
                  QProgressBar *progressbar)
{
    long int totalsamples = (totalbytes / 4);
    progressbar->setTotalSteps(totalsamples);
    progressbar->setProgress(0);
    qApp->processEvents();

    vorbis_comment vc;
    vorbis_comment_init(&vc);

    QCString utf8str = metadata->Artist().utf8();
    char *artist = utf8str.data();
    vorbis_comment_add_tag(&vc, (char *)"artist", artist);    
 
    utf8str = metadata->Title().utf8();
    char *title = utf8str.data();
    vorbis_comment_add_tag(&vc, (char *)"title", title);

    utf8str = metadata->Album().utf8();
    char *album = utf8str.data();
    vorbis_comment_add_tag(&vc, (char *)"album", album);

    char tracknum[10];
    sprintf(tracknum, "%d", metadata->Track());
    vorbis_comment_add_tag(&vc, (char *)"tracknumber", tracknum);

    FILE *in = fopen(infile, "r");
    FILE *out = fopen(outfile, "w");

    ogg_stream_state os;
    ogg_page og;
    ogg_packet op;

    vorbis_dsp_state vd;
    vorbis_block vb;
    vorbis_info vi;

    long samplesdone = 0;
    long packetsdone = 0;
    int eos, ret;
    long bytes_written = 0;

    vorbis_info_init(&vi);

    float quality = 1.0;
    if (qualitylevel == 0)
        quality = 0.4;
    if (qualitylevel == 1)
        quality = 0.7;
   
    if (vorbis_encode_setup_vbr(&vi, 2, 44100, quality))
    {
        cout << "Couldn't initialize vorbis encoder\n";
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
    int result;

    vorbis_analysis_headerout(&vd, &vc, &header_main, &header_comments, 
                              &header_codebooks);

    ogg_stream_packetin(&os, &header_main);
    ogg_stream_packetin(&os, &header_comments);
    ogg_stream_packetin(&os, &header_codebooks);

    long int totalread = 0;
    while ((result = ogg_stream_flush(&os, &og)))
    {
        if (!result)
            break;
        ret = write_page(&og, out);
        if (ret != og.header_len + og.body_len)
        {
            cout << "Failed to write header to output stream\n";
            goto cleanup;
        }
    }

    eos = 0;
    while (!eos)
    {
        float **buffer = vorbis_analysis_buffer(&vd, 1024);
        long samples_read = read_samples(buffer, in, 1024, totalsamples,
                                         totalread);
 
        if (samples_read == 0)
            vorbis_analysis_wrote(&vd, 0);
        else
        {
            samplesdone += samples_read;
            if (packetsdone >= 10)
            {
                packetsdone = 0;
                progressbar->setProgress(samplesdone);
                qApp->processEvents();
            }

            vorbis_analysis_wrote(&vd, samples_read);
        }

        while (vorbis_analysis_blockout(&vd, &vb) == 1)
        {
            vorbis_analysis(&vb, NULL);
            vorbis_bitrate_addblock(&vb);
 
            while (vorbis_bitrate_flushpacket(&vd, &op))
            {
                ogg_stream_packetin(&os, &op);
                packetsdone++;

                while (!eos)
                {
                    int result = ogg_stream_pageout(&os, &og);
                    if (!result)
                        break;

                    ret = write_page(&og, out);
                    if (ret != og.header_len + og.body_len)
                    {
                        cout << "Failed writing vorbis output stream\n";
                        goto cleanup;
                    }
                    bytes_written += ret;

                    if (ogg_page_eos(&og))
                        eos = 1;
                }
            }
        }
    }

cleanup:
    ogg_stream_clear(&os);
    vorbis_block_clear(&vb);
    vorbis_dsp_clear(&vd);
    vorbis_comment_clear(&vc);
    vorbis_info_clear(&vi);

    fclose(in);
    fclose(out);
}
