#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <math.h>
#include <stdio.h>
#include <qregexp.h>
using namespace std;

#include <mad.h>
#include <id3tag.h>

#include "maddecoder.h"
#include "metadata.h"
#include "constants.h"
#include "buffer.h"
#include "output.h"

#include <mythtv/mythcontext.h>

#define XING_MAGIC     (('X' << 24) | ('i' << 16) | ('n' << 8) | 'g')

MadDecoder::MadDecoder(const QString &file, DecoderFactory *d, QIODevice *i, 
                       Output *o)
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
    output_size = 0;

    filename_format = gContext->GetSetting("NonID3FileNameFormat").upper();
    ignore_id3 = gContext->GetNumSetting("Ignore_ID3", 0);
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
    output_size = 0;

    if (! input()) {
        error("DecoderMAD: cannot initialize.  No input.");
        return FALSE;
    }

    if (! input_buf)
        input_buf = new char[globalBufferSize];

    if (! output_buf)
        output_buf = new char[globalBufferSize];

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
        output()->configure(freq, channels, 16, bitrate);

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
    output_size = 0;
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
        output()->recycler()->mutex()->lock();

        while ((! done && ! finish) && output()->recycler()->full()) {
            mutex()->unlock();

            output()->recycler()->cond()->wait(output()->recycler()->mutex());

            mutex()->lock();
            done = user_stop;
        }

        if (user_stop || finish) {
            inited = FALSE;
            done = TRUE;
        } else {
            ulong sz = output_bytes < bks ? output_bytes : bks;
            Buffer *b = output()->recycler()->get();

            memcpy(b->data, output_buf, sz);
            if (sz != bks) memset(b->data + sz, 0, bks - sz);

            b->nbytes = bks;
            b->rate = bitrate;
            output_size += b->nbytes;
            output()->recycler()->add();

            output_bytes -= sz;
            memmove(output_buf, output_buf + sz, output_bytes);
            output_at = output_bytes;
        }

        if (output()->recycler()->full()) {
            output()->recycler()->cond()->wakeOne();
        }

        output()->recycler()->mutex()->unlock();
    }
}

