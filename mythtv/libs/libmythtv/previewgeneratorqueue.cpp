
#include "previewgeneratorqueue.h"

// C++
#include <algorithm>
using std::max;

// QT
#include <QCoreApplication>
#include <QFileInfo>

// libmythbase
#include "mythcorecontext.h"
#include "mythlogging.h"
#include "mythdirs.h"
#include "mthread.h"

// libmyth
#include "mythcontext.h"
#include "remoteutil.h"

// libmythtv
#include "previewgenerator.h"

#define LOC QString("PreviewQueue: ")

PreviewGeneratorQueue *PreviewGeneratorQueue::s_pgq = NULL;

/**
 * Create the singleton queue of preview generators.  This should be
 * called once at program start-up.  All generation requests on this
 * queue will will be created with the maxAttempts and minBlockSeconds
 * parameters supplied here.
 *
 * \param[in] mode Local or Remote (or both)
 * \param[in] maxAttempts How many times total will the code attempt
 * 	      to generate a preview for a specific file, before giving
 * 	      up and ignoring all future requests.
 * \param[in] minBlockSeconds How long after a failed preview
 *            generation attempt will the code ignore subsequent
 *            requests.
 */
void PreviewGeneratorQueue::CreatePreviewGeneratorQueue(
    PreviewGenerator::Mode mode,
    uint maxAttempts, uint minBlockSeconds)
{
    s_pgq = new PreviewGeneratorQueue(mode, maxAttempts, minBlockSeconds);
}

/**
 * Destroy the singleton queue of preview generators.  This should be
 * called once at program shutdown.  It handles stopping all the
 * running threads before deleting the actual PreviewGeneratorQueue
 * object.
 */
void PreviewGeneratorQueue::TeardownPreviewGeneratorQueue()
{
    s_pgq->exit(0);
    s_pgq->wait();
    delete s_pgq;
    s_pgq = NULL;
}

/*
 * Create the queue object for holding preview generators.
 *
 * Create the singleton queue of preview generators.  This should be
 * called once at program start-up.  All generation requests on this
 * queue will will be created with the maxAttempts and minBlockSeconds
 * parameters supplied here.
 *
 * \param[in] mode Local or Remote (or both)
 * \param[in] maxAttempts How many times total will the code attempt
 * 	      to generate a preview for a specific file, before giving
 * 	      up and ignoring all future requests.
 * \param[in] minBlockSeconds How long after a failed preview
 *            generation attempt will the code ignore subsequent
 *            requests.
 *
 * \note Never call this routine directly. Call the
 * CreatePreviewGeneratorQueue function instead.
 */
PreviewGeneratorQueue::PreviewGeneratorQueue(
    PreviewGenerator::Mode mode,
    uint maxAttempts, uint minBlockSeconds) :
    MThread("PreviewGeneratorQueue"),
    m_mode(mode),
    m_running(0), m_maxThreads(2),
    m_maxAttempts(maxAttempts), m_minBlockSeconds(minBlockSeconds)
{
    if (PreviewGenerator::kLocal & mode)
    {
        int idealThreads = QThread::idealThreadCount();
        m_maxThreads = (idealThreads >= 1) ? idealThreads * 2 : 2;
    }

    moveToThread(qthread());
    start();
}

/**
 * Destroy the preview generation queue.  This function doesn't
 * directly destroy the PreviewGenState objects, but marks them for
 * later destruction by the thread(s) that created them.
 *
 * \note Never directly destroy this object. Call the
 * TeardownPreviewGeneratorQueue function instead.
 */
PreviewGeneratorQueue::~PreviewGeneratorQueue()
{
    // disconnect preview generators
    QMutexLocker locker(&m_lock);
    PreviewMap::iterator it = m_previewMap.begin();
    for (;it != m_previewMap.end(); ++it)
    {
        if ((*it).gen)
            (*it).gen->deleteLater();
        (*it).gen = NULL;
    }
    locker.unlock();
    wait();
}

