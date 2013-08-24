/*
  Shoutcast decoder for MythTV.
  Eskil Heyn Olsen, 2005, distributed under the GPL as part of mythtv.
  Paul Harrison, July 2010, Updated for Qt4
 */

// c/c++
#include <assert.h>
#include <algorithm>

// qt
#include <QApplication>
#include <QRegExp>
#include <QAbstractSocket>
#include <QTimer>

// mythtv
#include <mythcorecontext.h>
#include <mcodecs.h>
#include <mythversion.h>
#include <musicmetadata.h>

// mythmusic
#include "shoutcast.h"


/****************************************************************************/

#define MAX_ALLOWED_HEADER_SIZE 1024 * 4
#define MAX_ALLOWED_META_SIZE 1024 * 100
#define MAX_REDIRECTS 3
#define PREBUFFER_SECS 10
#define ICE_UDP_PORT 6000

/****************************************************************************/

const char* ShoutCastIODevice::stateString (const State &s)
{
#define TO_STRING(a) case a: return #a
    switch (s) {
        TO_STRING (NOT_CONNECTED);
        TO_STRING (RESOLVING);
        TO_STRING (CONNECTING);
        TO_STRING (CANT_RESOLVE);
        TO_STRING (CANT_CONNECT);
        TO_STRING (CONNECTED);
        TO_STRING (WRITING_HEADER);
        TO_STRING (READING_HEADER);
        TO_STRING (PLAYING);
        TO_STRING (STREAMING);
        TO_STRING (STREAMING_META);
        TO_STRING (STOPPED);
    default:
        return "unknown state";
    }
#undef TO_STRING
}

/****************************************************************************/

class ShoutCastRequest
{
  public:
    ShoutCastRequest(void) { }
    ShoutCastRequest(const QUrl &url) { setUrl(url); }
    ~ShoutCastRequest(void) { }
    const char *data(void) { return m_data.data(); }
    uint size(void) { return m_data.size(); }

  private:
    void setUrl(const QUrl &url)
    {
        QString hdr;
        hdr = QString("GET %PATH% HTTP/1.1\r\n"
                      "Host: %HOST%\r\n"
                      "User-Agent: MythMusic/%VERSION%\r\n"
                      "Accept: */*\r\n");

        QString path = url.path();
        QString host = url.host();

        if (path.isEmpty())
            path = "/";

        if (url.hasQuery())
            path += '?' + url.encodedQuery();

        if (url.port() != -1)
            host += QString(":%1").arg(url.port());

        hdr.replace("%PATH%", path);
        hdr.replace("%HOST%", host);
        hdr.replace("%VERSION%", MYTH_BINARY_VERSION);

        if (!url.userName().isEmpty() && !url.password().isEmpty()) 
        {
            QString authstring = url.userName() + ":" + url.password();
            QString auth = QCodecs::base64Encode(authstring.toLocal8Bit());

            hdr += "Authorization: Basic " + auth + "\r\n";
        }

        hdr += QString("TE: trailers\r\n"
                       "Icy-Metadata: 1\r\n"
                       "\r\n");

        LOG(VB_NETWORK, LOG_INFO, QString("ShoutCastRequest: '%1'").arg(hdr));

        m_data = hdr.toAscii();
    }

    QByteArray m_data;
};

class IceCastRequest
{
  public:
    IceCastRequest(void) { }
    IceCastRequest(const QUrl &url) { setUrl(url); }
    ~IceCastRequest(void) { }
    const char *data(void) { return m_data.data(); }
    uint size(void) { return m_data.size(); }

  private:
    void setUrl(const QUrl &url)
    {
        QString hdr;
        hdr = QString("GET %PATH% HTTP/1.1\r\n"
                      "Host: %HOST%\r\n"
                      "User-Agent: MythMusic/%VERSION%\r\n"
                      "Accept: */*\r\n");

        QString path = url.path();
        QString host = url.host();

        if (path.isEmpty())
            path = "/";

        if (url.hasQuery())
            path += '?' + url.encodedQuery();

        if (url.port() != -1)
            host += QString(":%1").arg(url.port());

        hdr.replace("%PATH%", path);
        hdr.replace("%HOST%", host);
        hdr.replace("%VERSION%", MYTH_BINARY_VERSION);

        if (!url.userName().isEmpty() && !url.password().isEmpty()) 
        {
            QString authstring = url.userName() + ":" + url.password();
            QString auth = QCodecs::base64Encode(authstring.toLocal8Bit());

            hdr += "Authorization: Basic " + auth + "\r\n";
        }

        hdr += QString("TE: trailers\r\n"
                       "x-audiocast-udpport: %1\r\n"
                       "\r\n").arg(ICE_UDP_PORT);

        LOG(VB_NETWORK, LOG_INFO, QString("IceCastRequest: '%1'").arg(hdr));

        m_data = hdr.toAscii();
    }

