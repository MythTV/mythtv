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
        - ?/7/2010 - Add streaming support 
*/

// QT headers
#include <QObject>
#include <QIODevice>
#include <QFile>

// Myth headers
#include <mythconfig.h>
#include <mythcontext.h>
#include <audiooutput.h>
#include <mythverbose.h>
#include "mythlogging.h"

using namespace std;

// Mythmusic Headers
#include "avfdecoder.h"
#include "metaioavfcomment.h"
#include "metaioid3.h"
#include "metaioflacvorbis.h"
#include "metaiooggvorbis.h"
#include "metaiomp4.h"
#include "metaiowavpack.h"

// size of the buffer used for streaming
#define BUFFER_SIZE 8192

// streaming callbacks
static int ReadFunc(void *opaque, uint8_t *buf, int buf_size)
{
    QIODevice *io = (QIODevice*)opaque;

    buf_size = min(buf_size, (int) io->bytesAvailable());
    return io->read((char*)buf, buf_size);
}

static int WriteFunc(void *opaque, uint8_t *buf, int buf_size)
{
    (void)opaque;
    (void)buf;
    (void)buf_size;
    // we don't support writing to the steam
    return -1;
}

static int64_t SeekFunc(void *opaque, int64_t offset, int whence)
{
    (void)opaque;
    (void)offset;
    (void)whence;
    // we dont support seeking while streaming
    return -1;
}

avfDecoder::avfDecoder(const QString &file, DecoderFactory *d, QIODevice *i,
                       AudioOutput *o) :
    Decoder(d, i, o),
    inited(false),              user_stop(false),
    stat(0),                    output_buf(NULL),
    output_at(0),               bks(0),
    bksFrames(0),               decodeBytes(0),
    finish(false),
    freq(0),                    bitrate(0),
    m_sampleFmt(FORMAT_NONE),   m_channels(0),
    totalTime(0.0),             seekTime(-1.0),
    devicename(""),
    m_inputFormat(NULL),        m_inputContext(NULL),
    m_decStream(NULL),          m_codec(NULL),
    m_audioDec(NULL),           m_buffer(NULL),
    m_byteIOContext(NULL),      errcode(0),
    m_samples(NULL)
{
    setFilename(file);
    memset(&m_params, 0, sizeof(AVFormatParameters));
}

avfDecoder::~avfDecoder(void)
{
    if (inited)
        deinit();

    av_freep((void *)&m_samples);
}

void avfDecoder::stop()
{
    user_stop = TRUE;
}

void avfDecoder::writeBlock()
{
    while (!user_stop && seekTime <= 0)
    {
        if(output()->AddFrames(output_buf, bksFrames, -1))
        {
            output_at -= bks;
            memmove(output_buf, output_buf + bks, output_at);
            break;
        }
        else
            usleep(output()->GetAudioBufferedTime()<<9);
    }
}

