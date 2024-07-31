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

// C++ headers
#include <chrono>

// QT headers
#include <QFile>
#include <QIODevice>
#include <QObject>
#include <QRegularExpression>
#include <QTimer>

// MythTV headers
#include <mythconfig.h>
#include <libmyth/audio/audiooutput.h>
#include <libmyth/audio/audiooutpututil.h>
#include <libmyth/mythcontext.h>
#include <libmythbase/mythlogging.h>
#include <libmythmetadata/metaio.h>
#include <libmythmetadata/metaioavfcomment.h>
#include <libmythmetadata/metaioflacvorbis.h>
#include <libmythmetadata/metaioid3.h>
#include <libmythmetadata/metaiomp4.h>
#include <libmythmetadata/metaiooggvorbis.h>
#include <libmythmetadata/metaiowavpack.h>
#include <libmythtv/mythavutil.h>

// Mythmusic Headers
#include "avfdecoder.h"
#include "decoderhandler.h"
#include "musicplayer.h"

extern "C" {
    #include <libavformat/avio.h>
    #include <libavutil/opt.h>
}

/****************************************************************************/

using ShoutCastMetaMap = QMap<QString,QString>;

class ShoutCastMetaParser
{
  public:
    ShoutCastMetaParser(void) = default;
    ~ShoutCastMetaParser(void) = default;

    void setMetaFormat(const QString &metaformat);
    ShoutCastMetaMap parseMeta(const QString &mdata);

  private:
    QString m_metaFormat;
    int     m_metaArtistPos {-1};
    int     m_metaTitlePos  {-1};
    int     m_metaAlbumPos  {-1};
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
    m_metaFormat = metaformat;

    m_metaArtistPos = 0;
    m_metaTitlePos  = 0;
    m_metaAlbumPos  = 0;

    int assign_index = 1;
    int pos = 0;

    pos = m_metaFormat.indexOf("%", pos);
    while (pos >= 0)
    {
        pos++;

        QChar ch;

        if (pos < m_metaFormat.length())
            ch = m_metaFormat.at(pos);

        if (!ch.isNull() && ch == '%')
        {
            pos++;
        }
        else if (!ch.isNull() && (ch == 'r' || ch == 'a' || ch == 'b' || ch == 't'))
        {
            if (ch == 'a')
                m_metaArtistPos = assign_index;

            if (ch == 'b')
                m_metaAlbumPos = assign_index;

            if (ch == 't')
                m_metaTitlePos = assign_index;

            assign_index++;
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("ShoutCastMetaParser: malformed metaformat '%1'")
                    .arg(m_metaFormat));
        }

        pos = m_metaFormat.indexOf("%", pos);
    }

    m_metaFormat.replace("%a", "(.*)");
    m_metaFormat.replace("%t", "(.*)");
    m_metaFormat.replace("%b", "(.*)");
    m_metaFormat.replace("%r", "(.*)");
    m_metaFormat.replace("%%", "%");
}

ShoutCastMetaMap ShoutCastMetaParser::parseMeta(const QString &mdata)
{
    ShoutCastMetaMap result;
    int title_begin_pos = mdata.indexOf("StreamTitle='");

    if (title_begin_pos >= 0)
    {
        title_begin_pos += 13;
        int title_end_pos = mdata.indexOf("';", title_begin_pos);
        QString title = mdata.mid(title_begin_pos, title_end_pos - title_begin_pos);
        QRegularExpression rx { m_metaFormat };
        auto match = rx.match(title);
        if (match.hasMatch())
        {
            LOG(VB_PLAYBACK, LOG_DEBUG, QString("ShoutCast: Meta     : '%1'")
                    .arg(mdata));
            LOG(VB_PLAYBACK, LOG_DEBUG,
                QString("ShoutCast: Parsed as: '%1' by '%2' on '%3'")
                .arg(m_metaTitlePos ? match.captured(m_metaTitlePos) : "",
                     m_metaArtistPos ? match.captured(m_metaArtistPos) : "",
                     m_metaAlbumPos ? match.captured(m_metaAlbumPos) : ""));

            if (m_metaTitlePos > 0)
                result["title"] = match.captured(m_metaTitlePos);

            if (m_metaArtistPos > 0)
                result["artist"] = match.captured(m_metaArtistPos);

            if (m_metaAlbumPos > 0)
                result["album"] = match.captured(m_metaAlbumPos);
        }
    }

    return result;
}

