/*
	flacdecoder.cpp

	(c) 2003-2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	flac decoder
	
	Very closely based on work by Brad Hughes
	Copyright (c) 2000-2001 Brad Hughes <bhughes@trolltech.com>

*/

#include "../config.h"

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <string>
using namespace std;

#include <qobject.h>
#include <qiodevice.h>

#include <mythtv/audiooutput.h>

#include "flacdecoder.h"
#include "constants.h"
#include "buffer.h"
//  #include "metadata.h"


#include "settings.h"

extern "C" FLAC__SeekableStreamDecoderReadStatus flacread(const FLAC__SeekableStreamDecoder *decoder, FLAC__byte bufferp[], unsigned *bytes, void *client_data)
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

extern "C" FLAC__SeekableStreamDecoderSeekStatus flacseek(const FLAC__SeekableStreamDecoder *decoder, FLAC__uint64 absolute_byte_offset, void *client_data) 
{
    decoder = decoder;
    FlacDecoder *dflac = (FlacDecoder *)client_data;

    if (!dflac->input()->isDirectAccess())
        return FLAC__SEEKABLE_STREAM_DECODER_SEEK_STATUS_ERROR;

    if (dflac->input()->at(absolute_byte_offset))
        return FLAC__SEEKABLE_STREAM_DECODER_SEEK_STATUS_OK;
    return FLAC__SEEKABLE_STREAM_DECODER_SEEK_STATUS_ERROR;
}

extern "C" FLAC__SeekableStreamDecoderTellStatus flactell(const FLAC__SeekableStreamDecoder *decoder, FLAC__uint64 *absolute_byte_offset, void *client_data)
{
    decoder = decoder;
    FlacDecoder *dflac = (FlacDecoder *)client_data;

    long t = dflac->input()->at();
    *absolute_byte_offset = t;

    return FLAC__SEEKABLE_STREAM_DECODER_TELL_STATUS_OK;
}

extern "C" FLAC__SeekableStreamDecoderLengthStatus flaclength(const FLAC__SeekableStreamDecoder *decoder, FLAC__uint64 *stream_length, void *client_data)
{
    decoder = decoder;

    FlacDecoder *dflac = (FlacDecoder *)client_data;

    *stream_length = dflac->input()->size();
    return FLAC__SEEKABLE_STREAM_DECODER_LENGTH_STATUS_OK;
}

extern "C" FLAC__bool flaceof(const FLAC__SeekableStreamDecoder *decoder, void *client_data)
{
    decoder = decoder;

    FlacDecoder *dflac = (FlacDecoder *)client_data;

    return dflac->input()->atEnd();
}

