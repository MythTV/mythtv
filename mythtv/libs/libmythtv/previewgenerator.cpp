// C headers
#include <cmath>
#include <utility>

// POSIX headers
#include <sys/types.h> // for utime
#include <sys/time.h>
#include <fcntl.h>
#include <utime.h>     // for utime

// Qt headers
#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QMetaType>
#include <QTemporaryFile>
#include <QUrl>

// MythTV headers
#include "mythconfig.h"

#include "ringbuffer.h"
#include "mythplayer.h"
#include "previewgenerator.h"
#include "tv_rec.h"
#include "mythsocket.h"
#include "remotefile.h"
#include "storagegroup.h"
#include "mythdate.h"
#include "playercontext.h"
#include "mythdirs.h"
#include "remoteutil.h"
#include "mythsystemlegacy.h"
#include "exitcodes.h"
#include "mythlogging.h"
#include "mythmiscutil.h"

#define LOC QString("Preview: ")

/** \class PreviewGenerator
 *  \brief This class creates a preview image of a recording.
 *
 *   The usage is simple: First, pass a ProgramInfo whose pathname points
 *   to a local or remote recording to the constructor. Then call either
 *   start(void) or Run(void) to generate the preview.
 *
 *   start(void) will create a thread that processes the request.
 *
 *   Run(void) will block until the preview completes.
 *
 *   The PreviewGenerator will send a PREVIEW_SUCCESS or a
 *   PREVIEW_FAILED event when the preview completes or fails.
 */

/**
 *  \brief Constructor
 *
 *   ProgramInfo::pathname must include recording prefix, so that
 *   the file can be found on the file system for local preview
 *   generation. When called by the backend 'mode' should be set to
 *   kLocal, otherwise the backend may deadlock if the PreviewGenerator
 *   cannot find the file.
 *
 *  \param pginfo     ProgramInfo for the recording we want a preview of.
 *  \param token      A token value used to match up this request with
 *                    the response from the backend. If a token isn't
 *                    provided, the code will generate one.
 *  \param mode       Specify the location where the file must exist
 *                    in order to be processed.
 */
PreviewGenerator::PreviewGenerator(const ProgramInfo *pginfo,
                                   QString            token,
                                   PreviewGenerator::Mode mode)
    : MThread("PreviewGenerator"),
      m_programInfo(*pginfo), m_mode(mode),
      m_pathname(pginfo->GetPathname()),
      m_token(std::move(token))
{
    // Qt requires that a receiver have the same thread affinity as the QThread
    // sending the event, which is used to dispatch MythEvents sent by
    // gCoreContext->dispatchNow(me)
    moveToThread(QApplication::instance()->thread());
}

PreviewGenerator::~PreviewGenerator()
{
    TeardownAll();
    wait();
}

void PreviewGenerator::SetOutputFilename(const QString &fileName)
{
    if (fileName.isEmpty())
        return;
    QFileInfo fileinfo = QFileInfo(fileName);
    m_outFileName = fileName;
    m_outFormat = fileinfo.suffix().toUpper();
}

void PreviewGenerator::TeardownAll(void)
{
    QMutexLocker locker(&m_previewLock);
    m_previewWaitCondition.wakeAll();
    m_listener = nullptr;
}

void PreviewGenerator::deleteLater()
{
    TeardownAll();
    QObject::deleteLater();
}

void PreviewGenerator::AttachSignals(QObject *obj)
{
    QMutexLocker locker(&m_previewLock);
    m_listener = obj;
}

/** \fn PreviewGenerator::RunReal(void)
 *  \brief This call creates a preview without starting a new thread.
 */
