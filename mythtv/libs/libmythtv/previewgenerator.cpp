// C headers
#include <cmath>

// POSIX headers
#include <sys/types.h> // for stat
#include <sys/stat.h>  // for stat
#include <unistd.h>    // for stat

// Qt headers
#include <qfileinfo.h>
#include <qimage.h>

// MythTV headers
#include "RingBuffer.h"
#include "NuppelVideoPlayer.h"
#include "previewgenerator.h"
#include "tv_rec.h"

#define LOC QString("Preview: ")
#define LOC_ERR QString("Preview Error: ")

/** \class Preview Generator
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

/** \fn PreviewGenerator(const ProgramInfo*,bool)
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
    : programInfo(*pginfo), localOnly(local_only),
      createSockets(false), eventSock(NULL), serverSock(NULL)
{
    if (IsLocal())
        return;

    // Try to find a local means to access file...
    QString baseName = programInfo.GetRecordBasename();
    QString prefix   = gContext->GetSetting("RecordFilePrefix");
    QString localFN  = QString("%1/%2").arg(prefix).arg(baseName);
    if (!QFileInfo(localFN).exists())
        return; // didn't find file locally, must use remote backend

    // Found file locally, so set the new pathname..
    QString msg = QString(
        "'%1' is not local, "
        "\n\t\t\treplacing with '%2', which is local.")
        .arg(programInfo.pathname).arg(localFN);
    VERBOSE(VB_RECORD, LOC + msg);
    programInfo.pathname = localFN;
}

PreviewGenerator::~PreviewGenerator()
{
    QMutexLocker locker(&previewLock);
    const QString filename = programInfo.pathname + ".png";
    emit previewThreadDone(filename);
}

/** \fn PreviewGenerator::Start(void)
 *  \brief This call starts a thread that will create a preview.
 */
void PreviewGenerator::Start(void)
{
    pthread_create(&previewThread, NULL, PreviewRun, this);
    // Lower scheduling priority, to avoid problems with recordings.
    struct sched_param sp = {9 /* lower than normal */};
    pthread_setschedparam(previewThread, SCHED_OTHER, &sp);
    // detach, so we don't have to join thread to free thread local mem.
    pthread_detach(previewThread);
}

/** \fn PreviewGenerator::Run(void)
 *  \brief This call creates a preview without starting a new thread.
 */
void PreviewGenerator::Run(void)
{
    if (IsLocal())
    {
        LocalPreviewRun();
    }
    else if (!localOnly)
    {
        RemotePreviewRun();
    }
    else
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + QString("Run() file not local: '%1'")
                .arg(programInfo.pathname));
    }
}

void *PreviewGenerator::PreviewRun(void *param)
{
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

    eventSock  = new QSocket(0, "preview event socket");

    QObject::connect(eventSock, SIGNAL(connected()), 
                     this,      SLOT(  EventSocketConnected()));
    QObject::connect(eventSock, SIGNAL(readyRead()), 
                     this,      SLOT(  EventSocketRead()));
    QObject::connect(eventSock, SIGNAL(connectionClosed()), 
                     this,      SLOT(  EventSocketClosed()));

    serverSock = gContext->ConnectServer(NULL/*eventSock*/, server, port);
    if (!serverSock)
    {
        eventSock->deleteLater();
        return false;
    }
    return true;
}

void PreviewGenerator::RemotePreviewRun(void)
{
    QStringList strlist = "QUERY_GENPIXMAP";
    programInfo.ToStringList(strlist);
    bool ok = false;

    if (createSockets)
    {
        if (!RemotePreviewSetup())
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to open sockets.");
            return;
        }

        if (serverSock)
            WriteStringList(serverSock, strlist);

        if (serverSock)
            ok = ReadStringList(serverSock, strlist, false);

        RemotePreviewTeardown();
    }
    else
    {
        ok = gContext->SendReceiveStringList(strlist);
    }

    if (ok)
    {
        QMutexLocker locker(&previewLock);
        emit previewReady(&programInfo);
    }
}

void PreviewGenerator::RemotePreviewTeardown(void)
{
    if (serverSock)
    {
        delete serverSock;
        serverSock = NULL;
    }
    if (eventSock)
        eventSock->deleteLater();;
}

