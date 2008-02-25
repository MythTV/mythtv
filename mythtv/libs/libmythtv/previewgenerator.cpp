// C headers
#include <cmath>

// POSIX headers
#include <sys/types.h> // for stat
#include <sys/stat.h>  // for stat
#include <unistd.h>    // for stat
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

// Qt headers
#include <qfileinfo.h>
#include <qimage.h>
#include <qurl.h>

// MythTV headers
#include "RingBuffer.h"
#include "NuppelVideoPlayer.h"
#include "previewgenerator.h"
#include "tv_rec.h"
#include "mythsocket.h"
#include "remotefile.h"
#include "storagegroup.h"
#include "util.h"

#define LOC QString("Preview: ")
#define LOC_ERR QString("Preview Error: ")
#define LOC_WARN QString("Preview Warning: ")

const char *PreviewGenerator::kInUseID = "preview_generator";

/** \class PreviewGenerator
 *  \brief This class creates a preview image of a recording.
 *
 *   The usage is simple: First, pass a ProgramInfo whose pathname points
 *   to a local or remote recording to the constructor. Then call either
 *   Start(void) or Run(void) to generate the preview.
 *
 *   Start(void) will create a thread that processes the request,
 *   creating a sockets the the backend if the recording is not local.
 *
 *   Run(void) will process the request in the current thread, and it
 *   uses the MythContext's server and event sockets if the recording
 *   is not local.
 *
 *   The PreviewGenerator will send Qt signals when the preview is ready
 *   and when the preview thread finishes running if Start(void) was called.
 */

/** \fn PreviewGenerator::PreviewGenerator(const ProgramInfo*,bool)
 *  \brief Constructor
 *
 *   ProgramInfo::pathname must include recording prefix, so that
 *   the file can be found on the file system for local preview
 *   generation. When called by the backend 'local_only' should be set
 *   to true, otherwise the backend may deadlock if the PreviewGenerator
 *   can not find the file.
 *
 *  \param pginfo     ProgramInfo for the recording we want a preview of.
 *  \param local_only If set to true, the preview will only be generated
 *                    if the file is local.
 */
PreviewGenerator::PreviewGenerator(const ProgramInfo *pginfo,
                                   bool local_only)
    : programInfo(*pginfo), localOnly(local_only), isConnected(false),
      createSockets(false), serverSock(NULL),      pathname(pginfo->pathname),
      timeInSeconds(true),  captureTime(-1),       outFileName(QString::null),
      outSize(0,0)
{
    if (IsLocal())
        return;

    // Try to find a local means to access file...
    QString localFN  = programInfo.GetPlaybackURL(false, true);
    QString localFNdir = QFileInfo(localFN).dirPath();
    if (!(localFN.left(1) == "/" &&
          QFileInfo(localFN).exists() &&
          QFileInfo(localFNdir).isWritable()))
        return; // didn't find file locally, must use remote backend

    // Found file locally, so set the new pathname..
    QString msg = QString(
        "'%1' is not local, "
        "\n\t\t\treplacing with '%2', which is local.")
        .arg(pathname).arg(localFN);
    VERBOSE(VB_RECORD, LOC + msg);
    pathname = localFN;
}

PreviewGenerator::~PreviewGenerator()
{
    TeardownAll();
}

void PreviewGenerator::SetOutputFilename(const QString &fileName)
{
    outFileName = QDeepCopy<QString>(fileName);
}

void PreviewGenerator::TeardownAll(void)
{
    if (!isConnected)
        return;

    const QString filename = programInfo.pathname + ".png";

    MythTimer t;
    t.start();
    for (bool done = false; !done;)
    {
        previewLock.lock();
        if (isConnected)
            emit previewThreadDone(filename, done);
        else
            done = true;
        previewLock.unlock();
        usleep(5000);
    }
    VERBOSE(VB_PLAYBACK, LOC + "previewThreadDone took "<<t.elapsed()<<"ms");
    disconnectSafe();
}

void PreviewGenerator::deleteLater()
{
    TeardownAll();
    QObject::deleteLater();
}

void PreviewGenerator::AttachSignals(QObject *obj)
{
    QMutexLocker locker(&previewLock);
    connect(this, SIGNAL(previewThreadDone(const QString&,bool&)),
            obj,  SLOT(  previewThreadDone(const QString&,bool&)));
    connect(this, SIGNAL(previewReady(const ProgramInfo*)),
            obj,  SLOT(  previewReady(const ProgramInfo*)));
    isConnected = true;
}