bool PreviewGenerator::RunReal(void)
{
    QString msg;
    QTime tm = QTime::currentTime();
    QElapsedTimer te; te.start();
    bool ok = false;
    bool is_local = IsLocal();

    if (!is_local && !!(m_mode & kRemote))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("RunReal() file not local: '%1'")
            .arg(m_pathname));
    }
    else if (!(m_mode & kLocal) && !(m_mode & kRemote))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("RunReal() Preview of '%1' failed "
                    "because mode was invalid 0x%2")
            .arg(m_pathname).arg((int)m_mode,0,16));
    }
    else if (!!(m_mode & kLocal) && LocalPreviewRun())
    {
        ok = true;
        msg = QString("Generated on %1 in %2 seconds, starting at %3")
            .arg(gCoreContext->GetHostName())
            .arg(te.elapsed()*0.001)
            .arg(tm.toString(Qt::ISODate));
    }
    else if (!!(m_mode & kRemote))
    {
        if (is_local && (m_mode & kLocal))
        {
            LOG(VB_GENERAL, LOG_WARNING, LOC + "Failed to save preview."
                    "\n\t\t\tYou may need to check user and group ownership on"
                    "\n\t\t\tyour frontend and backend for quicker previews.\n"
                    "\n\t\t\tAttempting to regenerate preview on backend.\n");
        }
        ok = RemotePreviewRun();
        if (ok)
        {
            msg = QString("Generated remotely in %1 seconds, starting at %2")
                .arg(te.elapsed()*0.001)
                .arg(tm.toString(Qt::ISODate));
        }
        else
        {
            msg = "Remote preview failed";
        }
    }
    else
    {
        msg = "Could not access recording";
    }

    QMutexLocker locker(&m_previewLock);
    if (m_listener)
    {
        // keep in sync with default filename in
        // PreviewGeneratorQueue::GeneratePreviewImage
        QString output_fn = m_outFileName.isEmpty() ?
            (m_programInfo.GetPathname()+".png") : m_outFileName;

        QDateTime dt;
        if (ok)
        {
            QFileInfo fi(output_fn);
            if (fi.exists())
                dt = fi.lastModified();
        }

        QString message = (ok) ? "PREVIEW_SUCCESS" : "PREVIEW_FAILED";
        QStringList list;
        list.push_back(QString::number(m_programInfo.GetRecordingID()));
        list.push_back(output_fn);
        list.push_back(msg);
        list.push_back(dt.isValid()?dt.toUTC().toString(Qt::ISODate):"");
        list.push_back(m_token);
        QCoreApplication::postEvent(m_listener, new MythEvent(message, list));
    }

    return ok;
}

bool PreviewGenerator::Run(void)
{
    QString msg;
    QTime tm = QTime::currentTime();
    QElapsedTimer te; te.start();
    bool ok = false;
    QString command = GetAppBinDir() + "mythpreviewgen";
    bool local_ok = ((IsLocal() || ((m_mode & kForceLocal) != 0)) &&
                     ((m_mode & kLocal) != 0) &&
                     QFileInfo(command).isExecutable());
    if (!local_ok)
    {
        if (!!(m_mode & kRemote))
        {
            ok = RemotePreviewRun();
            if (ok)
            {
                msg =
                    QString("Generated remotely in %1 seconds, starting at %2")
                    .arg(te.elapsed()*0.001)
                    .arg(tm.toString(Qt::ISODate));
            }
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("Run() cannot generate preview locally for: '%1'")
                    .arg(m_pathname));
            msg = "Failed, local preview requested for remote file.";
        }
    }
    else
    {
        // This is where we fork and run mythpreviewgen to actually make preview
        QStringList cmdargs;

        cmdargs << "--size"
                << QString("%1x%2").arg(m_outSize.width()).arg(m_outSize.height());
        if (m_captureTime >= 0)
        {
            if (m_timeInSeconds)
                cmdargs << "--seconds";
            else
                cmdargs << "--frame";
            cmdargs << QString::number(m_captureTime);
        }
        cmdargs << "--chanid"
                << QString::number(m_programInfo.GetChanID())
                << "--starttime"
                << m_programInfo.GetRecordingStartTime(MythDate::kFilename);

        if (!m_outFileName.isEmpty())
            cmdargs << "--outfile" << m_outFileName;

        // Timeout in 30s
        auto *ms = new MythSystemLegacy(command, cmdargs,
                                        kMSDontBlockInputDevs |
                                        kMSDontDisableDrawing |
                                        kMSProcessEvents      |
                                        kMSAutoCleanup        |
                                        kMSPropagateLogs);
        ms->SetNice(10);
        ms->SetIOPrio(7);

        ms->Run(30);
        uint ret = ms->Wait();
        delete ms;

        if (ret != GENERIC_EXIT_OK)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("Encountered problems running '%1 %2' - (%3)")
                    .arg(command).arg(cmdargs.join(" ")).arg(ret));
        }
        else
        {
            LOG(VB_PLAYBACK, LOG_INFO, LOC + "Preview process returned 0.");
            QString outname = (!m_outFileName.isEmpty()) ?
                m_outFileName : (m_pathname + ".png");

            QString lpath = QFileInfo(outname).fileName();
            if (lpath == outname)
            {
                StorageGroup sgroup;
                QString tmpFile = sgroup.FindFile(lpath);
                outname = (tmpFile.isEmpty()) ? outname : tmpFile;
            }

            QFileInfo fi(outname);
            ok = (fi.exists() && fi.isReadable() && (fi.size() != 0));
            if (ok)
            {
                LOG(VB_PLAYBACK, LOG_INFO, LOC + "Preview process ran ok.");
                msg = QString("Generated on %1 in %2 seconds, starting at %3")
                    .arg(gCoreContext->GetHostName())
                    .arg(te.elapsed()*0.001)
                    .arg(tm.toString(Qt::ISODate));
            }
            else
            {
                LOG(VB_GENERAL, LOG_ERR, LOC + "Preview process not ok." +
                    QString("\n\t\t\tfileinfo(%1)").arg(outname) +
                    QString(" exists: %1").arg(fi.exists()) +
                    QString(" readable: %1").arg(fi.isReadable()) +
                    QString(" size: %1").arg(fi.size()));
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    QString("Despite command '%1' returning success")
                        .arg(command));
                msg = QString("Failed to read preview image despite "
                              "preview process returning success.");
            }
        }
    }

    QMutexLocker locker(&m_previewLock);

    // Backdate file to start of preview time in case a bookmark was made
    // while we were generating the preview.
    QString output_fn = m_outFileName.isEmpty() ?
        (m_programInfo.GetPathname()+".png") : m_outFileName;

    QDateTime dt;
    if (ok)
    {
        QFileInfo fi(output_fn);
        if (fi.exists())
            dt = fi.lastModified();
    }

    QString message = (ok) ? "PREVIEW_SUCCESS" : "PREVIEW_FAILED";
    if (m_listener)
    {
        QStringList list;
        list.push_back(QString::number(m_programInfo.GetRecordingID()));
        list.push_back(output_fn);
        list.push_back(msg);
        list.push_back(dt.isValid()?dt.toUTC().toString(Qt::ISODate):"");
        list.push_back(m_token);
        QCoreApplication::postEvent(m_listener, new MythEvent(message, list));
    }

    return ok;
}

