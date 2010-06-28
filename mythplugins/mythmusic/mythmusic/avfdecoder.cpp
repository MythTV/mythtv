/*
    MythMusic libav* Decoder
    Originally written by Kevin Kuphal with contributions and updates from
    many others

    Special thanks to
       ffmpeg team for libavcodec and libavformat
       qemacs team for their av support which I used to understand the libraries
       getid3.sourceforget.net project for the ASF information used here

    This library decodes various media files into PCM data
    returned to the MythMusic output buffer.

    Revision History
        - Initial release
        - 1/9/2004 - Improved seek support
        - ?/?/2009 - Extended to support many more filetypes and bug fixes
*/

// C++ headers
#include <iostream>
#include <string>

// QT headers
#include <QObject>
#include <QIODevice>
#include <QFile>

// Myth headers
#include <mythconfig.h>
#include <mythcontext.h>
#include <audiooutput.h>
#include <mythverbose.h>

using namespace std;

// Mythmusic Headers
#include "avfdecoder.h"
#include "constants.h"
#include "metadata.h"
#include "metaioavfcomment.h"
#include "metaioid3.h"
#include "metaioflacvorbis.h"
#include "metaiooggvorbis.h"
#include "metaiomp4.h"
#include "metaiowavpack.h"

avfDecoder::avfDecoder(const QString &file, DecoderFactory *d, QIODevice *i,
                       AudioOutput *o) :
    Decoder(d, i, o),
    inited(false),   user_stop(false),
    stat(0),         output_buf(NULL),
    output_bytes(0), output_at(0),
    bks(0),          done(false),
    finish(false),   len(0),
    freq(0),         bitrate(0),
    m_channels(0),   output_size(0),
    totalTime(0.0),  seekTime(-1.0),
    devicename(""),  start(0),
    end(0),          m_outputFormat(0),
    m_inputFormat(NULL),
    m_outputContext(NULL),  m_inputContext(NULL),
    m_decStream(NULL),      m_codec(NULL),
    m_audioEnc(NULL),       m_audioDec(NULL),
    m_pkt(&m_pkt1),         errcode(0),
    ptr(NULL),              dec_len(0),
    data_size(0),           m_samples(NULL)
{
    setFilename(file);

    memset(&m_params, 0, sizeof(AVFormatParameters));

}