static void myth_av_log(void *ptr, int level, const char* fmt, va_list vl)
{
    if (VERBOSE_LEVEL_NONE())
        return;

    static QString   s_fullLine("");
    static QMutex    s_stringLock;
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

    s_stringLock.lock();
    if (s_fullLine.isEmpty() && ptr) {
        AVClass* avc = *(AVClass**)ptr;
        s_fullLine = QString("[%1 @ %2] ")
            .arg(avc->item_name(ptr))
            .arg(reinterpret_cast<size_t>(avc),QT_POINTER_SIZE,8,QChar('0'));
    }

    s_fullLine += QString::vasprintf(fmt, vl);
    if (s_fullLine.endsWith("\n"))
    {
        LOG(verbose_mask, verbose_level, s_fullLine.trimmed());
        s_fullLine.truncate(0);
    }
    s_stringLock.unlock();
}

avfDecoder::avfDecoder(const QString &file, DecoderFactory *d, AudioOutput *o) :
    Decoder(d, o),
    m_outputBuffer((uint8_t *)av_malloc(AudioOutput::kMaxSizeBuffer))
{
    MThread::setObjectName("avfDecoder");
    setURL(file);

    bool debug = VERBOSE_LEVEL_CHECK(VB_LIBAV, LOG_ANY);
    av_log_set_level((debug) ? AV_LOG_DEBUG : AV_LOG_ERROR);
    av_log_set_callback(myth_av_log);
}

avfDecoder::~avfDecoder(void)
{
    delete m_mdataTimer;

    if (m_inited)
        deinit();

    if (m_outputBuffer)
        av_freep(reinterpret_cast<void*>(&m_outputBuffer));

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
        connect(m_mdataTimer, &QTimer::timeout, this, &avfDecoder::checkMetatdata);

        m_mdataTimer->start(500ms);

        // we don't get metadata updates for MMS streams so grab the metadata from the headers
        if (getURL().startsWith("mmsh://"))
        {
            AVDictionaryEntry *tag = nullptr;
            MusicMetadata mdata =  gPlayer->getDecoderHandler()->getMetadata();

            tag = av_dict_get(m_inputContext->getContext()->metadata, "title", tag, AV_DICT_IGNORE_SUFFIX);
            mdata.setTitle(tag->value);

            tag = av_dict_get(m_inputContext->getContext()->metadata, "artist", tag, AV_DICT_IGNORE_SUFFIX);
            mdata.setArtist(tag->value);

            mdata.setAlbum("");
            mdata.setLength(-1ms);

            DecoderHandlerEvent ev(DecoderHandlerEvent::kMeta, mdata);
            dispatch(ev);
        }
    }

    // determine the stream format
    // this also populates information needed for metadata
    if (avformat_find_stream_info(m_inputContext->getContext(), nullptr) < 0)
    {
        error("Could not determine the stream format.");
        deinit();
        return false;
    }

    // let FFmpeg finds the best audio stream (should only be one), also catter
    // should the file/stream not be an audio one
    const AVCodec *codec = nullptr;
    int selTrack = av_find_best_stream(m_inputContext->getContext(), AVMEDIA_TYPE_AUDIO,
                                       -1, -1, &codec, 0);

    if (selTrack < 0)
    {
        error(QString("Could not find audio stream."));
        deinit();
        return false;
    }

    // Store the audio codec of the stream
    m_audioDec = m_codecMap.GetCodecContext
        (m_inputContext->getContext()->streams[selTrack]);

    // Store the input format of the context
    m_inputFormat = m_inputContext->getContext()->iformat;

    if (avcodec_open2(m_audioDec, codec, nullptr) < 0)
    {
        error(QString("Could not open audio codec: %1")
            .arg(m_audioDec->codec_id));
        deinit();
        return false;
    }

    m_freq = m_audioDec->sample_rate;
    m_channels = m_audioDec->ch_layout.nb_channels;

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

    const AudioSettings settings(format, m_audioDec->ch_layout.nb_channels,
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
    setOutput(nullptr);

    // Cleanup here
    if (m_inputContext && m_inputContext->getContext())
    {
        for (uint i = 0; i < m_inputContext->getContext()->nb_streams; i++)
        {
            AVStream *st = m_inputContext->getContext()->streams[i];
            m_codecMap.FreeCodecContext(st);
        }
    }

    m_audioDec = nullptr;
    m_inputFormat = nullptr;
}