void PreviewGenerator::run(void)
{
    RunProlog();
    Run();
    RunEpilog();
}

bool PreviewGenerator::RemotePreviewRun(void)
{
    QStringList strlist( "QUERY_GENPIXMAP2" );
    if (m_token.isEmpty())
    {
        m_token = QString("%1:%2")
            .arg(m_programInfo.MakeUniqueKey()).arg(random());
    }
    strlist.push_back(m_token);
    m_programInfo.ToStringList(strlist);
    strlist.push_back(m_timeInSeconds ? "s" : "f");
    strlist.push_back(QString::number(m_captureTime));
    if (m_outFileName.isEmpty())
    {
        strlist.push_back("<EMPTY>");
    }
    else
    {
        QFileInfo fi(m_outFileName);
        strlist.push_back(fi.fileName());
    }
    strlist.push_back(QString::number(m_outSize.width()));
    strlist.push_back(QString::number(m_outSize.height()));

    gCoreContext->addListener(this);
    m_pixmapOk = false;

    bool ok = gCoreContext->SendReceiveStringList(strlist);
    if (!ok || strlist.empty() || (strlist[0] != "OK"))
    {
        if (!ok)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                "Remote Preview failed due to communications error.");
        }
        else if (strlist.size() > 1)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                "Remote Preview failed, reason given: " + strlist[1]);
        }

        gCoreContext->removeListener(this);

        return false;
    }

    QMutexLocker locker(&m_previewLock);

    // wait up to 35 seconds for the preview to complete
    // The backend waits 30s for creation
    if (!m_gotReply)
        m_previewWaitCondition.wait(&m_previewLock, 35 * 1000);

    if (!m_gotReply)
        LOG(VB_GENERAL, LOG_NOTICE, LOC + "RemotePreviewRun() -- no reply..");

    gCoreContext->removeListener(this);

    return m_pixmapOk;
}