    QByteArray m_data;
};

/****************************************************************************/


class ShoutCastResponse 
{
  public:
    ShoutCastResponse(void) { }
    ~ShoutCastResponse(void) { }

    int getMetaint(void)
    {
        if (m_data.contains("icy-metaint"))
            return getInt("icy-metaint");
        else
            return -1;
    }
    int getBitrate(void) { return getInt("icy-br"); }
    QString getGenre(void) { return getString("icy-genre"); }
    QString getName(void) { return getString("icy-name"); }
    int getStatus(void) { return getInt("status"); }
    bool isICY(void) { return QString(m_data["protocol"]).left(3) == "ICY"; }
    QString getContent(void) { return getString("content-type"); }
    QString getLocation(void) { return getString("location"); }

    QString getString(const QString &key) { return m_data[key]; }
    int getInt(const QString &key) { return m_data[key].toInt(); }

    int fillResponse(const char *data, int len);

  private:
    QMap<QString, QString> m_data;
};

/** \brief Consume bytes and parse shoutcast header
    \returns number of bytes consumed
*/
int ShoutCastResponse::fillResponse(const char *s, int l)
{
    QByteArray d(s, l);
    int result = 0;
    // check each line
    for (;;)
    {
        int pos = d.indexOf("\r");

        if (pos <= 0)
            break;

        // Extract the line
        QByteArray snip(d.data(), pos + 1);
        d.remove(0, pos + 2);
        result += pos + 2;

        if (snip.left(4) == "ICY ")
        {
            int space = snip.indexOf(' ');
            m_data["protocol"] = "ICY";
            QString tmp = snip.mid(space).simplified();
            int second_space = tmp.indexOf(' ');
            if (second_space > 0) 
                m_data["status"] = tmp.left(second_space);
            else
                m_data["status"] = tmp;
        }
        else if (snip.left(7) == "HTTP/1.")
        {
            int space = snip.indexOf(' ');
            m_data["protocol"] = snip.left(space);
            QString tmp = snip.mid(space).simplified();
            int second_space = tmp.indexOf(' ');
            if (second_space > 0)
                m_data["status"] = tmp.left(second_space);
            else
                m_data["status"] = tmp;
        } 
        else if (snip.left(9).toLower() == "location:")
        {
            m_data["location"] = snip.mid(9).trimmed();
        } 
        else if (snip.left(13).toLower() == "content-type:")
        {
            m_data["content-type"] = snip.mid(13).trimmed();
        }
        else if (snip.left(4) == "icy-")
        {
            int pos = snip.indexOf(':');
            QString key = snip.left(pos);
            m_data[key.toAscii()] = snip.mid(pos+1).trimmed();
        }
    }

    return result;
}

/****************************************************************************/

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
        QChar ch = m_meta_format.at(pos);

        if (ch == '%')
        {
            pos++;
        }
        else if (ch == 'r' || ch == 'a' || ch == 'b' || ch == 't')
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

/****************************************************************************/

ShoutCastIODevice::ShoutCastIODevice(void)
    :  m_redirects (0), 
       m_scratchpad_pos (0),
       m_state (NOT_CONNECTED)
{
    m_socket = new QTcpSocket;
    m_response = new ShoutCastResponse;

    connect(m_socket, SIGNAL(hostFound()), SLOT(socketHostFound()));
    connect(m_socket, SIGNAL(connected()), SLOT(socketConnected()));
    connect(m_socket, SIGNAL(disconnected()), SLOT(socketConnectionClosed()));
    connect(m_socket, SIGNAL(readyRead()), SLOT(socketReadyRead()));
    connect(m_socket, SIGNAL(error(QAbstractSocket::SocketError)), 
            SLOT(socketError(QAbstractSocket::SocketError)));

    switchToState(NOT_CONNECTED);

    setOpenMode(ReadWrite);
}

ShoutCastIODevice::~ShoutCastIODevice(void)
{
    delete m_response;
    m_socket->close();
    m_socket->disconnect(this);
    m_socket->deleteLater();
    m_socket = NULL;
}