extern "C" FLAC__StreamDecoderWriteStatus flacwrite(const FLAC__SeekableStreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 * const buffer[], void *client_data)
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
    FlacDecoder *dflac = (FlacDecoder *)client_data;
    dflac->flacError(status);
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
    output_size = 0;

    decoder = 0;

    filename_format = mfdContext->GetSetting("NonID3FileNameFormat").upper();
    ignore_id3 = mfdContext->GetNumSetting("Ignore_ID3", 0);

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
            
    while ((! done && ! finish) && output_bytes > min)
    {
        if (user_stop || finish)
        {
            inited = FALSE;
            done = TRUE;
        } 
        else 
        {
            ulong sz = output_bytes < bks ? output_bytes : bks;

            int samples = (sz*8)/(chan*bitspersample);
            if (output()->AddSamples(output_buf, samples, -1))
            {
                output_bytes -= sz;
                memmove(output_buf, output_buf + sz, output_bytes);
                output_at = output_bytes;
            } 
            else 
            {
                mutex()->unlock();
                usleep(500);
                mutex()->lock();
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
    output_size = 0;
    seekTime = -1.0;
    totalTime = 0.0;

    if (! input())
    {
        warning("FlacDecoder: cannot initialize, no input");
        return FALSE;
    }

    if (! output_buf)
        output_buf = new char[globalBufferSize];
    output_at = 0;
    output_bytes = 0;

    if (! input()->isOpen())
    {
        if (! input()->open(IO_ReadOnly)) 
        {
            warning("FlacOgg: Failed to open input.");
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
    output_size = 0;
    setInput(0);
    setOutput(0);
}

void FlacDecoder::run()
{
    mutex()->lock();

    if (! inited) {
        mutex()->unlock();

        return;
    }

    mutex()->unlock();

    bool flacok = true;
    FLAC__SeekableStreamDecoderState decoderstate;

    while (! done && ! finish)
    {
        mutex()->lock();
        // decode

        if (seekTime >= 0.0) 
        {
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
            {
                flush();
            }
        } 
        else 
        {
            flush(TRUE);

            if (output())
            {
		        output()->Drain();
            }

            done = TRUE;
            if (!user_stop) 
            {
                finish = TRUE;
            }
        }

        mutex()->unlock();
    }
    

    mutex()->lock();
    if(finish)
    {
        message("decoder finish");
    }
    else if(done)
    {
        message("decoder stop");
    }
    mutex()->unlock();

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


AudioMetadata *FlacDecoder::getMetadata()
{

    QString artist = "", album = "", title = "", genre = "";
    int year = 0, tracknum = 0, length = 0;

    FILE *input = fopen(filename.ascii(), "r");

    if (!input)
        return NULL;

    FLAC__Metadata_Chain *chain = FLAC__metadata_chain_new();
    if (!FLAC__metadata_chain_read(chain, filename.ascii()))
    {
        FLAC__metadata_chain_delete(chain); 
        return NULL;
    }

    bool found_vc_block = false;

    FLAC__StreamMetadata *block = 0;
    FLAC__Metadata_Iterator *iterator = FLAC__metadata_iterator_new();

    FLAC__metadata_iterator_init(iterator, chain);

    block = FLAC__metadata_iterator_get_block(iterator);

    FLAC__ASSERT(0 != block);
    FLAC__ASSERT(block->type == FLAC__METADATA_TYPE_STREAMINFO);

    int samplerate = block->data.stream_info.sample_rate;
    int totalsamples = block->data.stream_info.total_samples;
    int bytesperms = (samplerate) / 1000;

    length = totalsamples / bytesperms;

    do {
        block = FLAC__metadata_iterator_get_block(iterator);
        if (block->type == FLAC__METADATA_TYPE_VORBIS_COMMENT)
            found_vc_block = true;
    } while (!found_vc_block && FLAC__metadata_iterator_next(iterator));

    if (!found_vc_block)
    {
        FLAC__metadata_chain_delete(chain);
        FLAC__metadata_iterator_delete(iterator);
        return NULL;
    }

    FLAC__ASSERT(0 != block);
    FLAC__ASSERT(block->type == FLAC__METADATA_TYPE_VORBIS_COMMENT);

    artist = getComment(block, "artist");
    album = getComment(block, "album");
    title = getComment(block, "title");
    genre = getComment(block, "genre");
    tracknum = getComment(block, "tracknumber").toInt(); 
    year = getComment(block, "date").toInt();

    metadataSanityCheck(&artist, &album, &title, &genre);

    AudioMetadata *retdata = new AudioMetadata(
                                                filename, 
                                                artist, 
                                                album, 
                                                title, 
                                                genre,
                                                year, 
                                                tracknum, 
                                                length
                                              );

    FLAC__metadata_chain_delete(chain);
    FLAC__metadata_iterator_delete(iterator);

    return retdata;
}    

/*
void FlacDecoder::commitMetadata(Metadata *mdata)
{
    FILE *input = fopen(filename.ascii(), "r");

    if (!input)
        return;

    FLAC__Metadata_Chain *chain = FLAC__metadata_chain_new();
    if (!FLAC__metadata_chain_read(chain, filename.ascii()))
    {
        FLAC__metadata_chain_delete(chain);
        return;
    }

    bool found_vc_block = false;
    FLAC__StreamMetadata *block = 0;
    FLAC__Metadata_Iterator *iterator = FLAC__metadata_iterator_new();

    FLAC__metadata_iterator_init(iterator, chain);

    do {
        block = FLAC__metadata_iterator_get_block(iterator);
        if (block->type == FLAC__METADATA_TYPE_VORBIS_COMMENT)
            found_vc_block = true;
    } while (!found_vc_block && FLAC__metadata_iterator_next(iterator));

    if (!found_vc_block)
    {
        block = FLAC__metadata_object_new(FLAC__METADATA_TYPE_VORBIS_COMMENT);

        while (FLAC__metadata_iterator_next(iterator))
            ;

        if (!FLAC__metadata_iterator_insert_block_after(iterator, block))
        {
            FLAC__metadata_chain_delete(chain);
            FLAC__metadata_iterator_delete(iterator);
            return;
        }

        FLAC__ASSERT(FLAC__metadata_iterator_get_block(iterator) == block);
    }

    FLAC__ASSERT(0 != block);
    FLAC__ASSERT(block->type == FLAC__METADATA_TYPE_VORBIS_COMMENT);

    if (block->data.vorbis_comment.comments > 0)
        FLAC__metadata_object_vorbiscomment_resize_comments(block, 0);

    setComment(block, "artist", mdata->Artist());
    setComment(block, "album", mdata->Album());
    setComment(block, "title", mdata->Title());
    setComment(block, "genre", mdata->Genre());

    char text[128];
    if (mdata->Track() != 0)
    {
        sprintf(text, "%d", mdata->Track());
        setComment(block, "tracknumber", text);
    }

    if (mdata->Year() != 0)
    {
        sprintf(text, "%d", mdata->Year());
        setComment(block, "date", text);
    }

    FLAC__metadata_chain_write(chain, false, false);

    FLAC__metadata_chain_delete(chain);
    FLAC__metadata_iterator_delete(iterator);
}
*/


QString FlacDecoder::getComment(FLAC__StreamMetadata *block, const char *label)
{
    FLAC__StreamMetadata_VorbisComment_Entry *entry;
    entry = block->data.vorbis_comment.comments;

    QString qlabel = label;
    QString retstr = "";
    for (unsigned int i = 0; i < block->data.vorbis_comment.num_comments; i++)
    {
        char fieldname[512];
        memset(fieldname, 0, 512);
        strncpy(fieldname, (char *)((entry + i)->entry), (entry + i)->length);
        QString entrytext = fieldname;
        int loc;

        if ((loc = entrytext.find("=")) && 
            entrytext.lower().left(qlabel.length()) == qlabel.lower())
        {
            retstr = QString::fromUtf8(entrytext.right(entrytext.length() - loc - 1));
        }
    }

    return retstr;
}

void FlacDecoder::flacError(FLAC__StreamDecoderErrorStatus status)
{
    status = status;
    
    log("FlacDecoder: got an error during flac "
        "decoding, but will try to continue", 3);

}




/*
void FlacDecoder::setComment(FLAC__StreamMetadata *block, const char *label,
                             const QString &data)
{
    if (data.length() < 1)
        return;

    QString test = getComment(block, label);

    QString thenewentry = QString(label).upper() + "=" + data;
    QCString utf8str = thenewentry.utf8();
    int thenewentrylen = utf8str.length();

    FLAC__StreamMetadata_VorbisComment_Entry entry;

    entry.length = thenewentrylen;
    entry.entry = (unsigned char *)utf8str.data();

    FLAC__metadata_object_vorbiscomment_insert_comment(block, block->data.vorbis_comment.num_comments, entry, true);
}


*/


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
        decoder->setOutput(output);
    }

    return decoder;
}
   
