#include <cmath>

#include <qfileinfo.h>
#include <qimage.h>
#include "previewgenerator.h"
#include "tv_rec.h"

#define LOC QString("PreviewGen: ")
#define LOC_ERR QString("PreviewGen, Error: ")

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

PreviewGenerator::PreviewGenerator(const ProgramInfo *pginfo)
    : programInfo(*pginfo),
      createSockets(false), eventSock(NULL), serverSock(NULL)
{
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
    else
    {
        RemotePreviewRun();
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
        TVRec::GetScreenGrab(&programInfo, programInfo.pathname, secsin,
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
    return (QFileInfo(programInfo.pathname).exists());
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
