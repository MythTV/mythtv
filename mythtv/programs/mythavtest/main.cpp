#include <unistd.h>
#include <iostream>

using namespace std;

#include <QApplication>
#include <QDir>
#include <QRegExp>
#include <QString>
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
#include "videooutbase.h"

// libmythui
#include "mythuihelper.h"
#include "mythmainwindow.h"

class VideoPerformanceTest
{
  public:
    VideoPerformanceTest(QString filename, bool novsync, bool onlydecode,
                         int runfor, bool deint, bool gpu)
      : m_file(std::move(filename)), m_noVideoSync(novsync), m_decodeOnly(onlydecode),
        m_secondsToRun(runfor), m_deinterlace(deint), m_allowGpu(gpu), m_ctx(nullptr)
    {
        if (m_secondsToRun < 1)
            m_secondsToRun = 1;
        if (m_secondsToRun > 3600)
            m_secondsToRun = 3600;
    }

   ~VideoPerformanceTest()
    {
        delete m_ctx;
    }

    void Test(void)
    {
        PIPMap dummy;

        if (m_noVideoSync) // TODO
            LOG(VB_GENERAL, LOG_INFO, "Will attempt to disable sync-to-vblank.");

        RingBuffer *rb  = RingBuffer::Create(m_file, false, true, 2000);
        MythPlayer  *mp  = new MythPlayer((PlayerFlags)(kAudioMuted | (m_allowGpu ? kDecodeAllowGPU : kNoFlags)));
        mp->GetAudio()->SetAudioInfo("NULL", "NULL", 0, 0);
        mp->GetAudio()->SetNoAudio();
        m_ctx = new PlayerContext("VideoPerformanceTest");
        m_ctx->SetRingBuffer(rb);
        m_ctx->SetPlayer(mp);
        ProgramInfo *pinfo = new ProgramInfo(m_file);
        m_ctx->SetPlayingInfo(pinfo); // makes a copy
        delete pinfo;
        mp->SetPlayerInfo(nullptr, GetMythMainWindow(), m_ctx);

        FrameScanType scan = m_deinterlace ? kScan_Interlaced : kScan_Progressive;
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
        LOG(VB_GENERAL, LOG_INFO, "Ensure Sync to VBlank is disabled.");
        LOG(VB_GENERAL, LOG_INFO, "Otherwise rate will be limited to that of the display.");
        LOG(VB_GENERAL, LOG_INFO, "-----------------------------------");
        LOG(VB_GENERAL, LOG_INFO, QString("Starting video performance test for '%1'.")
            .arg(m_file));
        LOG(VB_GENERAL, LOG_INFO, QString("Test will run for %1 seconds.")
            .arg(m_secondsToRun));

        if (m_decodeOnly)
            LOG(VB_GENERAL, LOG_INFO, "Decoding frames only - skipping display.");

        bool doublerate = vo->NeedsDoubleFramerate();
        if (m_deinterlace)
        {
            LOG(VB_GENERAL, LOG_INFO, QString("Deinterlacing: %1")
                .arg(doublerate ? "doublerate" : "singlerate"));
            if (doublerate)
                LOG(VB_GENERAL, LOG_INFO, "Output will show fields per second");
        }
        else
        {
            LOG(VB_GENERAL, LOG_INFO, "Deinterlacing disabled");
        }

        DecoderBase* dec = mp->GetDecoder();
        if (dec)
            LOG(VB_GENERAL, LOG_INFO, QString("Using decoder: %1").arg(dec->GetCodecDecoderName()));

        Jitterometer *jitter = new Jitterometer("Performance: ", mp->GetFrameRate() * (doublerate ? 2 : 1));

        int ms = m_secondsToRun * 1000;
        QTime start = QTime::currentTime();
        while (true)
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

            if (!m_decodeOnly)
            {
                vo->ProcessFrame(frame, nullptr, nullptr, dummy, scan);
                vo->PrepareFrame(frame, scan, nullptr);
                vo->Show(scan);

                if (vo->NeedsDoubleFramerate() && m_deinterlace)
                {
                    vo->PrepareFrame(frame, kScan_Intr2ndField, nullptr);
                    vo->Show(scan);
                }
            }
            vo->DoneDisplayingFrame(frame);
            jitter->RecordCycleTime();
        }
        LOG(VB_GENERAL, LOG_INFO, "-----------------------------------");
        delete jitter;
    }

  private:
    QString        m_file;
    bool           m_noVideoSync;
    bool           m_decodeOnly;
    int            m_secondsToRun;
    bool           m_deinterlace;
    bool           m_allowGpu;
    PlayerContext *m_ctx;
};

int main(int argc, char *argv[])
{

#if CONFIG_OMX_RPI
    setenv("QT_XCB_GL_INTEGRATION","none",0);
#endif

    // try and disable sync to vblank on linux x11
    qputenv("vblank_mode", "0"); // Intel and AMD
    qputenv("__GL_SYNC_TO_VBLANK", "0"); // NVidia

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
        MythAVTestCommandLineParser::PrintVersion();
        return GENERIC_EXIT_OK;
    }

    QApplication a(argc, argv);
    QCoreApplication::setApplicationName(MYTH_APPNAME_MYTHAVTEST);

    int retval = cmdline.ConfigureLogging();
    if (retval != GENERIC_EXIT_OK)
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
    else if (!cmdline.GetArgs().empty())
        filename = cmdline.GetArgs()[0];

    gContext = new MythContext(MYTH_BINARY_VERSION, true);
    if (!gContext->Init())
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to init MythContext, exiting.");
        return GENERIC_EXIT_NO_MYTHCONTEXT;
    }

    cmdline.ApplySettingsOverride();

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
                    cmdline.toBool("deinterlace"),
                    cmdline.toBool("gpu"));
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
            TV::StartTV(nullptr, kStartTVNoFlags);
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