/** \fn PreviewGenerator::disconnectSafe(void)
 *  \brief disconnects signals while holding previewLock, ensuring that
 *         no one will receive a signal from this class after this call.
 */
void PreviewGenerator::disconnectSafe(void)
{
    QMutexLocker locker(&previewLock);
    QObject::disconnect(this, NULL, NULL, NULL);
    isConnected = false;
}

/** \fn PreviewGenerator::Start(void)
 *  \brief This call starts a thread that will create a preview.
 */
void PreviewGenerator::Start(void)
{
    pthread_create(&previewThread, NULL, PreviewRun, this);
    // detach, so we don't have to join thread to free thread local mem.
    pthread_detach(previewThread);
}

/** \fn PreviewGenerator::RunReal(void)
 *  \brief This call creates a preview without starting a new thread.
 */
bool PreviewGenerator::RunReal(void)
{
    bool ok = false;
    bool is_local = IsLocal();
    if (is_local && LocalPreviewRun())
    {
        ok = true;
    }
    else if (!localOnly)
    {
        if (is_local)
        {
            VERBOSE(VB_IMPORTANT, LOC_WARN + "Failed to save preview."
                    "\n\t\t\tYou may need to check user and group ownership on"
                    "\n\t\t\tyour frontend and backend for quicker previews.\n"
                    "\n\t\t\tAttempting to regenerate preview on backend.\n");
        }
        ok = RemotePreviewRun();
    }
    else
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + QString("Run() file not local: '%1'")
                .arg(pathname));
    }

    return ok;
}

bool PreviewGenerator::Run(void)
{
    bool ok = false;
    if (!IsLocal())
    {
        if (!localOnly)
        {
            ok = RemotePreviewRun();
        }
        else
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    QString("Run() file not local: '%1'")
                    .arg(pathname));
        }
    }
    else
    {
        // This is where we fork and run mythbackend to actually make preview
        QString command = gContext->GetInstallPrefix() +
                                    "/bin/mythbackend --generate-preview ";
        command += QString("%1x%2")
            .arg(outSize.width()).arg(outSize.height());
        if (captureTime >= 0)
            command += QString("@%1%2")
                .arg(captureTime).arg(timeInSeconds ? "s" : "f");
        command += " ";
        command += QString("--chanid %1 ").arg(programInfo.chanid);
        command += QString("--starttime %1 ")
            .arg(programInfo.recstartts.toString("yyyyMMddhhmmss"));
        if (!outFileName.isEmpty())
            command += QString("--outfile \"%1\" ").arg(outFileName);

        int ret = myth_system(command);
        if (ret)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Encountered problems running " +
                    QString("'%1'").arg(command));
        }
        else
        {
            VERBOSE(VB_PLAYBACK, LOC + "Preview process returned 0.");
            QString outname = (!outFileName.isEmpty()) ?
                outFileName : (pathname + ".png");

            QString lpath = QFileInfo(outname).fileName();
            if (lpath == outname)
            {
                StorageGroup sgroup;
                QString tmpFile = sgroup.FindRecordingFile(lpath);
                outname = (tmpFile.isEmpty()) ? outname : tmpFile;
            }

            QFileInfo fi(outname);
            ok = (fi.exists() && fi.isReadable() && fi.size());
            if (ok)
                VERBOSE(VB_PLAYBACK, LOC + "Preview process ran ok.");
            else
            {
                VERBOSE(VB_IMPORTANT, LOC_ERR + "Preview process not ok." +
                        QString("\n\t\t\tfileinfo(%1)").arg(outname)
                        <<" exists: "<<fi.exists()
                        <<" readable: "<<fi.isReadable()
                        <<" size: "<<fi.size());
            }
        }
    }

    if (ok)
    {
        QMutexLocker locker(&previewLock);
        emit previewReady(&programInfo);
    }

    return ok;
}

void *PreviewGenerator::PreviewRun(void *param)
{
    // Lower scheduling priority, to avoid problems with recordings.
    if (setpriority(PRIO_PROCESS, 0, 9))
        VERBOSE(VB_IMPORTANT, LOC + "Setting priority failed." + ENO);
    PreviewGenerator *gen = (PreviewGenerator*) param;
    gen->createSockets = true;
    gen->Run();
    gen->deleteLater();
    return NULL;
}

