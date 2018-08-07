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
#include <QTimer>

// Myth headers
#include <mythconfig.h>
#include <mythcontext.h>
#include <audiooutput.h>
#include <audiooutpututil.h>
#include <mythlogging.h>
#include <decoderhandler.h>
#include <mythavutil.h>

using namespace std;

// Mythmusic Headers
#include "avfdecoder.h"
#include "metaio.h"
#include "metaioavfcomment.h"
#include "metaioid3.h"
#include "metaioflacvorbis.h"
#include "metaiooggvorbis.h"
#include "metaiomp4.h"
#include "metaiowavpack.h"
#include "decoderhandler.h"
#include "musicplayer.h"

extern "C" {
#include "libavformat/avio.h"
#include "libavutil/opt.h"
}

/****************************************************************************/

typedef QMap<QString,QString> ShoutCastMetaMap;

class ShoutCastMetaParser
{
  public:
    ShoutCastMetaParser(void) :
        m_meta_artist_pos(-1), m_meta_title_pos(-1), m_meta_album_pos(-1) { }
    ~ShoutCastMetaParser(void) { }

    void setMetaFormat(const QString &metaformat);
    ShoutCastMetaMap parseMeta(const QString &meta);

  private:
    QString m_meta_format;
    int m_meta_artist_pos;
    int m_meta_title_pos;
    int m_meta_album_pos;
};

void ShoutCastMetaParser::setMetaFormat(const QString &metaformat)
{
/*
  We support these metatags :
  %a - artist
  %t - track
  %b - album
  %r - random bytes
 */
    m_meta_format = metaformat;

    m_meta_artist_pos = 0;
    m_meta_title_pos = 0;
    m_meta_album_pos = 0;

    int assign_index = 1;
    int pos = 0;

    pos = m_meta_format.indexOf("%", pos);
    while (pos >= 0)
    {
        pos++;

        QChar ch;

        if (pos < m_meta_format.length())
            ch = m_meta_format.at(pos);

        if (!ch.isNull() && ch == '%')
        {
            pos++;
        }
        else if (!ch.isNull() && (ch == 'r' || ch == 'a' || ch == 'b' || ch == 't'))
        {
            if (ch == 'a')
                m_meta_artist_pos = assign_index;

            if (ch == 'b')
                m_meta_album_pos = assign_index;

            if (ch == 't')
                m_meta_title_pos = assign_index;

            assign_index++;
        }
        else
            LOG(VB_GENERAL, LOG_ERR,
                QString("ShoutCastMetaParser: malformed metaformat '%1'")
                    .arg(m_meta_format));

        pos = m_meta_format.indexOf("%", pos);
    }

    m_meta_format.replace("%a", "(.*)");
    m_meta_format.replace("%t", "(.*)");
    m_meta_format.replace("%b", "(.*)");
    m_meta_format.replace("%r", "(.*)");
    m_meta_format.replace("%%", "%");
}