void ShoutCastIODevice::connectToUrl(const QUrl &url)
{
    m_url = url;
    switchToState (RESOLVING);
    setOpenMode(ReadWrite);
    open(ReadWrite);
    return m_socket->connectToHost(m_url.host(), m_url.port() == -1 ? 80 : m_url.port());
}

void ShoutCastIODevice::close(void)
{
    return m_socket->close();
}

bool ShoutCastIODevice::flush(void)
{
    return m_socket->flush();
}

qint64 ShoutCastIODevice::size(void) const
{
    return m_buffer->readBufAvail(); 
}

qint64 ShoutCastIODevice::readData(char *data, qint64 maxlen)
{
    // the decoder wants more data from the stream
    // but first we must filter out any headers and metadata

    if (m_buffer->readBufAvail() == 0)
    {
        LOG(VB_PLAYBACK, LOG_ERR, "ShoutCastIODevice: No data in buffer!!");
        switchToState(STOPPED);
        return -1;
    }

    if (m_state == STREAMING_META && parseMeta())
        switchToState(STREAMING);

    if (m_state == STREAMING)
    {
        if (m_bytesTillNextMeta > 0)
        {
            // take maxlen or what ever is left till the next metadata
            if (maxlen > m_bytesTillNextMeta)
                maxlen = m_bytesTillNextMeta;

            maxlen = m_buffer->read(data, maxlen);

            m_bytesTillNextMeta -= maxlen;

            if (m_bytesTillNextMeta == 0)
                switchToState(STREAMING_META);
        }
        else
            maxlen = m_buffer->read(data, maxlen);
    }

    if (m_state != STOPPED) 
        LOG(VB_NETWORK, LOG_INFO,
            QString("ShoutCastIODevice: %1 kb in buffer, btnm=%2/%3 "
                    "state=%4, len=%5")
                .arg(m_buffer->readBufAvail() / 1024, 4)
                .arg(m_bytesTillNextMeta, 4)
                .arg(m_response->getMetaint())
                .arg(stateString (m_state))
                .arg(maxlen));
    else
        LOG(VB_NETWORK, LOG_INFO, QString("ShoutCastIODevice: stopped"));

    return maxlen;
}

qint64 ShoutCastIODevice::writeData(const char *data, qint64 sz)
{
    return m_socket->write(data, sz);
}

qint64 ShoutCastIODevice::bytesAvailable(void) const
{
    return m_buffer->readBufAvail();
}

void ShoutCastIODevice::socketHostFound(void)
{
    LOG(VB_NETWORK, LOG_INFO, "ShoutCastIODevice: Host Found");
    switchToState(CONNECTING);
}

void ShoutCastIODevice::socketConnected(void)
{
    LOG(VB_NETWORK, LOG_INFO, "ShoutCastIODevice: Connected");
    switchToState(CONNECTED);

    ShoutCastRequest request(m_url);
    qint64 written = m_socket->write(request.data(), request.size());
    LOG(VB_NETWORK, LOG_INFO,
        QString("ShoutCastIODevice: Sending Request, %1 of %2 bytes")
            .arg(written).arg(request.size()));

    if (written != request.size())
    {
        LOG(VB_NETWORK, LOG_INFO, "ShoutCastIODevice: buffering write");
        m_scratchpad = QByteArray(request.data() + written, request.size() - written);
        m_scratchpad_pos = 0;
        connect(m_socket, SIGNAL (bytesWritten(qint64)), SLOT(socketBytesWritten(qint64)));
        switchToState(WRITING_HEADER);
    }
    else
        switchToState(READING_HEADER);

    m_started = false;
    m_bytesDownloaded = 0;
    m_bytesTillNextMeta = 0;
    m_response_gotten = false;
}

void ShoutCastIODevice::socketConnectionClosed(void)
{
    LOG(VB_NETWORK, LOG_INFO, "ShoutCastIODevice: Connection Closed");
    switchToState(STOPPED);
}

