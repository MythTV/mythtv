/*
    MythTV WMA Decoder
    Written by Kevin Kuphal

    Special thanks to 
       ffmpeg team for libavcodec and libavformat
       qemacs team for their av support which I used to understand the libraries
       getid3.sourceforget.net project for the ASF information used here
        
    This library decodes Windows Media (WMA/ASF) files into PCM data 
    returned to the MythMusic output buffer.  

    Revision History
        - Initial release
        - 1/9/2004 - Improved seek support
*/

#include <iostream>
#include <string>
#include <qobject.h>
#include <qiodevice.h>
#include <qfile.h>

using namespace std;

#include "avfdecoder.h"
#include "constants.h"
#include <mythtv/audiooutput.h>
#include "metadata.h"
#include "metaioavfcomment.h"

#include <mythtv/mythcontext.h>

typedef struct {
    uint32_t v1;
    uint16_t v2;
    uint16_t v3;
    uint8_t v4[8];
} GUID;

avfDecoder::avfDecoder(const QString &file, DecoderFactory *d, QIODevice *i, 
                       AudioOutput *o) 
          : Decoder(d, i, o)
{
    filename = file;
    inited = FALSE;
    user_stop = FALSE;
    stat = 0;
    bks = 0;
    done = FALSE;
    finish = FALSE;
    len = 0;
    freq = 0;
    bitrate = 0;
    seekTime = -1.0;
    totalTime = 0.0;
    chan = 0;
    output_buf = 0;
    output_bytes = 0;
    output_at = 0;

    ic = NULL;
    oc = NULL;
    ifmt = NULL;
    ap = &params;
    pkt = &pkt1;
}

avfDecoder::~avfDecoder(void)
{
    if (inited)
        deinit();

    if (output_buf)
    {
        delete [] output_buf;
        output_buf = NULL;
    }
}

void avfDecoder::stop()
{
    user_stop = TRUE;
}