bool PreviewGenerator::event(QEvent *e)
{
    if (e->type() != MythEvent::MythEventMessage)
        return QObject::event(e);

    auto *me = dynamic_cast<MythEvent*>(e);
    if (me == nullptr)
        return false;
    if (me->Message() != "GENERATED_PIXMAP" || me->ExtraDataCount() < 3)
        return QObject::event(e);

    bool ok = me->ExtraData(0) == "OK";
    bool ours = false;
    uint i = ok ? 4 : 3;
    for (; i < (uint) me->ExtraDataCount() && !ours; i++)
        ours |= me->ExtraData(i) == m_token;
    if (!ours)
        return false;

    const QString& pginfokey = me->ExtraData(1);

    QMutexLocker locker(&m_previewLock);
    m_gotReply = true;
    m_pixmapOk = ok;
    if (!ok)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + pginfokey + ": " + me->ExtraData(2));
        m_previewWaitCondition.wakeAll();
        return true;
    }

    if (me->ExtraDataCount() < 5)
    {
        m_pixmapOk = false;
        m_previewWaitCondition.wakeAll();
        return true; // could only happen with very broken client...
    }

    QDateTime datetime = MythDate::fromString(me->ExtraData(3));
    if (!datetime.isValid())
    {
        m_pixmapOk = false;
        LOG(VB_GENERAL, LOG_ERR, LOC + pginfokey + "Got invalid date");
        m_previewWaitCondition.wakeAll();
        return false;
    }

    size_t     length     = me->ExtraData(4).toULongLong();
    quint16    checksum16 = me->ExtraData(5).toUInt();
    QByteArray data       = QByteArray::fromBase64(me->ExtraData(6).toLatin1());
    if ((size_t) data.size() < length)
    {   // (note data.size() may be up to 3
        //  bytes longer after decoding
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Preview size check failed %1 < %2")
                .arg(data.size()).arg(length));
        data.clear();
    }
    data.resize(length);

    if (checksum16 != qChecksum(data.constData(), data.size()))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Preview checksum failed");
        data.clear();
    }

    m_pixmapOk = (data.isEmpty()) ? false : SaveOutFile(data, datetime);

    m_previewWaitCondition.wakeAll();

    return true;
}

bool PreviewGenerator::SaveOutFile(const QByteArray &data, const QDateTime &dt)
{
    if (m_outFileName.isEmpty())
    {
        QString remotecachedirname =
            QString("%1/cache/remotecache").arg(GetConfDir());
        QDir remotecachedir(remotecachedirname);

        if (!remotecachedir.exists())
        {
            if (!remotecachedir.mkdir(remotecachedirname))
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    "Remote Preview failed because we could not create a "
                    "remote cache directory");
                return false;
            }
        }

        QString filename = m_programInfo.GetBasename() + ".png";
        SetOutputFilename(QString("%1/%2").arg(remotecachedirname).arg(filename));
    }

    QFile file(m_outFileName);
    bool ok = file.open(QIODevice::Unbuffered|QIODevice::WriteOnly);
    if (!ok)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Failed to open: '%1'")
                .arg(m_outFileName));
    }

    off_t offset = 0;
    size_t remaining = data.size();
    uint failure_cnt = 0;
    while ((remaining > 0) && (failure_cnt < 5))
    {
        ssize_t written = file.write(data.data() + offset, remaining);
        if (written < 0)
        {
            failure_cnt++;
            usleep(50000);
            continue;
        }

        failure_cnt  = 0;
        offset      += written;
        remaining   -= written;
    }

    if (ok && !remaining)
    {
        file.close();
        struct utimbuf times {};
#if QT_VERSION < QT_VERSION_CHECK(5,8,0)
        times.actime = times.modtime = dt.toTime_t();
#else
        times.actime = times.modtime = dt.toSecsSinceEpoch();
#endif
        utime(m_outFileName.toLocal8Bit().constData(), &times);
        LOG(VB_FILE, LOG_INFO, LOC + QString("Saved: '%1'").arg(m_outFileName));
    }
    else
    {
        file.remove();
    }

    return ok;
}