void ShoutCastIODevice::socketReadyRead(void)
{
    // only read enough data to fill our buffer
    int available = DecoderIOFactory::DefaultBufferSize - m_buffer->readBufAvail();

    QByteArray data = m_socket->read(available);

    m_bytesDownloaded += data.size();
    m_buffer->write(data);

    // send buffer status event
    emit bufferStatus(m_buffer->readBufAvail(), DecoderIOFactory::DefaultBufferSize);

    if (!m_started && m_bytesDownloaded > DecoderIOFactory::DefaultPrebufferSize)
    {
        m_socket->setReadBufferSize(DecoderIOFactory::DefaultPrebufferSize);
        m_started = true;
    }

    // if we are waiting for the HEADER and we have enough data process that
    if (m_state == READING_HEADER)
    {
        if (parseHeader())
        {
            if (m_response->getStatus() == 200)
            {
                switchToState(PLAYING);

                m_response_gotten = true;

                m_bytesTillNextMeta = m_response->getMetaint();

                switchToState(STREAMING);
            }
            else if (m_response->getStatus() == 302 || m_response->getStatus() == 301)
            {
                if (++m_redirects > MAX_REDIRECTS)
                {
                    LOG(VB_NETWORK, LOG_ERR, QString("Too many redirects"));
                    switchToState(STOPPED);
                }
                else
                {
                    LOG(VB_NETWORK, LOG_INFO, QString("Redirect to %1").arg(m_response->getLocation()));
                    m_socket->close();
                    QUrl redirectURL(m_response->getLocation());
                    connectToUrl(redirectURL);
                    return;
                }
            }
            else
            {
                LOG(VB_NETWORK, LOG_ERR, QString("Unknown response status %1")
                        .arg(m_response->getStatus()));
                switchToState(STOPPED);
            }
        }
    }
}

void ShoutCastIODevice::socketBytesWritten(qint64)
{
    qint64 written = m_socket->write(m_scratchpad.data() + m_scratchpad_pos,
                                     m_scratchpad.size() - m_scratchpad_pos);
    LOG(VB_NETWORK, LOG_INFO,
        QString("ShoutCastIO: %1 bytes written").arg(written));

    m_scratchpad_pos += written;
    if (m_scratchpad_pos == m_scratchpad.size())
    {
        m_scratchpad.truncate(0);
        disconnect (m_socket, SIGNAL(bytesWritten(qint64)), this, 0);
        switchToState(READING_HEADER);
    }
}

void ShoutCastIODevice::socketError(QAbstractSocket::SocketError error)
{
    switch (error)
    {
        case QAbstractSocket::ConnectionRefusedError:
            LOG(VB_NETWORK, LOG_ERR,
                "ShoutCastIODevice: Error Connection Refused");
            switchToState(CANT_CONNECT);
            break;
        case QAbstractSocket::RemoteHostClosedError:
            LOG(VB_NETWORK, LOG_ERR,
                "ShoutCastIODevice: Error Remote Host Closed The Connection");
            switchToState(CANT_CONNECT);
            break;
        case QAbstractSocket::HostNotFoundError:
            LOG(VB_NETWORK, LOG_ERR,
                "ShoutCastIODevice: Error Host Not Found");
            switchToState(CANT_RESOLVE);
            break;
        case QAbstractSocket::SocketTimeoutError:
            LOG(VB_NETWORK, LOG_ERR, "ShoutCastIODevice: Error Socket Timeout");
            switchToState(STOPPED);
            break;
        default:
            LOG(VB_NETWORK, LOG_ERR,
                QString("ShoutCastIODevice: Got socket error '%1'")
                    .arg(errorString()));
            switchToState(STOPPED);
            break;
    }
}

void ShoutCastIODevice::switchToState(const State &state) 
{
    switch (state) 
    {
        case PLAYING:
            LOG(VB_PLAYBACK, LOG_INFO, QString("Playing %1 (%2) at %3 kbps")
                .arg(m_response->getName())
                .arg(m_response->getGenre())
                .arg(m_response->getBitrate()));
            break;
        case STREAMING:
            if (m_state == STREAMING_META)
                m_bytesTillNextMeta = m_response->getMetaint();
            break;
        case STOPPED:
            m_socket->close();
            break;
        default:
            break;
    }

    m_state = state;
    emit changedState(m_state);
}

bool ShoutCastIODevice::parseHeader(void)
{
    int available = min(4096, (int)m_buffer->readBufAvail());
    QByteArray data;
    m_buffer->read(data, available, false);
    int consumed = m_response->fillResponse(data.data(), data.size());

    LOG(VB_NETWORK, LOG_INFO,
        QString("ShoutCastIODevice: Receiving header, %1 bytes").arg(consumed));
    {
        QString tmp;
        tmp = QString::fromAscii(data.data(), consumed);
        LOG(VB_NETWORK, LOG_INFO,
            QString("ShoutCastIODevice: Receiving header\n%1").arg(tmp));
    }

    m_buffer->remove(0, consumed);

    if (m_buffer->readBufAvail() >= 2)
    {
        data.clear();
        m_buffer->read(data, 2, false);
        if (data.size() == 2 && data[0] == '\r' && data[1] == '\n')
        {
            m_buffer->remove(0, 2);
            return true;
        }
    }

    return false;
}