void avfDecoder::flush(bool final)
{
    ulong min = final ? 0 : bks;

    while ((!done && !finish && seekTime <= 0) && output_bytes > min) 
    {
        if (user_stop || finish) 
        {
            inited = FALSE;
            done = TRUE;
        } 
        else 
        {
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

bool avfDecoder::initialize()
{
    bks = blockSize();

    inited = user_stop = done = finish = FALSE;
    len = freq = bitrate = 0;
    stat = chan = 0;
    seekTime = -1.0;
    totalTime = 0.0;

    filename = ((QFile *)input())->name();
   
    if (!output_buf)
        output_buf = new char[globalBufferSize];
    output_at = 0;
    output_bytes = 0;

    // open device
    // register av codecs
    av_register_all();

    // open the media file
    // this should populate the input context
    if (av_open_input_file(&ic, filename, ifmt, 0, ap) < 0)
        return FALSE;    

    // determine the stream format
    // this also populates information needed for metadata
    if (av_find_stream_info(ic) < 0) 
        return FALSE;

    // Store the audio codec of the stream
    audio_dec = ic->streams[0]->codec;

    // Store the input format of the context
    ifmt = ic->iformat;

    // Determine the output format
    // Given we are outputing to a sound card, this will always
    // be a PCM format
    fmt = guess_format("audio_device", NULL, NULL);
    if (!fmt)
        return FALSE;

    // Populate the output context
    // Create the output stream and attach to output context
    // Set various parameters to match the input format
    oc = (AVFormatContext *)av_mallocz(sizeof(AVFormatContext));
    oc->oformat = fmt;

    dec_st = av_new_stream(oc,0);
    dec_st->codec->codec_type = CODEC_TYPE_AUDIO;
    dec_st->codec->codec_id = oc->oformat->audio_codec;
    dec_st->codec->sample_rate = audio_dec->sample_rate;
    dec_st->codec->channels = audio_dec->channels;
    dec_st->codec->bit_rate = audio_dec->bit_rate;
    av_set_parameters(oc, NULL);

    // Prepare the decoding codec
    // The format is different than the codec
    // While we could get fed a WAV file, it could contain a number
    // of different codecs
    codec = avcodec_find_decoder(audio_dec->codec_id);
    if (!codec)
        return FALSE;
    if (avcodec_open(audio_dec,codec) < 0)
        return FALSE;
    totalTime = (ic->duration / AV_TIME_BASE) * 1000;

    freq = audio_dec->sample_rate;
    chan = audio_dec->channels;

    if (output())
    {
        output()->Reconfigure(16, audio_dec->channels, audio_dec->sample_rate,
                              false /* AC3/DTS pass through */);
        output()->SetSourceBitrate(audio_dec->bit_rate);
    }

    inited = TRUE;
    return TRUE;
}

void avfDecoder::seek(double pos)
{
    seekTime = pos;
}

void avfDecoder::deinit()
{
    inited = user_stop = done = finish = FALSE;
    len = freq = bitrate = 0;
    stat = chan = 0;
    setInput(0);
    setOutput(0);

    // Cleanup here
    if(ic)
    {
        av_close_input_file(ic);
        ic = NULL;
    }
    if(oc)
    {
        av_free(oc);
        oc = NULL;
    }
}

void avfDecoder::run()
{
    int mem_len;
    char *s;

    lock();

    if (!inited) 
    {
        unlock();
        return;
    }

    stat = DecoderEvent::Decoding;

    unlock();

    {
        DecoderEvent e((DecoderEvent::Type) stat);
        dispatch(e);
    }

    av_read_play(ic);
    while (!done && !finish && !user_stop) 
    {
        lock();

        // Look to see if user has requested a seek
        if (seekTime >= 0.0) 
        {
            cerr << "avfdecoder.o: seek time " << seekTime << endl;
            if (av_seek_frame(ic, -1, (int64_t)(seekTime * AV_TIME_BASE), 0)
                              < 0)
            {
                cerr << "avfdecoder.o: error seeking" << endl;
            }

            seekTime = -1.0;
        }

        // Read a packet from the input context
        // if (av_read_packet(ic, pkt) < 0)
        if (av_read_frame(ic, pkt) < 0)
        {
            cerr << "avfdecoder.o: read frame failed" << endl;
            unlock();
            finish = TRUE;
            break;
        }

        // Get the pointer to the data and its length
        ptr = pkt->data;
        len = pkt->size;
        unlock();

        while (len > 0 && !done && !finish && !user_stop && seekTime <= 0.0)  
        {
            lock();
            // Decode the stream to the output codec
            // Samples is the output buffer
            // data_size is the size in bytes of the frame
            // ptr is the input buffer
            // len is the size of the input buffer
            dec_len = avcodec_decode_audio(audio_dec, samples, &data_size, 
                                           ptr, len);    
            if (dec_len < 0) 
            {
                unlock();
                break;
            }

            s = (char *)samples;
            unlock();

            while (data_size > 0 && !done && !finish && !user_stop && 
                   seekTime <= 0.0) 
             {
                lock();
                // Store and check the size
                // It is possible the returned data is larger than
                // the output buffer.  If so, flush the buffer and
                // limit the data written to the buffer size
                mem_len = data_size;
                if ((output_at + data_size) > globalBufferSize)
                {
                    // if (output()) { flush(); }
                    mem_len = globalBufferSize - output_at;
                }

                // Copy the data to the output buffer
                memcpy((char *)(output_buf + output_at), s, mem_len);

                // Increment the output pointer and count
                output_at += mem_len;
                output_bytes += mem_len;

                // Move the input buffer pointer and mark off
                // what we sent to the output buffer
                data_size -= mem_len;
                s += mem_len;
            
                if (output())
                    flush();

                unlock();
            }

            lock();
            flush();
            ptr += dec_len;
            len -= dec_len;
            unlock();
        }
        av_free_packet(pkt);
    }

    flush(TRUE);
    if (output())
        output()->Drain();

    if (finish)
        stat = DecoderEvent::Finished;
    else if (user_stop)
        stat = DecoderEvent::Stopped;

    // unlock();

    {
        DecoderEvent e((DecoderEvent::Type) stat);
        dispatch(e);
    }

    deinit();
}

MetaIO* avfDecoder::doCreateTagger(void)
{
    return new MetaIOAVFComment();
}    

bool avfDecoderFactory::supports(const QString &source) const
{
     QStringList list = QStringList::split("|", extension());
     for (QStringList::Iterator it = list.begin(); it != list.end(); ++it)
     {
         if (*it == source.right((*it).length()).lower())
             return true;
     }

     return false;
}

const QString &avfDecoderFactory::extension() const
{
    static QString ext(".wma|.wav");
    return ext;
}

const QString &avfDecoderFactory::description() const
{
    static QString desc(QObject::tr("Windows Media Audio"));
    return desc;
}

Decoder *avfDecoderFactory::create(const QString &file, QIODevice *input, 
                                  AudioOutput *output, bool deletable)
{
    if (deletable)
        return new avfDecoder(file, this, input, output);

    static avfDecoder *decoder = 0;
    if (!decoder) 
    {
        decoder = new avfDecoder(file, this, input, output);
    } 
    else 
    {
        decoder->setInput(input);
        decoder->setOutput(output);
    }

    return decoder;
}

