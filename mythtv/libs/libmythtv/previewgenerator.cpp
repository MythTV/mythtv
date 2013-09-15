// C headers
#include <cmath>

// POSIX headers
#include <sys/types.h> // for utime
#include <sys/time.h>
#include <fcntl.h>
#include <utime.h>     // for utime

// Qt headers
#include <QCoreApplication>
#include <QTemporaryFile>
#include <QFileInfo>
#include <QMetaType>
#include <QImage>
#include <QDir>
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
 *   generation. When called by the backend 'local_only' should be set
 *   to true, otherwise the backend may deadlock if the PreviewGenerator
 *   cannot find the file.
 *
 *  \param pginfo     ProgramInfo for the recording we want a preview of.
 *  \param local_only If set to true, the preview will only be generated
 *                    if the file is local.
 */
PreviewGenerator::PreviewGenerator(const ProgramInfo *pginfo,
                                   const QString     &_token,
                                   PreviewGenerator::Mode _mode)
    : MThread("PreviewGenerator"),
      programInfo(*pginfo), mode(_mode), listener(NULL),
      pathname(pginfo->GetPathname()),
      timeInSeconds(true),  captureTime(-1),  outFileName(QString::null),
      outSize(0,0), token(_token), gotReply(false), pixmapOk(false)
{
}

PreviewGenerator::~PreviewGenerator()
{
    TeardownAll();
    wait();
}

void PreviewGenerator::SetOutputFilename(const QString &fileName)
{
    outFileName = fileName;
}

void PreviewGenerator::TeardownAll(void)
{
    QMutexLocker locker(&previewLock);
    previewWaitCondition.wakeAll();
    listener = NULL;
}

void PreviewGenerator::deleteLater()
{
    TeardownAll();
    QObject::deleteLater();
}

void PreviewGenerator::AttachSignals(QObject *obj)
{
    QMutexLocker locker(&previewLock);
    listener = obj;
}

/** \fn PreviewGenerator::RunReal(void)
 *  \brief This call creates a preview without starting a new thread.
 */
bool PreviewGenerator::RunReal(void)
{
    QString msg;
    QTime tm = QTime::currentTime();
    bool ok = false;
    bool is_local = IsLocal();

    if (!is_local && !!(mode & kRemote))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("RunReal() file not local: '%1'")
            .arg(pathname));
    }
    else if (!(mode & kLocal) && !(mode & kRemote))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("RunReal() Preview of '%1' failed "
                    "because mode was invalid 0x%2")
            .arg(pathname).arg((int)mode,0,16));
    }
    else if (!!(mode & kLocal) && LocalPreviewRun())
    {
        ok = true;
        msg = QString("Generated on %1 in %2 seconds, starting at %3")
            .arg(gCoreContext->GetHostName())
            .arg(tm.elapsed()*0.001)
            .arg(tm.toString(Qt::ISODate));
    }
    else if (!!(mode & kRemote))
    {
        if (is_local && (mode & kLocal))
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
                .arg(tm.elapsed()*0.001)
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

    QMutexLocker locker(&previewLock);
    if (listener)
    {
        QString output_fn = outFileName.isEmpty() ?
            (programInfo.GetPathname()+".png") : outFileName;

        QDateTime dt;
        if (ok)
        {
            QFileInfo fi(output_fn);
            if (fi.exists())
                dt = fi.lastModified();
        }

        QString message = (ok) ? "PREVIEW_SUCCESS" : "PREVIEW_FAILED";
        QStringList list;
        list.push_back(programInfo.MakeUniqueKey());
        list.push_back(output_fn);
        list.push_back(msg);
        list.push_back(dt.isValid()?dt.toString(Qt::ISODate):"");
        list.push_back(token);
        QCoreApplication::postEvent(listener, new MythEvent(message, list));
    }

    return ok;
}