bool PreviewGenerator::SavePreview(QString filename,
                                   const unsigned char *data,
                                   uint width, uint height, float aspect)
{
    if (!data || !width || !height)
        return false;

    const QImage img((unsigned char*) data,
                     width, height, 32, NULL, 65536 * 65536,
                     QImage::LittleEndian);

    float ppw = gContext->GetNumSetting("PreviewPixmapWidth", 160);
    float pph = gContext->GetNumSetting("PreviewPixmapHeight", 120);

    aspect = (aspect <= 0) ? ((float) width) / height : aspect;

    if (aspect > ppw / pph)
        pph = rint(ppw / aspect);
    else
        ppw = rint(pph * aspect);

    QImage small_img = img.smoothScale((int) ppw, (int) pph);

    return small_img.save(filename.ascii(), "PNG");
}

void PreviewGenerator::LocalPreviewRun(void)
{
    float aspect = 0;
    int   secsin = (gContext->GetNumSetting("PreviewPixmapOffset", 64) +
                    gContext->GetNumSetting("RecordPreRoll",       0));
    int   len, width, height, sz;

    len = width = height = sz = 0;
    unsigned char *data = (unsigned char*)
        GetScreenGrab(&programInfo, programInfo.pathname, secsin,
                      sz, width, height, aspect);

    if (SavePreview(programInfo.pathname+".png", data, width, height, aspect))
    {
        QMutexLocker locker(&previewLock);
        emit previewReady(&programInfo);
    }

    if (data)
        delete[] data;
}

bool PreviewGenerator::IsLocal(void) const
{
    return QFileInfo(programInfo.pathname).exists();
}

void PreviewGenerator::EventSocketConnected(void)
{
    QString str = QString("ANN Playback %1 %2")
        .arg(gContext->GetHostName()).arg(true);
    QStringList strlist = str;
    WriteStringList(eventSock, strlist);
    ReadStringList(eventSock, strlist);
}

void PreviewGenerator::EventSocketClosed(void)
{
    VERBOSE(VB_IMPORTANT, LOC_ERR + "Event socket closed.");
    if (serverSock)
    {
        QSocketDevice *tmp = serverSock;
        serverSock = NULL;
        delete tmp;
    }
}

void PreviewGenerator::EventSocketRead(void)
{
    while (eventSock->state() == QSocket::Connected &&
           eventSock->bytesAvailable() > 0)
    {
        QStringList strlist;
        if (!ReadStringList(eventSock, strlist))
            continue;
    }
}

/** \fn PreviewGenerator::GetScreenGrab(const ProgramInfo*,const QString&,int,int&,int&,int&)
 *  \brief Returns a PIX_FMT_RGBA32 buffer containg a frame from the video.
 *
 *  \param pginfo       Recording to grab from.
 *  \param filename     File containing recording.
 *  \param secondsin    Seconds into the video to seek before
 *                      capturing a frame.
 *  \param bufferlen    Returns size of buffer returned (in bytes).
 *  \param video_width  Returns width of frame grabbed.
 *  \param video_height Returns height of frame grabbed.
 *  \return Buffer allocated with new containing frame in RGBA32 format if
 *          successful, NULL otherwise.
 */
char *PreviewGenerator::GetScreenGrab(
    const ProgramInfo *pginfo, const QString &filename, int secondsin,
    int &bufferlen,
    int &video_width, int &video_height, float &video_aspect)
{
    (void) pginfo;
    (void) filename;
    (void) secondsin;
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
            struct stat status;
            stat(filename.ascii(), &status);
            // Using off_t requires a lot of 32/64 bit checking.
            // So just get the size in blocks.
            unsigned long long bsize = status.st_blksize;
            unsigned long long nblk  = status.st_blocks;
            unsigned long long approx_size = nblk * bsize;
            invalid = (approx_size < 8*1024);
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

    NuppelVideoPlayer *nvp = new NuppelVideoPlayer("Preview", pginfo);
    nvp->SetRingBuffer(rbuf);

    retbuf = nvp->GetScreenGrab(secondsin, bufferlen,
                                video_width, video_height, video_aspect);

    delete nvp;
    delete rbuf;

#else // USING_FRONTEND
    QString msg = "Backend compiled without USING_FRONTEND !!!!";
    VERBOSE(VB_IMPORTANT, LOC_ERR + msg);
#endif // USING_FRONTEND
    return retbuf;
}
