 #include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <string>
#include <qobject.h>
#include <qiodevice.h>
using namespace std;

#include "vorbisdecoder.h"
#include "constants.h"
#include <mythtv/audiooutput.h>
#include "metadata.h"
#include "metaiooggvorbiscomment.h"

#include <mythtv/mythconfig.h>
#include <mythtv/mythcontext.h>

// static functions for OggVorbis

static size_t oggread (void *buf, size_t size, size_t nmemb, void *src) {
    if (! src) return 0;

    VorbisDecoder *dogg = (VorbisDecoder *) src;
    int len = dogg->input()->readBlock((char *) buf, (size * nmemb));
    return len / size;
}

static int oggseek(void *src, int64_t offset, int whence) {
    VorbisDecoder *dogg = (VorbisDecoder *) src;

    if (! dogg->input()->isDirectAccess())
        return -1;

    long start = 0;
    switch (whence) {
    case SEEK_END:
        start = dogg->input()->size();
        break;

    case SEEK_CUR:
        start = dogg->input()->at();
        break;

    case SEEK_SET:
    default:
        start = 0;
    }

    if (dogg->input()->at(start + offset))
        return 0;
    return -1;
}

static int oggclose(void *src)
{
    VorbisDecoder *dogg = (VorbisDecoder *) src;
    dogg->input()->close();
    return 0;
}

static long oggtell(void *src)
{
    VorbisDecoder *dogg = (VorbisDecoder *) src;
    long t = dogg->input()->at();
    return t;
}

VorbisDecoder::VorbisDecoder(const QString &file, DecoderFactory *d, 
                             QIODevice *i, AudioOutput *o) 
             : Decoder(d, i, o)
{
    filename = file;
    inited = FALSE;
    user_stop = FALSE;
    stat = 0;
    output_buf = 0;
    output_bytes = 0;
    output_at = 0;
    bks = 0;
    done = FALSE;
    finish = FALSE;
    len = 0;
    freq = 0;
    bitrate = 0;
    seekTime = -1.0;
    totalTime = 0.0;
    chan = 0;
}

VorbisDecoder::~VorbisDecoder(void)
{
    if (inited)
        deinit();

    if (output_buf)
        delete [] output_buf;
    output_buf = 0;
}

void VorbisDecoder::stop()
{
    user_stop = TRUE;
}

void VorbisDecoder::flush(bool final)
{
    ulong min = final ? 0 : bks;
            
    while ((! done && ! finish) && output_bytes > min) {

        if (user_stop || finish) {
            inited = FALSE;
            done = TRUE;
        } else {
            ulong sz = output_bytes < bks ? output_bytes : bks;

            int samples = (sz*8)/(chan*16);
            if (output()->AddSamples(output_buf, samples, -1))
            {
                output_bytes -= sz;
                memmove(output_buf, output_buf + sz, output_bytes);
                output_at = output_bytes;
            } else {
                unlock();
                usleep(500);
                lock();
                done = user_stop;
            }
        }
    }
}

bool VorbisDecoder::initialize()
{
    bks = blockSize();

    inited = user_stop = done = finish = FALSE;
    len = freq = bitrate = 0;
    stat = chan = 0;
    seekTime = -1.0;
    totalTime = 0.0;

    if (! input()) {
        error("DecoderOgg: cannot initialize.  No input.");

        return FALSE;
    }

    if (! output_buf)
        output_buf = new char[globalBufferSize];
    output_at = 0;
    output_bytes = 0;

    if (! input()->isOpen()) {
        if (! input()->open(IO_ReadOnly)) {
            error("DecoderOgg: Failed to open input. Error " +
                  QString::number(input()->status()) + ".");
            return FALSE;
        }
    }

    ov_callbacks oggcb =
        {
            oggread,
            oggseek,
            oggclose,
            oggtell
        };
    if (ov_open_callbacks(this, &oggfile, NULL, 0, oggcb) < 0) {
        error("DecoderOgg: Cannot open stream.");

        return FALSE;
    }

    freq = 0;
    bitrate = ov_bitrate(&oggfile, -1) / 1000;
    chan = 0;

    totalTime = long(ov_time_total(&oggfile, 0));
    totalTime = totalTime < 0 ? 0 : totalTime;

    vorbis_info *ogginfo = ov_info(&oggfile, -1);
    if (ogginfo) {
        freq = ogginfo->rate;
        chan = ogginfo->channels;
    }

    if (output()) 
    {
        output()->Reconfigure(16, chan, freq, false /*AC3/DTS pass through*/);
        output()->SetSourceBitrate(bitrate);
    }

    inited = TRUE;
    return TRUE;
}

void VorbisDecoder::seek(double pos)
{
    seekTime = pos;
}

void VorbisDecoder::deinit()
{
    ov_clear(&oggfile);

    inited = user_stop = done = finish = FALSE;
    len = freq = bitrate = 0;
    stat = chan = 0;
    setInput(0);
    setOutput(0);
}

void VorbisDecoder::run()
{
    lock();

    if (! inited) {
        unlock();

        return;
    }

    stat = DecoderEvent::Decoding;

    unlock();

    {
        DecoderEvent e((DecoderEvent::Type) stat);
        dispatch(e);
    }

    int section = 0;

    while (! done && ! finish) {
        lock();
        // decode

        if (seekTime >= 0.0) {
            ov_time_seek(&oggfile, double(seekTime));
            seekTime = -1.0;
        }

#ifdef WORDS_BIGENDIAN
        len = ov_read(&oggfile, (char *) (output_buf + output_at), bks, 1, 2, 1,
                      &section);
#else
        len = ov_read(&oggfile, (char *) (output_buf + output_at), bks, 0, 2, 1,
                      &section);
#endif

        if (len > 0) {
            bitrate = ov_bitrate_instant(&oggfile) / 1000;

            output_at += len;
            output_bytes += len;

            if (output())
            {
                output()->SetSourceBitrate(bitrate);
                flush();
            }
        } else if (len == 0) {
            flush(TRUE);

            if (output()) {
		output()->Drain();
            }

            done = TRUE;
            if (! user_stop) {
                finish = TRUE;
            }
        } else {
            // error in read
            error("DecoderOgg: Error while decoding stream, File appears to be "
                  "corrupted");

            finish = TRUE;
        }

        unlock();
    }

    lock();

    if (finish)
        stat = DecoderEvent::Finished;
    else if (user_stop)
        stat = DecoderEvent::Stopped;

    unlock();

    {
        DecoderEvent e((DecoderEvent::Type) stat);
        dispatch(e);
    }

    deinit();
}

MetaIO *VorbisDecoder::doCreateTagger(void)
{
    return new MetaIOOggVorbisComment();
}

bool VorbisDecoderFactory::supports(const QString &source) const
{
    return (source.right(extension().length()).lower() == extension());
}

const QString &VorbisDecoderFactory::extension() const
{
    static QString ext(".ogg");
    return ext;
}

const QString &VorbisDecoderFactory::description() const
{
    static QString desc(QObject::tr("Ogg Vorbis Audio"));
    return desc;
}

Decoder *VorbisDecoderFactory::create(const QString &file, QIODevice *input, 
                                      AudioOutput *output, bool deletable)
{
    if (deletable)
        return new VorbisDecoder(file, this, input, output);

    static VorbisDecoder *decoder = 0;
    if (! decoder) {
        decoder = new VorbisDecoder(file, this, input, output);
    } else {
        decoder->setInput(input);
        decoder->setFilename(file);
        decoder->setOutput(output);
    }

    return decoder;
}
   