bool PreviewGenerator::Run(void)
{
    QString msg;
    QDateTime dtm = MythDate::current();
    QTime tm = QTime::currentTime();
    bool ok = false;
    QString command = GetAppBinDir() + "mythpreviewgen";
    bool local_ok = ((IsLocal() || !!(mode & kForceLocal)) &&
                     (!!(mode & kLocal)) &&
                     QFileInfo(command).isExecutable());
    if (!local_ok)
    {
        if (!!(mode & kRemote))
        {
            ok = RemotePreviewRun();
            if (ok)
            {
                msg =
                    QString("Generated remotely in %1 seconds, starting at %2")
                    .arg(tm.elapsed()*0.001)
                    .arg(tm.toString(Qt::ISODate));
            }
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("Run() cannot generate preview locally for: '%1'")
                    .arg(pathname));
            msg = "Failed, local preview requested for remote file.";
        }
    }
    else
    {
        // This is where we fork and run mythpreviewgen to actually make preview
        QStringList cmdargs;

        cmdargs << "--size"
                << QString("%1x%2").arg(outSize.width()).arg(outSize.height());
        if (captureTime >= 0)
        {
            if (timeInSeconds)
                cmdargs << "--seconds";
            else
                cmdargs << "--frame";
            cmdargs << QString::number(captureTime);
        }
        cmdargs << "--chanid"
                << QString::number(programInfo.GetChanID())
                << "--starttime"
                << programInfo.GetRecordingStartTime(MythDate::kFilename);

        if (!outFileName.isEmpty())
            cmdargs << "--outfile" << outFileName;

        // Timeout in 30s
        MythSystemLegacy *ms = new MythSystemLegacy(command, cmdargs,
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
                QString("Encountered problems running '%1' (%2)")
                    .arg(command) .arg(ret));
        }
        else
        {
            LOG(VB_PLAYBACK, LOG_INFO, LOC + "Preview process returned 0.");
            QString outname = (!outFileName.isEmpty()) ?
                outFileName : (pathname + ".png");

            QString lpath = QFileInfo(outname).fileName();
            if (lpath == outname)
            {
                StorageGroup sgroup;
                QString tmpFile = sgroup.FindFile(lpath);
                outname = (tmpFile.isEmpty()) ? outname : tmpFile;
            }

            QFileInfo fi(outname);
            ok = (fi.exists() && fi.isReadable() && fi.size());
            if (ok)
            {
                LOG(VB_PLAYBACK, LOG_INFO, LOC + "Preview process ran ok.");
                msg = QString("Generated on %1 in %2 seconds, starting at %3")
                    .arg(gCoreContext->GetHostName())
                    .arg(tm.elapsed()*0.001)
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

    QMutexLocker locker(&previewLock);

    // Backdate file to start of preview time in case a bookmark was made
    // while we were generating the preview.
    QString output_fn = outFileName.isEmpty() ?
        (programInfo.GetPathname()+".png") : outFileName;

    QDateTime dt;
    if (ok)
    {
        QFileInfo fi(output_fn);
        if (fi.exists())
            dt = fi.lastModified();
    }

    QString message = (ok) ? "PREVIEW_SUCCESS" : "PREVIEW_FAILED";
    if (listener)
    {
        QStringList list;
        list.push_back(programInfo.MakeUniqueKey());
        list.push_back(outFileName.isEmpty() ?
                       (programInfo.GetPathname()+".png") : outFileName);
        list.push_back(msg);
        list.push_back(dt.isValid()?dt.toString(Qt::ISODate):"");
        list.push_back(token);
        QCoreApplication::postEvent(listener, new MythEvent(message, list));
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
    if (token.isEmpty())
    {
        token = QString("%1:%2")
            .arg(programInfo.MakeUniqueKey()).arg(random());
    }
    strlist.push_back(token);
    programInfo.ToStringList(strlist);
    strlist.push_back(timeInSeconds ? "s" : "f");
    strlist.push_back(QString::number(captureTime));
    if (outFileName.isEmpty())
    {
        strlist.push_back("<EMPTY>");
    }
    else
    {
        QFileInfo fi(outFileName);
        strlist.push_back(fi.fileName());
    }
    strlist.push_back(QString::number(outSize.width()));
    strlist.push_back(QString::number(outSize.height()));

    gCoreContext->addListener(this);
    pixmapOk = false;

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

    QMutexLocker locker(&previewLock);

    // wait up to 35 seconds for the preview to complete
    // The backend waits 30s for creation
    if (!gotReply)
        previewWaitCondition.wait(&previewLock, 35 * 1000);

    if (!gotReply)
        LOG(VB_GENERAL, LOG_NOTICE, LOC + "RemotePreviewRun() -- no reply..");

    gCoreContext->removeListener(this);

    return pixmapOk;
}

bool PreviewGenerator::event(QEvent *e)
{
    if (e->type() != (QEvent::Type) MythEvent::MythEventMessage)
        return QObject::event(e);

    MythEvent *me = (MythEvent*)e;
    if (me->Message() != "GENERATED_PIXMAP" || me->ExtraDataCount() < 3)
        return QObject::event(e);
            
    bool ok = me->ExtraData(0) == "OK";
    bool ours = false;
    uint i = ok ? 4 : 3;
    for (; i < (uint) me->ExtraDataCount() && !ours; i++)
        ours |= me->ExtraData(i) == token;
    if (!ours)
        return false;

    QString pginfokey = me->ExtraData(1);

    QMutexLocker locker(&previewLock);
    gotReply = true;
    pixmapOk = ok;
    if (!ok)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + pginfokey + ": " + me->ExtraData(2));
        previewWaitCondition.wakeAll();
        return true;
    }

    if (me->ExtraDataCount() < 5)
    {
        pixmapOk = false;
        previewWaitCondition.wakeAll();
        return true; // could only happen with very broken client...
    }

    QDateTime datetime = MythDate::fromString(me->ExtraData(3));
    if (!datetime.isValid())
    {
        pixmapOk = false;
        LOG(VB_GENERAL, LOG_ERR, LOC + pginfokey + "Got invalid date");
        previewWaitCondition.wakeAll();
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

    pixmapOk = (data.isEmpty()) ? false : SaveOutFile(data, datetime);

    previewWaitCondition.wakeAll();

    return true;
}

bool PreviewGenerator::SaveOutFile(const QByteArray &data, const QDateTime &dt)
{
    if (outFileName.isEmpty())
    {
        QString remotecachedirname =
            QString("%1/remotecache").arg(GetConfDir());
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

        QString filename = programInfo.GetBasename() + ".png";
        outFileName = QString("%1/%2").arg(remotecachedirname).arg(filename);
    }

    QFile file(outFileName);
    bool ok = file.open(QIODevice::Unbuffered|QIODevice::WriteOnly);
    if (!ok)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Failed to open: '%1'")
                .arg(outFileName));
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
        struct utimbuf times;
        times.actime = times.modtime = dt.toTime_t();
        utime(outFileName.toLocal8Bit().constData(), &times);
        LOG(VB_FILE, LOG_INFO, LOC + QString("Saved: '%1'").arg(outFileName));
    }
    else
    {
        file.remove();
    }

    return ok;
}

