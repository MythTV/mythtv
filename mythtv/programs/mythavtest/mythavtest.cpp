#include <iostream>
#include <unistd.h>
#include <utility>

#include <QtGlobal>
#include <QApplication>
#include <QDir>
#include <QString>
#include <QSurfaceFormat>
#include <QTime>

// libmyth*
#include "libmyth/mythcontext.h"
#include "libmythbase/compat.h"
#include "libmythbase/exitcodes.h"
#include "libmythbase/mythdbcon.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythmiscutil.h"
#include "libmythbase/mythversion.h"
#include "libmythbase/programinfo.h"
#include "libmythbase/signalhandling.h"
#include "libmythtv/dbcheck.h"
#include "libmythtv/jitterometer.h"
#include "libmythtv/mythplayerui.h"
#include "libmythtv/mythvideoout.h"
#include "libmythtv/tv_play.h"
#include "libmythui/mythmainwindow.h"
#include "libmythui/mythuihelper.h"

#include "mythavtest_commandlineparser.h"

class VideoPerformanceTest
{
  public:
    VideoPerformanceTest(QString filename, bool decodeno, bool onlydecode,
                         std::chrono::seconds runfor, bool deint, bool gpu)
      : m_file(std::move(filename)),
        m_noDecode(decodeno),
        m_decodeOnly(onlydecode),
        m_secondsToRun(std::clamp(runfor, 1s, 3600s)),
        m_deinterlace(deint),
        m_allowGpu(gpu)
    {
    }

   ~VideoPerformanceTest()
    {
        delete m_ctx;
    }

    void Test(void)
    {
        MythMediaBuffer *rb = MythMediaBuffer::Create(m_file, false, true, 2s);
        m_ctx = new PlayerContext("VideoPerformanceTest");
        auto *mp  = new MythPlayerUI(GetMythMainWindow(), nullptr, m_ctx, static_cast<PlayerFlags>(kAudioMuted | (m_allowGpu ? kDecodeAllowGPU: kNoFlags)));
        mp->GetAudio()->SetAudioInfo("NULL", "NULL", 0, 0);
        mp->GetAudio()->SetNoAudio();
        m_ctx->SetRingBuffer(rb);
        m_ctx->SetPlayer(mp);
        auto *pinfo = new ProgramInfo(m_file);
        m_ctx->SetPlayingInfo(pinfo); // makes a copy
        delete pinfo;

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
            .arg(m_secondsToRun.count()));

        if (m_noDecode)
            LOG(VB_GENERAL, LOG_INFO, "No decode after startup - checking display performance");
        else if (m_decodeOnly)
            LOG(VB_GENERAL, LOG_INFO, "Decoding frames only - skipping display.");
        DecoderBase* dec = mp->GetDecoder();
        if (dec)
            LOG(VB_GENERAL, LOG_INFO, QString("Using decoder: %1").arg(dec->GetCodecDecoderName()));

        auto *jitter = new Jitterometer("Performance: ", static_cast<int>(mp->GetFrameRate()));

        QTime start = QTime::currentTime();
        MythVideoFrame *frame = nullptr;
        while (true)
        {
            mp->ProcessCallbacks();
            auto duration = std::chrono::milliseconds(start.msecsTo(QTime::currentTime()));
            if (duration < 0ms || duration >  m_secondsToRun)
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
                MythDeintType doubledeint = frame->GetDoubleRateOption(DEINT_CPU | DEINT_SHADER | DEINT_DRIVER);
                vo->PrepareFrame(frame, scan);
                vo->RenderFrame(frame, scan);
                vo->EndFrame();

                if (doubledeint && m_deinterlace)
                {
                    doubledeint = frame->GetDoubleRateOption(DEINT_CPU);
                    MythDeintType other = frame->GetDoubleRateOption(DEINT_SHADER | DEINT_DRIVER);
                    if (doubledeint && !other)
                        vo->PrepareFrame(frame, kScan_Intr2ndField);
                    vo->RenderFrame(frame, kScan_Intr2ndField);
                    vo->EndFrame();
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
    QString               m_file;
    bool                  m_noDecode     {false};
    bool                  m_decodeOnly   {false};
    std::chrono::seconds  m_secondsToRun {1s};
    bool                  m_deinterlace  {false};
    bool                  m_allowGpu     {false};
    PlayerContext        *m_ctx          {nullptr};
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
        MythAVTestCommandLineParser::PrintVersion();
        return GENERIC_EXIT_OK;
    }

    int swapinterval = 1;
    if (cmdline.toBool("test"))
    {
        // try and disable sync to vblank on linux x11
        qputenv("vblank_mode", "0"); // Intel and AMD
        qputenv("__GL_SYNC_TO_VBLANK", "0"); // NVidia
        // the default surface format has a swap interval of 1. This is used by
        // the MythMainwindow widget that then drives vsync for all widgets/children
        // (i.e. MythPainterWindow) and we cannot override it on some drivers. So
        // force the default here.
        swapinterval = 0;
    }

    MythDisplay::ConfigureQtGUI(swapinterval, cmdline);

    QApplication a(argc, argv);
    QCoreApplication::setApplicationName(MYTH_APPNAME_MYTHAVTEST);

    int retval = cmdline.ConfigureLogging();
    if (retval != GENERIC_EXIT_OK)
        return retval;

    if (!cmdline.toString("geometry").isEmpty())
        MythMainWindow::ParseGeometryOverride(cmdline.toString("geometry"));

    QString filename = "";
    if (!cmdline.toString("infile").isEmpty())
        filename = cmdline.toString("infile");
    else if (!cmdline.GetArgs().empty())
        filename = cmdline.GetArgs().at(0);

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

#if defined(Q_OS_MACOS)
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
    SignalHandler::Init();
#endif

    if (cmdline.toBool("test"))
    {
        std::chrono::seconds seconds = 5s;
        if (!cmdline.toString("seconds").isEmpty())
            seconds = std::chrono::seconds(cmdline.toInt("seconds"));
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
