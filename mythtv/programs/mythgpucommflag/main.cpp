#include <iostream>
using namespace std;

#include <QApplication>
#include <QDesktopWidget>
#include <QX11Info>
#include <QFile>

#include "exitcodes.h"
#include "mythcontext.h"
#include "commandlineparser.h"
#include "mythlogging.h"
#include "mythversion.h"
#include "ringbuffer.h"
#include "gpuplayer.h"
#include "playercontext.h"
#include "remotefile.h"
#include "jobqueue.h"
#include "mythuihelper.h"

#include "openclinterface.h"
#include "packetqueue.h"
#include "flagresults.h"
#include "audioconsumer.h"
#include "videoconsumer.h"

#define LOC      QString("MythGPUCommFlag: ")

Display *mythDisplay = NULL;
int mythScreen = 0;

namespace
{
    void cleanup()
    {
        delete gContext;
        gContext = NULL;
    }

    class CleanupGuard
    {
      public:
        typedef void (*CleanupFunc)();

      public:
        CleanupGuard(CleanupFunc cleanFunction) :
            m_cleanFunction(cleanFunction) {}

        ~CleanupGuard()
        {
            m_cleanFunction();
        }

      private:
        CleanupFunc m_cleanFunction;
    };
}

MythGPUCommFlagCommandLineParser cmdline;
OpenCLDeviceMap *devMap = NULL;

static QString get_filename(ProgramInfo *program_info)
{
    QString filename = program_info->GetPathname();
    if (!QFile::exists(filename))
        filename = program_info->GetPlaybackURL(true);
    return filename;
}

static bool DoesFileExist(ProgramInfo *program_info)
{
    QString filename = get_filename(program_info);
    long long size = 0;
    bool exists = false;

    if (filename.startsWith("myth://"))
    {
        RemoteFile remotefile(filename, false, false, 0);
        struct stat filestat;

        if (remotefile.Exists(filename, &filestat))
        {
            exists = true;
            size = filestat.st_size;
        }
    }
    else
    {
        QFile file(filename);
        if (file.exists())
        {
            exists = true;
            size = file.size();
        }
    }

    if (!exists)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Couldn't find file %1, aborting.")
                .arg(filename));
        return false;
    }

    if (size == 0)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("File %1 is zero-byte, aborting.")
                .arg(filename));
        return false;
    }

    return true;
}

