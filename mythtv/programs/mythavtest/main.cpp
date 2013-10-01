#include <unistd.h>
#include <iostream>

using namespace std;

#include <QString>
#include <QRegExp>
#include <QDir>
#include <QApplication>
#include <QTime>

#include "tv_play.h"
#include "programinfo.h"
#include "commandlineparser.h"
#include "mythplayer.h"
#include "jitterometer.h"

#include "exitcodes.h"
#include "mythcontext.h"
#include "mythversion.h"
#include "mythdbcon.h"
#include "compat.h"
#include "dbcheck.h"
#include "mythlogging.h"
#include "signalhandling.h"
#include "mythmiscutil.h"

// libmythui
#include "mythuihelper.h"
#include "mythmainwindow.h"

class VideoPerformanceTest
{
  public:
    VideoPerformanceTest(const QString &filename, bool novsync, bool onlydecode,
                         int runfor, bool deint)
      : file(filename), novideosync(novsync), decodeonly(onlydecode),
        secondstorun(runfor), deinterlace(deint), ctx(NULL)
    {
        if (secondstorun < 1)
            secondstorun = 1;
        if (secondstorun > 3600)
            secondstorun = 3600;
    }

   ~VideoPerformanceTest()
    {
        delete ctx;
    }

    void Test(void)
    {
        PIPMap dummy;

        if (novideosync) // TODO
            LOG(VB_GENERAL, LOG_INFO, "Will attempt to disable sync-to-vblank.");

        RingBuffer *rb  = RingBuffer::Create(file, false, true, 2000);
        MythPlayer  *mp  = new MythPlayer(kAudioMuted);
        mp->GetAudio()->SetAudioInfo("NULL", "NULL", 0, 0);
        mp->GetAudio()->SetNoAudio();
        ctx = new PlayerContext("VideoPerformanceTest");
        ctx->SetRingBuffer(rb);
        ctx->SetPlayer(mp);
        ctx->SetPlayingInfo(new ProgramInfo(file));
        mp->SetPlayerInfo(NULL, GetMythMainWindow(), ctx);
        FrameScanType scan = deinterlace ? kScan_Interlaced : kScan_Progressive;
        if (!mp->StartPlaying())
        {
            LOG(VB_GENERAL, LOG_ERR, "Failed to start playback.");
            return;
        }

        VideoOutput *vo = mp->GetVideoOutput();
        if (!vo)
        {
            LOG(VB_GENERAL, LOG_ERR, "No video output.");
            return;
        }

        LOG(VB_GENERAL, LOG_INFO, "-----------------------------------");
        LOG(VB_GENERAL, LOG_INFO, QString("Starting video performance test for '%1'.")
            .arg(file));
        LOG(VB_GENERAL, LOG_INFO, QString("Test will run for %1 seconds.")
            .arg(secondstorun));

        if (decodeonly)
            LOG(VB_GENERAL, LOG_INFO, "Decoding frames only - skipping display.");
        LOG(VB_GENERAL, LOG_INFO, QString("Deinterlacing %1")
            .arg(deinterlace ? "enabled" : "disabled"));

        Jitterometer *jitter = new Jitterometer("Performance: ", mp->GetFrameRate());

        int ms = secondstorun * 1000;
        QTime start = QTime::currentTime();
        while (1)
        {
            int duration = start.msecsTo(QTime::currentTime());
            if (duration < 0 || duration > ms)
            {
                LOG(VB_GENERAL, LOG_INFO, "Complete.");
                break;
            }

            if (mp->IsErrored())
            {
                LOG(VB_GENERAL, LOG_ERR, "Playback error.");
                break;
            }

            if (mp->GetEof() != kEofStateNone)
            {
                LOG(VB_GENERAL, LOG_INFO, "End of file.");
                break;
            }

            if (!mp->PrebufferEnoughFrames())
                continue;

            mp->SetBuffering(false);
            vo->StartDisplayingFrame();
            VideoFrame *frame = vo->GetLastShownFrame();
            mp->CheckAspectRatio(frame);

            if (!decodeonly)
            {
                vo->ProcessFrame(frame, NULL, NULL, dummy, scan);
                vo->PrepareFrame(frame, scan, NULL);
                vo->Show(scan);
            }
            vo->DoneDisplayingFrame(frame);
            jitter->RecordCycleTime();
        }
        LOG(VB_GENERAL, LOG_INFO, "-----------------------------------");
        delete jitter;
    }

