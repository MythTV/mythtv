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
#include <audiooutpututil.h>
#include <mythlogging.h>
#include <decoderhandler.h>

using namespace std;

// Mythmusic Headers
#include "avfdecoder.h"
#include "metaioavfcomment.h"
#include "metaioid3.h"
#include "metaioflacvorbis.h"
#include "metaiooggvorbis.h"
#include "metaiomp4.h"
#include "metaiowavpack.h"
#include "decoderhandler.h"

extern "C" {
#include "libavformat/avio.h"
}

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
    QIODevice *io = (QIODevice*)opaque;

    if (whence == AVSEEK_SIZE)
    {
        return io->size();
    }
    else if (whence == SEEK_SET)
    {
        if (offset <= io->size())
            return io->seek(offset);
        else
            return -1;
    }
    else if (whence == SEEK_END)
    {
        int64_t newPos = io->size() + offset;
        if (newPos >= 0 && newPos <= io->size())
            return io->seek(newPos);
        else
            return -1;
    }
    else if (whence == SEEK_CUR)
    {
        int64_t newPos = io->pos() + offset;
        if (newPos >= 0 && newPos < io->size())
            return io->seek(newPos);
        else
            return -1;
    }
    else
        return -1;

     return -1;
}

static void myth_av_log(void *ptr, int level, const char* fmt, va_list vl)
{
    if (VERBOSE_LEVEL_NONE)
        return;

    static QString full_line("");
    static const int msg_len = 255;
    static QMutex string_lock;
    uint64_t   verbose_mask  = VB_GENERAL;
    LogLevel_t verbose_level = LOG_DEBUG;

    // determine mythtv debug level from av log level
    switch (level)
    {
        case AV_LOG_PANIC:
            verbose_level = LOG_EMERG;
            break;
        case AV_LOG_FATAL:
            verbose_level = LOG_CRIT;
            break;
        case AV_LOG_ERROR:
            verbose_level = LOG_ERR;
            verbose_mask |= VB_LIBAV;
            break;
        case AV_LOG_DEBUG:
        case AV_LOG_VERBOSE:
        case AV_LOG_INFO:
            verbose_level = LOG_DEBUG;
            verbose_mask |= VB_LIBAV;
            break;
        case AV_LOG_WARNING:
            verbose_mask |= VB_LIBAV;
            break;
        default:
            return;
    }

    if (!VERBOSE_LEVEL_CHECK(verbose_mask, verbose_level))
        return;

    string_lock.lock();
    if (full_line.isEmpty() && ptr) {
        AVClass* avc = *(AVClass**)ptr;
        full_line.sprintf("[%s @ %p] ", avc->item_name(ptr), avc);
    }

    char str[msg_len+1];
    int bytes = vsnprintf(str, msg_len+1, fmt, vl);

    // check for truncated messages and fix them
    if (bytes > msg_len)
    {
        LOG(VB_GENERAL, LOG_WARNING,
            QString("Libav log output truncated %1 of %2 bytes written")
                .arg(msg_len).arg(bytes));
        str[msg_len-1] = '\n';
    }

    full_line += QString(str);
    if (full_line.endsWith("\n"))
    {
        LOG(verbose_mask, verbose_level, full_line.trimmed());
        full_line.truncate(0);
    }
    string_lock.unlock();
}

avfDecoder::avfDecoder(const QString &file, DecoderFactory *d, AudioOutput *o) :
    Decoder(d, o),
    m_inited(false),              m_userStop(false),
    m_stat(0),                    m_finish(false),
    m_freq(0),                    m_bitrate(0),
    m_channels(0),
    m_seekTime(-1.0),             m_devicename(""),
    m_inputFormat(NULL),          m_inputContext(NULL),
    m_audioDec(NULL),             m_inputIsFile(false),
    m_buffer(NULL),               m_byteIOContext(NULL),
    m_errCode(0)
{
    setObjectName("avfDecoder");
    setFilename(file);

    m_outputBuffer =
        (uint8_t *)av_malloc(AVCODEC_MAX_AUDIO_FRAME_SIZE * sizeof(int32_t));

    bool debug = VERBOSE_LEVEL_CHECK(VB_LIBAV, LOG_ANY);
    av_log_set_level((debug) ? AV_LOG_DEBUG : AV_LOG_ERROR);
    av_log_set_callback(myth_av_log);
}

avfDecoder::~avfDecoder(void)
{
    if (m_inited)
        deinit();
    if (m_outputBuffer)
        av_freep(&m_outputBuffer);
}

void avfDecoder::stop()
{
    m_userStop = true;
}