avfDecoder::~avfDecoder(void)
{
    if (inited)
        deinit();

    av_freep((void *)&m_samples);

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

            int samples = (sz*8)/(m_channels*16);
            // Never buffer more than 5000ms of audio since this slows down
            // actions such as seeking or track changes made after decoding is
            // complete but audio remains in the buffer
            bool ok = (output()->GetAudioBufferedTime() <= 5000);
            if (ok) ok = output()->AddSamples(output_buf, samples, -1);
            if (ok)
            {
                output_bytes -= sz;
                memmove(output_buf, output_buf + sz, output_bytes);
                output_at = output_bytes;
            } else {
                unlock();
                usleep(5000);
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
    stat = m_channels = 0;
    seekTime = -1.0;
    totalTime = 0.0;

    filename = ((QFile *)input())->fileName();

    if (!m_samples)
    {
        m_samples = (int16_t *)av_mallocz(AVCODEC_MAX_AUDIO_FRAME_SIZE / 2 *
                                          sizeof(*m_samples));
        if (!m_samples)
        {
            VERBOSE(VB_GENERAL, "Could not allocate output buffer in "
                    "avfDecoder::initialize");
            return false;
        }
    }

    if (!output_buf)
        output_buf = new char[globalBufferSize];
    output_at = 0;
    output_bytes = 0;

    // open device
    // register av codecs
    av_register_all();

    // open the media file
    // this should populate the input context
    int error;
    error = av_open_input_file(&m_inputContext, filename.toLocal8Bit().constData(),
                               m_inputFormat, 0, &m_params);

    if (error < 0)
    {
        VERBOSE(VB_IMPORTANT, QString("Could not open file (%1)").arg(filename));
        VERBOSE(VB_IMPORTANT, QString("AV decoder. Error: %1").arg(error));
        deinit();
        return FALSE;
    }

    // determine the stream format
    // this also populates information needed for metadata
    if (av_find_stream_info(m_inputContext) < 0)
    {
        VERBOSE(VB_GENERAL, "Could not determine the stream format.");
        deinit();
        return FALSE;
    }

    // Store the audio codec of the stream
    m_audioDec = m_inputContext->streams[0]->codec;

    // Store the input format of the context
    m_inputFormat = m_inputContext->iformat;

    // Determine the output format
    // Given we are outputing to a sound card, this will always
    // be a PCM format

#if HAVE_BIGENDIAN
    m_outputFormat = guess_format("s16be", NULL, NULL);
#else
    m_outputFormat = guess_format("s16le", NULL, NULL);
#endif

    if (!m_outputFormat)
    {
        VERBOSE(VB_IMPORTANT, "avfDecoder.o - failed to get output format");
        deinit();
        return FALSE;
    }

    // Populate the output context
    // Create the output stream and attach to output context
    // Set various parameters to match the input format
    m_outputContext = (AVFormatContext *)av_mallocz(sizeof(AVFormatContext));
    m_outputContext->oformat = m_outputFormat;

    m_decStream = av_new_stream(m_outputContext,0);
    m_decStream->codec->codec_type = CODEC_TYPE_AUDIO;
    m_decStream->codec->codec_id = m_outputContext->oformat->audio_codec;
    m_decStream->codec->sample_rate = m_audioDec->sample_rate;
    m_decStream->codec->channels = m_audioDec->channels;
    m_decStream->codec->bit_rate = m_audioDec->bit_rate;
    av_set_parameters(m_outputContext, NULL);

    // Prepare the decoding codec
    // The format is different than the codec
    // While we could get fed a WAV file, it could contain a number
    // of different codecs
    m_codec = avcodec_find_decoder(m_audioDec->codec_id);
    if (!m_codec)
    {
        VERBOSE(VB_GENERAL, QString("Could not find audio codec: %1")
                                                    .arg(m_audioDec->codec_id));
        deinit();
        return FALSE;
    }
    if (avcodec_open(m_audioDec,m_codec) < 0)
    {
        VERBOSE(VB_GENERAL, QString("Could not open audio codec: %1")
                                                    .arg(m_audioDec->codec_id));
        deinit();
        return FALSE;
    }
    if (AV_TIME_BASE > 0)
        totalTime = (m_inputContext->duration / AV_TIME_BASE) * 1000;

    freq = m_audioDec->sample_rate;
    m_channels = m_audioDec->channels;

    if (m_channels <= 0)
    {
        VERBOSE(VB_IMPORTANT, QString("AVCodecContext tells us %1 channels are "
                                      "available, this is bad, bailing.")
                                      .arg(m_channels));
        deinit();
        return false;
    }

    if (output())
    {
        const AudioSettings settings(
            16 /*bits*/, m_audioDec->channels, m_audioDec->codec_id, 
            m_audioDec->sample_rate, false /* AC3/DTS pass through */);
        output()->Reconfigure(settings);
        output()->SetSourceBitrate(m_audioDec->bit_rate);
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
    stat = m_channels = 0;
    setInput(0);
    setOutput(0);

    // Cleanup here
    if (m_inputContext)
    {
        av_close_input_file(m_inputContext);
        m_inputContext = NULL;
    }

    if (m_outputContext)
    {
        av_free(m_outputContext);
        m_outputContext = NULL;
    }

    m_decStream = NULL;
    m_codec = NULL;
    m_audioEnc = m_audioDec = NULL;
    m_inputFormat = NULL;
    m_outputFormat = NULL;
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

    av_read_play(m_inputContext);
    while (!done && !finish && !user_stop)
    {
        lock();

        // Look to see if user has requested a seek
        if (seekTime >= 0.0)
        {
            VERBOSE(VB_GENERAL, QString("avfdecoder.o: seek time %1")
                .arg(seekTime));
            if (av_seek_frame(m_inputContext, -1, (int64_t)(seekTime * AV_TIME_BASE), 0)
                              < 0)
            {
                VERBOSE(VB_IMPORTANT, "Error seeking");
            }

            seekTime = -1.0;
        }

        // Read a packet from the input context
        // if (av_read_packet(m_inputContext, m_pkt) < 0)
        if (av_read_frame(m_inputContext, m_pkt) < 0)
        {
            VERBOSE(VB_IMPORTANT, "Read frame failed");
            VERBOSE(VB_FILE, ("... for file '" + filename) + "'");
            unlock();
            finish = TRUE;
            break;
        }

        // Get the pointer to the data and its length
        ptr = m_pkt->data;
        len = m_pkt->size;
        unlock();

        while (len > 0 && !done && !finish && !user_stop && seekTime <= 0.0)
        {
            lock();
            // Decode the stream to the output codec
            // m_samples is the output buffer
            // data_size is the size in bytes of the frame
            // ptr is the input buffer
            // len is the size of the input buffer
            data_size = AVCODEC_MAX_AUDIO_FRAME_SIZE;
            dec_len = avcodec_decode_audio2(m_audioDec, m_samples, &data_size,
                                            ptr, len);
            if (dec_len < 0)
            {
                unlock();
                break;
            }

            s = (char *)m_samples;
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
        av_free_packet(m_pkt);
    }

    flush(TRUE);

    if (output() && !user_stop)
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
    QString extension = filename.section('.', -1);

    if (extension == "mp3")
        return new MetaIOID3();
    else if (extension == "ogg" || extension == "oga")
        return new MetaIOOggVorbis();
    else if (extension == "flac")
        return new MetaIOFLACVorbis();
    else if (extension == "m4a")
        return new MetaIOMP4();
    else if (extension == "wv")
        return new MetaIOWavPack();
    else
        return new MetaIOAVFComment();
}

bool avfDecoderFactory::supports(const QString &source) const
{
    QStringList list = extension().split("|", QString::SkipEmptyParts);
    for (QStringList::const_iterator it = list.begin(); it != list.end(); ++it)
    {
        if (*it == source.right((*it).length()).toLower())
            return true;
    }

    return false;
}

const QString &avfDecoderFactory::extension() const
{
    static QString ext(".mp3|.mp2|.ogg|.oga|.flac|.wma|.wav|.ac3|.oma|.omg|"
                       ".atp|.ra|.dts|.aac|.m4a|.aa3|.tta|.mka|.aiff|.swa|.wv");
    return ext;
}

const QString &avfDecoderFactory::description() const
{
    static QString desc(QObject::tr("Internal Decoder"));
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