/**
 * Submit a request for the generation of a preview image.  This
 * information will be packaged up into an event and sent over to the
 * worker thread that generates previews.
 *
 * \param[in] pginfo Generate the image for this program.
 * \param[in] outputsize Generate a preview image of these dimensions.  If this
 * 	      is not specified the generated image will be the same size
 * 	      as the program.
 * \param[in] outputfile Use this specific filename for the preview
 * 	      image. If empty, a default name will be created based on the
 * 	      program information.
 * \param[in] time An offset from the start of the program. This can be
 *            seconds or frames. (See the next argument.)
 * \param[in] in_seconds If true, the 'time' offset is in seconds. If
 *            false, the offset is in frames.
 * \param[in] token A user specified value used to match up this
 *            request with the response from the backend, and as a key for
 *            some indexing.  A token isn't required, but is strongly
 *            suggested.
 */
void PreviewGeneratorQueue::GetPreviewImage(
    const ProgramInfo &pginfo,
    const QSize &outputsize,
    const QString &outputfile,
    long long time, bool in_seconds,
    QString token)
{
    if (!s_pgq)
        return;

    if (pginfo.GetPathname().isEmpty() ||
        pginfo.GetBasename() == pginfo.GetPathname())
    {
        return;
    }

    QStringList extra;
    pginfo.ToStringList(extra);
    extra += token;
    extra += QString::number(outputsize.width());
    extra += QString::number(outputsize.height());
    extra += outputfile;
    extra += QString::number(time);
    extra += (in_seconds ? "1" : "0");
    MythEvent *e = new MythEvent("GET_PREVIEW", extra);
    QCoreApplication::postEvent(s_pgq, e);
}

/**
 * Request notifications when a preview event is generated.  These
 * will be MythEvent messages, and will be one of PREVIEW_QUEUED,
 * PREVIEW_FAILED or PREVIEW_SUCCESS.
 *
 * \param[in] listener The object to receive events.
 */
void PreviewGeneratorQueue::AddListener(QObject *listener)
{
    if (!s_pgq)
        return;

    QMutexLocker locker(&s_pgq->m_lock);
    s_pgq->m_listeners.insert(listener);
}

/**
 * Stop receiving notifications when a preview event is generated.
 * This object will no longer receive PREVIEW_QUEUED, PREVIEW_FAILED,
 * or PREVIEW_SUCCESS events.
 *
 * \param[in] listener The object that should no longer receive events.
 */
void PreviewGeneratorQueue::RemoveListener(QObject *listener)
{
    if (!s_pgq)
        return;

    QMutexLocker locker(&s_pgq->m_lock);
    s_pgq->m_listeners.remove(listener);
}

/**
 * The event handler running on the preview generation thread.
 *
 * \param[in] e The received message.  This should be one of the
 * messages GET_PREVIEW, PREVIEW_SUCCESS, or PREVIEW_FAILED.
 *
 * \warning This function should only be called from the preview
 * generation thread.

 * \bug This function appears to incorrectly compute the value of
 * lastBlockTime.  The call to max() will correctly ensure that if the
 * old value of lastBlockTime is zero, that the new time for the first
 * "retry" will be two.  The problem is that all subsequent "retries"
 * will also be limited to two, so there is no increasing back off
 * interval like it appears was intended.
 */
