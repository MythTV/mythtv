#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <math.h>
#include <stdio.h>
#include <qregexp.h>
#include <sys/stat.h>
using namespace std;

#include <mad.h>
#include <id3tag.h>

#include "maddecoder.h"
#include "metadata.h"
#include "constants.h"
#include <mythtv/audiooutput.h>
#include "metaioid3v2.h"

#include <mythtv/mythconfig.h>
#include <mythtv/mythcontext.h>

#define XING_MAGIC     (('X' << 24) | ('i' << 16) | ('n' << 8) | 'g')

MadDecoder::MadDecoder(const QString &file, DecoderFactory *d, QIODevice *i, 
                       AudioOutput *o)
          : Decoder(d, i, o)
{
    filename = file;
    inited = false;
    user_stop = false;
    done = false;
    finish = false;
    derror = false;
    eof = false;
    totalTime = 0.;
    seekTime = -1.;
    stat = 0;
    channels = 0;
    bks = 0;
    bitrate = 0;
    freq = 0;
    len = 0;
    input_buf = 0;
    input_bytes = 0;
    output_buf = 0;
    output_bytes = 0;
    output_at = 0;
}

MadDecoder::~MadDecoder(void)
{
    if (inited)
        deinit();

    if (input_buf)
        delete [] input_buf;
    input_buf = 0;

    if (output_buf)
        delete [] output_buf;
    output_buf = 0;
}

bool MadDecoder::initialize()
{   
    bks = blockSize();
    
    inited = false;
    user_stop = false;
    done = false;
    finish = false;
    derror = false;
    eof = false;
    totalTime = 0.;
    seekTime = -1.;
    stat = 0;
    channels = 0; 
    bitrate = 0;
    freq = 0;
    len = 0;
    input_bytes = 0;
    output_bytes = 0;
    output_at = 0;

    if (! input()) {
        error("DecoderMAD: cannot initialize.  No input.");
        return FALSE;
    }

    if (! input_buf)
        input_buf = new char[globalBufferSize];

    if (! output_buf)
        output_buf = new char[globalBufferSize * 2];

    if (! input()->isOpen()) {
        if (! input()->open(IO_ReadOnly)) {
            error("DecoderMAD: Failed to open input.  Error " +
                  QString::number(input()->status()) + ".");
            return FALSE;
        }
    }

    mad_stream_init(&stream);
    mad_frame_init(&frame);
    mad_synth_init(&synth);

    if (! findHeader()) {
        error("DecoderMAD: Cannot find a valid MPEG header.");
        return FALSE;
    }

    if (output())
    {
        output()->Reconfigure(16, channels, freq);
        output()->SetSourceBitrate(bitrate);
    }

    inited = TRUE;
    return TRUE;
}

void MadDecoder::deinit()
{
    if (input())
        input()->close();

    mad_synth_finish(&synth);
    mad_frame_finish(&frame);
    mad_stream_finish(&stream);

    inited = false;
    user_stop = false;
    done = false;
    finish = false;
    derror = false;
    eof = false;
    totalTime = 0.;
    seekTime = -1.;
    stat = 0;
    channels = 0;
    bks = 0;
    bitrate = 0;
    freq = 0;
    len = 0;
    input_bytes = 0;
    output_bytes = 0;
    output_at = 0;
}

bool MadDecoder::findXingHeader(struct mad_bitptr ptr, unsigned int bitlen)
{
    if (bitlen < 64 || mad_bit_read(&ptr, 32) != XING_MAGIC)
        goto fail;

    xing.flags = mad_bit_read(&ptr, 32);
    bitlen -= 64;

    if (xing.flags & XING_FRAMES) {
        if (bitlen < 32)
            goto fail;

        xing.frames = mad_bit_read(&ptr, 32);
        bitlen -= 32;
    }

    if (xing.flags & XING_BYTES) {
        if (bitlen < 32)
            goto fail;

        xing.bytes = mad_bit_read(&ptr, 32);
        bitlen -= 32;
    }

    if (xing.flags & XING_TOC) {
        int i;

        if (bitlen < 800)
            goto fail;

        for (i = 0; i < 100; ++i)
            xing.toc[i] = mad_bit_read(&ptr, 8);

        bitlen -= 800;
    }

    if (xing.flags & XING_SCALE) {
        if (bitlen < 32)
            goto fail;

        xing.scale = mad_bit_read(&ptr, 32);
        bitlen -= 32;
    }

    return true;

 fail:
    xing.flags = 0;
    xing.frames = 0;
    xing.bytes = 0;
    xing.scale = 0;
    return false;
}