bool avfDecoder::initialize()
{
    m_inited = m_userStop = m_finish = false;
    m_freq = m_bitrate = 0;
    m_stat = m_channels = 0;
    m_seekTime = -1.0;

    // give up if we dont have an audiooutput set
    if (!output())
    {
        error("avfDecoder: initialise called with a NULL audiooutput");
        return false;
    }

    if (!m_outputBuffer)
    {
        error("avfDecoder: couldn't allocate memory");
        return false;
    }

    // register av codecs
    av_register_all();

    output()->PauseUntilBuffered();

    m_inputIsFile = (dynamic_cast<QFile*>(input()) != NULL);

    if (m_inputContext)
        avformat_free_context(m_inputContext);

    m_inputContext = avformat_alloc_context();

    // open device
    if (m_inputIsFile)
    {
        filename = ((QFile *)input())->fileName();
        LOG(VB_PLAYBACK, LOG_INFO,
            QString("avfDecoder: playing file %1").arg(filename));
    }
    else
    {
        // if the input is not a file then setup the buffer
        // and iocontext to stream from it
        m_buffer = (unsigned char*) av_malloc(BUFFER_SIZE + FF_INPUT_BUFFER_PADDING_SIZE);
        m_byteIOContext = avio_alloc_context(m_buffer, BUFFER_SIZE, 0, input(),
                                             &ReadFunc, &WriteFunc, &SeekFunc);
        filename = "stream";

        // we can only seek in files streamed using MusicSGIODevice
        m_byteIOContext->seekable = (dynamic_cast<MusicSGIODevice*>(input()) != NULL) ? 1 : 0;

        // probe the stream
        AVProbeData probe_data;
        probe_data.filename = filename.toLocal8Bit().constData();
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

        LOG(VB_PLAYBACK, LOG_INFO,
            QString("avfDecoder: playing stream, format probed is: %1")
                .arg(m_inputFormat->long_name));
    }

    // open the media file
    // this should populate the input context
    int err;
    if (m_inputIsFile)
    {
        err = avformat_open_input(&m_inputContext,
                                  filename.toLocal8Bit().constData(),
                                  m_inputFormat, NULL);
    }
    else
    {
        m_inputContext->pb = m_byteIOContext;
        err = avformat_open_input(&m_inputContext, "decoder",
                                  m_inputFormat, NULL);
    }

    if (err < 0)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("Could not open file (%1)").arg(filename));
        LOG(VB_GENERAL, LOG_ERR, QString("AV decoder. Error: %1").arg(err));
        error(QString("Could not open file  (%1)").arg(filename) +
              QString("\nAV decoder. Error: %1").arg(err));
        deinit();
        return false;
    }

    // determine the stream format
    // this also populates information needed for metadata
    if (avformat_find_stream_info(m_inputContext, NULL) < 0)
    {
        error("Could not determine the stream format.");
        deinit();
        return false;
    }

    // let FFmpeg finds the best audio stream (should only be one), also catter
    // should the file/stream not be an audio one
    AVCodec *codec;
    int selTrack = av_find_best_stream(m_inputContext, AVMEDIA_TYPE_AUDIO,
                                       -1, -1, &codec, 0);

    if (selTrack < 0)
    {
        error(QString("Could not find audio stream."));
        deinit();
        return false;
    }

    // Store the audio codec of the stream
    m_audioDec = m_inputContext->streams[selTrack]->codec;

    // Store the input format of the context
    m_inputFormat = m_inputContext->iformat;

    if (avcodec_open2(m_audioDec, codec, NULL) < 0)
    {
        error(QString("Could not open audio codec: %1")
            .arg(m_audioDec->codec_id));
        deinit();
        return false;
    }

    m_freq = m_audioDec->sample_rate;
    m_channels = m_audioDec->channels;

    if (m_channels <= 0)
    {
        error(QString("AVCodecContext tells us %1 channels are "
                                      "available, this is bad, bailing.")
                                      .arg(m_channels));
        deinit();
        return false;
    }

    AudioFormat format =
        AudioOutputSettings::AVSampleFormatToFormat(m_audioDec->sample_fmt,
                                                    m_audioDec->bits_per_raw_sample);
    if (format == FORMAT_NONE)
    {
        error(QString("Error: Unsupported sample format: %1")
              .arg(av_get_sample_fmt_name(m_audioDec->sample_fmt)));
        deinit();
        return false;
    }

    const AudioSettings settings(format, m_audioDec->channels,
                                 m_audioDec->codec_id,
                                 m_audioDec->sample_rate, false);

    output()->Reconfigure(settings);
    output()->SetSourceBitrate(m_audioDec->bit_rate);

    m_inited = true;
    return true;
}