bool PreviewGeneratorQueue::event(QEvent *e)
{
    if (e->type() != (QEvent::Type) MythEvent::MythEventMessage)
        return QObject::event(e);

    MythEvent *me = (MythEvent*)e;
    if (me->Message() == "GET_PREVIEW")
    {
        const QStringList &list = me->ExtraDataList();
        QStringList::const_iterator it = list.begin();
        ProgramInfo evinfo(it, list.end());
        QString token;
        QSize outputsize;
        QString outputfile;
        long long time = -1LL;
        bool time_fmt_sec;
        if (it != list.end())
            token = (*it++);
        if (it != list.end())
            outputsize.setWidth((*it++).toInt());
        if (it != list.end())
            outputsize.setHeight((*it++).toInt());
        if (it != list.end())
            outputfile = (*it++);
        if (it != list.end())
            time = (*it++).toLongLong();
        QString fn;
        if (it != list.end())
        {
            time_fmt_sec = (*it++).toInt() != 0;
            fn = GeneratePreviewImage(evinfo, outputsize, outputfile,
                                      time, time_fmt_sec, token);
        }
        return true;
    }
    else if (me->Message() == "PREVIEW_SUCCESS" ||
             me->Message() == "PREVIEW_FAILED")
    {
        uint recordedingID = me->ExtraData(0).toUInt(); // pginfo->GetRecordingID()
        QString filename   = me->ExtraData(1); // outFileName
        QString msg        = me->ExtraData(2);
        QString datetime   = me->ExtraData(3);
        QString token      = me->ExtraData(4);

        {
            QMutexLocker locker(&m_lock);
            QMap<QString,QString>::iterator kit = m_tokenToKeyMap.find(token);
            if (kit == m_tokenToKeyMap.end())
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    QString("Failed to find token %1 in map.").arg(token));
                return true;
            }
            PreviewMap::iterator it = m_previewMap.find(*kit);
            if (it == m_previewMap.end())
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    QString("Failed to find key %1 in map.").arg(*kit));
                return true;
            }

            if ((*it).gen)
                (*it).gen->deleteLater();
            (*it).gen           = NULL;
            (*it).genStarted    = false;
            if (me->Message() == "PREVIEW_SUCCESS")
            {
                (*it).attempts      = 0;
                (*it).lastBlockTime = 0;
                (*it).blockRetryUntil = QDateTime();
            }
            else
            {
                (*it).lastBlockTime =
                    max(m_minBlockSeconds, (*it).lastBlockTime * 2);
                (*it).blockRetryUntil =
                    MythDate::current().addSecs((*it).lastBlockTime);
            }

            QStringList list;
            list.push_back(QString::number(recordedingID));
            list.push_back(filename);
            list.push_back(msg);
            list.push_back(datetime);
            QSet<QString>::const_iterator tit = (*it).tokens.begin();
            for (; tit != (*it).tokens.end(); ++tit)
            {
                kit = m_tokenToKeyMap.find(*tit);
                if (kit != m_tokenToKeyMap.end())
                    m_tokenToKeyMap.erase(kit);
                list.push_back(*tit);
            }

            if (list.size() > 4)
            {
                QSet<QObject*>::iterator sit = m_listeners.begin();
                for (; sit != m_listeners.end(); ++sit)
                {
                    MythEvent *e = new MythEvent(me->Message(), list);
                    QCoreApplication::postEvent(*sit, e);
                }
                (*it).tokens.clear();
            }

            m_running = (m_running > 0) ? m_running - 1 : 0;
        }

        UpdatePreviewGeneratorThreads();

        return true;
    }
    return false;
}

/**
 * Send a message back to all objects that have requested creation of
 * a specific preview.
 *
 * \param[in] pginfo The program info structure from which the preview
 *            was generated.
 * \param[in] eventname The name of the event being sent to all
 * 	      listeners.  This will always be one of PREVIEW_FAILED,
 * 	      PREVIEW_QUEUED, or PREVIEW_SUCCESS.
 * \param[in] filename For a SUCCESS message, this is the name of the
 * 	      newly generated preview file. For a QUEUED message, this
 * 	      is an empty string. For a FAILED message, this will be
 * 	      the internal key value.
 * \param[in] token The token specified by the listener when it
 * 	      requested the preview generation.
 * \param[in] msg A text string giving more information about the
 *            processing of the event.
 * \param[in] dt For a PREVIEW_SUCCESS message, this is the the last
 * 	      modified time of the preview file.  For the other
 * 	      messages, this is a null datetime.
 */
void PreviewGeneratorQueue::SendEvent(
    const ProgramInfo &pginfo,
    const QString &eventname,
    const QString &filename, const QString &token, const QString &msg,
    const QDateTime &dt)
{
    QStringList list;
    list.push_back(QString::number(pginfo.GetRecordingID()));
    list.push_back(filename);
    list.push_back(msg);
    list.push_back(dt.toUTC().toString(Qt::ISODate));
    list.push_back(token);

    QMutexLocker locker(&m_lock);
    QSet<QObject*>::iterator it = m_listeners.begin();
    for (; it != m_listeners.end(); ++it)
    {
        MythEvent *e = new MythEvent(eventname, list);
        QCoreApplication::postEvent(*it, e);
    }
}

