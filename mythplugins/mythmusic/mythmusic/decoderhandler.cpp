// c/c++
#include <unistd.h>
#include <stdio.h>
#include <assert.h>

// qt
#include <QApplication>
#include <QUrl>
#include <QFileInfo>

// mythtv
#include <mythdownloadmanager.h>
#include <mythdirs.h>
#include <mythverbose.h>

// mythmusic
#include "decoderhandler.h"
#include "decoder.h"
#include "metadata.h"
#include "streaminput.h"
#include "shoutcast.h"

/**********************************************************************/

QEvent::Type DecoderHandlerEvent::Ready = (QEvent::Type) QEvent::registerEventType();
QEvent::Type DecoderHandlerEvent::Meta = (QEvent::Type) QEvent::registerEventType();
QEvent::Type DecoderHandlerEvent::Info = (QEvent::Type) QEvent::registerEventType();
QEvent::Type DecoderHandlerEvent::OperationStart = (QEvent::Type) QEvent::registerEventType();
QEvent::Type DecoderHandlerEvent::OperationStop = (QEvent::Type) QEvent::registerEventType();
QEvent::Type DecoderHandlerEvent::Error = (QEvent::Type) QEvent::registerEventType();

DecoderHandlerEvent::DecoderHandlerEvent(Type t, const Metadata &meta)
    : MythEvent(t), m_msg(NULL), m_meta(NULL)
{ 
    m_meta = new Metadata(meta);
}

DecoderHandlerEvent::~DecoderHandlerEvent(void)
{
    if (m_msg)
        delete m_msg;

    if (m_meta)
        delete m_meta;
}

MythEvent* DecoderHandlerEvent::clone(void) const
{
    DecoderHandlerEvent *result = new DecoderHandlerEvent(*this);

    if (m_msg)
        result->m_msg = new QString(*m_msg);

    if (m_meta)
        result->m_meta = new Metadata(*m_meta);

    return result;
}

/**********************************************************************/

DecoderIOFactory::DecoderIOFactory(DecoderHandler *parent) 
{
    m_handler = parent;
}

DecoderIOFactory::~DecoderIOFactory(void)
{
}

void DecoderIOFactory::doConnectDecoder(const QString &format)
{
    m_handler->doOperationStop();
    m_handler->doConnectDecoder(getUrl(), format);
}

Decoder *DecoderIOFactory::getDecoder(void)
{
    return m_handler->getDecoder();
}

void DecoderIOFactory::doFailed(const QString &message)
{
    m_handler->doOperationStop();
    m_handler->doFailed(getUrl(), message);
}

void DecoderIOFactory::doInfo(const QString &message)
{
    m_handler->doInfo(message);
}

void DecoderIOFactory::doOperationStart(const QString &name)
{
    m_handler->doOperationStart(name);
}

void DecoderIOFactory::doOperationStop(void)
{
    m_handler->doOperationStop();
}

/**********************************************************************/

DecoderIOFactoryFile::DecoderIOFactoryFile(DecoderHandler *parent)
    : DecoderIOFactory(parent), m_input (NULL)
{
}

DecoderIOFactoryFile::~DecoderIOFactoryFile(void)
{
    if (m_input)
        delete m_input;
}

QIODevice* DecoderIOFactoryFile::takeInput(void)
{
    QIODevice *result = m_input;
    m_input = NULL;
    return result;
}

void DecoderIOFactoryFile::start(void)
{
    QString sourcename = getMetadata().Filename();
    VERBOSE(VB_PLAYBACK, QString("DecoderIOFactory: Opening Local File %1").arg(sourcename));
    m_input = new QFile(sourcename);
    doConnectDecoder(getUrl());
}

DecoderIOFactoryUrl::DecoderIOFactoryUrl(DecoderHandler *parent) : DecoderIOFactory(parent)
{
    m_accessManager = new QNetworkAccessManager(this);
    m_input = new MusicIODevice();
    connect(m_input, SIGNAL(freeSpaceAvailable()), SLOT(readyRead()));

    m_input->open(QIODevice::ReadWrite);

    m_bytesWritten = 0;
}

DecoderIOFactoryUrl::~DecoderIOFactoryUrl(void)
{
    doClose();

    m_accessManager->deleteLater();

    if (m_input)
        delete m_input;
}