bool MadDecoder::findHeader()
{
    bool result = false;
    int count = 0;

    while (1) {
        if (input_bytes < globalBufferSize) {
            int bytes = input()->readBlock(input_buf + input_bytes,
                                           globalBufferSize - input_bytes);
            if (bytes <= 0) {
                if (bytes == -1)
                    result = false;;
                break;
            }
            input_bytes += bytes;
        }

        mad_stream_buffer(&stream, (unsigned char *) input_buf, input_bytes);

        bool done = false;
        while (! done) {
            if (mad_frame_decode(&frame, &stream) != -1)
                done = true;
            else if (!MAD_RECOVERABLE(stream.error))
                break;
            count++;
        }

        findXingHeader(stream.anc_ptr, stream.anc_bitlen);

        result = done;

        if (stream.error == MAD_ERROR_BUFLEN)
        {
            // we've got a large tag.  picture, most likely.
            count = 0;
            input_bytes = 0;
            continue;
        }

        if (count || stream.error != MAD_ERROR_BUFLEN)
            break;

        input_bytes = &input_buf[input_bytes] - (char *) stream.next_frame;
        memmove(input_buf, stream.next_frame, input_bytes);
    }

    if (result && count) {
        freq = frame.header.samplerate;
        channels = MAD_NCHANNELS(&frame.header);
        bitrate = frame.header.bitrate / 1000;
        calcLength(&frame.header);
    }

    return result;
}

void MadDecoder::calcLength(struct mad_header *header)
{
    if (! input() || ! input()->isDirectAccess())
        return;

    totalTime = 0.;
    if (xing.flags & XING_FRAMES) {
        mad_timer_t timer;

        timer = header->duration;
        mad_timer_multiply(&timer, xing.frames);

        totalTime = double(mad_timer_count(timer, MAD_UNITS_MILLISECONDS)) / 
                   1000.0;
    } else if (header->bitrate > 0)
        totalTime = input()->size() * 8 / header->bitrate;
}

void MadDecoder::seek(double pos)
{
    seekTime = pos;
}

void MadDecoder::stop()
{
    user_stop = TRUE;
}