void avfDecoder::seek(double pos)
{
    if (m_inputIsFile || (m_byteIOContext && m_byteIOContext->seekable))
        m_seekTime = pos;
}

void avfDecoder::deinit()
{
    m_inited = m_userStop = m_finish = false;
    m_freq = m_bitrate = 0;
    m_stat = m_channels = 0;
    setOutput(0);

    // Cleanup here
    if (m_inputContext)
    {
        for (uint i = 0; i < m_inputContext->nb_streams; i++)
        {
            AVStream *st = m_inputContext->streams[i];
            if (st->codec && st->codec->codec)
                avcodec_close(st->codec);
        }
        avformat_close_input(&m_inputContext);
    }

    m_audioDec = NULL;
    m_inputFormat = NULL;

    if (m_buffer)
    {
        av_freep(&m_buffer);
    }

    if (m_byteIOContext)
    {
        delete m_byteIOContext;
        m_byteIOContext = NULL;
    }
}

void avfDecoder::run()
{
    RunProlog();
    if (!m_inited)
    {
        RunEpilog();
        return;
    }

    AVPacket pkt, tmp_pkt;
    memset(&pkt, 0, sizeof(AVPacket));
    av_init_packet(&pkt);

    m_stat = DecoderEvent::Decoding;
    {
        DecoderEvent e((DecoderEvent::Type) m_stat);
        dispatch(e);
    }

    av_read_play(m_inputContext);

    while (!m_finish && !m_userStop)
    {
        // Look to see if user has requested a seek
        if (m_seekTime >= 0.0)
        {
            LOG(VB_GENERAL, LOG_INFO, QString("avfdecoder.o: seek time %1")
                    .arg(m_seekTime));

            if (av_seek_frame(m_inputContext, -1,
                              (int64_t)(m_seekTime * AV_TIME_BASE), 0) < 0)
                LOG(VB_GENERAL, LOG_ERR, "Error seeking");

            m_seekTime = -1.0;
        }

        while (!m_finish && !m_userStop && m_seekTime <= 0.0)
        {
            // Read a packet from the input context
            int res = av_read_frame(m_inputContext, &pkt);
            if (res < 0)
            {
                if (res != AVERROR_EOF)
                {
                    LOG(VB_GENERAL, LOG_ERR, QString("Read frame failed: %1").arg(res));
                    LOG(VB_FILE, LOG_ERR, ("... for file '" + filename) + "'");
                }

                m_finish = true;
                break;
            }

            av_init_packet(&tmp_pkt);
            tmp_pkt.data = pkt.data;
            tmp_pkt.size = pkt.size;

            while (tmp_pkt.size > 0 && !m_finish &&
                   !m_userStop && m_seekTime <= 0.0)
            {
                int data_size = 0;

                int ret = output()->DecodeAudio(m_audioDec,
                                                m_outputBuffer,
                                                data_size,
                                                &tmp_pkt);

                if (ret < 0)
                    break;

                // Increment the output pointer and count
                tmp_pkt.size -= ret;
                tmp_pkt.data += ret;

                if (data_size <= 0)
                    continue;

                output()->AddData(m_outputBuffer, data_size, -1, 0);
            }

            av_free_packet(&pkt);

            // Wait until we need to decode or supply more samples
            while (!m_finish && !m_userStop && m_seekTime <= 0.0)
            {
                int64_t buffered = output()->GetAudioBufferedTime();
                // never go below 1s buffered
                if (buffered < 1000)
                    break;
                else
                {
                    // wait
                    usleep((buffered - 1000) * 1000);
                }
            }
        }
    }

    if (m_userStop)
    {
        m_inited = false;
    }
    else
    {
        // Drain ao buffer, making sure we play all remaining audio samples
        output()->Drain();
    }

    if (m_finish)
        m_stat = DecoderEvent::Finished;
    else if (m_userStop)
        m_stat = DecoderEvent::Stopped;

    {
        DecoderEvent e((DecoderEvent::Type) m_stat);
        dispatch(e);
    }

    deinit();
    RunEpilog();
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

Decoder *avfDecoderFactory::create(const QString &file, AudioOutput *output, bool deletable)
{
    if (deletable)
        return new avfDecoder(file, this, output);

    static avfDecoder *decoder = 0;
    if (!decoder)
    {
        decoder = new avfDecoder(file, this, output);
    }
    else
    {
        decoder->setOutput(output);
    }

    return decoder;
}