bool avfDecoder::initialize()
{
    inited = user_stop = finish = false;
    freq = bitrate = 0;
    stat = m_channels = 0;
    m_sampleFmt = FORMAT_NONE;
    seekTime = -1.0;
    totalTime = 0.0;

    // give up if we dont have an audiooutput set
    if (!output())
    {
        error("avfDecoder: initialise called with a NULL audiooutput"); 
        return false;
    }

    // register av codecs
    av_register_all();

    output()->PauseUntilBuffered();

    m_inputIsFile = !input()->isSequential();

    // open device
    if (m_inputIsFile)
    {
        filename = ((QFile *)input())->fileName();
        VERBOSE(VB_PLAYBACK, QString("avfDecoder: playing file %1").arg(filename));
    }
    else
    {
        // if the input is not a file then setup the buffer
        // and iocontext to stream from it
        m_buffer = (unsigned char*) av_malloc(BUFFER_SIZE + FF_INPUT_BUFFER_PADDING_SIZE);
        m_byteIOContext = new ByteIOContext;
        init_put_byte(m_byteIOContext, m_buffer, BUFFER_SIZE, 0, input(), &ReadFunc, &WriteFunc, &SeekFunc);
        filename = "stream";

        m_byteIOContext->is_streamed = 1;

        // probe the stream
        AVProbeData probe_data;
        probe_data.filename = filename;
        probe_data.buf_size = min(BUFFER_SIZE, (int) input()->bytesAvailable());
        probe_data.buf = m_buffer;
        input()->read((char*)probe_data.buf, probe_data.buf_size);
        m_inputFormat = av_probe_input_format(&probe_data, 1);

        if (!m_inputFormat)
        {
            error("Could not identify the stream type in "
                  "avfDecoder::initialize"); 
            deinit();
            return false;
        }

        VERBOSE(VB_PLAYBACK, QString("avfDecoder: playing stream, format probed is: %1").arg(m_inputFormat->long_name));
    }

    if (!m_samples)
    {
        m_samples = (int16_t *)av_mallocz(AVCODEC_MAX_AUDIO_FRAME_SIZE *
                                          sizeof(int32_t));
        if (!m_samples)
        {
            error("Could not allocate output buffer in "
                  "avfDecoder::initialize");

            deinit();
            return false;
        }
    }

    // open the media file
    // this should populate the input context
    int err;
    if (m_inputIsFile)
        err = av_open_input_file(&m_inputContext, filename.toLocal8Bit().constData(),
                                    m_inputFormat, 0, &m_params);
    else
        err = av_open_input_stream(&m_inputContext, m_byteIOContext, "decoder",
                                      m_inputFormat, &m_params);

    if (err < 0)
    {
        VERBOSE(VB_IMPORTANT, QString("Could not open file (%1)").arg(filename));
        VERBOSE(VB_IMPORTANT, QString("AV decoder. Error: %1").arg(err));
        error(QString("Could not open file  (%1)").arg(filename) +
              QString("\nAV decoder. Error: %1").arg(err));
        deinit();
        return false;
    }

    // determine the stream format
    // this also populates information needed for metadata
    if (av_find_stream_info(m_inputContext) < 0)
    {
        error("Could not determine the stream format.");
        deinit();
        return false;
    }

    // Store the audio codec of the stream
    m_audioDec = m_inputContext->streams[0]->codec;

    // Store the input format of the context
    m_inputFormat = m_inputContext->iformat;

    // Prepare the decoding codec
    // The format is different than the codec
    // While we could get fed a WAV file, it could contain a number
    // of different codecs
    m_codec = avcodec_find_decoder(m_audioDec->codec_id);
    if (!m_codec)
    {
        error(QString("Could not find audio codec: %1")
                              .arg(m_audioDec->codec_id));
        deinit();
        return false;
    }

    if (avcodec_open(m_audioDec,m_codec) < 0)
    {
        error(QString("Could not open audio codec: %1").arg(m_audioDec->codec_id));
        deinit();
        return false;
    }

    if (AV_TIME_BASE > 0)
        totalTime = (m_inputContext->duration / AV_TIME_BASE) * 1000;

    freq = m_audioDec->sample_rate;
    m_channels = m_audioDec->channels;

    if (m_channels <= 0)
    {
        error(QString("AVCodecContext tells us %1 channels are "
                                      "available, this is bad, bailing.")
                                      .arg(m_channels));
        deinit();
        return false;
    }

    switch (m_audioDec->sample_fmt)
    {
        case SAMPLE_FMT_U8:     m_sampleFmt = FORMAT_U8;    break;
        case SAMPLE_FMT_S16:    m_sampleFmt = FORMAT_S16;   break;
        case SAMPLE_FMT_FLT:    m_sampleFmt = FORMAT_FLT;   break;
        case SAMPLE_FMT_DBL:    m_sampleFmt = FORMAT_NONE;  break;
        case SAMPLE_FMT_S32:
            switch (m_audioDec->bits_per_raw_sample)
            {
                case  0:    m_sampleFmt = FORMAT_S32;   break;
                case 24:    m_sampleFmt = FORMAT_S24;   break;
                case 32:    m_sampleFmt = FORMAT_S32;   break;
                default:    m_sampleFmt = FORMAT_NONE;
            }
            break;
        default:                m_sampleFmt = FORMAT_NONE;
    }

    if (m_sampleFmt == FORMAT_NONE)
    {
        int bps =
            av_get_bits_per_sample_fmt(m_audioDec->sample_fmt);
        if (m_audioDec->sample_fmt == SAMPLE_FMT_S32 &&
            m_audioDec->bits_per_raw_sample)
        {
            bps = m_audioDec->bits_per_raw_sample;
        }
        error(QString("Error: Unsupported sample format "
                      "with %1 bits").arg(bps));
        return false;
    }

    const AudioSettings settings(m_sampleFmt, m_audioDec->channels,
                                    m_audioDec->codec_id,
                                    m_audioDec->sample_rate, false);

    output()->Reconfigure(settings);
    output()->SetSourceBitrate(m_audioDec->bit_rate);

    // 20ms worth
    bks = (freq * m_channels *
           AudioOutputSettings::SampleSize(m_sampleFmt)) / 50;

    bksFrames = freq / 50;

    // decode 8 bks worth of samples each time we need more
    decodeBytes = bks << 3;

    output_buf = (char *)av_malloc(decodeBytes +
                                   AVCODEC_MAX_AUDIO_FRAME_SIZE * 2);
    output_at = 0;

    inited = true;
    return true;
}