bool ShoutCastIODevice::getResponse(ShoutCastResponse &response)
{
    if (!m_response_gotten)
        return false;

    response = *m_response;
    return true;
}

bool ShoutCastIODevice::parseMeta(void)
{
    QByteArray data;
    m_buffer->read(data, 1);
    unsigned char ch = data[0];

    qint64 meta_size = 16 * ch;
    if (meta_size == 0)
        return true;

    // sanity check 
    if (meta_size > MAX_ALLOWED_META_SIZE)
    {
        LOG(VB_PLAYBACK, LOG_ERR,
            QString("ShoutCastIODevice: Error in stream, got a meta size of %1")
                .arg(meta_size));
        switchToState(STOPPED);
        return false;
    }

    LOG(VB_NETWORK, LOG_INFO,
        QString("ShoutCastIODevice: Reading %1 bytes of meta").arg(meta_size));

    // read metadata from our buffer
    data.clear();
    m_buffer->read(data, meta_size);

    // sanity check, check we have enough data
    if (meta_size > data.size())
    {
        LOG(VB_PLAYBACK, LOG_ERR,
            QString("ShoutCastIODevice: Not enough data, we have %1, but the "
                    "metadata size is %1")
                .arg(data.size()).arg(meta_size));
        switchToState(STOPPED);
        return false;
    }

    QString metadataString = QString::fromUtf8(data.constData());

    // avoid sending signals if the data hasn't changed
    if (m_last_metadata == metadataString)
        return true;

    m_last_metadata = metadataString;
    emit meta(metadataString);

    return true;
}

/****************************************************************************/

DecoderIOFactoryShoutCast::DecoderIOFactoryShoutCast(DecoderHandler *parent)
    : DecoderIOFactory(parent), m_timer(NULL), m_input(NULL), m_prebuffer(160000)
{
    m_timer = new QTimer(this);
}

DecoderIOFactoryShoutCast::~DecoderIOFactoryShoutCast(void)
{
    closeIODevice();
}

QIODevice* DecoderIOFactoryShoutCast::getInput(void)
{
    return m_input;
}

void DecoderIOFactoryShoutCast::makeIODevice(void)
{
    closeIODevice();

    m_input = new ShoutCastIODevice();

    qRegisterMetaType<ShoutCastIODevice::State>("ShoutCastIODevice::State");
    connect(m_input, SIGNAL(meta(const QString&)),
            this,    SLOT(shoutcastMeta(const QString&)));
    connect(m_input, SIGNAL(changedState(ShoutCastIODevice::State)),
            this,    SLOT(shoutcastChangedState(ShoutCastIODevice::State)));
    connect(m_input, SIGNAL(bufferStatus(int, int)),
            this,    SLOT(shoutcastBufferStatus(int,int)));
}

void DecoderIOFactoryShoutCast::closeIODevice(void)
{
    if (m_input)
    {
        m_input->disconnect();
        if (m_input->isOpen())
        {
            m_input->close();
        }
        m_input->deleteLater();
        m_input = NULL;
    }
}

void DecoderIOFactoryShoutCast::start(void)
{
    LOG(VB_PLAYBACK, LOG_INFO,
        QString("DecoderIOFactoryShoutCast %1").arg(m_handler->getUrl().toString()));
    doOperationStart(tr("Connecting"));

    makeIODevice();
    m_input->connectToUrl(m_handler->getUrl());
}

void DecoderIOFactoryShoutCast::stop(void)
{
    if (m_timer)
        m_timer->disconnect();

    doOperationStop();
}

void DecoderIOFactoryShoutCast::periodicallyCheckResponse(void)
{
    int res = checkResponseOK();

    if (res == 0)
    {
        ShoutCastResponse response;
        m_input->getResponse(response);
        m_prebuffer = PREBUFFER_SECS * response.getBitrate() * 125; // 125 = 1000/8 (kilo, bits...)
        LOG(VB_NETWORK, LOG_INFO,
            QString("kbps is %1, prebuffering %2 secs = %3 kb")
                .arg(response.getBitrate()).arg(PREBUFFER_SECS)
                .arg(m_prebuffer/1024));
        m_timer->stop();
        m_timer->disconnect();
        connect(m_timer, SIGNAL(timeout()), this, SLOT(periodicallyCheckBuffered()));
        m_timer->start(500);
    }
    else if (res < 0)
    {
        m_timer->stop();
        doFailed("Cannot parse this stream");
    }
}