bool PreviewGenerator::SavePreview(const QString &filename,
                                   const unsigned char *data,
                                   uint width, uint height, float aspect,
                                   int desired_width, int desired_height,
                                   const QString &format)
{
    if (!data || !width || !height)
        return false;

    const QImage img((unsigned char*) data,
                     width, height, QImage::Format_RGB32);

    float ppw = max(desired_width, 0);
    float pph = max(desired_height, 0);
    bool desired_size_exactly_specified = true;
    if ((ppw < 1.0F) && (pph < 1.0F))
    {
        ppw = img.width();
        pph = img.height();
        desired_size_exactly_specified = false;
    }

    aspect = (aspect <= 0.0F) ? ((float) width) / height : aspect;
    pph = (pph < 1.0F) ? (ppw / aspect) : pph;
    ppw = (ppw < 1.0F) ? (pph * aspect) : ppw;

    if (!desired_size_exactly_specified)
    {
        if (aspect > ppw / pph)
            pph = (ppw / aspect);
        else
            ppw = (pph * aspect);
    }

    ppw = max(1.0F, ppw);
    pph = max(1.0F, pph);;

    QImage small_img = img.scaled((int) ppw, (int) pph,
        Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    QTemporaryFile f(QFileInfo(filename).absoluteFilePath()+".XXXXXX");
    f.setAutoRemove(false);
    if (f.open() && small_img.save(&f, format.toLocal8Bit().constData()))
    {
        // Let anybody update it
        bool ret = makeFileAccessible(f.fileName().toLocal8Bit().constData());
        if (!ret)
        {
            LOG(VB_GENERAL, LOG_ERR, "Unable to change permissions on "
                                     "preview image. Backends and frontends "
                                     "running under different users will be "
                                     "unable to access it");
        }
        QFile of(filename);
        of.remove();
        if (f.rename(filename))
        {
            LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Saved preview '%0' %1x%2")
                    .arg(filename).arg((int) ppw).arg((int) pph));
            return true;
        }
        f.remove();
    }

    return false;
}

bool PreviewGenerator::LocalPreviewRun(void)
{
    m_programInfo.MarkAsInUse(true, kPreviewGeneratorInUseID);
    m_programInfo.SetIgnoreProgStart(true);
    m_programInfo.SetAllowLastPlayPos(false);

    float aspect = 0;
    long long captime = m_captureTime;

    QDateTime dt = MythDate::current();

    if (captime > 0)
        LOG(VB_GENERAL, LOG_INFO, "Preview from time spec");

    if (captime < 0)
    {
        captime = m_programInfo.QueryBookmark();
        if (captime > 0)
        {
            m_timeInSeconds = false;
            LOG(VB_GENERAL, LOG_INFO,
                QString("Preview from bookmark (frame %1)").arg(captime));
        }
        else
            captime = -1;
    }

    if (captime <= 0)
    {
        m_timeInSeconds = true;
        int startEarly = 0;
        int programDuration = 0;
        int preroll =  gCoreContext->GetNumSetting("RecordPreRoll", 0);
        if (m_programInfo.GetScheduledStartTime().isValid() &&
            m_programInfo.GetScheduledEndTime().isValid() &&
            (m_programInfo.GetScheduledStartTime() !=
             m_programInfo.GetScheduledEndTime()))
        {
            programDuration = m_programInfo.GetScheduledStartTime()
                .secsTo(m_programInfo.GetScheduledEndTime());
        }
        if (m_programInfo.GetRecordingStartTime().isValid() &&
            m_programInfo.GetScheduledStartTime().isValid() &&
            (m_programInfo.GetRecordingStartTime() !=
             m_programInfo.GetScheduledStartTime()))
        {
            startEarly = m_programInfo.GetRecordingStartTime()
                .secsTo(m_programInfo.GetScheduledStartTime());
        }
        if (programDuration > 0)
        {
            captime = programDuration / 3;
            if (captime > 600)
                captime = 600;
            captime += startEarly;
        }
        if (captime < 0)
            captime = 600;
        captime += preroll;
        LOG(VB_GENERAL, LOG_INFO,
            QString("Preview at calculated offset (%1 seconds)").arg(captime));
    }

    int width = 0;
    int height = 0;
    int sz = 0;
    auto *data = (unsigned char*) GetScreenGrab(m_programInfo, m_pathname,
                                                captime, m_timeInSeconds,
                                                sz, width, height, aspect);

    QString outname = CreateAccessibleFilename(m_pathname, m_outFileName);

    QString format = (m_outFormat.isEmpty()) ? "PNG" : m_outFormat;

    int dw = (m_outSize.width()  < 0) ? width  : m_outSize.width();
    int dh = (m_outSize.height() < 0) ? height : m_outSize.height();

    bool ok = SavePreview(outname, data, width, height, aspect, dw, dh,
                          format);

    if (ok)
    {
        // Backdate file to start of preview time in case a bookmark was made
        // while we were generating the preview.
        struct utimbuf times {};
#if QT_VERSION < QT_VERSION_CHECK(5,8,0)
        times.actime = times.modtime = dt.toTime_t();
#else
        times.actime = times.modtime = dt.toSecsSinceEpoch();
#endif
        utime(outname.toLocal8Bit().constData(), &times);
    }

    delete[] data;

    m_programInfo.MarkAsInUse(false, kPreviewGeneratorInUseID);

    return ok;
}