QIODevice* DecoderIOFactoryUrl::takeInput(void) 
{
    QIODevice *result = m_input;
    //m_input = NULL;
    return result;
}

void DecoderIOFactoryUrl::start(void)
{
    VERBOSE(VB_PLAYBACK, QString("DecoderIOFactory: Url %1").arg(getUrl().toString()));

    m_started = false;

    doOperationStart("Fetching remote file");

    m_reply = m_accessManager->get(QNetworkRequest(getUrl()));

    connect(m_reply, SIGNAL(readyRead()), this, SLOT(readyRead()));
    connect(m_accessManager, SIGNAL(finished(QNetworkReply*)),
            this, SLOT(replyFinished(QNetworkReply*)));
}

void DecoderIOFactoryUrl::stop(void)
{
    doClose();
}

void DecoderIOFactoryUrl::replyFinished(QNetworkReply *reply)
{
    if (reply->error() != QNetworkReply::NoError) 
    {
        doFailed("Cannot retrieve remote file.");
        return;
    }

    QUrl possibleRedirectUrl = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();

    if (!possibleRedirectUrl.isEmpty() && (m_redirectedURL != possibleRedirectUrl))
    {
        VERBOSE(VB_PLAYBACK, QString("DecoderIOFactory: Got redirected to %1").arg(possibleRedirectUrl));

        m_redirectCount++;

        if (m_redirectCount > MaxRedirects)
        {
            doFailed("Too many redirects");
        }
        else
        {
            setUrl(possibleRedirectUrl);
            m_redirectedURL = possibleRedirectUrl;
            start();
        }

        return;
    }

    m_redirectedURL.clear();

    if (!m_started)
        doStart();
}

void DecoderIOFactoryUrl::readyRead(void)
{
    int available = DecoderIOFactory::DefaultBufferSize - m_input->bytesAvailable();
    QByteArray data = m_reply->read(available);

    m_bytesWritten += data.size();
    m_input->writeData(data.data(), data.size());

    if (!m_started && m_bytesWritten > DecoderIOFactory::DefaultPrebufferSize)
    {
        m_reply->setReadBufferSize(DecoderIOFactory::DefaultPrebufferSize);
        doStart();
    }

//    VERBOSE(VB_IMPORTANT, QString("DecoderIOFactoryUrl::readyRead file size: %1").arg(m_bytesWritten));
}

void DecoderIOFactoryUrl::doStart(void)
{
    doConnectDecoder(getUrl());
    m_started = true;
}

void DecoderIOFactoryUrl::doClose(void)
{
    if (m_input && m_input->isOpen())
        m_input->close();
}

/**********************************************************************/

DecoderHandler::DecoderHandler(void)
    : m_state(STOPPED),
      m_io_factory(NULL),
      m_decoder(NULL),
      m_op(false),
      m_redirects(0)
{
}

DecoderHandler::~DecoderHandler(void)
{
    stop();
}

void DecoderHandler::start(Metadata *mdata)
{
    m_state = LOADING;

    m_playlist.clear();
    m_meta = mdata;
    m_playlist_pos = -1;
    m_redirects = 0;

    QUrl url(mdata->Filename());
    bool result = createPlaylist(url);
    if (m_state == LOADING && result)
    {
        for (int ii = 0; ii < m_playlist.size(); ii++)
            VERBOSE(VB_PLAYBACK, QString("Track %1 = %2")
                .arg(ii)
                .arg(m_playlist.get(ii)->File()));
        next();
    }
    else
    {
        if (m_state != STOPPED)
        {
            doFailed(url, "Could not get playlist");
        }
    }
}

void DecoderHandler::error(const QString &e)
{
    QString *str = new QString(e);
    DecoderHandlerEvent ev(DecoderHandlerEvent::Error, str);
    dispatch(ev);
}

bool DecoderHandler::done(void)
{
    if (m_state == STOPPED)
        return true;

    if (m_playlist_pos + 1 >= m_playlist.size())
    {
        m_state = STOPPED;
        return true;
    }

    return false;
}