void MadDecoder::run()
{
    mutex()->lock();

    if (! inited) {
        mutex()->unlock();
        return;
    }

    stat = DecoderEvent::Decoding;

    mutex()->unlock();

    {
        DecoderEvent e((DecoderEvent::Type) stat);
        dispatch(e);
    }

    while (! done && ! finish && ! derror) {
        mutex()->lock();

        if (seekTime >= 0.0) {
            long seek_pos = long(seekTime * input()->size() / totalTime);
            input()->at(seek_pos);
            output_size = long(seekTime) * long(freq * channels * 16 / 2);
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
                    break;
                }

                input_bytes += len;
            }

            mad_stream_buffer(&stream, (unsigned char *) input_buf, 
                              input_bytes);
        }

        seekTime = -1.;

        mutex()->unlock();

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

            mutex()->lock();

            if (seekTime >= 0.) {
                mutex()->unlock();
                break;
            }

            mad_synth_frame(&synth, &frame);
            madOutput();
            mutex()->unlock();
        }
    }

    mutex()->lock();

    if (! user_stop && eof) {
        flush(TRUE);

        if (output()) {
            output()->recycler()->mutex()->lock();
            while (! output()->recycler()->empty() && ! user_stop) {
                output()->recycler()->cond()->wakeOne();
                mutex()->unlock();
                output()->recycler()->cond()->wait(
                                                 output()->recycler()->mutex());
                mutex()->lock();
            }
            output()->recycler()->mutex()->unlock();
        }

        done = TRUE;
        if (! user_stop)
            finish = TRUE;
    }

    if (finish)
        stat = DecoderEvent::Finished;
    else if (user_stop)
        stat = DecoderEvent::Stopped;

    mutex()->unlock();

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
    unsigned int samples, channels;
    mad_fixed_t const *left, *right;

    samples = synth.pcm.length;
    channels = synth.pcm.channels;
    left = synth.pcm.samples[0];
    right = synth.pcm.samples[1];


    bitrate = frame.header.bitrate / 1000;

    while (samples--)
    {
        signed int sample;

        if (output_bytes + 4096 > globalBufferSize)
        {
            flush();
        }
        sample = fix_sample(16, *left++);
        *(output_buf + output_at++) = ((sample >> 0) & 0xff);
        *(output_buf + output_at++) = ((sample >> 8) & 0xff);
        output_bytes += 2;

        if (channels == 2)
        {
            sample = fix_sample(16, *right++);
            *(output_buf + output_at++) = ((sample >> 0) & 0xff);
            *(output_buf + output_at++) = ((sample >> 8) & 0xff);
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

Metadata *MadDecoder::getMetadata(QSqlDatabase *db)
{
    Metadata *testdb = new Metadata(filename);
    if (testdb->isInDatabase(db))
        return testdb;

    delete testdb;

    QString artist = "", album = "", title = "", genre = "";
    int year = 0, tracknum = 0, length = 0;

    if (!ignore_id3)
    {
        // use ID3 header
        id3_file *id3file = id3_file_open(filename.ascii(), 
                                          ID3_FILE_MODE_READONLY);
        if (!id3file)
        {
            return NULL;
        }

        id3_tag *tag = id3_file_tag(id3file);

        if (!tag)
        {
            id3_file_close(id3file);
            return NULL;
        }

        struct {
            char const *id;
            char const *name;
        } const info[] = {
            { ID3_FRAME_TITLE,  "Title"  },
            { ID3_FRAME_ARTIST, "Artist" },
            { ID3_FRAME_ALBUM,  "Album"  },
            { ID3_FRAME_YEAR,   "Year"   },
            { ID3_FRAME_TRACK,  "Track"  },
            { ID3_FRAME_GENRE,  "Genre"  },
        };

        for (unsigned int i = 0; i < sizeof(info) / sizeof(info[0]); ++i)
        {
            struct id3_frame *frame = id3_tag_findframe(tag, info[i].id, 0);
            if (!frame)
                continue;

            id3_ucs4_t const *ucs4;
            id3_latin1_t *latin1;
            union id3_field *field;
            unsigned int nstrings;

            field = &frame->fields[1];
            nstrings = id3_field_getnstrings(field);

            for (unsigned int j = 0; j < nstrings; ++j)
            {
                ucs4 = id3_field_getstrings(field, j);
                assert(ucs4);

                if (!strcmp(info[i].id, ID3_FRAME_GENRE))
                    ucs4 = id3_genre_name(ucs4);

                latin1 = id3_ucs4_latin1duplicate(ucs4);
                if (!latin1)
                    continue;

                switch (i)
                {
                    case 0: title = (char *)latin1; break;
                    case 1: artist = (char *)latin1; break;
                    case 2: album = (char *)latin1; break;
                    case 3: year = atoi((char *)latin1); break;
                    case 4: tracknum = atoi((char *)latin1); break;
                    case 5: genre = (char *)latin1; break;
                    default: break;
                }

                free(latin1);
            }
        }

        id3_file_close(id3file);
    }
    else
    {
        getMetadataFromFilename(filename, QString(".mp3$"), artist, album, 
                                title, genre, tracknum);
    }

    struct mad_stream stream;
    struct mad_header header;
    mad_timer_t timer;

    unsigned char buffer[8192];
    unsigned int buflen = 0;

    mad_stream_init(&stream);
    mad_header_init(&header);
   
    timer = mad_timer_zero;

    FILE *input = fopen(filename.ascii(), "r");

    while (1) 
    {
        if (buflen < sizeof(buffer)) 
        {
            int bytes;
            bytes = fread(buffer + buflen, 1, sizeof(buffer) - buflen, input);
            if (bytes <= 0)
                break;
            buflen += bytes;
        }

        mad_stream_buffer(&stream, buffer, buflen);

        while (1)
        {
            if (mad_header_decode(&header, &stream) == -1)
            {
                if (!MAD_RECOVERABLE(stream.error))
                    break;
                if (stream.error == MAD_ERROR_LOSTSYNC)
                {
                    int tagsize = id3_tag_query(stream.this_frame,
                                                stream.bufend - 
                                                stream.this_frame);
                    if (tagsize > 0)
                        mad_stream_skip(&stream, tagsize);
                }
            }
            mad_timer_add(&timer, header.duration);
        }
        
        if (stream.error != MAD_ERROR_BUFLEN)
            break;

        memmove(buffer, stream.next_frame, &buffer[buflen] - stream.next_frame);
        buflen -= stream.next_frame - &buffer[0];
    }

    mad_header_finish(&header);
    mad_stream_finish(&stream);

    fclose(input);

    length = mad_timer_count(timer, MAD_UNITS_MILLISECONDS);

    Metadata *retdata = new Metadata(filename, artist, album, title, genre,
                                     year, tracknum, length);

    retdata->dumpToDatabase(db);

    return retdata;
}

void MadDecoder::commitMetadata(Metadata *mdata)
{
    mdata = mdata;
}


bool MadDecoderFactory::supports(const QString &source) const
{
    return (source.right(extension().length()).lower() == extension());
}

const QString &MadDecoderFactory::extension() const
{
    static QString ext(".mp3");
    return ext;
}

const QString &MadDecoderFactory::description() const
{
    static QString desc("MPEG Layer 1/2/3 Audio (MAD decoder)");
    return desc;
}

Decoder *MadDecoderFactory::create(const QString &file, QIODevice *input, 
                                   Output *output, bool deletable)
{
    if (deletable)
        return new MadDecoder(file, this, input, output);

    static MadDecoder *decoder = 0;

    if (! decoder) {
        decoder = new MadDecoder(file, this, input, output);
    } else {
        decoder->setInput(input);
        decoder->setOutput(output);
    }

    return decoder;
}