bool PreviewGenerator::RemotePreviewSetup(void)
{
    QString server = gContext->GetSetting("MasterServerIP", "localhost");
    int port       = gContext->GetNumSetting("MasterServerPort", 6543);

    serverSock = gContext->ConnectServer(NULL, server, port);
    return serverSock;
}

bool PreviewGenerator::RemotePreviewRun(void)
{
    QStringList strlist = "QUERY_GENPIXMAP";
    programInfo.ToStringList(strlist);
    strlist.push_back(timeInSeconds ? "s" : "f");
    encodeLongLong(strlist, captureTime);
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

    bool ok = false;

    if (createSockets)
    {
        if (!RemotePreviewSetup())
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to open sockets.");
            return false;
        }

        if (serverSock)
        {
            serverSock->writeStringList(strlist);
            ok = serverSock->readStringList(strlist, false);
        }

        RemotePreviewTeardown();
    }
    else
    {
        ok = gContext->SendReceiveStringList(strlist);
    }

    if (!ok || strlist.empty() || (strlist[0] != "OK"))
    {
        if (!ok)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    "Remote Preview failed due to communications error.");
        }
        else if (strlist.size() > 1)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    "Remote Preview failed, reason given: " <<strlist[1]);
        }
        else
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    "Remote Preview failed due to an uknown error.");
        }
        return false;
    }

    if (outFileName.isEmpty())
        return true;

    // find file, copy/move to output file name & location...

    QString url = QString::null;
    QString fn = QFileInfo(outFileName).fileName();
    QByteArray data;
    ok = false;

    QStringList fileNames; 
    fileNames.push_back(CreateAccessibleFilename(programInfo.pathname, fn));
    fileNames.push_back(CreateAccessibleFilename(programInfo.pathname, ""));

    QStringList::const_iterator it = fileNames.begin();
    for ( ; it != fileNames.end() && (!ok || data.isEmpty()); ++it)
    {
        data.resize(0);
        url = *it;
        RemoteFile *rf = new RemoteFile(url, false, 0);
        ok = rf->SaveAs(data);
        delete rf;
    }

    if (ok && data.size())
    {
        QFile file(outFileName);
        ok = file.open(IO_Raw|IO_WriteOnly);
        if (!ok)
        {
            VERBOSE(VB_IMPORTANT, QString("Failed to open: '%1'")
                    .arg(outFileName));
        }

        off_t offset = 0;
        size_t remaining = (ok) ? data.size() : 0;
        uint failure_cnt = 0;
        while ((remaining > 0) && (failure_cnt < 5))
        {
            ssize_t written = file.writeBlock(data.data() + offset, remaining);
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
            VERBOSE(VB_PLAYBACK, QString("Saved: '%1'")
                    .arg(outFileName));
        }
    }

    return ok && data.size();
}

void PreviewGenerator::RemotePreviewTeardown(void)
{
    if (serverSock)
    {
        serverSock->DownRef();
        serverSock = NULL;
    }
}

