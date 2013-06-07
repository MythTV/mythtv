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
#include <mythlogging.h>
#include <compat.h> // For random() on MINGW32
#include <remotefile.h>
#include <mythcorecontext.h>
#include <musicmetadata.h>


// mythmusic
#include "decoderhandler.h"
#include "decoder.h"
#include "shoutcast.h"

/**********************************************************************/

QEvent::Type DecoderHandlerEvent::Ready = (QEvent::Type) QEvent::registerEventType();
QEvent::Type DecoderHandlerEvent::Meta = (QEvent::Type) QEvent::registerEventType();
QEvent::Type DecoderHandlerEvent::BufferStatus = (QEvent::Type) QEvent::registerEventType();
QEvent::Type DecoderHandlerEvent::OperationStart = (QEvent::Type) QEvent::registerEventType();
QEvent::Type DecoderHandlerEvent::OperationStop = (QEvent::Type) QEvent::registerEventType();
QEvent::Type DecoderHandlerEvent::Error = (QEvent::Type) QEvent::registerEventType();

DecoderHandlerEvent::DecoderHandlerEvent(Type t, const MusicMetadata &meta)
    : MythEvent(t), m_msg(NULL), m_meta(NULL), m_available(0), m_maxSize(0)
{ 
    m_meta = new MusicMetadata(meta);
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
        result->m_meta = new MusicMetadata(*m_meta);

    result->m_available = m_available;
    result->m_maxSize = m_maxSize;

    return result;
}

void DecoderHandlerEvent::getBufferStatus(int *available, int *maxSize) const
{
    *available = m_available;
    *maxSize = m_maxSize;
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

QIODevice* DecoderIOFactoryFile::getInput(void)
{
    return m_input;
}

void DecoderIOFactoryFile::start(void)
{
    QString sourcename = getMetadata().Filename();

    LOG(VB_PLAYBACK, LOG_INFO,
        QString("DecoderIOFactory: Opening Local File %1").arg(sourcename));

    m_input = new QFile(sourcename);
    doConnectDecoder(getUrl().toLocalFile());
}

/**********************************************************************/

DecoderIOFactorySG::DecoderIOFactorySG(DecoderHandler *parent)
    : DecoderIOFactory(parent), m_input(NULL)
{
}

DecoderIOFactorySG::~DecoderIOFactorySG(void)
{
    if (m_input)
        delete m_input;
}

QIODevice* DecoderIOFactorySG::getInput(void)
{
    return m_input;
}

void DecoderIOFactorySG::start(void)
{
    QString url = getUrl().toString();
    LOG(VB_PLAYBACK, LOG_INFO,
        QString("DecoderIOFactorySG: Opening Myth URL %1").arg(url));
    m_input = new MusicSGIODevice(url);
    doConnectDecoder(getUrl().path());
}

/**********************************************************************/

DecoderIOFactoryUrl::DecoderIOFactoryUrl(DecoderHandler *parent)
    : DecoderIOFactory(parent), m_started(false),
    m_accessManager(new QNetworkAccessManager(this)),
    m_reply(NULL), m_input(new MusicIODevice()),
    m_redirectCount(0), m_bytesWritten(0)

{
    connect(m_input, SIGNAL(freeSpaceAvailable()), SLOT(readyRead()));

    m_input->open(QIODevice::ReadWrite);
}

DecoderIOFactoryUrl::~DecoderIOFactoryUrl(void)
{
    doClose();

    m_accessManager->deleteLater();

    if (m_input)
        delete m_input;
}

QIODevice* DecoderIOFactoryUrl::getInput(void)
{
    return m_input;
}

void DecoderIOFactoryUrl::start(void)
{
    LOG(VB_PLAYBACK, LOG_INFO,
        QString("DecoderIOFactory: Url %1").arg(getUrl().toString()));

    m_started = false;

    doOperationStart(tr("Fetching remote file"));

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
        LOG(VB_PLAYBACK, LOG_INFO,
            QString("DecoderIOFactory: Got redirected to %1")
                .arg(possibleRedirectUrl.toString()));

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
}

void DecoderIOFactoryUrl::doStart(void)
{
    doConnectDecoder(getUrl().toString());
    m_started = true;
}

void DecoderIOFactoryUrl::doClose(void)
{
    if (m_input && m_input->isOpen())
        m_input->close();
}

/**********************************************************************/

DecoderHandler::DecoderHandler(void) :
    m_state(STOPPED),
    m_playlist_pos(0),
    m_io_factory(NULL),
    m_decoder(NULL),
    m_meta(NULL),
    m_op(false),
    m_redirects(0)
{
}

DecoderHandler::~DecoderHandler(void)
{
    stop();
}

void DecoderHandler::start(MusicMetadata *mdata)
{
    m_state = LOADING;

    m_playlist.clear();
    m_meta = mdata;
    m_playlist_pos = -1;
    m_redirects = 0;

    QUrl url;
    if (QFileInfo(mdata->Filename()).isAbsolute())
        url = QUrl::fromLocalFile(mdata->Filename());
    else
        url.setUrl(mdata->Filename());

    createPlaylist(url);
}

void DecoderHandler::doStart(bool result)
{
    doOperationStop();

    QUrl url;
    if (QFileInfo(m_meta->Filename()).isAbsolute())
        url = QUrl::fromLocalFile(m_meta->Filename());
    else
        url.setUrl(m_meta->Filename());

    if (m_state == LOADING && result)
    {
        for (int ii = 0; ii < m_playlist.size(); ii++)
            LOG(VB_PLAYBACK, LOG_INFO, QString("Track %1 = %2")
                .arg(ii) .arg(m_playlist.get(ii)->File()));
        next();
    }
    else
    {
        if (m_state == STOPPED)
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
        m_playlist_pos = random() % m_playlist.size();
    }
    else
    {
        m_playlist_pos++;
    }

    PlayListFileEntry *entry = m_playlist.get(m_playlist_pos);

    QUrl url;
    if (QFileInfo(entry->File()).isAbsolute())
        url = QUrl::fromLocalFile(entry->File());
    else
        url.setUrl(entry->File());

    LOG(VB_PLAYBACK, LOG_INFO, QString("Now playing '%1'").arg(url.toString()));

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
    LOG(VB_PLAYBACK, LOG_INFO, QString("DecoderHandler: Stopping decoder"));

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
        delete m_decoder;
        m_decoder = NULL;
    }

    deleteIOFactory();
    doOperationStop();

    m_state = STOPPED;
}