void MadDecoder::flush(bool final)
{
    ulong min = final ? 0 : bks;

    while ((! done && ! finish) && output_bytes > min) {

        if (user_stop || finish) {
            inited = FALSE;
            done = TRUE;
        } else {
            ulong sz = output_bytes < bks ? output_bytes : bks;

            int samples = (sz*8)/(channels*16);
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

void MadDecoder::run()
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

    while (! done && ! finish && ! derror) {
        lock();

        if (seekTime >= 0.0) {
            long seek_pos = long(seekTime * input()->size() / totalTime);
            input()->at(seek_pos);
            input_bytes = 0;
            output_at = 0;
            output_bytes = 0;
            eof = false;
        }

        finish = eof;

        if (! eof) {
            if (stream.next_frame && seekTime == -1.) {
                input_bytes = &input_buf[input_bytes] - 
                               (char *) stream.next_frame;
                memmove(input_buf, stream.next_frame, input_bytes);
            }

            if (input_bytes < globalBufferSize) {
                int len = input()->readBlock((char *) input_buf + input_bytes,
                                             globalBufferSize - input_bytes);

                if (len == 0) {
                    eof = true;
                } else if (len < 0) {
                    derror = true;
                    unlock();
                    break;
                }

                input_bytes += len;
            }

            mad_stream_buffer(&stream, (unsigned char *) input_buf, 
                              input_bytes);
        }

        seekTime = -1.;

        unlock();

        while (! done && ! finish && ! derror) {
            if (mad_frame_decode(&frame, &stream) == -1) {
                if (stream.error == MAD_ERROR_BUFLEN)
                    break;

                if (! MAD_RECOVERABLE(stream.error)) {
                    derror = true;
                    break;
                }
                continue;
            }

            lock();

            if (seekTime >= 0.) {
                unlock();
                break;
            }

            mad_synth_frame(&synth, &frame);
            madOutput();
            unlock();
        }
    }

    lock();

    if (! user_stop && eof) {
        flush(TRUE);

        if (output()) {
            output()->Drain();
        }

        done = TRUE;
        if (! user_stop)
            finish = TRUE;
    }

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

static inline signed int scale(mad_fixed_t sample)
{
  /* round */
  sample += (1L << (MAD_F_FRACBITS - 16));

  /* clip */
  if (sample >= MAD_F_ONE)
    sample = MAD_F_ONE - 1;
  else if (sample < -MAD_F_ONE)
    sample = -MAD_F_ONE;

  /* quantize */
  return sample >> (MAD_F_FRACBITS + 1 - 16);
}

static inline signed long fix_sample(unsigned int bits, mad_fixed_t sample)
{
    mad_fixed_t quantized, check;
    quantized = sample;
    check = (sample >> MAD_F_FRACBITS) + 1;
    if (check & ~1) {
        if (sample >= MAD_F_ONE)
            quantized = MAD_F_ONE - 1;
        else if (sample < -MAD_F_ONE)
            quantized = -MAD_F_ONE;
    }
    quantized &= ~((1L << (MAD_F_FRACBITS + 1 - bits)) - 1);
    return quantized >> (MAD_F_FRACBITS + 1 - bits);
}

enum mad_flow MadDecoder::madOutput()
{
    unsigned int samples;
    mad_fixed_t const *left, *right;

    samples = synth.pcm.length;
    left = synth.pcm.samples[0];
    right = synth.pcm.samples[1];

    freq = frame.header.samplerate;
    channels = MAD_NCHANNELS(&frame.header);
    bitrate = frame.header.bitrate / 1000;

    if (output())
    {
        output()->Reconfigure(16, channels, freq);
        output()->SetSourceBitrate(bitrate);
    }

    while (samples--)
    {
        signed int sample;

        if (output_bytes + 4096 > globalBufferSize)
        {
            flush();
        }
        sample = fix_sample(16, *left++);
#ifdef WORDS_BIGENDIAN
        *(output_buf + output_at++) = ((sample >> 8) & 0xff);
        *(output_buf + output_at++) = ((sample >> 0) & 0xff);
#else
        *(output_buf + output_at++) = ((sample >> 0) & 0xff);
        *(output_buf + output_at++) = ((sample >> 8) & 0xff);
#endif
        output_bytes += 2;

        if (channels == 2)
        {
            sample = fix_sample(16, *right++);
#ifdef WORDS_BIGENDIAN
            *(output_buf + output_at++) = ((sample >> 8) & 0xff);
            *(output_buf + output_at++) = ((sample >> 0) & 0xff);
#else
            *(output_buf + output_at++) = ((sample >> 0) & 0xff);
            *(output_buf + output_at++) = ((sample >> 8) & 0xff);
#endif
            output_bytes += 2;
        }
    }

    if (done || finish) {
        return MAD_FLOW_STOP;
    }

    return MAD_FLOW_CONTINUE;
}

enum mad_flow MadDecoder::madError(struct mad_stream *stream,
                                   struct mad_frame *)
{
    if (MAD_RECOVERABLE(stream->error))
        return MAD_FLOW_CONTINUE;
    fprintf(stderr, "MADERROR!\n");
    return MAD_FLOW_STOP;
}

MetaIO *MadDecoder::doCreateTagger(void)
{
    return new MetaIOID3v2();
}

bool MadDecoderFactory::supports(const QString &source) const
{
    bool res = false;
    QStringList list = QStringList::split("|", extension());

    for (QStringList::Iterator it = list.begin(); it != list.end(); ++it)
    {
        if (*it == source.right((*it).length()).lower())
        {
            res = true;
            break;
        }
    }
    return res;
}

const QString &MadDecoderFactory::extension() const
{
    static QString ext(".mp3|.mp2");
    return ext;
}

const QString &MadDecoderFactory::description() const
{
    static QString desc(QObject::tr("MPEG Layer 1/2/3 Audio (MAD decoder)"));
    return desc;
}

Decoder *MadDecoderFactory::create(const QString &file, QIODevice *input, 
                                   AudioOutput *output, bool deletable)
{
    if (deletable)
        return new MadDecoder(file, this, input, output);

    static MadDecoder *decoder = 0;

    if (! decoder) {
        decoder = new MadDecoder(file, this, input, output);
    } else {
        decoder->setInput(input);
        decoder->setFilename(file);
        decoder->setOutput(output);
    }

    return decoder;
}