  private:
    QString file;
    bool    novideosync;
    bool    decodeonly;
    int     secondstorun;
    bool    deinterlace;
    PlayerContext *ctx;
};

int main(int argc, char *argv[])
{
    MythAVTestCommandLineParser cmdline;
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

    QApplication a(argc, argv);
    QCoreApplication::setApplicationName(MYTH_APPNAME_MYTHAVTEST);

    int retval;
    if ((retval = cmdline.ConfigureLogging()) != GENERIC_EXIT_OK)
        return retval;

    if (!cmdline.toString("display").isEmpty())
    {
        MythUIHelper::SetX11Display(cmdline.toString("display"));
    }

    if (!cmdline.toString("geometry").isEmpty())
    {
        MythUIHelper::ParseGeometryOverride(cmdline.toString("geometry"));
    }

    QString filename = "";
    if (!cmdline.toString("infile").isEmpty())
        filename = cmdline.toString("infile");
    else if (cmdline.GetArgs().size() >= 1)
        filename = cmdline.GetArgs()[0];

    gContext = new MythContext(MYTH_BINARY_VERSION);
    if (!gContext->Init())
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to init MythContext, exiting.");
        return GENERIC_EXIT_NO_MYTHCONTEXT;
    }

    cmdline.ApplySettingsOverride();

    if (setuid(getuid()) != 0)
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to setuid(), exiting.");
        return GENERIC_EXIT_NOT_OK;
    }

    QString themename = gCoreContext->GetSetting("Theme");
    QString themedir = GetMythUI()->FindThemeDir(themename);
    if (themedir.isEmpty())
    {
        QString msg = QString("Fatal Error: Couldn't find theme '%1'.")
            .arg(themename);
        LOG(VB_GENERAL, LOG_ERR, msg);
        return GENERIC_EXIT_NO_THEME;
    }

    GetMythUI()->LoadQtConfig();

#if defined(Q_OS_MACX)
    // Mac OS X doesn't define the AudioOutputDevice setting
#else
    QString auddevice = gCoreContext->GetSetting("AudioOutputDevice");
    if (auddevice.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, "Fatal Error: Audio not configured, you need "
                                 "to run 'mythfrontend', not 'mythtv'.");
        return GENERIC_EXIT_SETUP_ERROR;
    }
#endif

    MythMainWindow *mainWindow = GetMythMainWindow();
#if CONFIG_DARWIN
    mainWindow->Init(OPENGL2_PAINTER);
#else
    mainWindow->Init();
#endif

#ifndef _WIN32
    QList<int> signallist;
    signallist << SIGINT << SIGTERM << SIGSEGV << SIGABRT << SIGBUS << SIGFPE
               << SIGILL;
#if ! CONFIG_DARWIN
    signallist << SIGRTMIN;
#endif
    SignalHandler::Init(signallist);
    signal(SIGHUP, SIG_IGN);
#endif

    if (cmdline.toBool("test"))
    {
        int seconds = 5;
        if (!cmdline.toString("seconds").isEmpty())
            seconds = cmdline.toInt("seconds");
        VideoPerformanceTest *test = new VideoPerformanceTest(filename, false,
                    cmdline.toBool("decodeonly"), seconds,
                    cmdline.toBool("deinterlace"));
        test->Test();
        delete test;
    }
    else
    {
        TV::InitKeys();
        setHttpProxy();

        if (!UpgradeTVDatabaseSchema(false))
        {
            LOG(VB_GENERAL, LOG_ERR, "Fatal Error: Incorrect database schema.");
            delete gContext;
            return GENERIC_EXIT_DB_OUTOFDATE;
        }

        if (filename.isEmpty())
        {
            TV::StartTV(NULL, kStartTVNoFlags);
        }
        else
        {
            ProgramInfo pginfo(filename);
            TV::StartTV(&pginfo, kStartTVNoFlags);
        }
    }
    DestroyMythMainWindow();

    delete gContext;

    SignalHandler::Done();

    return GENERIC_EXIT_OK;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