void DecoderHandler::customEvent(QEvent *event)
{
    if (DecoderHandlerEvent *dhe = dynamic_cast<DecoderHandlerEvent*>(event))
    {
        // Proxy all DecoderHandlerEvents
        return dispatch(*dhe);
    }
    else if (event->type() == MythEvent::MythEventMessage)
    {
        MythEvent *me = (MythEvent *)event;
        QStringList tokens = me->Message().split(" ", QString::SkipEmptyParts);

        if (tokens.isEmpty())
            return;

        if (tokens[0] == "DOWNLOAD_FILE")
        {
            QStringList args = me->ExtraDataList();

            if (tokens[1] == "UPDATE")
            {
            }
            else if (tokens[1] == "FINISHED")
            {
                QString downloadUrl = args[0];
                int fileSize  = args[2].toInt();
                int errorCode = args[4].toInt();
                QString filename = args[1];

                if ((errorCode != 0) || (fileSize == 0))
                {
                    LOG(VB_GENERAL, LOG_ERR, QString("DecoderHandler: failed to download playlist from '%1'")
                        .arg(downloadUrl));
                    QUrl url(downloadUrl);
                    m_state = STOPPED;
                    doOperationStop();
                    doFailed(url, "Could not get playlist");
                }
                else
                {
                    QUrl fileUrl(filename);
                    createPlaylistFromFile(fileUrl);
                }
            }
        }
    }
}

void DecoderHandler::createPlaylist(const QUrl &url)
{
    QString extension = QFileInfo(url.path()).suffix();
    LOG(VB_NETWORK, LOG_INFO,
        QString("File %1 has extension %2")
            .arg(QFileInfo(url.path()).fileName()).arg(extension));

    if (extension == "pls" || extension == "m3u")
    {
        if (url.scheme() == "file" || QFileInfo(url.toString()).isAbsolute())
            createPlaylistFromFile(url);
        else
            createPlaylistFromRemoteUrl(url);

        return;
    }

    createPlaylistForSingleFile(url);
}