void DecoderIOFactoryShoutCast::periodicallyCheckBuffered(void)
{
    LOG(VB_NETWORK, LOG_INFO,
        QString("DecoderIOFactoryShoutCast: prebuffered %1/%2KB")
            .arg(m_input->bytesAvailable()/1024).arg(m_prebuffer/1024));

    if (m_input->bytesAvailable() < m_prebuffer || m_input->bytesAvailable() == 0)
        return;

    ShoutCastResponse response;
    m_input->getResponse(response);
    LOG(VB_PLAYBACK, LOG_INFO,
        QString("contents '%1'").arg(response.getContent()));
    if (response.getContent() == "audio/mpeg")
        doConnectDecoder("create-mp3-decoder.mp3");
    else if (response.getContent() == "audio/aacp")
        doConnectDecoder("create-aac-decoder.m4a");
    else if (response.getContent() == "application/ogg")
        doConnectDecoder("create-ogg-decoder.ogg");
    else if (response.getContent() == "audio/aac")
        doConnectDecoder("create-aac-decoder.aac");
    else
    {
        doFailed(tr("Unsupported content type for ShoutCast stream: %1")
                    .arg (response.getContent()));
    }

    m_timer->disconnect();
    m_timer->stop();

    m_lastStatusTime.start();
}

void DecoderIOFactoryShoutCast::shoutcastMeta(const QString &metadata)
{
    LOG(VB_PLAYBACK, LOG_INFO,
        QString("DecoderIOFactoryShoutCast: metadata changed - %1")
            .arg(metadata));
    ShoutCastMetaParser parser;
    parser.setMetaFormat(m_handler->getMetadata().MetadataFormat());

    ShoutCastMetaMap meta_map = parser.parseMeta(metadata);

    MusicMetadata mdata = m_handler->getMetadata();
    mdata.setTitle(meta_map["title"]);
    mdata.setArtist(meta_map["artist"]);
    mdata.setAlbum(meta_map["album"]);
    mdata.setLength(-1);

    DecoderHandlerEvent ev(DecoderHandlerEvent::Meta, mdata);
    dispatch(ev);
}

void DecoderIOFactoryShoutCast::shoutcastBufferStatus(int available, int maxSize)
{
    if (m_lastStatusTime.elapsed() < 1000)
        return;

    if (m_input->getState() == ShoutCastIODevice::PLAYING ||
        m_input->getState() == ShoutCastIODevice::STREAMING ||
        m_input->getState() == ShoutCastIODevice::STREAMING_META)
    {
        DecoderHandlerEvent ev(DecoderHandlerEvent::BufferStatus, available, maxSize);
        dispatch(ev);
    }

    m_lastStatusTime.restart();
}

void DecoderIOFactoryShoutCast::shoutcastChangedState(ShoutCastIODevice::State state)
{
    LOG(VB_PLAYBACK, LOG_INFO, QString("ShoutCast changed state to %1")
        .arg(ShoutCastIODevice::stateString(state)));

    if (state == ShoutCastIODevice::RESOLVING)
        doOperationStart(tr("Finding radio stream"));

    if (state == ShoutCastIODevice::CANT_RESOLVE)
        doFailed(tr("Cannot find radio.\nCheck the URL is correct."));

    if (state == ShoutCastIODevice::CONNECTING)
        doOperationStart(tr("Connecting to radio stream"));

    if (state == ShoutCastIODevice::CANT_CONNECT)
        doFailed(tr("Cannot connect to radio.\nCheck the URL is correct."));

    if (state == ShoutCastIODevice::CONNECTED)
    {
        doOperationStart(tr("Connected to radio stream"));
        m_timer->stop();
        m_timer->disconnect();
        connect(m_timer, SIGNAL(timeout()),
                this, SLOT(periodicallyCheckResponse()));
        m_timer->start(300);
    }

    if (state == ShoutCastIODevice::PLAYING)
    {
        doOperationStart(tr("Buffering"));
    }

    if (state == ShoutCastIODevice::STOPPED)
        stop();
}

int DecoderIOFactoryShoutCast::checkResponseOK()
{
    ShoutCastResponse response;

    if (!m_input->getResponse(response))
        return 1;

    if (!response.isICY() && response.getStatus() == 302 &&
        !response.getLocation().isEmpty())
    {
        // restart with new location...
        m_handler->setUrl(response.getLocation());
        start();
        return 1;
    }

    if (response.getStatus() != 200)
        return -1;

    return 0;
}