int main(int argc, char **argv)
{
    // Parse commandline
    if (!cmdline.Parse(argc, argv))
    {
        cmdline.PrintHelp();
        return GENERIC_EXIT_INVALID_CMDLINE;
    }

    if (cmdline.toBool("showhelp"))
    {
        cmdline.PrintHelp();
        return GENERIC_EXIT_OK;
    }

    if (cmdline.toBool("showversion"))
    {
        cmdline.PrintVersion();
        return GENERIC_EXIT_OK;
    }

#ifdef Q_WS_X11
    bool useX = (getenv("DISPLAY") != 0) ||
                !cmdline.toString("display").isEmpty();

    if (cmdline.toBool("noX"))
        useX = false;
#else
    bool useX = true;
#endif

    QApplication app(argc, argv, useX);
    QApplication::setApplicationName("mythgpucommflag");
    int retval = cmdline.ConfigureLogging("general", false);
//                                          !cmdline.toBool("noprogress"));
    if (retval != GENERIC_EXIT_OK)
        return retval;

    QString display(cmdline.toString("display"));
    display.detach();

    if (useX)
    {
        QDesktopWidget *desktop = app.desktop();
        QX11Info info = desktop->x11Info();

        mythDisplay = info.display();
        mythScreen  = info.screen();

        // Since NULL video output somehow needs the display.
        if (!display.isEmpty())
            MythUIHelper::SetX11Display(display);
    }

    if (!useX)
    {
        LOG(VB_GENERAL, LOG_NOTICE,
            "Use of X disabled, reverting to software video decoding");
    }

    CleanupGuard callCleanup(cleanup);
    gContext = new MythContext(MYTH_BINARY_VERSION);
    
    if (!gContext->Init( false,  /* use gui */
                         false,  /* prompt for backend */
                         false,  /* bypass auto discovery */
                         false) ) /* ignoreDB */
    {
        LOG(VB_GENERAL, LOG_EMERG, "Failed to init MythContext, exiting.");
        return GENERIC_EXIT_NO_MYTHCONTEXT;
    }

    cmdline.ApplySettingsOverride();

    // Determine capabilities
    devMap = new OpenCLDeviceMap();

    OpenCLDevice *devices[2];    // 0 = video, 1 = audio;
    bool found;

    for (found = false, devices[0] = NULL; !found; )
    {
        devices[0] = devMap->GetBestDevice(devices[0],
                                           cmdline.toBool("swvideo"));

        found = devices[0] ? devices[0]->Initialize() : true;
    }

    if (devices[0])
    {
        LOG(VB_GENERAL, LOG_INFO, "OpenCL instance for video processing");
        devices[0]->Display();
    }
    else
        LOG(VB_GENERAL, LOG_INFO, "Video processing via software");

    for (found = false, devices[1] = devices[0]; !found; )
    {
        devices[1] = devMap->GetBestDevice(devices[1],
                                           cmdline.toBool("swaudio"));

        found = devices[1] ? devices[1]->Initialize() : true;
    }

    if (devices[1])
    {
        LOG(VB_GENERAL, LOG_INFO, "OpenCL instance for audio processing");
        devices[1]->Display();
    }
    else
        LOG(VB_GENERAL, LOG_INFO, "Audio processing via software");

    ProgramInfo pginfo;
    int jobID = -1;

    if (cmdline.toBool("chanid") && cmdline.toBool("starttime"))
    {
        // operate on a recording in the database
        uint chanid = cmdline.toUInt("chanid");
        QDateTime starttime = cmdline.toDateTime("starttime");
        pginfo = ProgramInfo(chanid, starttime);
    }
    else if (cmdline.toBool("jobid"))
    {
        int jobType = JOB_NONE;
        jobID = cmdline.toInt("jobid");
        uint chanid;
        QDateTime starttime;

        if (!JobQueue::GetJobInfoFromID(jobID, jobType, chanid, starttime))
        {
            cerr << "mythgpucommflag: ERROR: Unable to find DB info for "
                 << "JobQueue ID# " << jobID << endl;
            return GENERIC_EXIT_NO_RECORDING_DATA;
        }
        int jobQueueCPU = gCoreContext->GetNumSetting("JobQueueCPU", 0);

        if (jobQueueCPU < 2)
        {
            myth_nice(17);
            myth_ioprio((0 == jobQueueCPU) ? 8 : 7);
        }

        pginfo = ProgramInfo(chanid, starttime);
    }
    else if (cmdline.toBool("file"))
    {
        pginfo = ProgramInfo(cmdline.toString("file"));
    }
    else
    {
        cerr << "You must indicate the recording to flag" << endl;
        return GENERIC_EXIT_NO_RECORDING_DATA;
    }
        
    QString filename = get_filename(&pginfo);

    if (!DoesFileExist(&pginfo))
    {
        // file not found on local filesystem
        // assume file is in Video storage group on local backend
        // and try again

        filename = QString("myth://Videos@%1/%2")
                            .arg(gCoreContext->GetHostName()).arg(filename);
        pginfo.SetPathname(filename);
        if (!DoesFileExist(&pginfo))
        {
            LOG(VB_GENERAL, LOG_ERR,
                "Unable to find file in defined storage paths.");
            return GENERIC_EXIT_PERMISSIONS_ERROR;
        }
    }

    // Open file
    RingBuffer *rb = RingBuffer::Create(filename, false);
    if (!rb)
    {
        LOG(VB_GENERAL, LOG_ERR, 
            QString("Unable to create RingBuffer for %1").arg(filename));
        return GENERIC_EXIT_PERMISSIONS_ERROR;
    }

    GPUPlayer *cfp = new GPUPlayer();
    PlayerContext *ctx = new PlayerContext(kFlaggerInUseID);
    ctx->SetSpecialDecode(kAVSpecialDecode_GPUDecode);
    ctx->SetPlayingInfo(&pginfo);
    ctx->SetRingBuffer(rb);
    ctx->SetPlayer(cfp);
    cfp->SetPlayerInfo(NULL, NULL, true, ctx);

    // Create input and output queues
    PacketQueue audioQ(2048);
    PacketQueue videoQ(2048);
    ResultsMap audioMarks;
    ResultsMap videoMarks;

    // Initialize the findings map
    FindingsInitialize();

    // Create consumer threads
    AudioConsumer *audioThread = new AudioConsumer(&audioQ, &audioMarks,
                                                   devices[1]);
    VideoConsumer *videoThread = new VideoConsumer(&videoQ, &videoMarks,
                                                   devices[0]);

    if (useX)
        videoThread->EnableX();

    audioThread->start();
    videoThread->start();

    LOG(VB_GENERAL, LOG_INFO, "Starting queuing packets");

    // Loop: Pull frames from file, queue on video or audio queue
    cfp->ProcessFile(true, NULL, NULL, &audioQ, &videoQ);

    // Wait for processing to finish
    LOG(VB_GENERAL, LOG_INFO, "Done queuing packets");
    audioThread->done();
    videoThread->done();

    audioThread->wait();
    videoThread->wait();

    // Close file
    delete ctx;

    LOG(VB_GENERAL, LOG_INFO, QString("audioMarks: %1, videoMarks: %2")
        .arg(audioMarks.size()) .arg(videoMarks.size()));

    // Dump results to an output file
    QFile dump("out/results");
    dump.open(QIODevice::WriteOnly);
    QString audioDump = audioMarks.toString("Audio markings");
    QString videoDump = videoMarks.toString("Video markings");
    dump.write(audioDump.toAscii());
    dump.write(videoDump.toAscii());
    dump.close();

    // Loop:
        // Summarize the various criteria to get commercial flag map

    // Send map to the db
    
    return(GENERIC_EXIT_OK);
}

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
