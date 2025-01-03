// c/c++
#include <cassert>
#include <cstdio>
#include <unistd.h>

// qt
#include <QApplication>
#include <QFileInfo>
#include <QUrl>

// MythTV
#include <libmythbase/mythcorecontext.h>
#include <libmythbase/mythdirs.h>
#include <libmythbase/mythdownloadmanager.h>
#include <libmythbase/mythlogging.h>
#include <libmythbase/mythrandom.h>
#include <libmythbase/remotefile.h>
#include <libmythmetadata/musicmetadata.h>

// mythmusic
#include "decoder.h"
#include "decoderhandler.h"

/**********************************************************************/

const QEvent::Type DecoderHandlerEvent::kReady = (QEvent::Type) QEvent::registerEventType();
const QEvent::Type DecoderHandlerEvent::kMeta = (QEvent::Type) QEvent::registerEventType();
const QEvent::Type DecoderHandlerEvent::kBufferStatus = (QEvent::Type) QEvent::registerEventType();
const QEvent::Type DecoderHandlerEvent::kOperationStart = (QEvent::Type) QEvent::registerEventType();
const QEvent::Type DecoderHandlerEvent::kOperationStop = (QEvent::Type) QEvent::registerEventType();
const QEvent::Type DecoderHandlerEvent::kError = (QEvent::Type) QEvent::registerEventType();

DecoderHandlerEvent::DecoderHandlerEvent(Type type, const MusicMetadata &meta)
    : MythEvent(type),
      m_meta(new MusicMetadata(meta))
{ 
}

DecoderHandlerEvent::~DecoderHandlerEvent(void)
{
    delete m_msg;
    delete m_meta;
}

MythEvent* DecoderHandlerEvent::clone(void) const
{
    auto *result = new DecoderHandlerEvent(*this);

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

DecoderHandler::~DecoderHandler(void)
{
    stop();
}

void DecoderHandler::start(MusicMetadata *mdata)
{
    m_state = LOADING;

    m_playlist.clear();
    m_meta = *mdata;
    m_playlistPos = -1;
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
        {
            PlayListFileEntry* file = m_playlist.get(ii);
            LOG(VB_PLAYBACK, LOG_INFO, QString("Track %1 = %2")
                .arg(ii) .arg(file ? file->File() : "<invalid>"));
        }
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
    auto *str = new QString(e);
    DecoderHandlerEvent ev(DecoderHandlerEvent::kError, str);
    dispatch(ev);
}

bool DecoderHandler::done(void)
{
    if (m_state == STOPPED)
        return true;

    if (m_playlistPos + 1 >= m_playlist.size())
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
        m_playlistPos = MythRandom(0, m_playlist.size() - 1);
    }
    else
    {
        m_playlistPos++;
    }

    PlayListFileEntry *entry = m_playlist.get(m_playlistPos);
    if (nullptr == entry)
        return false;

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
        m_decoder = nullptr;
    }

    doOperationStop();

    m_state = STOPPED;
}

void DecoderHandler::customEvent(QEvent *event)
{
    if (event == nullptr)
        return;

    if (auto *dhe = dynamic_cast<DecoderHandlerEvent*>(event))
    {
        // Proxy all DecoderHandlerEvents
        dispatch(*dhe);
        return;
    }
    if (event->type() == MythEvent::kMythEventMessage)
    {
        auto *me = dynamic_cast<MythEvent *>(event);
        if (me == nullptr)
            return;
        QStringList tokens = me->Message().split(" ", Qt::SkipEmptyParts);

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
                const QString& downloadUrl = args[0];
                int fileSize  = args[2].toInt();
                int errorCode = args[4].toInt();
                const QString& filename = args[1];

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
            .arg(QFileInfo(url.path()).fileName(), extension));

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
    auto *entry = new PlayListFileEntry;

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
    QElapsedTimer time;
    time.start();
    while (m_state == LOADING)
    {
        if (time.hasExpired(30000))
        {
            doOperationStop();
            GetMythDownloadManager()->cancelDownload(url.toString());
            LOG(VB_GENERAL, LOG_ERR, QString("DecoderHandler:: Timed out trying to download playlist from: %1")
                .arg(url.toString()));
            m_state = STOPPED;
        }

        QCoreApplication::processEvents();
        usleep(500);
    }
}

void DecoderHandler::doConnectDecoder(const QUrl &url, const QString &format) 
{
    if (m_decoder && !m_decoder->factory()->supports(format)) 
    {
        delete m_decoder;
        m_decoder = nullptr;
    }

    if (!m_decoder)
    {
        m_decoder = Decoder::create(format, nullptr, true);
        if (m_decoder == nullptr)
        {
            doFailed(url, QString("No decoder for this format '%1'").arg(format));
            return;
        }
    }

    m_decoder->setURL(url.toString());

    DecoderHandlerEvent ev(DecoderHandlerEvent::kReady);
    dispatch(ev);
}

void DecoderHandler::doFailed(const QUrl &url, const QString &message)
{
    LOG(VB_NETWORK, LOG_ERR,
        QString("DecoderHandler error: '%1' - %2").arg(message, url.toString()));
    DecoderHandlerEvent ev(DecoderHandlerEvent::kError, new QString(message));
    dispatch(ev);
}

void DecoderHandler::doOperationStart(const QString &name)
{
    m_op = true;
    DecoderHandlerEvent ev(DecoderHandlerEvent::kOperationStart, new QString(name));
    dispatch(ev);
}

void DecoderHandler::doOperationStop(void)
{
    if (!m_op)
        return;

    m_op = false;
    DecoderHandlerEvent ev(DecoderHandlerEvent::kOperationStop);
    dispatch(ev);
}
