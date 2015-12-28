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

DecoderHandler::DecoderHandler(void) :
    m_state(STOPPED),
    m_playlist_pos(0),
    m_decoder(NULL),
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
    m_meta = *mdata;
    m_playlist_pos = -1;
    m_redirects = 0;

    if (QFileInfo(mdata->Filename()).isAbsolute())
        m_url = QUrl::fromLocalFile(mdata->Filename());
    else
        m_url.setUrl(mdata->Filename());

    createPlaylist(m_url);
}

void DecoderHandler::doStart(bool result)
{
    doOperationStop();

    if (QFileInfo(m_meta.Filename()).isAbsolute())
        m_url = QUrl::fromLocalFile(m_meta.Filename());
    else
        m_url.setUrl(m_meta.Filename());

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
            doFailed(m_url, "Could not get playlist");
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

    if (m_meta.Format() == "cast")
    {
        m_playlist_pos = random() % m_playlist.size();
    }
    else
    {
        m_playlist_pos++;
    }

    PlayListFileEntry *entry = m_playlist.get(m_playlist_pos);

    if (QFileInfo(entry->File()).isAbsolute())
        m_url = QUrl::fromLocalFile(entry->File());
    else
        m_url.setUrl(entry->File());

    LOG(VB_PLAYBACK, LOG_INFO, QString("Now playing '%1'").arg(m_url.toString()));

    // we use the avfdecoder for everything except CD tracks
    if (m_url.toString().endsWith(".cda"))
        doConnectDecoder(m_url, ".cda");
    else
    {
        // we don't know what format radio stations are so fake a format
        // and hope avfdecoder can decode it
        doConnectDecoder(m_url, ".mp3");
    }

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
                    QUrl fileUrl = QUrl::fromLocalFile(filename);
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

    if (extension == "pls" || extension == "m3u" || extension == "asx")
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

    m_decoder->setURL(url.toString());

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