/** \brief Generate a preview image for the specified program.
 *
 * This function will find an existing preview image for a program, or
 * schedules the generation of a preview image if it doesn't exist.
 * If any of the arguments that could affect the generated image
 * (size, outputfile, time offset) have been specified, then a new
 * image will always be generated.
 *
 * \param pginfo Generate the image for this program.
 * \param size Generate a preview image of these dimensions.  If this
 * 	  is not specified the generated image will be the same size
 * 	  as the program.
 * \param outputfile Use this specific filename for the preview
 * 	  image. If empty, a default name will be created based on the
 * 	  program information.
 * \param time An offset from the start of the program. This can be
 *        seconds or frames. (See the next argument.)
 * \param in_seconds If true, the 'time' offset is in seconds. If
 *        false, the offset is in frames.
 * \param token An arbitrary string value used to match up this
 *        request with the response from the backend, and as a key for
 *        some indexing.  A token isn't required, but is strongly
 *        suggested.
 * \return The filename of the preview images. This will be null if
 *         the preview does not yet or will never exist.
 *
 * \note This function will always send one of three events.  If the
 * program has been marked for deletion, this function will send a
 * PREVIEW_FAILED event.  If a preview already exists, the function
 * sends a PREVIEW_SUCCESS event.  Otherwise it sends a PREVIEW_QUEUED
 * event.
 *
 * \warning This function should only be called from the preview
 * generation thread.
 */
QString PreviewGeneratorQueue::GeneratePreviewImage(
    ProgramInfo &pginfo,
    const QSize &size,
    const QString &outputfile,
    long long time, bool in_seconds,
    QString token)
{
    QString key = QString("%1_%2x%3_%4%5")
        .arg(pginfo.GetBasename()).arg(size.width()).arg(size.height())
        .arg(time).arg(in_seconds?"s":"f");

    if (pginfo.GetAvailableStatus() == asPendingDelete)
    {
        SendEvent(pginfo, "PREVIEW_FAILED", key, token,
                  "Pending Delete", QDateTime());
        return QString();
    }

    // keep in sync with default filename in PreviewGenerator::RunReal
    QString filename = (outputfile.isEmpty()) ?
        pginfo.GetPathname() + ".png" : outputfile;
    QString ret_file = filename;
    QString ret;

    bool is_special = !outputfile.isEmpty() || time >= 0 ||
        size.width() || size.height();

    bool needs_gen = true;
    if (!is_special)
    {
        QDateTime previewLastModified;
        bool streaming = !filename.startsWith("/");
        bool locally_accessible = false;
        bool bookmark_updated = false;

        QDateTime bookmark_ts = pginfo.GetBookmarkUpdate();
        QDateTime cmp_ts;
        if (bookmark_ts.isValid())
            cmp_ts = bookmark_ts;
        else if (MythDate::current() >= pginfo.GetRecordingEndTime())
            cmp_ts = pginfo.GetLastModifiedTime();
        else
            cmp_ts = pginfo.GetRecordingStartTime();

        if (streaming)
        {
            ret_file = QString("%1/%2")
                .arg(GetRemoteCacheDir()).arg(filename.section('/', -1));

            QFileInfo finfo(ret_file);
            if (finfo.isReadable() && finfo.lastModified() >= cmp_ts)
            {
                // This is just an optimization to avoid
                // hitting the backend if our cached copy
                // is newer than the bookmark, or if we have
                // a preview and do not update it when the
                // bookmark changes.
                previewLastModified = finfo.lastModified();
            }
            else if (!IsGeneratingPreview(key))
            {
                previewLastModified =
                    RemoteGetPreviewIfModified(pginfo, ret_file);
            }
        }
        else
        {
            QFileInfo fi(filename);
            if ((locally_accessible = fi.isReadable()))
                previewLastModified = fi.lastModified();
        }

        bookmark_updated =
            (!previewLastModified.isValid() || (previewLastModified <= cmp_ts));

        if (bookmark_updated && bookmark_ts.isValid() &&
            previewLastModified.isValid())
        {
            ClearPreviewGeneratorAttempts(key);
        }

        bool preview_exists = previewLastModified.isValid();

        if (0)
        {
            QString alttext = (bookmark_ts.isValid()) ? QString() :
                QString("\n\t\t\tcmp_ts:               %1")
                .arg(cmp_ts.toString(Qt::ISODate));
            LOG(VB_GENERAL, LOG_INFO,
                QString("previewLastModified:  %1\n\t\t\t"
                        "bookmark_ts:          %2%3\n\t\t\t"
                        "pginfo.lastmodified:  %4")
                    .arg(previewLastModified.toString(Qt::ISODate))
                    .arg(bookmark_ts.toString(Qt::ISODate))
                    .arg(alttext)
                    .arg(pginfo.GetLastModifiedTime(MythDate::ISODate)) +
                QString("Title: %1\n\t\t\t")
                    .arg(pginfo.toString(ProgramInfo::kTitleSubtitle)) +
                QString("File  '%1' \n\t\t\tCache '%2'")
                    .arg(filename).arg(ret_file) +
                QString("\n\t\t\tPreview Exists: %1, Bookmark Updated: %2, "
                        "Need Preview: %3")
                    .arg(preview_exists).arg(bookmark_updated)
                    .arg((bookmark_updated || !preview_exists)));
        }

        needs_gen = bookmark_updated || !preview_exists;

        if (!needs_gen)
        {
            if (locally_accessible)
                ret = filename;
            else if (preview_exists && QFileInfo(ret_file).isReadable())
                ret = ret_file;
        }
    }

    if (needs_gen && !IsGeneratingPreview(key))
    {
        uint attempts = IncPreviewGeneratorAttempts(key);
        if (attempts < m_maxAttempts)
        {
            LOG(VB_PLAYBACK, LOG_INFO, LOC +
                QString("Requesting preview for '%1'") .arg(key));
            PreviewGenerator *pg = new PreviewGenerator(&pginfo, token, m_mode);
            if (!outputfile.isEmpty() || time >= 0 ||
                size.width() || size.height())
            {
                pg->SetPreviewTime(time, in_seconds);
                pg->SetOutputFilename(outputfile);
                pg->SetOutputSize(size);
            }

            SetPreviewGenerator(key, pg);

            LOG(VB_PLAYBACK, LOG_INFO, LOC +
                QString("Requested preview for '%1'").arg(key));
        }
        else if (attempts >= m_maxAttempts)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("Attempted to generate preview for '%1' "
                        "%2 times; >= max(%3)")
                    .arg(key).arg(attempts).arg(m_maxAttempts));
        }
    }
    else if (needs_gen)
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC +
            QString("Not requesting preview for %1,"
                    "as it is already being generated")
                .arg(pginfo.toString(ProgramInfo::kTitleSubtitle)));
        IncPreviewGeneratorPriority(key, token);
    }

    UpdatePreviewGeneratorThreads();

    if (!ret.isEmpty())
    {
        QString msg = "On Disk";
        QDateTime dt = QFileInfo(ret).lastModified();
        SendEvent(pginfo, "PREVIEW_SUCCESS", ret, token, msg, dt);
    }
    else
    {
        uint queue_depth, token_cnt;
        GetInfo(key, queue_depth, token_cnt);
        QString msg = QString("Queue depth %1, our tokens %2")
            .arg(queue_depth).arg(token_cnt);
        SendEvent(pginfo, "PREVIEW_QUEUED", ret, token, msg, QDateTime());
    }

    return ret;
}

