#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <string>
#include <qobject.h>
#include <qiodevice.h>
using namespace std;

#include "flacdecoder.h"
#include "constants.h"
#include <mythtv/audiooutput.h>
#include "metadata.h"
#include "metaioflacvorbiscomment.h"

#include <mythtv/mythcontext.h>

#include <qtimer.h>

static FLAC__SeekableStreamDecoderReadStatus flacread(const FLAC__SeekableStreamDecoder *decoder, FLAC__byte bufferp[], unsigned *bytes, void *client_data)
{
    decoder = decoder;

    FlacDecoder *dflac = (FlacDecoder *) client_data;
    int len = dflac->input()->readBlock((char *)bufferp, *bytes);

    if (len == -1)
    {
        return FLAC__SEEKABLE_STREAM_DECODER_READ_STATUS_ERROR;
    }
   
    *bytes = len;
    return FLAC__SEEKABLE_STREAM_DECODER_READ_STATUS_OK;
}

static FLAC__SeekableStreamDecoderSeekStatus flacseek(const FLAC__SeekableStreamDecoder *decoder, FLAC__uint64 absolute_byte_offset, void *client_data) 
{
    decoder = decoder;
    FlacDecoder *dflac = (FlacDecoder *)client_data;

    if (!dflac->input()->isDirectAccess())
        return FLAC__SEEKABLE_STREAM_DECODER_SEEK_STATUS_ERROR;

    if (dflac->input()->at(absolute_byte_offset))
        return FLAC__SEEKABLE_STREAM_DECODER_SEEK_STATUS_OK;
    return FLAC__SEEKABLE_STREAM_DECODER_SEEK_STATUS_ERROR;
}

static FLAC__SeekableStreamDecoderTellStatus flactell(const FLAC__SeekableStreamDecoder *decoder, FLAC__uint64 *absolute_byte_offset, void *client_data)
{
    decoder = decoder;
    FlacDecoder *dflac = (FlacDecoder *)client_data;

    long t = dflac->input()->at();
    *absolute_byte_offset = t;

    return FLAC__SEEKABLE_STREAM_DECODER_TELL_STATUS_OK;
}

static FLAC__SeekableStreamDecoderLengthStatus flaclength(const FLAC__SeekableStreamDecoder *decoder, FLAC__uint64 *stream_length, void *client_data)
{
    decoder = decoder;

    FlacDecoder *dflac = (FlacDecoder *)client_data;

    *stream_length = dflac->input()->size();
    return FLAC__SEEKABLE_STREAM_DECODER_LENGTH_STATUS_OK;
}

static FLAC__bool flaceof(const FLAC__SeekableStreamDecoder *decoder, void *client_data)
{
    decoder = decoder;

    FlacDecoder *dflac = (FlacDecoder *)client_data;

    return dflac->input()->atEnd();
}

static FLAC__StreamDecoderWriteStatus flacwrite(const FLAC__SeekableStreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 * const buffer[], void *client_data)
{
    decoder = decoder;

    FlacDecoder *dflac = (FlacDecoder *)client_data;

    dflac->doWrite(frame, buffer);

    return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

void FlacDecoder::doWrite(const FLAC__Frame *frame, 
                          const FLAC__int32 * const buffer[])
{
    unsigned int samples = frame->header.blocksize;

    unsigned int cursamp;
    int sample;
    int channel;

    if (bitspersample == 8)
    {
        for (cursamp = 0; cursamp < samples; cursamp++)
        {
            for (channel = 0; channel < chan; channel++)
            {
               sample = (FLAC__int8)buffer[channel][cursamp];
               *(output_buf + output_at++) = ((sample >> 0) & 0xff);
               output_bytes += 1;
            }
        }   
    }
    else if (bitspersample == 16)
    {
        for (cursamp = 0; cursamp < samples; cursamp++)
        {
            for (channel = 0; channel < chan; channel++)
            { 
               sample = (FLAC__int16)buffer[channel][cursamp];             
               *(output_buf + output_at++) = ((sample >> 0) & 0xff);
               *(output_buf + output_at++) = ((sample >> 8) & 0xff);
               output_bytes += 2;
            }
        }
    }
}

static void flacmetadata(const FLAC__SeekableStreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data)
{
    decoder = decoder;

    FlacDecoder *dflac = (FlacDecoder *)client_data;

    dflac->setFlacMetadata(metadata);
}

void FlacDecoder::setFlacMetadata(const FLAC__StreamMetadata *metadata)
{
    bitspersample = metadata->data.stream_info.bits_per_sample;
    chan = metadata->data.stream_info.channels;
    freq = metadata->data.stream_info.sample_rate;
    totalsamples = metadata->data.stream_info.total_samples;
   
    if (output())
    {
        output()->Reconfigure(bitspersample, chan, freq);
        output()->SetSourceBitrate(44100 * 2 * 16);
    }
}

static void flacerror(const FLAC__SeekableStreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data)
{
    decoder = decoder;

    FLAC__FileDecoder *file_decoder = (FLAC__FileDecoder *)client_data;

    file_decoder = file_decoder;
    status = status;
}


FlacDecoder::FlacDecoder(const QString &file, DecoderFactory *d, QIODevice *i, 
                         AudioOutput *o) 
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

    decoder = 0;
}