void avfDecoder::seek(double pos)
{
    if (m_inputIsFile)
        seekTime = pos;
}

void avfDecoder::deinit()
{
    inited = user_stop = finish = FALSE;
    freq = bitrate = 0;
    stat = m_channels = 0;
    m_sampleFmt = FORMAT_NONE;
    setInput(0);
    setOutput(0);

    // Cleanup here
    if (m_inputContext)
    {
        if (m_inputIsFile)
            av_close_input_file(m_inputContext);
        else
            av_close_input_stream(m_inputContext);
        m_inputContext = NULL;
    }

    if (output_buf)
        av_free(output_buf);
    output_buf = NULL;

    m_decStream = NULL;
    m_codec = NULL;
    m_audioDec = NULL;
    m_inputFormat = NULL;

    if (m_buffer)
    {
        av_free(m_buffer);
        m_buffer = NULL;
    }

    if (m_byteIOContext)
    {
        delete m_byteIOContext;
        m_byteIOContext = NULL;
    }
}

void avfDecoder::run()
{
    if (!inited)
        return;

    threadRegister("avfDecoder");
    AVPacket pkt, tmp_pkt;
    char *s;
    int data_size, dec_len;
    uint fill, total;
    // account for possible frame expansion in aobase (upmix, float conv)
    uint thresh = bks * 12 / AudioOutputSettings::SampleSize(m_sampleFmt);

    stat = DecoderEvent::Decoding;
    {
        DecoderEvent e((DecoderEvent::Type) stat);
        dispatch(e);
    }

    av_read_play(m_inputContext);

    while (!finish && !user_stop)
    {
        // Look to see if user has requested a seek
        if (seekTime >= 0.0)
        {
            VERBOSE(VB_GENERAL, QString("avfdecoder.o: seek time %1")
                                .arg(seekTime));

            if (av_seek_frame(m_inputContext, -1,
                              (int64_t)(seekTime * AV_TIME_BASE), 0) < 0)
                VERBOSE(VB_IMPORTANT, "Error seeking");

            seekTime = -1.0;
        }

        // Do we need to decode more samples?
        if (output_at < bks)
        {
            while (output_at < decodeBytes &&
                   !finish && !user_stop && seekTime <= 0.0)
            {
                // Read a packet from the input context
                if (av_read_frame(m_inputContext, &pkt) < 0)
                {
                    VERBOSE(VB_IMPORTANT, "Read frame failed");
                    VERBOSE(VB_FILE, ("... for file '" + filename) + "'");
                    finish = TRUE;
                    break;
                }

		tmp_pkt.data = pkt.data;
		tmp_pkt.size = pkt.size;

                while (tmp_pkt.size > 0 && !finish &&
		       !user_stop && seekTime <= 0.0)
                {
                    // Decode the stream to the output codec
                    // m_samples is the output buffer
                    // data_size is the size in bytes of the frame
                    // tmp_pkt is the input buffer
                    data_size = AVCODEC_MAX_AUDIO_FRAME_SIZE;
                    dec_len = avcodec_decode_audio3(m_audioDec, m_samples,
                                                    &data_size, &tmp_pkt);
                    if (dec_len < 0)
                        break;

                    s = (char *)m_samples;
                    // Copy the data to the output buffer
                    memcpy((char *)(output_buf + output_at), s, data_size);

                    // Increment the output pointer and count
                    output_at += data_size;
                    tmp_pkt.size -= dec_len;
                    tmp_pkt.data += dec_len;
                }

                av_free_packet(&pkt);
            }
        }

        if (!output())
            continue;

        // Wait until we need to decode or supply more samples
        while (!finish && !user_stop && seekTime <= 0.0)
        {
            output()->GetBufferStatus(fill, total);
            // Make sure we have decoded samples ready and that the
            // audiobuffer is reasonably populated
            if (fill < thresh<<3)
                break;
            else
                // Wait for half of the buffer to drain
                usleep(output()->GetAudioBufferedTime()<<9);
        }

        // write a block if there's sufficient space for it
        if (!user_stop && output_at >= bks && fill <= total - thresh)
            writeBlock();
    }

    if (user_stop)
        inited = FALSE;

    else if (output())
    {
        // Drain our buffer
        while (output_at >= bks)
            writeBlock();

        // Drain ao buffer
        output()->Drain();
    }

    if (finish)
        stat = DecoderEvent::Finished;
    else if (user_stop)
        stat = DecoderEvent::Stopped;

    {
        DecoderEvent e((DecoderEvent::Type) stat);
        dispatch(e);
    }

    deinit();
    threadDeregister();
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