bool PreviewGenerator::SavePreview(QString filename,
                                   const unsigned char *data,
                                   uint width, uint height, float aspect,
                                   int desired_width, int desired_height)
{
    if (!data || !width || !height)
        return false;

    const QImage img((unsigned char*) data,
                     width, height, 32, NULL, 65536 * 65536,
                     QImage::LittleEndian);

    float ppw = max(desired_width, 0);
    float pph = max(desired_height, 0);
    bool desired_size_exactly_specified = true;
    if ((ppw < 1.0f) && (pph < 1.0f))
    {
        ppw = gContext->GetNumSetting("PreviewPixmapWidth",  320);
        pph = gContext->GetNumSetting("PreviewPixmapHeight", 240);
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

    QImage small_img = img.smoothScale((int) ppw, (int) pph);

    if (small_img.save(filename, "PNG"))
    {
        chmod(filename.ascii(), 0666); // Let anybody update it

        VERBOSE(VB_PLAYBACK, LOC +
                QString("Saved preview '%0' %1x%2")
                .arg(filename).arg((int) ppw).arg((int) pph));

        return true;
    }

    // Save failed; if file exists, try saving to .new and moving over
    QString newfile = filename + ".new";
    if (QFileInfo(filename.ascii()).exists() &&
        small_img.save(newfile.ascii(), "PNG"))
    {
        chmod(newfile.ascii(), 0666);
        rename(newfile.ascii(), filename.ascii());

        VERBOSE(VB_PLAYBACK, LOC +
                QString("Saved preview '%0' %1x%2")
                .arg(filename).arg((int) ppw).arg((int) pph));

        return true;
    }

    // Couldn't save, nothing else I can do?
    return false;
}

bool PreviewGenerator::LocalPreviewRun(void)
{
    programInfo.MarkAsInUse(true, kInUseID);

    float aspect = 0;
    int   len, width, height, sz;
    long long captime = captureTime;
    if (captime < 0)
    {
        timeInSeconds = true;
        captime = (gContext->GetNumSetting("PreviewPixmapOffset", 64) +
                   gContext->GetNumSetting("RecordPreRoll",       0));
    }

    len = width = height = sz = 0;
    unsigned char *data = (unsigned char*)
        GetScreenGrab(&programInfo, pathname,
                      captime, timeInSeconds,
                      sz, width, height, aspect);

    QString outname = CreateAccessibleFilename(pathname, outFileName);

    int dw = (outSize.width()  < 0) ? width  : outSize.width();
    int dh = (outSize.height() < 0) ? height : outSize.height();

    bool ok = SavePreview(outname, data, width, height, aspect, dw, dh);

    if (data)
        delete[] data;

    programInfo.MarkAsInUse(false);

    return ok;
}

QString PreviewGenerator::CreateAccessibleFilename(
    const QString &pathname, const QString &outFileName)
{
    QString outname = pathname + ".png";

    if (outFileName.isEmpty())
        return QDeepCopy<QString>(outname);

    outname = outFileName;
    QFileInfo fi(outname);
    if (outname == fi.fileName())
    {
        QString dir = QString::null;
        if (pathname.contains(":"))
        {
            QUrl uinfo(pathname);
            uinfo.setPath("");
            dir = uinfo.toString();
        }
        else
        {
            dir = QFileInfo(pathname).dirPath();
        }
        outname = dir  + "/" + fi.fileName();
        VERBOSE(VB_IMPORTANT, LOC + QString("outfile '%1' -> '%2'")
                .arg(outFileName).arg(outname));
    }

    return QDeepCopy<QString>(outname);
}

bool PreviewGenerator::IsLocal(void) const
{
    QString pathdir = QFileInfo(pathname).dirPath();
    return (QFileInfo(pathname).exists() && QFileInfo(pathdir).isWritable());
}

/** \fn PreviewGenerator::GetScreenGrab(const ProgramInfo*, const QString&,
    long long, bool, int&, int&, int&, float&)
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
    const ProgramInfo *pginfo, const QString &filename,
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
#ifdef USING_FRONTEND
    if (!MSqlQuery::testDBConnection())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Previewer could not connect to DB.");
        return NULL;
    }

    // pre-test local files for existence and size. 500 ms speed-up...
    if (filename.left(1)=="/")
    {
        QFileInfo info(filename);
        bool invalid = !info.exists() || !info.isReadable() || !info.isFile();
        if (!invalid)
        {
            // Check size too, QFileInfo can not handle large files
            unsigned long long fsize =
                myth_get_approximate_large_file_size(filename);
            invalid = (fsize < 8*1024);
        }
        if (invalid)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Previewer file " +
                    QString("'%1'").arg(filename) + " is not valid.");
            return NULL;
        }
    }

    RingBuffer *rbuf = new RingBuffer(filename, false, false, 0);
    if (!rbuf->IsOpen())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Previewer could not open file: " +
                QString("'%1'").arg(filename));
        delete rbuf;
        return NULL;
    }

    NuppelVideoPlayer *nvp = new NuppelVideoPlayer(kInUseID, pginfo);
    nvp->SetRingBuffer(rbuf);

    if (time_in_secs)
        retbuf = nvp->GetScreenGrab(seektime, bufferlen,
                                    video_width, video_height, video_aspect);
    else
        retbuf = nvp->GetScreenGrabAtFrame(
            seektime, true, bufferlen,
            video_width, video_height, video_aspect);

    delete nvp;
    delete rbuf;

#else // USING_FRONTEND
    QString msg = "Backend compiled without USING_FRONTEND !!!!";
    VERBOSE(VB_IMPORTANT, LOC_ERR + msg);
#endif // USING_FRONTEND

    if (retbuf)
    {
        VERBOSE(VB_GENERAL, LOC +
                QString("Grabbed preview '%0' %1x%2@%3%4")
                .arg(filename).arg(video_width).arg(video_height)
                .arg((Q_LLONG)seektime).arg((time_in_secs) ? "s" : "f"));
    }

    return retbuf;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