FlacDecoder::~FlacDecoder(void)
{
    if (inited)
        deinit();

    if (output_buf)
        delete [] output_buf;
    output_buf = 0;
}

void FlacDecoder::stop()
{
    user_stop = TRUE;
}

void FlacDecoder::flush(bool final)
{
    ulong min = final ? 0 : bks;
            
    while ((! done && ! finish) && output_bytes > min) {
        if (user_stop || finish) {
            inited = FALSE;
            done = TRUE;
        } else {
            ulong sz = output_bytes < bks ? output_bytes : bks;

            int samples = (sz*8)/(chan*bitspersample);
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

bool FlacDecoder::initialize()
{
    bks = blockSize();

    inited = user_stop = done = finish = FALSE;
    len = freq = bitrate = 0;
    stat = chan = 0;
    seekTime = -1.0;
    totalTime = 0.0;

    if (! input()) {
        error("FlacDecoder: cannot initialize.  No input.");

        return FALSE;
    }

    if (! output_buf)
        output_buf = new char[globalBufferSize];
    output_at = 0;
    output_bytes = 0;

    if (! input()->isOpen()) {
        if (! input()->open(IO_ReadOnly)) {
            error("FlacDecoder: Failed to open input. Error " +
                  QString::number(input()->status()) + ".");
            return FALSE;
        }
    }

    decoder = FLAC__seekable_stream_decoder_new();
    FLAC__seekable_stream_decoder_set_md5_checking(decoder, false);
    FLAC__seekable_stream_decoder_set_read_callback(decoder, flacread);
    FLAC__seekable_stream_decoder_set_seek_callback(decoder, flacseek);
    FLAC__seekable_stream_decoder_set_tell_callback(decoder, flactell);
    FLAC__seekable_stream_decoder_set_length_callback(decoder, flaclength);
    FLAC__seekable_stream_decoder_set_eof_callback(decoder, flaceof);
    FLAC__seekable_stream_decoder_set_write_callback(decoder, flacwrite);
    FLAC__seekable_stream_decoder_set_metadata_callback(decoder, flacmetadata);
    FLAC__seekable_stream_decoder_set_error_callback(decoder, flacerror);
    FLAC__seekable_stream_decoder_set_client_data(decoder, this);

    freq = 0;
    bitrate = 0;
    chan = 0;

    totalTime = 0; 
    totalTime = totalTime < 0 ? 0 : totalTime;

    FLAC__seekable_stream_decoder_init(decoder);
    FLAC__seekable_stream_decoder_process_until_end_of_metadata(decoder);

    inited = TRUE;
    return TRUE;
}

void FlacDecoder::seek(double pos)
{
    seekTime = pos;
}

void FlacDecoder::deinit()
{
    FLAC__seekable_stream_decoder_finish(decoder);
    FLAC__seekable_stream_decoder_delete(decoder);

    if (input()->isOpen())
        input()->close();

    decoder = 0;

    inited = user_stop = done = finish = FALSE;
    len = freq = bitrate = 0;
    stat = chan = 0;
    setInput(0);
    setOutput(0);
}

void FlacDecoder::run()
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

    bool flacok = true;
    FLAC__SeekableStreamDecoderState decoderstate;

    while (! done && ! finish) {
        lock();
        // decode

        if (seekTime >= 0.0) {
            FLAC__uint64 sample = (FLAC__uint64)(seekTime * 44100.0);
            if (sample > totalsamples - 50)
                sample = totalsamples - 50;
            FLAC__seekable_stream_decoder_seek_absolute(decoder, sample);
            seekTime = -1.0;
        }

        flacok = FLAC__seekable_stream_decoder_process_single(decoder);
        decoderstate = FLAC__seekable_stream_decoder_get_state(decoder);

        if (decoderstate == 0 || decoderstate == 1)
        {
            if (output())
                flush();
        } 
        else 
        {
            flush(TRUE);

            if (output()) {
		output()->Drain();
            }

            done = TRUE;
            if (!user_stop) {
                finish = TRUE;
            }
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

typedef struct {
        char *field; /* the whole field as passed on the command line, i.e. "NAM
E=VALUE" */
        char *field_name;
        /* according to the vorbis spec, field values can contain \0 so simple C
 *  strings are not enough here */
        unsigned field_value_length;
        char *field_value;
} Argument_VcField;

MetaIO *FlacDecoder::doCreateTagger(void)
{
    return new MetaIOFLACVorbisComment();
}

bool FlacDecoderFactory::supports(const QString &source) const
{
    return (source.right(extension().length()).lower() == extension());
}


const QString &FlacDecoderFactory::extension() const
{
    static QString ext(".flac");
    return ext;
}


const QString &FlacDecoderFactory::description() const
{
    static QString desc(QObject::tr("FLAC Audio"));
    return desc;
}

Decoder *FlacDecoderFactory::create(const QString &file, QIODevice *input, 
                                    AudioOutput *output, bool deletable)
{
    if (deletable)
        return new FlacDecoder(file, this, input, output);

    static FlacDecoder *decoder = 0;
    if (! decoder) {
        decoder = new FlacDecoder(file, this, input, output);
    } else {
        decoder->setInput(input);
        decoder->setFilename(file);
        decoder->setOutput(output);
    }

    return decoder;
}
   