void avfDecoder::run()
{
    RunProlog();
    if (!m_inited)
    {
        RunEpilog();
        return;
    }

    AVPacket *pkt = av_packet_alloc();
    AVPacket *tmp_pkt = av_packet_alloc();
    if ((pkt == nullptr) || (tmp_pkt == nullptr))
    {
        LOG(VB_GENERAL, LOG_ERR, "packet allocation failed");
        return;
    }

    m_stat = DecoderEvent::kDecoding;
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
            // Play all pending and restart buffering, else REW/FFWD
            // takes 1 second per keypress at the "buffered" wait below.
            output()->Drain();  // (see issue #784)
        }

        while (!m_finish && !m_userStop && m_seekTime <= 0.0)
        {
            // Read a packet from the input context
            int res = av_read_frame(m_inputContext->getContext(), pkt);
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

            av_packet_ref(tmp_pkt, pkt);

            while (tmp_pkt->size > 0 && !m_finish &&
                   !m_userStop && m_seekTime <= 0.0)
            {
                int data_size = 0;

                int ret = output()->DecodeAudio(m_audioDec,
                                                m_outputBuffer,
                                                data_size,
                                                tmp_pkt);

                if (ret < 0)
                    break;

                // Increment the output pointer and count
                tmp_pkt->size -= ret;
                tmp_pkt->data += ret;

                if (data_size <= 0)
                    continue;

                output()->AddData(m_outputBuffer, data_size, -1ms, 0);
            }

            av_packet_unref(pkt);

            // Wait until we need to decode or supply more samples
            while (!m_finish && !m_userStop && m_seekTime <= 0.0)
            {
                std::chrono::milliseconds buffered = output()->GetAudioBufferedTime();
                // never go below 1s buffered
                if (buffered < 1s)
                    break;
                // wait
                long count = buffered.count();
                const struct timespec ns {0, (count - 1000) * 1000000};
                nanosleep(&ns, nullptr);
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
        m_stat = DecoderEvent::kFinished;
    else if (m_userStop)
        m_stat = DecoderEvent::kStopped;

    {
        DecoderEvent e((DecoderEvent::Type) m_stat);
        dispatch(e);
    }

    av_packet_free(&pkt);
    av_packet_free(&tmp_pkt);
    deinit();
    RunEpilog();
}

void avfDecoder::checkMetatdata(void)
{
    uint8_t *pdata = nullptr;

    if (av_opt_get(m_inputContext->getContext(), "icy_metadata_packet", AV_OPT_SEARCH_CHILDREN, &pdata) >= 0)
    {
        QString shout = QString::fromUtf8((const char*) pdata);

        if (m_lastMetadata != shout)
        {
            m_lastMetadata = shout;
            ShoutCastMetaParser parser;
            parser.setMetaFormat(gPlayer->getDecoderHandler()->getMetadata().MetadataFormat());
            ShoutCastMetaMap meta_map = parser.parseMeta(shout);

            QString parsed =  meta_map["title"] + "\\" + meta_map["artist"] + "\\" + meta_map["album"];
            if (m_lastMetadataParsed != parsed)
            {
                m_lastMetadataParsed = parsed;

                LOG(VB_PLAYBACK, LOG_INFO, QString("avfDecoder: shoutcast metadata changed - %1").arg(shout));
                LOG(VB_PLAYBACK, LOG_INFO, QString("avfDecoder: new metadata (%1)").arg(parsed));

                MusicMetadata mdata =  gPlayer->getDecoderHandler()->getMetadata();
                mdata.setTitle(meta_map["title"]);
                mdata.setArtist(meta_map["artist"]);
                mdata.setAlbum(meta_map["album"]);
                mdata.setLength(-1ms);

                DecoderHandlerEvent ev(DecoderHandlerEvent::kMeta, mdata);
                dispatch(ev);
            }
        }

        av_free(pdata);
    }

    if (m_inputContext->getContext()->pb)
    {
        int available = (int) (m_inputContext->getContext()->pb->buf_end - m_inputContext->getContext()->pb->buffer);
        int maxSize = m_inputContext->getContext()->pb->buffer_size;
        DecoderHandlerEvent ev(DecoderHandlerEvent::kBufferStatus, available, maxSize);
        dispatch(ev);
    }
}

bool avfDecoderFactory::supports(const QString &source) const
{
    QStringList list = extension().split("|", Qt::SkipEmptyParts);
    return std::any_of(list.cbegin(), list.cend(),
                       [source](const auto& str)
                           { return str == source.right(str.length()).toLower(); } );
}

const QString &avfDecoderFactory::extension() const
{
    return MetaIO::kValidFileExtensions;
}

const QString &avfDecoderFactory::description() const
{
    static QString s_desc(tr("Internal Decoder"));
    return s_desc;
}

Decoder *avfDecoderFactory::create(const QString &file, AudioOutput *output, bool deletable)
{
    if (deletable)
        return new avfDecoder(file, this, output);

    static avfDecoder *s_decoder = nullptr;
    if (!s_decoder)
    {
        s_decoder = new avfDecoder(file, this, output);
    }
    else
    {
        s_decoder->setOutput(output);
    }

    return s_decoder;
}