bool PreviewGenerator::SavePreview(QString filename,
                                   const unsigned char *data,
                                   uint width, uint height, float aspect,
                                   int desired_width, int desired_height)
{
    if (!data || !width || !height)
        return false;

    const QImage img((unsigned char*) data,
                     width, height, QImage::Format_RGB32);

    float ppw = max(desired_width, 0);
    float pph = max(desired_height, 0);
    bool desired_size_exactly_specified = true;
    if ((ppw < 1.0f) && (pph < 1.0f))
    {
        ppw = gCoreContext->GetNumSetting("PreviewPixmapWidth",  320);
        pph = gCoreContext->GetNumSetting("PreviewPixmapHeight", 240);
        desired_size_exactly_specified = false;
    }

    aspect = (aspect <= 0.0f) ? ((float) width) / height : aspect;
    pph = (pph < 1.0f) ? (ppw / aspect) : pph;
    ppw = (ppw < 1.0f) ? (pph * aspect) : ppw;

    if (!desired_size_exactly_specified)
    {
        if (aspect > ppw / pph)
            pph = (ppw / aspect);
        else
            ppw = (pph * aspect);
    }

    ppw = max(1.0f, ppw);
    pph = max(1.0f, pph);;

    QImage small_img = img.scaled((int) ppw, (int) pph,
        Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    QTemporaryFile f(QFileInfo(filename).absoluteFilePath()+".XXXXXX");
    f.setAutoRemove(false);
    if (f.open() && small_img.save(&f, "PNG"))
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
    programInfo.MarkAsInUse(true, kPreviewGeneratorInUseID);

    float aspect = 0;
    int   width, height, sz;
    long long captime = captureTime;

    QDateTime dt = MythDate::current();

    if (captime > 0)
        LOG(VB_GENERAL, LOG_INFO, "Preview from time spec");

    if (captime < 0)
    {
        captime = programInfo.QueryBookmark();
        if (captime > 0)
        {
            timeInSeconds = false;
            LOG(VB_GENERAL, LOG_INFO,
                QString("Preview from bookmark (frame %1)").arg(captime));
        }
        else
            captime = -1;
    }

    if (captime <= 0)
    {
        timeInSeconds = true;
        int startEarly = 0;
        int programDuration = 0;
        int preroll =  gCoreContext->GetNumSetting("RecordPreRoll", 0);
        if (programInfo.GetScheduledStartTime().isValid() &&
            programInfo.GetScheduledEndTime().isValid() &&
            (programInfo.GetScheduledStartTime() !=
             programInfo.GetScheduledEndTime()))
        {
            programDuration = programInfo.GetScheduledStartTime()
                .secsTo(programInfo.GetScheduledEndTime());
        }
        if (programInfo.GetRecordingStartTime().isValid() &&
            programInfo.GetScheduledStartTime().isValid() &&
            (programInfo.GetRecordingStartTime() !=
             programInfo.GetScheduledStartTime()))
        {
            startEarly = programInfo.GetRecordingStartTime()
                .secsTo(programInfo.GetScheduledStartTime());
        }
        if (programDuration > 0)
        {
            captime = startEarly + (programDuration / 3);
        }
        if (captime < 0)
            captime = 600;
        captime += preroll;
        LOG(VB_GENERAL, LOG_INFO,
            QString("Preview at calculated offset (%1 seconds)").arg(captime));
    }

    width = height = sz = 0;
    unsigned char *data = (unsigned char*)
        GetScreenGrab(programInfo, pathname,
                      captime, timeInSeconds,
                      sz, width, height, aspect);

    QString outname = CreateAccessibleFilename(pathname, outFileName);

    int dw = (outSize.width()  < 0) ? width  : outSize.width();
    int dh = (outSize.height() < 0) ? height : outSize.height();

    bool ok = SavePreview(outname, data, width, height, aspect, dw, dh);

    if (ok)
    {
        // Backdate file to start of preview time in case a bookmark was made
        // while we were generating the preview.
        struct utimbuf times;
        times.actime = times.modtime = dt.toTime_t();
        utime(outname.toLocal8Bit().constData(), &times);
    }

    delete[] data;

    programInfo.MarkAsInUse(false, kPreviewGeneratorInUseID);

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
        QString dir = QString::null;
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
    QString tmppathname = pathname;

    if (tmppathname.startsWith("dvd:"))
        tmppathname = tmppathname.section(":", 1, 1);

    if (!QFileInfo(tmppathname).isReadable())
        return false;

    tmppathname = outFileName.isEmpty() ? tmppathname : outFileName;
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
 *  \brief Returns a PIX_FMT_RGBA32 buffer containg a frame from the video.
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
 *          successful, NULL otherwise.
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
    char *retbuf = NULL;
    bufferlen = 0;

    if (!MSqlQuery::testDBConnection())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Previewer could not connect to DB.");
        return NULL;
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
            return NULL;
        }
    }

    RingBuffer *rbuf = RingBuffer::Create(filename, false, false, 0);
    if (!rbuf->IsOpen())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Previewer could not open file: " +
                QString("'%1'").arg(filename));
        delete rbuf;
        return NULL;
    }

    PlayerContext *ctx = new PlayerContext(kPreviewGeneratorInUseID);
    ctx->SetRingBuffer(rbuf);
    ctx->SetPlayingInfo(&pginfo);
    ctx->SetPlayer(new MythPlayer((PlayerFlags)(kAudioMuted | kVideoIsNull)));
    ctx->player->SetPlayerInfo(NULL, NULL, ctx);

    if (time_in_secs)
        retbuf = ctx->player->GetScreenGrab(seektime, bufferlen,
                                    video_width, video_height, video_aspect);
    else
        retbuf = ctx->player->GetScreenGrabAtFrame(
            seektime, true, bufferlen,
            video_width, video_height, video_aspect);

    delete ctx;

    if (retbuf)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("Grabbed preview '%0' %1x%2@%3%4")
                .arg(filename).arg(video_width).arg(video_height)
                .arg(seektime).arg((time_in_secs) ? "s" : "f"));
    }

    return retbuf;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