bool DecoderHandler::next(void)
{
    if (done())
        return false;

    if (m_meta && m_meta->Format() == "cast")
    {
        m_playlist_pos = rand () % m_playlist.size ();
    }
    else
    {
        m_playlist_pos++;
    }

    PlayListFileEntry *entry = m_playlist.get(m_playlist_pos);
    QUrl url(entry->File());

    VERBOSE(VB_PLAYBACK, QString("Now playing '%1'").arg(url.toString()));

    deleteIOFactory();
    createIOFactory(url);

    if (! haveIOFactory())
        return false;

    getIOFactory()->addListener(this);
    getIOFactory()->setUrl(url);
    getIOFactory()->setMeta(m_meta);
    getIOFactory()->start();
    m_state = ACTIVE;

    return true;
}

void DecoderHandler::stop(void)
{
    VERBOSE(VB_PLAYBACK, QString("DecoderHandler: Stopping decoder"));

    if (m_decoder && m_decoder->isRunning())
    {
        m_decoder->lock();
        m_decoder->stop();
        m_decoder->unlock();
    }

    if (m_decoder)
    {
        m_decoder->lock();
        m_decoder->cond()->wakeAll();
        m_decoder->unlock();
    }

    if (m_decoder)
    {
        m_decoder->wait();
        delete m_decoder->input();
        delete m_decoder;
        m_decoder = NULL;
    }

    deleteIOFactory();
    doOperationStop();

    m_state = STOPPED;
}

void DecoderHandler::customEvent(QEvent *e)
{
    if (class DecoderHandlerEvent *event = dynamic_cast<DecoderHandlerEvent*>(e)) 
    {
        // Proxy all DecoderHandlerEvents
        return dispatch(*event);
    }
}

bool DecoderHandler::createPlaylist(const QUrl &url)
{
    QString extension = QFileInfo(url.path()).fileName().right(4).toLower();
    VERBOSE (VB_NETWORK, QString ("File %1 has extension %2").arg (url.fileName()).arg(extension));

    if (extension == ".pls" || extension == ".m3u")
    {
        if (url.toString().startsWith('/'))
            return createPlaylistFromFile(url);
        else
            return createPlaylistFromRemoteUrl(url);
    }

    return createPlaylistForSingleFile(url);
}

bool DecoderHandler::createPlaylistForSingleFile(const QUrl &url)
{
    PlayListFileEntry *entry = new PlayListFileEntry;

    if (url.scheme() == "file" || url.toString().startsWith('/'))
        entry->setFile(QFileInfo(url.path()).absolutePath() + '/' + QFileInfo(url.path()).fileName());
    else
        entry->setFile(url.toString());

    m_playlist.add(entry);

    return m_playlist.size() > 0;
}

bool DecoderHandler::createPlaylistFromFile(const QUrl &url)
{
    QFile f(QFileInfo(url.path()).absolutePath() + "/" + QFileInfo(url.path()).fileName());
    f.open(QIODevice::ReadOnly);
    QTextStream stream(&f);

    if (PlayListFile::parse(&m_playlist, &stream) < 0) 
        return false;

    return m_playlist.size() > 0;
}

bool DecoderHandler::createPlaylistFromRemoteUrl(const QUrl &url) 
{
    VERBOSE(VB_NETWORK, QString("Retrieving playlist from '%1'").arg(url.toString()));

    doOperationStart("Retrieving playlist");

    QByteArray data;

    if (!GetMythDownloadManager()->download(url, &data))
        return false;

    doOperationStop();

    QTextStream stream(&data, QIODevice::ReadOnly);

    bool result = PlayListFile::parse(&m_playlist, &stream) > 0;

    return result;
}

void DecoderHandler::doConnectDecoder(const QUrl &url, const QString &format) 
{
    if (m_decoder && !m_decoder->factory()->supports(format)) 
    {
        delete m_decoder;
        m_decoder = NULL;
    }

    if (!m_decoder)
    {
        if ((m_decoder = Decoder::create(format, NULL, NULL, true)) == NULL)
        {
            doFailed(url, QString("No decoder for this format '%1'").arg(format));
            return;
        }
    }

    m_decoder->setInput(getIOFactory()->takeInput());
    m_decoder->setFilename(url.toString());

    DecoderHandlerEvent ev(DecoderHandlerEvent::Ready);
    dispatch(ev);
}