/**
 * \param[in] key The name of the specific preview being
 * 	      generated. Keys are generated internally to this file
 * 	      and are in the form \<basenane\>_\<w\>x\<h\>_\<offset\>.
 *
 * \param[out] queue_depth The total number of items in the queue to
 * 	       be processed.  This number does not include previews
 * 	       that are currently in process of being generated.
 *
 * \param[out] token_cnt The number of listeners that are waiting for
 *             this preview to be generated.
 */
void PreviewGeneratorQueue::GetInfo(
    const QString &key, uint &queue_depth, uint &token_cnt)
{
    QMutexLocker locker(&m_lock);
    queue_depth = m_queue.size();
    PreviewMap::iterator pit = m_previewMap.find(key);
    token_cnt = (pit == m_previewMap.end()) ? 0 : (*pit).tokens.size();
}

/**
 * \param[in] key The name of the specific preview being
 * 	      generated. Keys are generated internally to this file
 * 	      and are in the form \<basenane\>_\<w\>x\<h\>_\<offset\>.
 *
 * \param[in] token
 */
void PreviewGeneratorQueue::IncPreviewGeneratorPriority(
    const QString &key, QString token)
{
    QMutexLocker locker(&m_lock);
    m_queue.removeAll(key);

    PreviewMap::iterator pit = m_previewMap.find(key);
    if (pit == m_previewMap.end())
        return;

    if ((*pit).gen && !(*pit).genStarted)
        m_queue.push_back(key);

    if (!token.isEmpty())
    {
        m_tokenToKeyMap[token] = key;
        (*pit).tokens.insert(token);
    }
}

/**
 * As long as there are items in the queue, make sure we're running
 * the maximum allowed number of preview generators.
 */