void DecoderHandler::createPlaylistForSingleFile(const QUrl &url)
{
    PlayListFileEntry *entry = new PlayListFileEntry;

    if (url.scheme() == "file" || QFileInfo(url.toString()).isAbsolute())
        entry->setFile(url.toLocalFile());
    else
        entry->setFile(url.toString());

    m_playlist.add(entry);

    doStart((m_playlist.size() > 0));
}

void DecoderHandler::createPlaylistFromFile(const QUrl &url)
{
    QString file = url.toLocalFile();

    PlayListFile::parse(&m_playlist, file);

    doStart((m_playlist.size() > 0));
}

void DecoderHandler::createPlaylistFromRemoteUrl(const QUrl &url) 
{
    LOG(VB_NETWORK, LOG_INFO,
        QString("Retrieving playlist from '%1'").arg(url.toString()));

    doOperationStart(tr("Retrieving playlist"));

    QString extension = QFileInfo(url.path()).suffix().toLower();
    QString saveFilename = GetConfDir() + "/MythMusic/playlist." + extension;
    GetMythDownloadManager()->queueDownload(url.toString(), saveFilename, this);

    //TODO should find a better way to do this
    QTime time;
    time.start();
    while (m_state == LOADING)
    {
        if (time.elapsed() > 30000)
        {
            doOperationStop();
            GetMythDownloadManager()->cancelDownload(url.toString());
            LOG(VB_GENERAL, LOG_ERR, QString("DecoderHandler:: Timed out trying to download playlist from: %1")
                .arg(url.toString()));
            m_state = STOPPED;
        }

        qApp->processEvents();
        usleep(500);
    }
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
        if ((m_decoder = Decoder::create(format, NULL, true)) == NULL)
        {
            doFailed(url, QString("No decoder for this format '%1'").arg(format));
            return;
        }
    }

    m_decoder->setFilename(url.toString());

    DecoderHandlerEvent ev(DecoderHandlerEvent::Ready);
    dispatch(ev);
}

void DecoderHandler::doFailed(const QUrl &url, const QString &message)
{
    LOG(VB_NETWORK, LOG_ERR,
        QString("DecoderHandler error: '%1' - %2").arg(message).arg(url.toString()));
    DecoderHandlerEvent ev(DecoderHandlerEvent::Error, new QString(message));
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

    if (url.scheme() == "myth")
        m_io_factory = new DecoderIOFactorySG(this);
    else if (m_meta && m_meta->Format() == "cast")
        m_io_factory = new DecoderIOFactoryShoutCast(this);
    else if (url.scheme() == "http")
        m_io_factory = new DecoderIOFactoryUrl(this);
    else
        m_io_factory = new DecoderIOFactoryFile(this);
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

bool MusicIODevice::open(OpenMode)
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

/**********************************************************************/

MusicSGIODevice::MusicSGIODevice(const QString &url)
{
    m_remotefile = new RemoteFile(url);
    setOpenMode(ReadWrite);
}

MusicSGIODevice::~MusicSGIODevice(void)
{
    m_remotefile->Close();
    delete m_remotefile;
}

bool MusicSGIODevice::open(OpenMode)
{
    return m_remotefile->isOpen();
}

bool MusicSGIODevice::seek(qint64 pos)
{
    long int newPos = -1;

    if (m_remotefile)
        newPos = m_remotefile->Seek(pos, 0);

    return (newPos == pos);
}

qint64 MusicSGIODevice::size(void) const
{
    return m_remotefile->GetFileSize();
}

qint64 MusicSGIODevice::readData(char *data, qint64 maxlen)
{
    qint64 res = m_remotefile->Read(data, maxlen);
    return res;
}

qint64 MusicSGIODevice::writeData(const char *data, qint64 sz)
{
    m_remotefile->Write(data, sz);
    return sz;
}

qint64 MusicSGIODevice::bytesAvailable(void) const
{
    return m_remotefile->GetFileSize();
}

int MusicSGIODevice::getch(void)
{
    assert(0);
    return -1;
}

int MusicSGIODevice::putch(int)
{
    assert(0);
    return -1;
}

int MusicSGIODevice::ungetch(int)
{
    assert(0);
    return -1;
}