void DecoderHandler::doFailed(const QUrl &url, const QString &message)
{
    VERBOSE(VB_NETWORK, QString("DecoderHandler: Unsupported file format: '%1' - %2")
            .arg(url.toString()).arg(message));
    DecoderHandlerEvent ev(DecoderHandlerEvent::Error, new QString(message));
    dispatch(ev);
}

void DecoderHandler::doInfo(const QString &message)
{
    DecoderHandlerEvent ev(DecoderHandlerEvent::Info, new QString(message));
    dispatch(ev);
}

void DecoderHandler::doOperationStart(const QString &name)
{
    m_op = true;
    DecoderHandlerEvent ev(DecoderHandlerEvent::OperationStart, new QString(name));
    dispatch(ev);
}

void DecoderHandler::doOperationStop(void)
{
    if (!m_op)
        return;

    m_op = false;
    DecoderHandlerEvent ev(DecoderHandlerEvent::OperationStop);
    dispatch(ev);
}

void DecoderHandler::createIOFactory(const QUrl &url)
{
    if (haveIOFactory()) 
        deleteIOFactory();

    if (url.toString().startsWith('/') || url.toString().endsWith(".cda"))
    {
        m_io_factory = new DecoderIOFactoryFile(this);
    }
    else if (m_meta && m_meta->Format() == "cast")
        m_io_factory = new DecoderIOFactoryShoutCast(this);
    else
        m_io_factory = new DecoderIOFactoryUrl(this);
}

void DecoderHandler::deleteIOFactory(void)
{
    if (!haveIOFactory())
        return;

    if (m_state == ACTIVE)
        m_io_factory->stop();

    m_io_factory->removeListener(this);
    m_io_factory->disconnect();
    m_io_factory->deleteLater();
    m_io_factory = NULL;
}

/**********************************************************************/

qint64 MusicBuffer::read(char *data, qint64 max, bool doRemove)
{
    QMutexLocker holder (&m_mutex);
    const char *buffer_data = m_buffer.data();

    if (max > m_buffer.size())
        max = m_buffer.size();

    memcpy(data, buffer_data, max);

    if (doRemove)
        m_buffer.remove(0, max);

    return max;
}

qint64 MusicBuffer::read(QByteArray &data, qint64 max, bool doRemove)
{
    QMutexLocker holder (&m_mutex);
    const char *buffer_data = m_buffer.data();

    if (max > m_buffer.size())
        max = m_buffer.size();

    data.append(buffer_data, max);

    if (doRemove)
        m_buffer.remove(0, max);

    return max;
}

void MusicBuffer::write(const char *data, uint sz)
{
    if (sz == 0)
        return;

    QMutexLocker holder(&m_mutex);
    m_buffer.append(data, sz);
}

void MusicBuffer::write(QByteArray &array)
{
    if (array.size() == 0)
        return;

    QMutexLocker holder(&m_mutex);
    m_buffer.append(array);
}

void MusicBuffer::remove(int index, int len)
{
    QMutexLocker holder(&m_mutex);
    m_buffer.remove(index, len);
}

/**********************************************************************/

MusicIODevice::MusicIODevice(void)
{
    m_buffer = new MusicBuffer;
    setOpenMode(ReadWrite);
}

MusicIODevice::~MusicIODevice(void)
{
    delete m_buffer;
}

bool MusicIODevice::open(int)
{
    return true;
}

qint64 MusicIODevice::size(void) const
{
    return m_buffer->readBufAvail(); 
}

qint64 MusicIODevice::readData(char *data, qint64 maxlen)
{
    qint64 res = m_buffer->read(data, maxlen);
    emit freeSpaceAvailable();
    return res;
}

qint64 MusicIODevice::writeData(const char *data, qint64 sz)
{
    m_buffer->write(data, sz);
    return sz;
}

qint64 MusicIODevice::bytesAvailable(void) const
{
    return m_buffer->readBufAvail();
}

int MusicIODevice::getch(void)
{
    assert(0);
    return -1;
}

int MusicIODevice::putch(int)
{
    assert(0);
    return -1;
}

int MusicIODevice::ungetch(int)
{
    assert(0);
    return -1;
}