ShoutCastMetaMap ShoutCastMetaParser::parseMeta(const QString &mdata)
{
    ShoutCastMetaMap result;
    int title_begin_pos = mdata.indexOf("StreamTitle='");
    int title_end_pos;

    if (title_begin_pos >= 0)
    {
        title_begin_pos += 13;
        title_end_pos = mdata.indexOf("';", title_begin_pos);
        QString title = mdata.mid(title_begin_pos, title_end_pos - title_begin_pos);
        QRegExp rx;
        rx.setPattern(m_meta_format);
        if (rx.indexIn(title) != -1)
        {
            LOG(VB_PLAYBACK, LOG_INFO, QString("ShoutCast: Meta     : '%1'")
                    .arg(mdata));
            LOG(VB_PLAYBACK, LOG_INFO,
                QString("ShoutCast: Parsed as: '%1' by '%2'")
                    .arg(rx.cap(m_meta_title_pos))
                    .arg(rx.cap(m_meta_artist_pos)));

            if (m_meta_title_pos > 0)
                result["title"] = rx.cap(m_meta_title_pos);

            if (m_meta_artist_pos > 0)
                result["artist"] = rx.cap(m_meta_artist_pos);

            if (m_meta_album_pos > 0)
                result["album"] = rx.cap(m_meta_album_pos);
        }
    }

    return result;
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
    m_mdataTimer(NULL),           m_errCode(0)
{
    MThread::setObjectName("avfDecoder");
    setURL(file);

    m_outputBuffer =
        (uint8_t *)av_malloc(AudioOutput::MAX_SIZE_BUFFER);

    bool debug = VERBOSE_LEVEL_CHECK(VB_LIBAV, LOG_ANY);
    av_log_set_level((debug) ? AV_LOG_DEBUG : AV_LOG_ERROR);
    av_log_set_callback(myth_av_log);
}

avfDecoder::~avfDecoder(void)
{
    if (m_mdataTimer)
        delete m_mdataTimer;

    if (m_inited)
        deinit();

    if (m_outputBuffer)
        av_freep(&m_outputBuffer);

    if (m_inputContext)
        delete m_inputContext;
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

    output()->PauseUntilBuffered();

    if (m_inputContext)
        delete m_inputContext;

    m_inputContext = new RemoteAVFormatContext(getURL());

    if (!m_inputContext->isOpen())
    {
        error(QString("Could not open url  (%1)").arg(m_url));
        deinit();
        return false;
    }

    // if this is a ice/shoutcast or MMS stream start polling for metadata changes and buffer status
    if (getURL().startsWith("http://") || getURL().startsWith("mmsh://"))
    {
        m_mdataTimer = new QTimer;
        m_mdataTimer->setSingleShot(false);
        connect(m_mdataTimer, SIGNAL(timeout()), this, SLOT(checkMetatdata()));

        m_mdataTimer->start(500);

        // we don't get metadata updates for MMS streams so grab the metadata from the headers
        if (getURL().startsWith("mmsh://"))
        {
            AVDictionaryEntry *tag = NULL;
            MusicMetadata mdata =  gPlayer->getDecoderHandler()->getMetadata();

            tag = av_dict_get(m_inputContext->getContext()->metadata, "title", tag, AV_DICT_IGNORE_SUFFIX);
            mdata.setTitle(tag->value);

            tag = av_dict_get(m_inputContext->getContext()->metadata, "artist", tag, AV_DICT_IGNORE_SUFFIX);
            mdata.setArtist(tag->value);

            mdata.setAlbum("");
            mdata.setLength(-1);

            DecoderHandlerEvent ev(DecoderHandlerEvent::Meta, mdata);
            dispatch(ev);
        }
    }

    // determine the stream format
    // this also populates information needed for metadata
    if (avformat_find_stream_info(m_inputContext->getContext(), NULL) < 0)
    {
        error("Could not determine the stream format.");
        deinit();
        return false;
    }

    // let FFmpeg finds the best audio stream (should only be one), also catter
    // should the file/stream not be an audio one
    AVCodec *codec;
    int selTrack = av_find_best_stream(m_inputContext->getContext(), AVMEDIA_TYPE_AUDIO,
                                       -1, -1, &codec, 0);

    if (selTrack < 0)
    {
        error(QString("Could not find audio stream."));
        deinit();
        return false;
    }

    // Store the audio codec of the stream
    m_audioDec = gCodecMap->getCodecContext
        (m_inputContext->getContext()->streams[selTrack]);

    // Store the input format of the context
    m_inputFormat = m_inputContext->getContext()->iformat;

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
    if (m_inputContext->getContext() && m_inputContext->getContext()->pb &&
        m_inputContext->getContext()->pb->seekable)
    {
        m_seekTime = pos;
    }
}

void avfDecoder::deinit()
{
    m_inited = m_userStop = m_finish = false;
    m_freq = m_bitrate = 0;
    m_stat = m_channels = 0;
    setOutput(0);

    // Cleanup here
    if (m_inputContext && m_inputContext->getContext())
    {
        for (uint i = 0; i < m_inputContext->getContext()->nb_streams; i++)
        {
            AVStream *st = m_inputContext->getContext()->streams[i];
            gCodecMap->freeCodecContext(st);
        }
    }

    m_audioDec = NULL;
    m_inputFormat = NULL;
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

    av_read_play(m_inputContext->getContext());

    while (!m_finish && !m_userStop)
    {
        // Look to see if user has requested a seek
        if (m_seekTime >= 0.0)
        {
            LOG(VB_GENERAL, LOG_INFO, QString("avfdecoder.o: seek time %1")
                    .arg(m_seekTime));

            if (av_seek_frame(m_inputContext->getContext(), -1,
                              (int64_t)(m_seekTime * AV_TIME_BASE), 0) < 0)
                LOG(VB_GENERAL, LOG_ERR, "Error seeking");

            m_seekTime = -1.0;
        }

        while (!m_finish && !m_userStop && m_seekTime <= 0.0)
        {
            // Read a packet from the input context
            int res = av_read_frame(m_inputContext->getContext(), &pkt);
            if (res < 0)
            {
                if (res != AVERROR_EOF)
                {
                    LOG(VB_GENERAL, LOG_ERR, QString("Read frame failed: %1").arg(res));
                    LOG(VB_FILE, LOG_ERR, ("... for file '" + m_url) + "'");
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

            av_packet_unref(&pkt);

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

void avfDecoder::checkMetatdata(void)
{
    uint8_t *mdata = NULL;

    if (av_opt_get(m_inputContext->getContext(), "icy_metadata_packet", AV_OPT_SEARCH_CHILDREN, &mdata) >= 0)
    {
        QString s = QString::fromUtf8((const char*) mdata);

        if (m_lastMetadata != s)
        {
            m_lastMetadata = s;

            LOG(VB_PLAYBACK, LOG_INFO, QString("avfDecoder: shoutcast metadata changed - %1").arg(m_lastMetadata));

            ShoutCastMetaParser parser;
            parser.setMetaFormat(gPlayer->getDecoderHandler()->getMetadata().MetadataFormat());

            ShoutCastMetaMap meta_map = parser.parseMeta(m_lastMetadata);

            MusicMetadata mdata =  gPlayer->getDecoderHandler()->getMetadata();
            mdata.setTitle(meta_map["title"]);
            mdata.setArtist(meta_map["artist"]);
            mdata.setAlbum(meta_map["album"]);
            mdata.setLength(-1);

            DecoderHandlerEvent ev(DecoderHandlerEvent::Meta, mdata);
            dispatch(ev);
        }

        av_free(mdata);
    }

    if (m_inputContext->getContext()->pb)
    {
        int available = (int) (m_inputContext->getContext()->pb->buf_end - m_inputContext->getContext()->pb->buffer);
        int maxSize = m_inputContext->getContext()->pb->buffer_size;
        DecoderHandlerEvent ev(DecoderHandlerEvent::BufferStatus, available, maxSize);
        dispatch(ev);
    }
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
    return MetaIO::ValidFileExtensions;
}

const QString &avfDecoderFactory::description() const
{
    static QString desc(tr("Internal Decoder"));
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