void PreviewGeneratorQueue::UpdatePreviewGeneratorThreads(void)
{
    QMutexLocker locker(&m_lock);
    QStringList &q = m_queue;
    if (!q.empty() && (m_running < m_maxThreads))
    {
        QString fn = q.back();
        q.pop_back();
        PreviewMap::iterator it = m_previewMap.find(fn);
        if (it != m_previewMap.end() && (*it).gen && !(*it).genStarted)
        {
            m_running++;
            (*it).gen->start();
            (*it).genStarted = true;
        }
    }
}

/** \brief Sets the PreviewGenerator for a specific file.
 *
 * \param[in] key The name of the specific preview being
 * 	      generated. Keys are generated internally to this file
 * 	      and are in the form \<basenane\>_\<w\>x\<h\>_\<offset\>.
 *
 * \param[in] g
 */
void PreviewGeneratorQueue::SetPreviewGenerator(
    const QString &key, PreviewGenerator *g)
{
    if (!g)
        return;

    {
        QMutexLocker locker(&m_lock);
        m_tokenToKeyMap[g->GetToken()] = key;
        PreviewGenState &state = m_previewMap[key];
        if (state.gen)
        {
            if (g && state.gen != g)
            {
                if (!g->GetToken().isEmpty())
                    state.tokens.insert(g->GetToken());
                g->deleteLater();
                g = NULL;
            }
        }
        else
        {
            g->AttachSignals(this);
            state.gen = g;
            state.genStarted = false;
            if (!g->GetToken().isEmpty())
                state.tokens.insert(g->GetToken());
        }
    }

    IncPreviewGeneratorPriority(key, "");
}

/**
 * Is a preview currently being generated for this key.  This returns
 * true if a PreviewGenerator has been started to create this preview.
 * It also returns true if the previous generation attempt failed, and
 * we're still in the blocking interval before another attempt is
 * allowed.
 *
 * \param[in] key The name of the specific preview being
 * 	      generated. Keys are generated internally to this file
 * 	      and are in the form \<basenane\>_\<w\>x\<h\>_\<offset\>.
 *
 * \return True if generating a preview or blocked. False if a new
 * attempt is allowed.
 */
bool PreviewGeneratorQueue::IsGeneratingPreview(const QString &key) const
{
    PreviewMap::const_iterator it;
    QMutexLocker locker(&m_lock);

    if ((it = m_previewMap.find(key)) == m_previewMap.end())
        return false;

    if ((*it).blockRetryUntil.isValid())
        return MythDate::current() < (*it).blockRetryUntil;

    return (*it).gen;
}

/**
 * \brief Increments and returns number of times we have
 *        started a PreviewGenerator to create this file.
 *
 * \param[in] key The name of the specific preview being
 * 	      generated. Keys are generated internally to this file
 * 	      and are in the form \<basenane\>_\<w\>x\<h\>_\<offset\>.
 */
uint PreviewGeneratorQueue::IncPreviewGeneratorAttempts(const QString &key)
{
    QMutexLocker locker(&m_lock);
    return m_previewMap[key].attempts++;
}

/**
 * Clears the number of times we have started a PreviewGenerator to
 * create this file.  This also resets the the blocking information so
 * that the next preview request for this file may run immediately.
 *
 * \param[in] key The name of the specific preview being
 * 	      generated. Keys are generated internally to this file
 * 	      and are in the form \<basenane\>_\<w\>x\<h\>_\<offset\>.
 */
void PreviewGeneratorQueue::ClearPreviewGeneratorAttempts(const QString &key)
{
    QMutexLocker locker(&m_lock);
    m_previewMap[key].attempts = 0;
    m_previewMap[key].lastBlockTime = 0;
    m_previewMap[key].blockRetryUntil =
        MythDate::current().addSecs(-60);
}


/**
 * \addtogroup myth_network_protocol
 * \par GET_PREVIEW \<programinfo\> \e token \e width \e height \e outputfile \e time \e time_fmt
 */
/**
 * \addtogroup myth_network_protocol
 * \par PREVIEW_SUCCESS \e recordingId \e outFileName \e msg \e datetime \e token
 */
/**
 * \addtogroup myth_network_protocol
 * \par PREVIEW_QUEUED \e recordingId \e outFileName \e msg \e datetime \e token
 */
/**
 * \addtogroup myth_network_protocol
 * \par PREVIEW_FAILED \e recordingId \e outFileName \e msg \e datetime \e token
 */
