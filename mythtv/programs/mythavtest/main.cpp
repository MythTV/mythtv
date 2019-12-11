#include <unistd.h>
#include <iostream>
#include <utility>

using namespace std;

#include <QApplication>
#include <QDir>
#include <QRegExp>
#include <QString>
#include <QSurfaceFormat>
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
#include "mythvideoout.h"

// libmythui
#include "mythuihelper.h"
#include "mythmainwindow.h"

class VideoPerformanceTest
{
  public:
    VideoPerformanceTest(QString filename, bool decodeno, bool onlydecode,
                         int runfor, bool deint, bool gpu)
      : m_file(std::move(filename)),
        m_noDecode(decodeno),
        m_decodeOnly(onlydecode),
        m_secondsToRun(runfor),
        m_deinterlace(deint),
        m_allowGpu(gpu),
        m_ctx(nullptr)
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
        RingBuffer *rb  = RingBuffer::Create(m_file, false, true, 2000);
        auto       *mp  = new MythPlayer(
            (PlayerFlags)(kAudioMuted | (m_allowGpu ? (kDecodeAllowGPU | kDecodeAllowEXT): kNoFlags)));
        mp->GetAudio()->SetAudioInfo("NULL", "NULL", 0, 0);
        mp->GetAudio()->SetNoAudio();
        m_ctx = new PlayerContext("VideoPerformanceTest");
        m_ctx->SetRingBuffer(rb);
        m_ctx->SetPlayer(mp);
        auto *pinfo = new ProgramInfo(m_file);
        m_ctx->SetPlayingInfo(pinfo); // makes a copy
        delete pinfo;
        mp->SetPlayerInfo(nullptr, GetMythMainWindow(), m_ctx);

        FrameScanType scan = m_deinterlace ? kScan_Interlaced : kScan_Progressive;
        if (!mp->StartPlaying())
        {
            LOG(VB_GENERAL, LOG_ERR, "Failed to start playback.");
            return;
        }

        MythVideoOutput *vo = mp->GetVideoOutput();
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

        if (m_noDecode)
            LOG(VB_GENERAL, LOG_INFO, "No decode after startup - checking display performance");
        else if (m_decodeOnly)
            LOG(VB_GENERAL, LOG_INFO, "Decoding frames only - skipping display.");
        DecoderBase* dec = mp->GetDecoder();
        if (dec)
            LOG(VB_GENERAL, LOG_INFO, QString("Using decoder: %1").arg(dec->GetCodecDecoderName()));

        auto *jitter = new Jitterometer("Performance: ", static_cast<int>(mp->GetFrameRate()));

        int ms = m_secondsToRun * 1000;
        QTime start = QTime::currentTime();
        VideoFrame *frame = nullptr;
        while (true)
        {
            QCoreApplication::processEvents();
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
            if ((m_noDecode && !frame) || !m_noDecode)
                frame = vo->GetLastShownFrame();
            mp->CheckAspectRatio(frame);

            if (!m_decodeOnly)
            {
                MythDeintType doubledeint = GetDoubleRateOption(frame, DEINT_CPU | DEINT_SHADER | DEINT_DRIVER);
                vo->ProcessFrame(frame, nullptr, dummy, scan);
                vo->PrepareFrame(frame, scan, nullptr);
                vo->Show(scan);

                if (doubledeint && m_deinterlace)
                {
                    doubledeint = GetDoubleRateOption(frame, DEINT_CPU);
                    MythDeintType other = GetDoubleRateOption(frame, DEINT_SHADER | DEINT_DRIVER);
                    if (doubledeint && !other)
                        vo->ProcessFrame(frame, nullptr, dummy, kScan_Intr2ndField);
                    vo->PrepareFrame(frame, kScan_Intr2ndField, nullptr);
                    vo->Show(scan);
                }
            }
            if (!m_noDecode)
                vo->DoneDisplayingFrame(frame);
            jitter->RecordCycleTime();
        }
        LOG(VB_GENERAL, LOG_INFO, "-----------------------------------");
        delete jitter;
    }

  private:
    QString m_file;
    bool    m_noDecode;
    bool    m_decodeOnly;
    int     m_secondsToRun;
    bool    m_deinterlace;
    bool    m_allowGpu;
    PlayerContext *m_ctx;
};

int main(int argc, char *argv[])
{

#if defined (Q_OS_LINUX)
#if defined (USING_VAAPI) || defined (USING_MMAL)
    // When using VAAPI (linux/desktop only) we want to use EGL to ensure we
    // can use zero copy video buffers for the best performance (N.B. not tested
    // on AMD desktops). For non-VAAPI users this should make no difference - on NVidia
    // installations it has no effect.
    // Likewise for MMAL (Raspberry Pi), we want EGL for zero copy direct rendering.
    // This is the only way to force Qt to use EGL and must be done before any
    // GUI is created.
    // If problems are encountered, set the environment variable NO_EGL

    // Disabled this for now as it does actually break NVidia desktops
    //if (qgetenv("NO_EGL").isEmpty())
    //    setenv("QT_XCB_GL_INTEGRATION", "xcb_egl", 0);


    // This makes Xlib calls thread-safe which seems to be required for hardware
    // accelerated Flash playback to work without causing mythfrontend to abort.
    QApplication::setAttribute(Qt::AA_X11InitThreads);
#endif
#endif

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

    QSurfaceFormat format;
    format.setDepthBufferSize(0);
    format.setStencilBufferSize(0);
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setSwapInterval(1);

    // try and disable vsync if running test
    if (cmdline.toBool("test"))
    {
        // try and disable sync to vblank on linux x11
        qputenv("vblank_mode", "0"); // Intel and AMD
        qputenv("__GL_SYNC_TO_VBLANK", "0"); // NVidia

        // the default surface format has a swap interval of 1. This is used by
        // the MythMainwindow widget that then drives vsync for all widgets/children
        // (i.e. MythPainterWindow) and we cannot override it on some drivers. So
        // force the default here.
        format.setSwapInterval(0);
    }

    QSurfaceFormat::setDefaultFormat(format);

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
    mainWindow->Init();

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
        auto *test = new VideoPerformanceTest(filename,
                    cmdline.toBool("nodecode"),
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