QString PreviewGenerator::CreateAccessibleFilename(
    const QString &pathname, const QString &outFileName)
{
    QString outname = pathname + ".png";

    if (outFileName.isEmpty())
        return outname;

    outname = outFileName;
    QFileInfo fi(outname);
    if (outname == fi.fileName())
    {
        QString dir;
        if (pathname.contains(':'))
        {
            QUrl uinfo(pathname);
            uinfo.setPath("");
            dir = uinfo.toString();
        }
        else
        {
            dir = QFileInfo(pathname).path();
        }
        outname = dir  + "/" + fi.fileName();
        LOG(VB_FILE, LOG_INFO, LOC + QString("outfile '%1' -> '%2'")
                .arg(outFileName).arg(outname));
    }

    return outname;
}

bool PreviewGenerator::IsLocal(void) const
{
    QString tmppathname = m_pathname;

    if (tmppathname.startsWith("dvd:"))
        tmppathname = tmppathname.section(":", 1, 1);

    if (!QFileInfo(tmppathname).isReadable())
        return false;

    tmppathname = m_outFileName.isEmpty() ? tmppathname : m_outFileName;
    QString pathdir = QFileInfo(tmppathname).path();

    if (!QFileInfo(pathdir).isWritable())
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
                QString("Output path '%1' is not writeable") .arg(pathdir));
        return false;
    }

    return true;
}

/**
 *  \brief Returns a AV_PIX_FMT_RGBA32 buffer containg a frame from the video.
 *
 *  \param pginfo       Recording to grab from.
 *  \param filename     File containing recording.
 *  \param seektime     Seconds or frames into the video to seek before
 *                      capturing a frame.
 *  \param time_in_secs if true time is in seconds, otherwise it is in frames.
 *  \param bufferlen    Returns size of buffer returned (in bytes).
 *  \param video_width  Returns width of frame grabbed.
 *  \param video_height Returns height of frame grabbed.
 *  \param video_aspect Returns aspect ratio of frame grabbed.
 *  \return Buffer allocated with new containing frame in RGBA32 format if
 *          successful, nullptr otherwise.
 */
char *PreviewGenerator::GetScreenGrab(
    const ProgramInfo &pginfo, const QString &filename,
    long long seektime, bool time_in_secs,
    int &bufferlen,
    int &video_width, int &video_height, float &video_aspect)
{
    (void) pginfo;
    (void) filename;
    (void) seektime;
    (void) time_in_secs;
    (void) bufferlen;
    (void) video_width;
    (void) video_height;
    char *retbuf = nullptr;
    bufferlen = 0;

    if (!MSqlQuery::testDBConnection())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Previewer could not connect to DB.");
        return nullptr;
    }

    // pre-test local files for existence and size. 500 ms speed-up...
    if (filename.startsWith("/"))
    {
        QFileInfo info(filename);
        bool invalid = (!info.exists() || !info.isReadable() ||
                        (info.isFile() && (info.size() < 8*1024)));
        if (invalid)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Previewer file " +
                    QString("'%1'").arg(filename) + " is not valid.");
            return nullptr;
        }
    }

    RingBuffer *rbuf = RingBuffer::Create(filename, false, false, 0);
    if (!rbuf || !rbuf->IsOpen())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Previewer could not open file: " +
                QString("'%1'").arg(filename));
        delete rbuf;
        return nullptr;
    }

    auto *ctx = new PlayerContext(kPreviewGeneratorInUseID);
    ctx->SetRingBuffer(rbuf);
    ctx->SetPlayingInfo(&pginfo);
    ctx->SetPlayer(new MythPlayer((PlayerFlags)(kAudioMuted | kVideoIsNull | kNoITV)));
    ctx->m_player->SetPlayerInfo(nullptr, nullptr, ctx);

    if (time_in_secs)
    {
        retbuf = ctx->m_player->GetScreenGrab(seektime, bufferlen,
                                    video_width, video_height, video_aspect);
    }
    else
    {
        retbuf = ctx->m_player->GetScreenGrabAtFrame(
            seektime, true, bufferlen,
            video_width, video_height, video_aspect);
    }

    delete ctx;

    if (retbuf)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("Grabbed preview '%0' %1x%2@%3%4")
                .arg(filename).arg(video_width).arg(video_height)
                .arg(seektime).arg((time_in_secs) ? "s" : "f"));
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Failed to grab preview '%0' %1x%2@%3%4")
            .arg(filename).arg(video_width).arg(video_height)
            .arg(seektime).arg((time_in_secs) ? "s" : "f"));
    }

    return retbuf;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
