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
            doFailed(url, "No decoder for this format");
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
    VERBOSE(VB_NETWORK, QString("DecoderHandler: Unsupported file format: '%1'").arg(url.toString()));
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

    if (url.toString().startsWith('/'))
    {
        m_io_factory = new DecoderIOFactoryFile(this);
    }
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
