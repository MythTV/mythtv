// C++ headers
#include <algorithm> // for min/max, clamp
#include <cmath>
#include <iostream> // for cerr
#include <thread> // for sleep_for

// Qt headers
#include <QCoreApplication>
#include <QString>

// MythTV headers
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythmiscutil.h"
#include "libmythbase/programinfo.h"
#include "libmythbase/sizetliteral.h"
#include "libmythtv/mythcommflagplayer.h"

// Commercial Flagging headers
#include "ClassicCommDetector.h"
#include "ClassicLogoDetector.h"
#include "ClassicSceneChangeDetector.h"

enum frameAspects : std::uint8_t {
    COMM_ASPECT_NORMAL = 0,
    COMM_ASPECT_WIDE
} FrameAspects;

// letter-box and pillar-box are not mutually exclusive
// So 3 is a valid value = (COMM_FORMAT_LETTERBOX | COMM_FORMAT_PILLARBOX)
// And 4 = COMM_FORMAT_MAX is the number of valid values.
enum frameFormats : std::uint8_t {
    COMM_FORMAT_NORMAL    = 0,
    COMM_FORMAT_LETTERBOX = 1,
    COMM_FORMAT_PILLARBOX = 2,
    COMM_FORMAT_MAX       = 4,
} FrameFormats;

static QString toStringFrameMaskValues(int mask, bool verbose)
{
    QString msg;

    if (verbose)
    {
        if (COMM_FRAME_SKIPPED & mask)
            msg += "skipped,";
        if (COMM_FRAME_BLANK & mask)
            msg += "blank,";
        if (COMM_FRAME_SCENE_CHANGE & mask)
            msg += "scene,";
        if (COMM_FRAME_LOGO_PRESENT & mask)
            msg += "logo,";
        if (COMM_FRAME_ASPECT_CHANGE & mask)
            msg += "aspect,";
        if (COMM_FRAME_RATING_SYMBOL & mask)
            msg += "rating,";

        if (!msg.isEmpty())
            msg = msg.left(msg.length() - 1);
        else
            msg = "noflags";
    }
    else
    {
        msg += (COMM_FRAME_SKIPPED       & mask) ? "s" : " ";
        msg += (COMM_FRAME_BLANK         & mask) ? "B" : " ";
        msg += (COMM_FRAME_SCENE_CHANGE  & mask) ? "S" : " ";
        msg += (COMM_FRAME_LOGO_PRESENT  & mask) ? "L" : " ";
        msg += (COMM_FRAME_ASPECT_CHANGE & mask) ? "A" : " ";
        msg += (COMM_FRAME_RATING_SYMBOL & mask) ? "R" : " ";
    }

    return msg;
}

static QString toStringFrameAspects(int aspect, bool verbose)
{
    if (verbose)
        return (COMM_ASPECT_NORMAL == aspect) ? "normal" : " wide ";
    return (COMM_ASPECT_NORMAL == aspect) ? "n" : "w";
}

static QString toStringFrameFormats(int format, bool verbose)
{
    switch (format)
    {
        case COMM_FORMAT_NORMAL:
            return (verbose) ? "normal" : " N ";
        case COMM_FORMAT_LETTERBOX:
            return (verbose) ? "letter" : " L ";
        case COMM_FORMAT_PILLARBOX:
            return (verbose) ? "pillar" : " P ";
        case COMM_FORMAT_LETTERBOX | COMM_FORMAT_PILLARBOX:
            return (verbose) ? "letter,pillar" : "L,P";
        case COMM_FORMAT_MAX:
            return (verbose) ? " max  " : " M ";
    }

    return (verbose) ? "unknown" : " U ";
}

QString FrameInfoEntry::GetHeader(void)
{
    return {"  frame     min/max/avg scene aspect format flags"};
}

QString FrameInfoEntry::toString(uint64_t frame, bool verbose) const
{
    return QString(
        "%1: %2/%3/%4 %5%  %6 %7 %8")
        .arg(frame,10)
        .arg(minBrightness,3)
        .arg(maxBrightness,3)
        .arg(avgBrightness,3)
        .arg(sceneChangePercent,3)
        .arg(toStringFrameAspects(aspect, verbose),
             toStringFrameFormats(format, verbose),
             toStringFrameMaskValues(flagMask, verbose));
}

ClassicCommDetector::ClassicCommDetector(SkipType commDetectMethod_in,
                                         bool showProgress_in,
                                         bool fullSpeed_in,
                                         MythCommFlagPlayer *player_in,
                                         QDateTime startedAt_in,
                                         QDateTime stopsAt_in,
                                         QDateTime recordingStartedAt_in,
                                         QDateTime recordingStopsAt_in) :


    m_commDetectMethod(commDetectMethod_in),
    m_player(player_in),
    m_startedAt(std::move(startedAt_in)),
    m_stopsAt(std::move(stopsAt_in)),
    m_recordingStartedAt(std::move(recordingStartedAt_in)),
    m_recordingStopsAt(std::move(recordingStopsAt_in)),
    m_stillRecording(m_recordingStopsAt > MythDate::current()),
    m_fullSpeed(fullSpeed_in),
    m_showProgress(showProgress_in)
{
    m_commDetectBlankFrameMaxDiff =
        gCoreContext->GetNumSetting("CommDetectBlankFrameMaxDiff", 25);
    m_commDetectDarkBrightness =
        gCoreContext->GetNumSetting("CommDetectDarkBrightness", 80);
    m_commDetectDimBrightness =
        gCoreContext->GetNumSetting("CommDetectDimBrightness", 120);
    m_commDetectBoxBrightness =
        gCoreContext->GetNumSetting("CommDetectBoxBrightness", 30);
    m_commDetectDimAverage =
        gCoreContext->GetNumSetting("CommDetectDimAverage", 35);
    m_commDetectMaxCommBreakLength =
        gCoreContext->GetNumSetting("CommDetectMaxCommBreakLength", 395);
    m_commDetectMinCommBreakLength =
        gCoreContext->GetNumSetting("CommDetectMinCommBreakLength", 60);
    m_commDetectMinShowLength =
        gCoreContext->GetNumSetting("CommDetectMinShowLength", 65);
    m_commDetectMaxCommLength =
        gCoreContext->GetNumSetting("CommDetectMaxCommLength", 125);

    m_commDetectBlankCanHaveLogo =
        !!gCoreContext->GetBoolSetting("CommDetectBlankCanHaveLogo", true);
}

void ClassicCommDetector::Init()
{
    QSize video_disp_dim = m_player->GetVideoSize();
    m_width  = video_disp_dim.width();
    m_height = video_disp_dim.height();
    m_fps = m_player->GetFrameRate();

    m_preRoll  = (long long)(
        std::max(int64_t(0), int64_t(m_recordingStartedAt.secsTo(m_startedAt))) * m_fps);
    m_postRoll = (long long)(
        std::max(int64_t(0), int64_t(m_stopsAt.secsTo(m_recordingStopsAt))) * m_fps);

    // CommDetectBorder's default value of 20 predates the change to use
    // ffmpeg's lowres decoding capability by 5 years.
    // I believe it should be adjusted based on the height of the lowres video
    // CommDetectBorder * height / 720 seems to produce reasonable results.
    // source height =  480 gives border = 20 *  480 / 4 / 720 = 2
    // source height =  720 gives border = 20 *  720 / 4 / 720 = 5
    // source height = 1080 gives border = 20 * 1080 / 4 / 720 = 7
    m_commDetectBorder =
        gCoreContext->GetNumSetting("CommDetectBorder", 20) * m_height / 720;

    m_currentAspect = COMM_ASPECT_WIDE;

    m_lastFrameNumber = -2;
    m_curFrameNumber = -1;
    m_verboseDebugging = qEnvironmentVariableIsSet("DEBUGCOMMFLAG");

    LOG(VB_COMMFLAG, LOG_INFO,
        QString("Commercial Detection initialized: "
                "width = %1, height = %2, fps = %3, method = %4")
            .arg(m_width).arg(m_height)
            .arg(m_player->GetFrameRate()).arg(m_commDetectMethod));

    if ((m_width * m_height) > 1000000)
    {
        m_horizSpacing = 10;
        m_vertSpacing = 10;
    }
    else if ((m_width * m_height) > 800000)
    {
        m_horizSpacing = 8;
        m_vertSpacing = 8;
    }
    else if ((m_width * m_height) > 400000)
    {
        m_horizSpacing = 6;
        m_vertSpacing = 6;
    }
    else if ((m_width * m_height) > 300000)
    {
        m_horizSpacing = 6;
        m_vertSpacing = 4;
    }
    else
    {
        m_horizSpacing = 4;
        m_vertSpacing = 4;
    }

    LOG(VB_COMMFLAG, LOG_INFO,
        QString("Using Sample Spacing of %1 horizontal & %2 vertical pixels.")
            .arg(m_horizSpacing).arg(m_vertSpacing));

    m_framesProcessed = 0;
    m_totalMinBrightness = 0;
    m_blankFrameCount = 0;

    m_aggressiveDetection = true;
    m_currentAspect = COMM_ASPECT_WIDE;
    m_decoderFoundAspectChanges = false;

    m_lastSentCommBreakMap.clear();

    // Check if close to 4:3
    if (fabs(((m_width*1.0)/m_height) - 1.333333) < 0.1)
        m_currentAspect = COMM_ASPECT_NORMAL;

    m_sceneChangeDetector = new ClassicSceneChangeDetector(m_width, m_height,
        m_commDetectBorder, m_horizSpacing, m_vertSpacing);
    connect(
         m_sceneChangeDetector,
         &SceneChangeDetectorBase::haveNewInformation,
         this,
         &ClassicCommDetector::sceneChangeDetectorHasNewInformation
    );

    m_frameIsBlank = false;
    m_stationLogoPresent = false;

    m_logoInfoAvailable = false;

    ClearAllMaps();

    if (m_verboseDebugging)
    {
        LOG(VB_COMMFLAG, LOG_DEBUG,
            "       Fr #      Min Max Avg Scn F A Mask");
        LOG(VB_COMMFLAG, LOG_DEBUG, 
            "       ------    --- --- --- --- - - ----");
    }
}

void ClassicCommDetector::deleteLater(void)
{
    if (m_sceneChangeDetector)
        m_sceneChangeDetector->deleteLater();

    if (m_logoDetector)
        m_logoDetector->deleteLater();

    CommDetectorBase::deleteLater();
}

bool ClassicCommDetector::go()
{
    int secsSince = 0;
    int requiredBuffer = 30;
    int requiredHeadStart = requiredBuffer;
    bool wereRecording = m_stillRecording;

    emit statusUpdate(QCoreApplication::translate("(mythcommflag)",
        "Building Head Start Buffer"));
    secsSince = m_recordingStartedAt.secsTo(MythDate::current());
    while (m_stillRecording && (secsSince < requiredHeadStart))
    {
        emit breathe();
        if (m_bStop)
            return false;

        std::this_thread::sleep_for(2s);
        secsSince = m_recordingStartedAt.secsTo(MythDate::current());
    }

    if (m_player->OpenFile() < 0)
        return false;

    Init();

    if (m_commDetectMethod & COMM_DETECT_LOGO)
    {
        // Use a different border for logo detection.
        // If we try to detect logos in letterboxed areas,
        // chances are we won't detect the logo.
        // Generally speaking, SD video is likely to be letter boxed
        // and HD video is not likely to be letter boxed.
        // To detect logos, try to exclude letterboxed area from SD video
        // but exclude too much from HD video and you'll miss the logo.
        // Using the same border for both with no scaling seems to be
        // a good compromise.
        int logoDetectBorder =
            gCoreContext->GetNumSetting("CommDetectLogoBorder", 16);
        m_logoDetector = new ClassicLogoDetector(this, m_width, m_height,
            logoDetectBorder);

        requiredHeadStart += std::max(
            int64_t(0), int64_t(m_recordingStartedAt.secsTo(m_startedAt)));
        requiredHeadStart += m_logoDetector->getRequiredAvailableBufferForSearch();

        emit statusUpdate(QCoreApplication::translate("(mythcommflag)",
            "Building Logo Detection Buffer"));
        secsSince = m_recordingStartedAt.secsTo(MythDate::current());
        while (m_stillRecording && (secsSince < requiredHeadStart))
        {
            emit breathe();
            if (m_bStop)
                return false;

            std::this_thread::sleep_for(2s);
            secsSince = m_recordingStartedAt.secsTo(MythDate::current());
        }
    }

    // Don't bother flagging short ~realtime recordings
    if ((wereRecording) && (!m_stillRecording) && (secsSince < requiredHeadStart))
        return false;

    m_aggressiveDetection =
        gCoreContext->GetBoolSetting("AggressiveCommDetect", true);

    if (!m_player->InitVideo())
    {
        LOG(VB_GENERAL, LOG_ERR,
            "NVP: Unable to initialize video for FlagCommercials.");
        return false;
    }

    if (m_commDetectMethod & COMM_DETECT_LOGO)
    {
        emit statusUpdate(QCoreApplication::translate("(mythcommflag)",
            "Searching for Logo"));

        if (m_showProgress)
        {
            std::cerr << "Finding Logo";
            std::cerr.flush();
        }
        LOG(VB_GENERAL, LOG_INFO, "Finding Logo");

        m_logoInfoAvailable = m_logoDetector->searchForLogo(m_player);
        if (!m_logoInfoAvailable)
        {
            if (COMM_DETECT_LOGO == m_commDetectMethod)
            {
                LOG(VB_COMMFLAG, LOG_WARNING, "No logo found, abort flagging.");
                return false;
            }
        }

        if (m_showProgress)
        {
            std::cerr << "\b\b\b\b\b\b\b\b\b\b\b\b            "
                         "\b\b\b\b\b\b\b\b\b\b\b\b";
            std::cerr.flush();
        }
    }

    emit breathe();
    if (m_bStop)
        return false;

    QElapsedTimer flagTime;
    flagTime.start();

    long long myTotalFrames = 0;
    if (m_recordingStopsAt < MythDate::current() )
        myTotalFrames = m_player->GetTotalFrameCount();
    else
        myTotalFrames = (long long)(m_player->GetFrameRate() *
                        (m_recordingStartedAt.secsTo(m_recordingStopsAt)));

    if (m_showProgress)
    {
        if (myTotalFrames)
            std::cerr << "\r  0%/          \r" << std::flush;
        else
            std::cerr << "\r     0/        \r" << std::flush;
    }


    float aspect = m_player->GetVideoAspect();
    int prevpercent = -1;

    SetVideoParams(aspect);

    emit breathe();

    m_player->ResetTotalDuration();

    while (m_player->GetEof() == kEofStateNone)
    {
        std::chrono::microseconds startTime {0us};
        if (m_stillRecording)
            startTime = nowAsDuration<std::chrono::microseconds>();

        MythVideoFrame* currentFrame = m_player->GetRawVideoFrame();
        long long currentFrameNumber = currentFrame->m_frameNumber;

        //Lucas: maybe we should make the nuppelvideoplayer send out a signal
        //when the aspect ratio changes.
        //In order to not change too many things at a time, I"m using basic
        //polling for now.
        float newAspect = currentFrame->m_aspect;
        if (newAspect != aspect)
        {
            SetVideoParams(aspect);
            aspect = newAspect;
        }

        if (((currentFrameNumber % 500) == 0) ||
            (((currentFrameNumber % 100) == 0) &&
             (m_stillRecording)))
        {
            emit breathe();
            if (m_bStop)
            {
                m_player->DiscardVideoFrame(currentFrame);
                return false;
            }
        }

        if ((m_sendCommBreakMapUpdates) &&
            ((m_commBreakMapUpdateRequested) ||
             ((currentFrameNumber % 500) == 0)))
        {
            frm_dir_map_t commBreakMap;
            frm_dir_map_t::iterator it;
            frm_dir_map_t::iterator lastIt;
            bool mapsAreIdentical = false;

            GetCommercialBreakList(commBreakMap);

            if ((commBreakMap.empty()) &&
                (m_lastSentCommBreakMap.empty()))
            {
                mapsAreIdentical = true;
            }
            else if (commBreakMap.size() == m_lastSentCommBreakMap.size())
            {
                // assume true for now and set false if we find a difference
                mapsAreIdentical = true;
                for (it = commBreakMap.begin();
                     it != commBreakMap.end() && mapsAreIdentical; ++it)
                {
                    lastIt = m_lastSentCommBreakMap.find(it.key());
                    if ((lastIt == m_lastSentCommBreakMap.end()) ||
                        (*lastIt != *it))
                        mapsAreIdentical = false;
                }
            }

            if (m_commBreakMapUpdateRequested || !mapsAreIdentical)
            {
                emit gotNewCommercialBreakList();
                m_lastSentCommBreakMap = commBreakMap;
            }

            if (m_commBreakMapUpdateRequested)
                m_commBreakMapUpdateRequested = false;
        }

        while (m_bPaused)
        {
            emit breathe();
            std::this_thread::sleep_for(1s);
        }

        // sleep a little so we don't use all cpu even if we're niced
        if (!m_fullSpeed && !m_stillRecording)
            std::this_thread::sleep_for(10ms);

        if (((currentFrameNumber % 500) == 0) ||
            ((m_showProgress || m_stillRecording) &&
             ((currentFrameNumber % 100) == 0)))
        {
            float flagFPS { 0.0 };
            float elapsed = flagTime.elapsed() / 1000.0F;

            if (elapsed != 0.0F)
                flagFPS = currentFrameNumber / elapsed;
            else
                flagFPS = 0.0;

            int percentage = 0;
            if (myTotalFrames)
                percentage = currentFrameNumber * 100 / myTotalFrames;

            percentage = std::min(percentage, 100);

            if (m_showProgress)
            {
                if (myTotalFrames)
                {
                    QString tmp = QString("\r%1%/%2fps  \r")
                        .arg(percentage, 3).arg((int)flagFPS, 4);
                    std::cerr << qPrintable(tmp) << std::flush;
                }
                else
                {
                    QString tmp = QString("\r%1/%2fps  \r")
                        .arg(currentFrameNumber, 6).arg((int)flagFPS, 4);
                    std::cerr << qPrintable(tmp) << std::flush;
                }
            }

            if (myTotalFrames)
            {
                emit statusUpdate(QCoreApplication::translate("(mythcommflag)",
                    "%1% Completed @ %2 fps.")
                        .arg(percentage).arg(flagFPS));
            }
            else
            {
                emit statusUpdate(QCoreApplication::translate("(mythcommflag)",
                    "%1 Frames Completed @ %2 fps.")
                        .arg(currentFrameNumber).arg(flagFPS));
            }

            if (percentage % 10 == 0 && prevpercent != percentage)
            {
                prevpercent = percentage;
                LOG(VB_GENERAL, LOG_INFO, QString("%1%% Completed @ %2 fps.")
                    .arg(percentage) .arg(flagFPS));
            }
        }

        ProcessFrame(currentFrame, currentFrameNumber);

        if (m_stillRecording)
        {
            int secondsRecorded =
                m_recordingStartedAt.secsTo(MythDate::current());
            int secondsFlagged = (int)(m_framesProcessed / m_fps);
            int secondsBehind = secondsRecorded - secondsFlagged;
            auto usecPerFrame = floatusecs(1000000.0F / m_player->GetFrameRate());

            auto endTime = nowAsDuration<std::chrono::microseconds>();

            floatusecs usecSleep = usecPerFrame - (endTime - startTime);

            if (secondsBehind > requiredBuffer)
            {
                if (m_fullSpeed)
                    usecSleep = 0us;
                else
                    usecSleep = usecSleep * 0.25;
            }
            else if (secondsBehind < requiredBuffer)
            {
                usecSleep = usecPerFrame * 1.5;
            }

            if (usecSleep > 0us)
                std::this_thread::sleep_for(usecSleep);
        }

        m_player->DiscardVideoFrame(currentFrame);
    }

    if (m_showProgress)
    {
#if 0
        float elapsed = flagTime.elapsed() / 1000.0;

        if (elapsed)
            flagFPS = currentFrameNumber / elapsed;
        else
            flagFPS = 0.0;
#endif

        if (myTotalFrames)
            std::cerr << "\b\b\b\b\b\b      \b\b\b\b\b\b";
        else
            std::cerr << "\b\b\b\b\b\b\b\b\b\b\b\b\b             "
                         "\b\b\b\b\b\b\b\b\b\b\b\b\b";
        std::cerr.flush();
    }

    return true;
}

void ClassicCommDetector::sceneChangeDetectorHasNewInformation(
    unsigned int framenum,bool isSceneChange,float debugValue)
{
    if (isSceneChange)
    {
        m_frameInfo[framenum].flagMask |= COMM_FRAME_SCENE_CHANGE;
        m_sceneMap[framenum] = MARK_SCENE_CHANGE;
    }
    else
    {
        m_frameInfo[framenum].flagMask &= ~COMM_FRAME_SCENE_CHANGE;
        m_sceneMap.remove(framenum);
    }

    m_frameInfo[framenum].sceneChangePercent = (int) (debugValue*100);
}

void ClassicCommDetector::GetCommercialBreakList(frm_dir_map_t &marks)
{

    LOG(VB_COMMFLAG, LOG_INFO, "CommDetect::GetCommBreakMap()");

    marks.clear();

    CleanupFrameInfo();

    bool blank = (COMM_DETECT_BLANK & m_commDetectMethod) != 0;
    bool scene = (COMM_DETECT_SCENE & m_commDetectMethod) != 0;
    bool logo  = (COMM_DETECT_LOGO  & m_commDetectMethod) != 0;

    if (COMM_DETECT_OFF == m_commDetectMethod)
        return;

    if (!blank && !scene && !logo)
    {
        LOG(VB_COMMFLAG, LOG_ERR, QString("Unexpected commDetectMethod: 0x%1")
                .arg(m_commDetectMethod,0,16));
        return;
    }

    if (blank && scene && logo)
    {
        BuildAllMethodsCommList();
        marks = m_commBreakMap;
        LOG(VB_COMMFLAG, LOG_INFO, "Final Commercial Break Map");
        return;
    }

    if (blank)
    {
        BuildBlankFrameCommList();
        marks = m_blankCommBreakMap;
    }

    if (scene)
    {
        BuildSceneChangeCommList();
        marks = m_sceneCommBreakMap;
    }

    if (logo)
    {
        BuildLogoCommList();
        marks = m_logoCommBreakMap;
    }

    int cnt = ((blank) ? 1 : 0) + ((scene) ? 1 : 0) + ((logo) ? 1 : 0);
    if (cnt == 2)
    {
        if (blank && scene)
        {
            marks = m_commBreakMap = Combine2Maps(
                m_blankCommBreakMap, m_sceneCommBreakMap);
        }
        else if (blank && logo)
        {
            marks = m_commBreakMap = Combine2Maps(
                m_blankCommBreakMap, m_logoCommBreakMap);
        }
        else if (scene && logo)
        {
            marks = m_commBreakMap = Combine2Maps(
                m_sceneCommBreakMap, m_logoCommBreakMap);
        }
    }

    LOG(VB_COMMFLAG, LOG_INFO, "Final Commercial Break Map");
}

void ClassicCommDetector::recordingFinished([[maybe_unused]] long long totalFileSize)
{
    m_stillRecording = false;
}

void ClassicCommDetector::requestCommBreakMapUpdate(void)
{
    m_commBreakMapUpdateRequested = true;
    m_sendCommBreakMapUpdates = true;
}

void ClassicCommDetector::SetVideoParams(float aspect)
{
    int newAspect = COMM_ASPECT_WIDE;

    LOG(VB_COMMFLAG, LOG_INFO,
        QString("CommDetect::SetVideoParams called with aspect = %1")
            .arg(aspect));
    // Default to Widescreen but use the same check as VideoOutput::MoveResize()
    // to determine if is normal 4:3 aspect
    if (fabs(aspect - 1.333333F) < 0.1F)
        newAspect = COMM_ASPECT_NORMAL;

    if (newAspect != m_currentAspect)
    {
        LOG(VB_COMMFLAG, LOG_INFO,
            QString("Aspect Ratio changed from %1 to %2 at frame %3")
                .arg(m_currentAspect).arg(newAspect)
                .arg(m_curFrameNumber));

        if (m_frameInfo.contains(m_curFrameNumber))
        {
            // pretend that this frame is blank so that we can create test
            // blocks on real aspect ratio change boundaries.
            m_frameInfo[m_curFrameNumber].flagMask |= COMM_FRAME_BLANK;
            m_frameInfo[m_curFrameNumber].flagMask |= COMM_FRAME_ASPECT_CHANGE;
            m_decoderFoundAspectChanges = true;
        }
        else if (m_curFrameNumber != -1)
        {
            LOG(VB_COMMFLAG, LOG_ERR,
                QString("Unable to keep track of Aspect ratio change because "
                        "frameInfo for frame number %1 does not exist.")
                    .arg(m_curFrameNumber));
        }
        m_currentAspect = newAspect;
    }
}

void ClassicCommDetector::ProcessFrame(MythVideoFrame *frame,
                                       long long frame_number)
{
    int max = 0;
    int min = 255;
    int blankPixelsChecked = 0;
    long long totBrightness = 0;
    auto *rowMax = new unsigned char[m_height];
    auto *colMax = new unsigned char[m_width];
    memset(rowMax, 0, sizeof(*rowMax)*m_height);
    memset(colMax, 0, sizeof(*colMax)*m_width);
    int topDarkRow = m_commDetectBorder;
    int bottomDarkRow = m_height - m_commDetectBorder - 1;
    int leftDarkCol = m_commDetectBorder;
    int rightDarkCol = m_width - m_commDetectBorder - 1;
    FrameInfoEntry fInfo {};

    if (!frame || !(frame->m_buffer) || frame_number == -1 ||
        frame->m_type != FMT_YV12)
    {
        LOG(VB_COMMFLAG, LOG_ERR, "CommDetect: Invalid video frame or codec, "
                                  "unable to process frame.");
        delete[] rowMax;
        delete[] colMax;
        return;
    }

    if (!m_width || !m_height)
    {
        LOG(VB_COMMFLAG, LOG_ERR, "CommDetect: Width or Height is 0, "
                                  "unable to process frame.");
        delete[] rowMax;
        delete[] colMax;
        return;
    }

    m_curFrameNumber = frame_number;
    unsigned char* framePtr = frame->m_buffer;
    int bytesPerLine = frame->m_pitches[0];

    fInfo.minBrightness = -1;
    fInfo.maxBrightness = -1;
    fInfo.avgBrightness = -1;
    fInfo.sceneChangePercent = -1;
    fInfo.aspect = m_currentAspect;
    fInfo.format = COMM_FORMAT_NORMAL;
    fInfo.flagMask = 0;

    int& flagMask = m_frameInfo[m_curFrameNumber].flagMask;

    // Fill in dummy info records for skipped frames.
    if (m_lastFrameNumber != (m_curFrameNumber - 1))
    {
        if (m_lastFrameNumber > 0)
        {
            fInfo.aspect = m_frameInfo[m_lastFrameNumber].aspect;
            fInfo.format = m_frameInfo[m_lastFrameNumber].format;
        }
        fInfo.flagMask = COMM_FRAME_SKIPPED;

        m_lastFrameNumber++;
        while(m_lastFrameNumber < m_curFrameNumber)
            m_frameInfo[m_lastFrameNumber++] = fInfo;

        fInfo.flagMask = 0;
    }
    m_lastFrameNumber = m_curFrameNumber;

    m_frameInfo[m_curFrameNumber] = fInfo;

    if (m_commDetectMethod & COMM_DETECT_BLANKS)
        m_frameIsBlank = false;

    if (m_commDetectMethod & COMM_DETECT_SCENE)
    {
        m_sceneChangeDetector->processFrame(frame);
    }

    m_stationLogoPresent = false;

    for(int y = m_commDetectBorder; y < (m_height - m_commDetectBorder);
            y += m_vertSpacing)
    {
        for(int x = m_commDetectBorder; x < (m_width - m_commDetectBorder);
                x += m_horizSpacing)
        {
            uchar pixel = framePtr[(y * bytesPerLine) + x];

            if (m_commDetectMethod & COMM_DETECT_BLANKS)
            {
                 bool checkPixel = false;
                 if (!m_commDetectBlankCanHaveLogo)
                     checkPixel = true;

                 if (!m_logoInfoAvailable ||
                     !m_logoDetector->pixelInsideLogo(x,y))
                     checkPixel=true;

                 if (checkPixel)
                 {
                     blankPixelsChecked++;
                     totBrightness += pixel;

                     min = std::min<int>(pixel, min);
                     max = std::max<int>(pixel, max);
                     rowMax[y] = std::max(pixel, rowMax[y]);
                     colMax[x] = std::max(pixel, colMax[x]);
                 }
            }
        }
    }

    if ((m_commDetectMethod & COMM_DETECT_BLANKS) && blankPixelsChecked)
    {
        for(int y = m_commDetectBorder; y < (m_height - m_commDetectBorder);
                y += m_vertSpacing)
        {
            if (rowMax[y] > m_commDetectBoxBrightness)
                break;
            topDarkRow = y;
        }

        for(int y = m_commDetectBorder; y < (m_height - m_commDetectBorder);
                y += m_vertSpacing)
            if (rowMax[y] >= m_commDetectBoxBrightness)
                bottomDarkRow = y;

        delete[] rowMax;
        rowMax = nullptr;

        for(int x = m_commDetectBorder; x < (m_width - m_commDetectBorder);
                x += m_horizSpacing)
        {
            if (colMax[x] > m_commDetectBoxBrightness)
                break;
            leftDarkCol = x;
        }

        for(int x = m_commDetectBorder; x < (m_width - m_commDetectBorder);
                x += m_horizSpacing)
            if (colMax[x] >= m_commDetectBoxBrightness)
                rightDarkCol = x;

        delete[] colMax;
        colMax = nullptr;

        m_frameInfo[m_curFrameNumber].format = COMM_FORMAT_NORMAL;
        if ((topDarkRow > m_commDetectBorder) &&
            (topDarkRow < (m_height * .20)) &&
            (bottomDarkRow < (m_height - m_commDetectBorder)) &&
            (bottomDarkRow > (m_height * .80)))
        {
            m_frameInfo[m_curFrameNumber].format |= COMM_FORMAT_LETTERBOX;
        }
        if ((leftDarkCol > m_commDetectBorder) &&
                 (leftDarkCol < (m_width * .20)) &&
                 (rightDarkCol < (m_width - m_commDetectBorder)) &&
                 (rightDarkCol > (m_width * .80)))
        {
            m_frameInfo[m_curFrameNumber].format |= COMM_FORMAT_PILLARBOX;
        }

        int avg = totBrightness / blankPixelsChecked;

        m_frameInfo[m_curFrameNumber].minBrightness = min;
        m_frameInfo[m_curFrameNumber].maxBrightness = max;
        m_frameInfo[m_curFrameNumber].avgBrightness = avg;

        m_totalMinBrightness += min;
        m_commDetectDimAverage = min + 10;

        // Is the frame really dark
        if (((max - min) <= m_commDetectBlankFrameMaxDiff) &&
            (max < m_commDetectDimBrightness))
            m_frameIsBlank = true;

        // Are we non-strict and the frame is blank
        if ((!m_aggressiveDetection) &&
            ((max - min) <= m_commDetectBlankFrameMaxDiff))
            m_frameIsBlank = true;

        // Are we non-strict and the frame is dark
        //                   OR the frame is dim and has a low avg brightness
        if ((!m_aggressiveDetection) &&
            ((max < m_commDetectDarkBrightness) ||
             ((max < m_commDetectDimBrightness) && (avg < m_commDetectDimAverage))))
            m_frameIsBlank = true;
    }

    if ((m_logoInfoAvailable) && (m_commDetectMethod & COMM_DETECT_LOGO))
    {
        m_stationLogoPresent =
            m_logoDetector->doesThisFrameContainTheFoundLogo(frame);
    }

#if 0
    if ((m_commDetectMethod == COMM_DETECT_ALL) &&
        (CheckRatingSymbol()))
    {
        flagMask |= COMM_FRAME_RATING_SYMBOL;
    }
#endif

    if (m_frameIsBlank)
    {
        m_blankFrameMap[m_curFrameNumber] = MARK_BLANK_FRAME;
        flagMask |= COMM_FRAME_BLANK;
        m_blankFrameCount++;
    }

    if (m_stationLogoPresent)
        flagMask |= COMM_FRAME_LOGO_PRESENT;

    //TODO: move this debugging code out of the perframe loop, and do it after
    // we've processed all frames. this is because a scenechangedetector can
    // now use a few frames to determine whether the frame a few frames ago was
    // a scene change or not.. due to this lookahead possibility the values
    // that are currently in the frameInfo array, might be changed a few frames
    // from now. The ClassicSceneChangeDetector doesn't use this though. future
    // scenechangedetectors might.

    if (m_verboseDebugging)
    {
        LOG(VB_COMMFLAG, LOG_DEBUG, QString("Frame: %1 -> %2 %3 %4 %5 %6 %7 %8")
            .arg(m_curFrameNumber, 6)
            .arg(m_frameInfo[m_curFrameNumber].minBrightness, 3)
            .arg(m_frameInfo[m_curFrameNumber].maxBrightness, 3)
            .arg(m_frameInfo[m_curFrameNumber].avgBrightness, 3)
            .arg(m_frameInfo[m_curFrameNumber].sceneChangePercent, 3)
            .arg(m_frameInfo[m_curFrameNumber].format, 1)
            .arg(m_frameInfo[m_curFrameNumber].aspect, 1)
            .arg(m_frameInfo[m_curFrameNumber].flagMask, 4, 16, QChar('0')));
    }

    m_framesProcessed++;
    delete[] rowMax;
    delete[] colMax;
}

void ClassicCommDetector::ClearAllMaps(void)
{
    LOG(VB_COMMFLAG, LOG_INFO, "CommDetect::ClearAllMaps()");

    m_frameInfo.clear();
    m_blankFrameMap.clear();
    m_blankCommMap.clear();
    m_blankCommBreakMap.clear();
    m_sceneMap.clear();
    m_sceneCommBreakMap.clear();
    m_commBreakMap.clear();
}

void ClassicCommDetector::GetBlankCommMap(frm_dir_map_t &comms)
{
    LOG(VB_COMMFLAG, LOG_INFO, "CommDetect::GetBlankCommMap()");

    if (m_blankCommMap.isEmpty())
        BuildBlankFrameCommList();

    comms = m_blankCommMap;
}

void ClassicCommDetector::GetBlankCommBreakMap(frm_dir_map_t &comms)
{
    LOG(VB_COMMFLAG, LOG_INFO, "CommDetect::GetBlankCommBreakMap()");

    if (m_blankCommBreakMap.isEmpty())
        BuildBlankFrameCommList();

    comms = m_blankCommBreakMap;
}

void ClassicCommDetector::GetSceneChangeMap(frm_dir_map_t &scenes,
                                            int64_t start_frame)
{
    LOG(VB_COMMFLAG, LOG_INFO, "CommDetect::GetSceneChangeMap()");

    frm_dir_map_t::iterator it;

    if (start_frame == -1)
        scenes.clear();

    for (it = m_sceneMap.begin(); it != m_sceneMap.end(); ++it)
        if ((start_frame == -1) || ((int64_t)it.key() >= start_frame))
            scenes[it.key()] = *it;
}

frm_dir_map_t ClassicCommDetector::Combine2Maps(const frm_dir_map_t &a,
                                                const frm_dir_map_t &b) const
{
    LOG(VB_COMMFLAG, LOG_INFO, "CommDetect::BuildMasterCommList()");

    frm_dir_map_t newMap;

    if (!a.empty())
    {
        frm_dir_map_t::const_iterator it = a.begin();
        for (; it != a.end(); ++it)
            newMap[it.key()] = *it;
    }

    if ((a.size() > 1) &&
        (b.size() > 1))
    {
        // see if beginning of the recording looks like a commercial
        frm_dir_map_t::const_iterator it_a;
        frm_dir_map_t::const_iterator it_b;

        it_a = a.begin();
        it_b = b.begin();

        if ((it_b.key() < 2) && (it_a.key() > 2))
        {
            newMap.remove(it_a.key());
            newMap[0] = MARK_COMM_START;
        }


        // see if ending of recording looks like a commercial
        frm_dir_map_t::const_iterator it;
        uint64_t max_a = 0;
        uint64_t max_b = 0;

        it = a.begin();
        for (; it != a.end(); ++it)
        {
            if ((*it == MARK_COMM_END) && (it.key() > max_a))
                max_a = it.key();
        }

        it = b.begin();
        for (; it != b.end(); ++it)
        {
            if ((*it == MARK_COMM_END) && (it.key() > max_b))
                max_b = it.key();
        }

        if ((max_a < (m_framesProcessed - 2)) &&
            (max_b > (m_framesProcessed - 2)))
        {
            newMap.remove(max_a);
            newMap[m_framesProcessed] = MARK_COMM_END;
        }
    }

    if ((a.size() > 3) &&
        (b.size() > 1))
    {
        frm_dir_map_t::const_iterator it_a;
        frm_dir_map_t::const_iterator it_b;

        it_a = a.begin();
        ++it_a;
        it_b = it_a;
        ++it_b;
        while (it_b != a.end())
        {
            uint64_t fdiff = it_b.key() - it_a.key();
            bool allTrue = false;

            if (fdiff < (62 * m_fps))
            {
                uint64_t f = it_a.key() + 1;

                allTrue = true;

                while ((f <= m_framesProcessed) && (f < it_b.key()) && (allTrue))
                    allTrue = FrameIsInBreakMap(f++, b);
            }

            if (allTrue)
            {
                newMap.remove(it_a.key());
                newMap.remove(it_b.key());
            }

            ++it_a; ++it_a;
            ++it_b;
            if (it_b != a.end())
                ++it_b;
        }
    }

    return newMap;
}

void ClassicCommDetector::UpdateFrameBlock(FrameBlock *fbp,
                                           const FrameInfoEntry& finfo,
                                           int format, int aspect)
{
    int value = 0;

    value = finfo.flagMask;

    if (value & COMM_FRAME_LOGO_PRESENT)
        fbp->logoCount++;

    if (value & COMM_FRAME_RATING_SYMBOL)
        fbp->ratingCount++;

    if (value & COMM_FRAME_SCENE_CHANGE)
        fbp->scCount++;

    if (finfo.format == format)
        fbp->formatMatch++;

    if (finfo.aspect == aspect)
        fbp->aspectMatch++;
}

void ClassicCommDetector::BuildAllMethodsCommList(void)
{
    LOG(VB_COMMFLAG, LOG_INFO, "CommDetect::BuildAllMethodsCommList()");

    int lastScore = 0;
    uint64_t lastStart = 0;
    uint64_t lastEnd = 0;
    int64_t firstLogoFrame = -1;
    int format = COMM_FORMAT_NORMAL;
    int aspect = COMM_ASPECT_NORMAL;
    QString msg;
    std::array<uint64_t,COMM_FORMAT_MAX> formatCounts {};
    frm_dir_map_t tmpCommMap;
    frm_dir_map_t::iterator it;

    m_commBreakMap.clear();

    auto *fblock = new FrameBlock[m_blankFrameCount + 2];

    int curBlock = 0;
    uint64_t curFrame = 1;

    FrameBlock *fbp = &fblock[curBlock];
    fbp->start = 0;
    fbp->bfCount = 0;
    fbp->logoCount = 0;
    fbp->ratingCount = 0;
    fbp->scCount = 0;
    fbp->scRate = 0.0;
    fbp->formatMatch = 0;
    fbp->aspectMatch = 0;
    fbp->score = 0;

    bool lastFrameWasBlank = true;

    if (m_decoderFoundAspectChanges)
    {
        uint64_t aspectFrames = 0;
        for (int64_t i = m_preRoll;
             i < ((int64_t)m_framesProcessed - (int64_t)m_postRoll); i++)
        {
            if ((m_frameInfo.contains(i)) &&
                (m_frameInfo[i].aspect == COMM_ASPECT_NORMAL))
                aspectFrames++;
        }

        if (aspectFrames < ((m_framesProcessed - m_preRoll - m_postRoll) / 2))
        {
            aspect = COMM_ASPECT_WIDE;
//          aspectFrames = m_framesProcessed - m_preRoll - m_postRoll - aspectFrames;
        }
    }
    else
    {
        formatCounts.fill(0);

        for(int64_t i = m_preRoll;
            i < ((int64_t)m_framesProcessed - (int64_t)m_postRoll); i++ )
        {
            if ((m_frameInfo.contains(i)) &&
                (m_frameInfo[i].format >= 0) &&
                (m_frameInfo[i].format < COMM_FORMAT_MAX))
                formatCounts[m_frameInfo[i].format]++;
        }

        uint64_t formatFrames = 0;
        for(int i = 0; i < COMM_FORMAT_MAX; i++)
        {
            if (formatCounts[i] > formatFrames)
            {
                format = i;
                formatFrames = formatCounts[i];
            }
        }
    }

    while (curFrame <= m_framesProcessed)
    {
        int value = m_frameInfo[curFrame].flagMask;

        bool nextFrameIsBlank = ((curFrame + 1) <= m_framesProcessed) &&
            ((m_frameInfo[curFrame + 1].flagMask & COMM_FRAME_BLANK) != 0);

        if (value & COMM_FRAME_BLANK)
        {
            fbp->bfCount++;

            if (!nextFrameIsBlank || !lastFrameWasBlank)
            {
                UpdateFrameBlock(fbp, m_frameInfo[curFrame], format, aspect);

                fbp->end = curFrame;
                fbp->frames = fbp->end - fbp->start + 1;
                fbp->length = fbp->frames / m_fps;

                if ((fbp->scCount) && (fbp->length > 1.05))
                    fbp->scRate = fbp->scCount / fbp->length;

                curBlock++;

                fbp = &fblock[curBlock];
                fbp->bfCount = 1;
                fbp->logoCount = 0;
                fbp->ratingCount = 0;
                fbp->scCount = 0;
                fbp->scRate = 0.0;
                fbp->score = 0;
                fbp->formatMatch = 0;
                fbp->aspectMatch = 0;
                fbp->start = curFrame;
            }

            lastFrameWasBlank = true;
        }
        else
        {
            lastFrameWasBlank = false;
        }

        UpdateFrameBlock(fbp, m_frameInfo[curFrame], format, aspect);

        if ((value & COMM_FRAME_LOGO_PRESENT) &&
            (firstLogoFrame == -1))
            firstLogoFrame = curFrame;

        curFrame++;
    }

    fbp->end = curFrame;
    fbp->frames = fbp->end - fbp->start + 1;
    fbp->length = fbp->frames / m_fps;

    if ((fbp->scCount) && (fbp->length > 1.05))
        fbp->scRate = fbp->scCount / fbp->length;

    int maxBlock = curBlock;
    curBlock = 0;
//  lastScore = 0;

    LOG(VB_COMMFLAG, LOG_INFO, "Initial Block pass");
    LOG(VB_COMMFLAG, LOG_DEBUG,
        "Block StTime StFrm  EndFrm Frames Secs    "
        "Bf  Lg Cnt RT Cnt SC Cnt SC Rt FmtMch AspMch Score");
    LOG(VB_COMMFLAG, LOG_INFO,
        "----- ------ ------ ------ ------ ------- "
        "--- ------ ------ ------ ----- ------ ------ -----");
    while (curBlock <= maxBlock)
    {
        fbp = &fblock[curBlock];

        msg = FormatMsg(curBlock, fbp);
        LOG(VB_COMMFLAG, LOG_DEBUG, msg);

        if (fbp->frames > m_fps)
        {
            if (m_verboseDebugging)
                LOG(VB_COMMFLAG, LOG_DEBUG,
                    QString("      FRAMES > %1").arg(m_fps));

            if (fbp->length > m_commDetectMaxCommLength)
            {
                if (m_verboseDebugging)
                    LOG(VB_COMMFLAG, LOG_DEBUG,
                        "      length > max comm length, +20");
                fbp->score += 20;
            }

            if (fbp->length > m_commDetectMaxCommBreakLength)
            {
                if (m_verboseDebugging)
                    LOG(VB_COMMFLAG, LOG_DEBUG,
                        "      length > max comm break length, +20");
                fbp->score += 20;
            }

            if ((fbp->length > 4) &&
                (fbp->logoCount > (fbp->frames * 0.60)) &&
                (fbp->bfCount < (fbp->frames * 0.10)))
            {
                if (m_verboseDebugging)
                {
                    LOG(VB_COMMFLAG, LOG_DEBUG,
                        "      length > 4 && logoCount > frames * 0.60 && "
                        "bfCount < frames * .10");
                }
                if (fbp->length > m_commDetectMaxCommBreakLength)
                {
                    if (m_verboseDebugging)
                        LOG(VB_COMMFLAG, LOG_DEBUG,
                            "      length > max comm break length, +20");
                    fbp->score += 20;
                }
                else
                {
                    if (m_verboseDebugging)
                        LOG(VB_COMMFLAG, LOG_DEBUG,
                            "      length <= max comm break length, +10");
                    fbp->score += 10;
                }
            }

            if ((m_logoInfoAvailable) &&
                (fbp->logoCount < (fbp->frames * 0.50)))
            {
                if (m_verboseDebugging)
                {
                    LOG(VB_COMMFLAG, LOG_DEBUG,
                        "      logoInfoAvailable && logoCount < frames * .50, "
                        "-10");
                }
                fbp->score -= 10;
            }

            if (fbp->ratingCount > (fbp->frames * 0.05))
            {
                if (m_verboseDebugging)
                    LOG(VB_COMMFLAG, LOG_DEBUG,
                        "      rating symbol present > 5% of time, +20");
                fbp->score += 20;
            }

            if ((fbp->scRate > 1.0) &&
                (fbp->logoCount < (fbp->frames * .90)))
            {
                if (m_verboseDebugging)
                    LOG(VB_COMMFLAG, LOG_DEBUG, "      scRate > 1.0, -10");
                fbp->score -= 10;

                if (fbp->scRate > 2.0)
                {
                    if (m_verboseDebugging)
                        LOG(VB_COMMFLAG, LOG_DEBUG, "      scRate > 2.0, -10");
                    fbp->score -= 10;
                }
            }

            if ((!m_decoderFoundAspectChanges) &&
                (fbp->formatMatch < (fbp->frames * .10)))
            {
                if (m_verboseDebugging)
                {
                    LOG(VB_COMMFLAG, LOG_DEBUG,
                        "      < 10% of frames match show letter/pillar-box "
                        "format, -20");
                }
                fbp->score -= 20;
            }

            if ((abs((int)(fbp->frames - (15 * m_fps))) < 5 ) ||
                (abs((int)(fbp->frames - (30 * m_fps))) < 6 ) ||
                (abs((int)(fbp->frames - (60 * m_fps))) < 8 ))
            {
                if (m_verboseDebugging)
                    LOG(VB_COMMFLAG, LOG_DEBUG,
                        "      block appears to be standard comm length, -10");
                fbp->score -= 10;
            }
        }
        else
        {
            if (m_verboseDebugging)
                LOG(VB_COMMFLAG, LOG_DEBUG,
                    QString("      FRAMES <= %1").arg(m_fps));

            if ((m_logoInfoAvailable) &&
                (fbp->start >= firstLogoFrame) &&
                (fbp->logoCount == 0))
            {
                if (m_verboseDebugging)
                    LOG(VB_COMMFLAG, LOG_DEBUG,
                        "      logoInfoAvailable && logoCount == 0, -10");
                fbp->score -= 10;
            }

            if ((!m_decoderFoundAspectChanges) &&
                (fbp->formatMatch < (fbp->frames * .10)))
            {
                if (m_verboseDebugging)
                {
                    LOG(VB_COMMFLAG, LOG_DEBUG,
                        "      < 10% of frames match show letter/pillar-box "
                        "format, -10");
                }
                fbp->score -= 10;
            }

            if (fbp->ratingCount > (fbp->frames * 0.25))
            {
                if (m_verboseDebugging)
                    LOG(VB_COMMFLAG, LOG_DEBUG,
                        "      rating symbol present > 25% of time, +10");
                fbp->score += 10;
            }
        }

        if ((m_decoderFoundAspectChanges) &&
            (fbp->aspectMatch < (fbp->frames * .10)))
        {
            if (m_verboseDebugging)
                LOG(VB_COMMFLAG, LOG_DEBUG,
                    "      < 10% of frames match show aspect, -20");
            fbp->score -= 20;
        }

        msg = FormatMsg("NOW", fbp);
        LOG(VB_COMMFLAG, LOG_DEBUG, msg);

//      lastScore = fbp->score;
        curBlock++;
    }

    curBlock = 0;
    lastScore = 0;

    LOG(VB_COMMFLAG, LOG_DEBUG, "============================================");
    LOG(VB_COMMFLAG, LOG_INFO, "Second Block pass");
    LOG(VB_COMMFLAG, LOG_DEBUG,
        "Block StTime StFrm  EndFrm Frames Secs    "
        "Bf  Lg Cnt RT Cnt SC Cnt SC Rt FmtMch AspMch Score");
    LOG(VB_COMMFLAG, LOG_DEBUG,
        "----- ------ ------ ------ ------ ------- "
        "--- ------ ------ ------ ----- ------ ------ -----");
    while (curBlock <= maxBlock)
    {
        fbp = &fblock[curBlock];

        msg = FormatMsg(curBlock, fbp);
        LOG(VB_COMMFLAG, LOG_DEBUG, msg);

        if ((curBlock > 0) && (curBlock < maxBlock))
        {
            int nextScore = fblock[curBlock + 1].score;

            if ((lastScore < 0) && (nextScore < 0) && (fbp->length < 35))
            {
                if (m_verboseDebugging)
                {
                    LOG(VB_COMMFLAG, LOG_DEBUG,
                        "      lastScore < 0 && nextScore < 0 "
                        "&& length < 35, setting -10");
                }
                fbp->score -= 10;
            }

            if ((fbp->bfCount > (fbp->frames * 0.95)) &&
                (fbp->frames < (2*m_fps)) &&
                (lastScore < 0 && nextScore < 0))
            {
                if (m_verboseDebugging)
                {
                    LOG(VB_COMMFLAG, LOG_DEBUG,
                        "      blanks > frames * 0.95 && frames < 2*m_fps && "
                        "lastScore < 0 && nextScore < 0, setting -10");
                }
                fbp->score -= 10;
            }

            if ((fbp->frames < (120*m_fps)) &&
                (lastScore < 0) &&
                (fbp->score > 0) &&
                (fbp->score < 20) &&
                (nextScore < 0))
            {
                if (m_verboseDebugging)
                {
                    LOG(VB_COMMFLAG, LOG_DEBUG,
                        "      frames < 120 * m_fps && (-20 < lastScore < 0) && "
                        "thisScore > 0 && nextScore < 0, setting score = -10");
                }
                fbp->score = -10;
            }

            if ((fbp->frames < (30*m_fps)) &&
                (lastScore > 0) &&
                (fbp->score < 0) &&
                (fbp->score > -20) &&
                (nextScore > 0))
            {
                if (m_verboseDebugging)
                {
                    LOG(VB_COMMFLAG, LOG_DEBUG,
                        "      frames < 30 * m_fps && (0 < lastScore < 20) && "
                        "thisScore < 0 && nextScore > 0, setting score = 10");
                }
                fbp->score = 10;
            }
        }

        if ((fbp->score == 0) && (lastScore > 30))
        {
            int offset = 1;
            while(((curBlock + offset) <= maxBlock) &&
                    (fblock[curBlock + offset].frames < (2 * m_fps)) &&
                    (fblock[curBlock + offset].score == 0))
                offset++;

            if ((curBlock + offset) <= maxBlock)
            {
                offset--;
                if (fblock[curBlock + offset + 1].score > 0)
                {
                    for (; offset >= 0; offset--)
                    {
                        fblock[curBlock + offset].score += 10;
                        if (m_verboseDebugging)
                        {
                            LOG(VB_COMMFLAG, LOG_DEBUG,
                                QString("      Setting block %1 score +10")
                                    .arg(curBlock+offset));
                        }
                    }
                }
                else if (fblock[curBlock + offset + 1].score < 0)
                {
                    for (; offset >= 0; offset--)
                    {
                        fblock[curBlock + offset].score -= 10;
                        if (m_verboseDebugging)
                        {
                            LOG(VB_COMMFLAG, LOG_DEBUG,
                                QString("      Setting block %1 score -10")
                                    .arg(curBlock+offset));
                        }
                    }
                }
            }
        }

        msg = FormatMsg("NOW", fbp);
        LOG(VB_COMMFLAG, LOG_DEBUG, msg);

        lastScore = fbp->score;
        curBlock++;
    }

    LOG(VB_COMMFLAG, LOG_DEBUG, "============================================");
    LOG(VB_COMMFLAG, LOG_INFO, "FINAL Block stats");
    LOG(VB_COMMFLAG, LOG_DEBUG,
        "Block StTime StFrm  EndFrm Frames Secs    "
        "Bf  Lg Cnt RT Cnt SC Cnt SC Rt FmtMch AspMch Score");
    LOG(VB_COMMFLAG, LOG_DEBUG,
        "----- ------ ------ ------ ------ ------- "
        "--- ------ ------ ------ ----- ------ ------ -----");
    curBlock = 0;
    lastScore = 0;
    int64_t breakStart = -1;
    while (curBlock <= maxBlock)
    {
        fbp = &fblock[curBlock];
        int thisScore = fbp->score;

        if ((breakStart >= 0) &&
            ((fbp->end - breakStart) > (m_commDetectMaxCommBreakLength * m_fps)))
        {
            if (((fbp->start - breakStart) >
                (m_commDetectMinCommBreakLength * m_fps)) ||
                (breakStart == 0))
            {
                if (m_verboseDebugging)
                {
                    LOG(VB_COMMFLAG, LOG_DEBUG,
                        QString("Closing commercial block at start of "
                                "frame block %1 with length %2, frame "
                                "block length of %3 frames would put comm "
                                "block length over max of %4 seconds.")
                            .arg(curBlock).arg(fbp->start - breakStart)
                            .arg(fbp->frames)
                            .arg(m_commDetectMaxCommBreakLength));
                }

                m_commBreakMap[breakStart] = MARK_COMM_START;
                m_commBreakMap[fbp->start] = MARK_COMM_END;
                lastStart = breakStart;
                lastEnd = fbp->start;
                breakStart = -1;
            }
            else
            {
                if (m_verboseDebugging)
                {
                    LOG(VB_COMMFLAG, LOG_DEBUG,
                        QString("Ignoring what appears to be commercial"
                                " block at frame %1 with length %2, "
                                "length of %3 frames would put comm "
                                "block length under min of %4 seconds.")
                            .arg(breakStart)
                            .arg(fbp->start - breakStart)
                            .arg(fbp->frames)
                            .arg(m_commDetectMinCommBreakLength));
                }
                breakStart = -1;
            }
        }
        if (thisScore == 0)
        {
            thisScore = lastScore;
        }
        else if (thisScore < 0)
        {
            if ((lastScore > 0) || (curBlock == 0))
            {
                if ((fbp->start - lastEnd) < (m_commDetectMinShowLength * m_fps))
                {
                    m_commBreakMap.remove(lastStart);
                    m_commBreakMap.remove(lastEnd);
                    breakStart = lastStart;

                    if (m_verboseDebugging)
                    {
                        if (breakStart)
                        {
                            LOG(VB_COMMFLAG, LOG_DEBUG,
                                QString("ReOpening commercial block at "
                                        "frame %1 because show less than "
                                        "%2 seconds")
                                    .arg(breakStart)
                                    .arg(m_commDetectMinShowLength));
                        }
                        else
                        {
                            LOG(VB_COMMFLAG, LOG_DEBUG,
                                "Opening initial commercial block "
                                "at start of recording, block 0.");
                        }
                    }
                }
                else
                {
                    breakStart = fbp->start;

                    if (m_verboseDebugging)
                    {
                        LOG(VB_COMMFLAG, LOG_DEBUG,
                            QString("Starting new commercial block at "
                                    "frame %1 from start of frame block %2")
                                .arg(fbp->start).arg(curBlock));
                    }
                }
            }
            else if (curBlock == maxBlock)
            {
                if ((fbp->end - breakStart) >
                    (m_commDetectMinCommBreakLength * m_fps))
                {
                    if (fbp->end <=
                        ((int64_t)m_framesProcessed - (int64_t)(2 * m_fps) - 2))
                    {
                        if (m_verboseDebugging)
                        {
                            LOG(VB_COMMFLAG, LOG_DEBUG,
                                QString("Closing final commercial block at "
                                        "frame %1").arg(fbp->end));
                        }

                        m_commBreakMap[breakStart] = MARK_COMM_START;
                        m_commBreakMap[fbp->end] = MARK_COMM_END;
                        lastStart = breakStart;
                        lastEnd = fbp->end;
                        breakStart = -1;
                    }
                }
                else
                {
                    if (m_verboseDebugging)
                    {
                        LOG(VB_COMMFLAG, LOG_DEBUG,
                            QString("Ignoring what appears to be commercial"
                                    " block at frame %1 with length %2, "
                                    "length of %3 frames would put comm "
                                    "block length under min of %4 seconds.")
                                .arg(breakStart)
                                .arg(fbp->start - breakStart)
                                .arg(fbp->frames)
                                .arg(m_commDetectMinCommBreakLength));
                    }
                    breakStart = -1;
                }
            }
        }
        // thisScore > 0
        else if ((lastScore < 0) &&
                 (breakStart != -1))
        {
            if (((fbp->start - breakStart) >
                (m_commDetectMinCommBreakLength * m_fps)) ||
                (breakStart == 0))
            {
                m_commBreakMap[breakStart] = MARK_COMM_START;
                m_commBreakMap[fbp->start] = MARK_COMM_END;
                lastStart = breakStart;
                lastEnd = fbp->start;

                if (m_verboseDebugging)
                {
                    LOG(VB_COMMFLAG, LOG_DEBUG,
                        QString("Closing commercial block at frame %1")
                            .arg(fbp->start));
                }
            }
            else
            {
                if (m_verboseDebugging)
                {
                    LOG(VB_COMMFLAG, LOG_DEBUG,
                        QString("Ignoring what appears to be commercial "
                                "block at frame %1 with length %2, "
                                "length of %3 frames would put comm block "
                                "length under min of %4 seconds.")
                            .arg(breakStart)
                            .arg(fbp->start - breakStart)
                            .arg(fbp->frames)
                            .arg(m_commDetectMinCommBreakLength));
                }
            }
            breakStart = -1;
        }

        msg = FormatMsg(curBlock, fbp);
        LOG(VB_COMMFLAG, LOG_DEBUG, msg);

        lastScore = thisScore;
        curBlock++;
    }

    if ((breakStart != -1) &&
        (breakStart <= ((int64_t)m_framesProcessed - (int64_t)(2 * m_fps) - 2)))
    {
        if (m_verboseDebugging)
        {
            LOG(VB_COMMFLAG, LOG_DEBUG,
                QString("Closing final commercial block started at "
                        "block %1 and going to end of program. length "
                        "is %2 frames")
                    .arg(curBlock)
                    .arg((m_framesProcessed - breakStart - 1)));
        }

        m_commBreakMap[breakStart] = MARK_COMM_START;
        // Create what is essentially an open-ended final skip region
        // by setting the end point 10 seconds past the end of the
        // recording.
        m_commBreakMap[m_framesProcessed + (10 * m_fps)] = MARK_COMM_END;
    }

    // include/exclude blanks from comm breaks
    tmpCommMap = m_commBreakMap;
    m_commBreakMap.clear();

    if (m_verboseDebugging)
        LOG(VB_COMMFLAG, LOG_DEBUG,
            "Adjusting start/end marks according to blanks.");
    for (it = tmpCommMap.begin(); it != tmpCommMap.end(); ++it)
    {
        if (*it == MARK_COMM_START)
        {
            uint64_t lastStartLower = it.key();
            uint64_t lastStartUpper = it.key();
            while ((lastStartLower > 0) &&
                   ((m_frameInfo[lastStartLower - 1].flagMask & COMM_FRAME_BLANK) != 0))
                lastStartLower--;
            while ((lastStartUpper < (m_framesProcessed - (2 * m_fps))) &&
                   ((m_frameInfo[lastStartUpper + 1].flagMask & COMM_FRAME_BLANK) != 0))
                lastStartUpper++;
            uint64_t adj = (lastStartUpper - lastStartLower) / 2;
            adj = std::min<uint64_t>(adj, MAX_BLANK_FRAMES);
            lastStart = lastStartLower + adj;

            if (m_verboseDebugging)
                LOG(VB_COMMFLAG, LOG_DEBUG, QString("Start Mark: %1 -> %2")
                        .arg(it.key()).arg(lastStart));

            m_commBreakMap[lastStart] = MARK_COMM_START;
        }
        else
        {
            uint64_t lastEndLower = it.key();
            uint64_t lastEndUpper = it.key();
            while ((lastEndUpper < (m_framesProcessed - (2 * m_fps))) &&
                   ((m_frameInfo[lastEndUpper + 1].flagMask & COMM_FRAME_BLANK) != 0))
                lastEndUpper++;
            while ((lastEndLower > 0) &&
                   ((m_frameInfo[lastEndLower - 1].flagMask & COMM_FRAME_BLANK) != 0))
                lastEndLower--;
            uint64_t adj = (lastEndUpper - lastEndLower) / 2;
            adj = std::min<uint64_t>(adj, MAX_BLANK_FRAMES);
            lastEnd = lastEndUpper - adj;

            if (m_verboseDebugging)
                LOG(VB_COMMFLAG, LOG_DEBUG, QString("End Mark  : %1 -> %2")
                        .arg(it.key()).arg(lastEnd));

            m_commBreakMap[lastEnd] = MARK_COMM_END;
        }
    }

    delete [] fblock;
}


void ClassicCommDetector::BuildBlankFrameCommList(void)
{
    LOG(VB_COMMFLAG, LOG_INFO, "CommDetect::BuildBlankFrameCommList()");

    if (m_blankFrameMap.count() == 0)
        return;

    auto *bframes = new long long[2_UZ * m_blankFrameMap.count()];
    auto *c_start = new long long[1_UZ * m_blankFrameMap.count()];
    auto *c_end   = new long long[1_UZ * m_blankFrameMap.count()];
    int frames = 0;
    int commercials = 0;

    m_blankCommMap.clear();

    for (auto it = m_blankFrameMap.begin(); it != m_blankFrameMap.end(); ++it)
        bframes[frames++] = it.key();

    // detect individual commercials from blank frames
    // commercial end is set to frame right before ending blank frame to
    //    account for instances with only a single blank frame between comms.
    for(int i = 0; i < frames; i++ )
    {
        for(int x=i+1; x < frames; x++ )
        {
            // check for various length spots since some channels don't
            // have blanks inbetween commercials just at the beginning and
            // end of breaks
            int gap_length = bframes[x] - bframes[i];
            if (((m_aggressiveDetection) &&
                ((abs((int)(gap_length - (5 * m_fps))) < 5 ) ||
                 (abs((int)(gap_length - (10 * m_fps))) < 7 ) ||
                 (abs((int)(gap_length - (15 * m_fps))) < 10 ) ||
                 (abs((int)(gap_length - (20 * m_fps))) < 11 ) ||
                 (abs((int)(gap_length - (30 * m_fps))) < 12 ) ||
                 (abs((int)(gap_length - (40 * m_fps))) < 1 ) ||
                 (abs((int)(gap_length - (45 * m_fps))) < 1 ) ||
                 (abs((int)(gap_length - (60 * m_fps))) < 15 ) ||
                 (abs((int)(gap_length - (90 * m_fps))) < 10 ) ||
                 (abs((int)(gap_length - (120 * m_fps))) < 10 ))) ||
                ((!m_aggressiveDetection) &&
                 ((abs((int)(gap_length - (5 * m_fps))) < 11 ) ||
                  (abs((int)(gap_length - (10 * m_fps))) < 13 ) ||
                  (abs((int)(gap_length - (15 * m_fps))) < 16 ) ||
                  (abs((int)(gap_length - (20 * m_fps))) < 17 ) ||
                  (abs((int)(gap_length - (30 * m_fps))) < 18 ) ||
                  (abs((int)(gap_length - (40 * m_fps))) < 3 ) ||
                  (abs((int)(gap_length - (45 * m_fps))) < 3 ) ||
                  (abs((int)(gap_length - (60 * m_fps))) < 20 ) ||
                  (abs((int)(gap_length - (90 * m_fps))) < 20 ) ||
                  (abs((int)(gap_length - (120 * m_fps))) < 20 ))))
            {
                c_start[commercials] = bframes[i];
                c_end[commercials] = bframes[x] - 1;
                commercials++;
                i = x-1;
                x = frames;
            }

            if ((!m_aggressiveDetection) &&
                ((abs((int)(gap_length - (30 * m_fps))) < (int)(m_fps * 0.85)) ||
                 (abs((int)(gap_length - (60 * m_fps))) < (int)(m_fps * 0.95)) ||
                 (abs((int)(gap_length - (90 * m_fps))) < (int)(m_fps * 1.05)) ||
                 (abs((int)(gap_length - (120 * m_fps))) < (int)(m_fps * 1.15))) &&
                ((x + 2) < frames) &&
                ((i + 2) < frames) &&
                ((bframes[i] + 1) == bframes[i+1]) &&
                ((bframes[x] + 1) == bframes[x+1]))
            {
                c_start[commercials] = bframes[i];
                c_end[commercials] = bframes[x];
                commercials++;
                i = x;
                x = frames;
            }
        }
    }

    int i = 0;

    // don't allow single commercial at head
    // of show unless followed by another
    if ((commercials > 1) &&
        (c_end[0] < (33 * m_fps)) &&
        (c_start[1] > (c_end[0] + (40 * m_fps))))
        i = 1;

    // eliminate any blank frames at end of commercials
    bool first_comm = true;
    for(; i < (commercials-1); i++)
    {
        long long r = c_start[i];
        long long adjustment = 0;

        if ((r < (30 * m_fps)) &&
            (first_comm))
            r = 1;

        m_blankCommMap[r] = MARK_COMM_START;

        r = c_end[i];
        if ( i < (commercials-1))
        {
            int x = 0;
            for(x = 0; x < (frames-1); x++)
                if (bframes[x] == r)
                    break;
            while((x < (frames-1)) &&
                    ((bframes[x] + 1 ) == bframes[x+1]) &&
                    (bframes[x+1] < c_start[i+1]))
            {
                r++;
                x++;
            }

            while((m_blankFrameMap.contains(r+1)) &&
                  (c_start[i+1] != (r+1)))
                {
                    r++;
                    adjustment++;
                }
        }
        else
        {
            while(m_blankFrameMap.contains(r+1))
            {
                r++;
                adjustment++;
            }
        }

        adjustment /= 2;
        adjustment = std::min<long long>(adjustment, MAX_BLANK_FRAMES);
        r -= adjustment;
        m_blankCommMap[r] = MARK_COMM_END;
        first_comm = false;
    }

    m_blankCommMap[c_start[i]] = MARK_COMM_START;
    m_blankCommMap[c_end[i]] = MARK_COMM_END;

    delete[] c_start;
    delete[] c_end;
    delete[] bframes;

    LOG(VB_COMMFLAG, LOG_INFO, "Blank-Frame Commercial Map" );
    for(auto it = m_blankCommMap.begin(); it != m_blankCommMap.end(); ++it)
        LOG(VB_COMMFLAG, LOG_INFO, QString("    %1:%2")
                .arg(it.key()).arg(*it));

    MergeBlankCommList();

    LOG(VB_COMMFLAG, LOG_INFO, "Merged Blank-Frame Commercial Break Map" );
    for(auto it = m_blankCommBreakMap.begin(); it != m_blankCommBreakMap.end(); ++it)
        LOG(VB_COMMFLAG, LOG_INFO, QString("    %1:%2")
                .arg(it.key()).arg(*it));
}


void ClassicCommDetector::BuildSceneChangeCommList(void)
{
    int section_start = -1;
    int seconds = (int)(m_framesProcessed / m_fps);
    int *sc_histogram = new int[seconds+1];

    m_sceneCommBreakMap.clear();

    memset(sc_histogram, 0, (seconds+1)*sizeof(int));
    for (uint64_t f = 1; f <= m_framesProcessed; f++)
    {
        if (m_sceneMap.contains(f))
            sc_histogram[(uint64_t)(f / m_fps)]++;
    }

    for(long long s = 0; s < (seconds + 1); s++)
    {
        if (sc_histogram[s] > 2)
        {
            if (section_start == -1)
            {
                auto f = (long long)(s * m_fps);
                for(int i = 0; i < m_fps; i++, f++)
                {
                    if (m_sceneMap.contains(f))
                    {
                        m_sceneCommBreakMap[f] = MARK_COMM_START;
                        i = (int)(m_fps) + 1;
                    }
                }
            }

            section_start = s;
        }

        if ((section_start >= 0) &&
            (s > (section_start + 32)))
        {
            auto f = (long long)(section_start * m_fps);
            bool found_end = false;

            for(int i = 0; i < m_fps; i++, f++)
            {
                if (m_sceneMap.contains(f))
                {
                    frm_dir_map_t::iterator dit =  m_sceneCommBreakMap.find(f);
                    if (dit != m_sceneCommBreakMap.end())
                        m_sceneCommBreakMap.erase(dit);
                    else
                        m_sceneCommBreakMap[f] = MARK_COMM_END;
                    i = (int)(m_fps) + 1;
                    found_end = true;
                }
            }
            section_start = -1;

            if (!found_end)
            {
                f = (long long)(section_start * m_fps);
                m_sceneCommBreakMap[f] = MARK_COMM_END;
            }
        }
    }
    delete[] sc_histogram;

    if (section_start >= 0)
        m_sceneCommBreakMap[m_framesProcessed] = MARK_COMM_END;

    frm_dir_map_t deleteMap;
    frm_dir_map_t::iterator it = m_sceneCommBreakMap.begin();
    frm_dir_map_t::iterator prev = it;
    if (it != m_sceneCommBreakMap.end())
    {
        ++it;
        while (it != m_sceneCommBreakMap.end())
        {
            if ((*it == MARK_COMM_END) &&
                (it.key() - prev.key()) < (30 * m_fps))
            {
                deleteMap[it.key()] = MARK_CUT_START;
                deleteMap[prev.key()] = MARK_CUT_START;
            }
            ++prev;
            if (it != m_sceneCommBreakMap.end())
                ++it;
        }

        frm_dir_map_t::iterator dit;
        for (dit = deleteMap.begin(); dit != deleteMap.end(); ++dit)
            m_sceneCommBreakMap.remove(dit.key());
    }

    LOG(VB_COMMFLAG, LOG_INFO, "Scene-Change Commercial Break Map" );
    for (it = m_sceneCommBreakMap.begin(); it != m_sceneCommBreakMap.end(); ++it)
    {
        LOG(VB_COMMFLAG, LOG_INFO, QString("    %1:%2")
            .arg(it.key()).arg(*it));
    }
}


void ClassicCommDetector::BuildLogoCommList()
{
    show_map_t showmap;
    GetLogoCommBreakMap(showmap);
    CondenseMarkMap(showmap, (int)(25 * m_fps), (int)(30 * m_fps));
    ConvertShowMapToCommMap(m_logoCommBreakMap, showmap);

    frm_dir_map_t::iterator it;
    LOG(VB_COMMFLAG, LOG_INFO, "Logo Commercial Break Map" );
    for(it = m_logoCommBreakMap.begin(); it != m_logoCommBreakMap.end(); ++it)
        LOG(VB_COMMFLAG, LOG_INFO, QString("    %1:%2")
                .arg(it.key()).arg(*it));
}

void ClassicCommDetector::MergeBlankCommList(void)
{
    frm_dir_map_t::iterator it;
    frm_dir_map_t::iterator prev;
    QMap<long long, long long> tmpMap;
    QMap<long long, long long>::Iterator tmpMap_it;
    QMap<long long, long long>::Iterator tmpMap_prev;

    m_blankCommBreakMap.clear();

    if (m_blankCommMap.isEmpty())
        return;

    for (it = m_blankCommMap.begin(); it != m_blankCommMap.end(); ++it)
        m_blankCommBreakMap[it.key()] = *it;

    if (m_blankCommBreakMap.isEmpty())
        return;

    it = m_blankCommMap.begin();
    prev = it;
    ++it;
    for(; it != m_blankCommMap.end(); ++it, ++prev)
    {
        // if next commercial starts less than 15*fps frames away then merge
        if ((((prev.key() + 1) == it.key()) ||
            ((prev.key() + (15 * m_fps)) > it.key())) &&
            (*prev == MARK_COMM_END) &&
            (*it == MARK_COMM_START))
        {
            m_blankCommBreakMap.remove(prev.key());
            m_blankCommBreakMap.remove(it.key());
        }
    }


    // make temp copy of commercial break list
    it = m_blankCommBreakMap.begin();
    prev = it;
    ++it;
    tmpMap[prev.key()] = it.key();
    for(; it != m_blankCommBreakMap.end(); ++it, ++prev)
    {
        if ((*prev == MARK_COMM_START) &&
            (*it == MARK_COMM_END))
            tmpMap[prev.key()] = it.key();
    }

    tmpMap_it = tmpMap.begin();
    tmpMap_prev = tmpMap_it;
    tmpMap_it++;
    for(; tmpMap_it != tmpMap.end(); ++tmpMap_it, ++tmpMap_prev)
    {
        // if we find any segments less than 35 seconds between commercial
        // breaks include those segments in the commercial break.
        if (((*tmpMap_prev + (35 * m_fps)) > tmpMap_it.key()) &&
            ((*tmpMap_prev - tmpMap_prev.key()) > (35 * m_fps)) &&
            ((*tmpMap_it - tmpMap_it.key()) > (35 * m_fps)))
        {
            m_blankCommBreakMap.remove(*tmpMap_prev);
            m_blankCommBreakMap.remove(tmpMap_it.key());
        }
    }
}

bool ClassicCommDetector::FrameIsInBreakMap(
    uint64_t f, const frm_dir_map_t &breakMap) const
{
    for (uint64_t i = f; i < m_framesProcessed; i++)
    {
        if (breakMap.contains(i))
        {
            int type = breakMap[i];
            if ((type == MARK_COMM_END) || (i == f))
                return true;
            if (type == MARK_COMM_START)
                return false;
        }
    }

    // We want from f down to 0, but without wrapping the counter to negative
    // on an unsigned counter.
    for (uint64_t i = (f + 1); i-- > 0; )
    {
        if (breakMap.contains(i))
        {
            int type = breakMap[i];
            if ((type == MARK_COMM_START) || (i == f))
                return true;
            if (type == MARK_COMM_END)
                return false;
        }
    }

    return false;
}

void ClassicCommDetector::DumpMap(frm_dir_map_t &map) const
{
    frm_dir_map_t::iterator it;
    QString msg;

    LOG(VB_COMMFLAG, LOG_INFO,
        "---------------------------------------------------");
    for (it = map.begin(); it != map.end(); ++it)
    {
        long long frame = it.key();
        int flag = *it;
        int my_fps = (int)ceil(m_fps);
        long long hour = (frame / my_fps) / 60 / 60;
        long long min = ((frame / my_fps) / 60) - (hour * 60);
        long long sec = (frame / my_fps) - (min * 60) - (hour * 60 * 60);
        long long frm = frame - ((sec * my_fps) + (min * 60 * my_fps) +
                           (hour * 60 * 60 * my_fps));
        int my_sec = (int)(frame / my_fps);
        msg = QString("%1 : %2 (%3:%4:%5.%6) (%7)")
            .arg(frame, 7).arg(flag).arg(hour, 2, 10, QChar('0')).arg(min, 2, 10, QChar('0'))
            .arg(sec, 2, 10, QChar('0')).arg(frm, 2, 10, QChar('0')).arg(my_sec);
        LOG(VB_COMMFLAG, LOG_INFO, msg);
    }
    LOG(VB_COMMFLAG, LOG_INFO,
        "---------------------------------------------------");
}

void ClassicCommDetector::CondenseMarkMap(show_map_t &map, int spacing,
                                          int length)
{
    show_map_t::iterator it;
    show_map_t::iterator prev;
    show_map_t tmpMap;

    if (map.size() <= 2)
        return;

    // merge any segments less than 'spacing' frames apart from each other
    LOG(VB_COMMFLAG, LOG_INFO, "Commercial Map Before condense:" );
    for (it = map.begin(); it != map.end(); ++it)
    {
        LOG(VB_COMMFLAG, LOG_INFO, QString("    %1:%2")
                .arg(it.key()).arg(*it));
        tmpMap[it.key()] = *it;
    }

    prev = tmpMap.begin();
    it = prev;
    ++it;
    while (it != tmpMap.end())
    {
        if ((*it == MARK_START) &&
            (*prev == MARK_END) &&
            ((it.key() - prev.key()) < (uint64_t)spacing))
        {
            map.remove(prev.key());
            map.remove(it.key());
        }
        ++prev;
        ++it;
    }

    if (map.empty())
        return;

    // delete any segments less than 'length' frames in length
    tmpMap.clear();
    for (it = map.begin(); it != map.end(); ++it)
        tmpMap[it.key()] = *it;

    prev = tmpMap.begin();
    it = prev;
    ++it;
    while (it != tmpMap.end())
    {
        if ((*prev == MARK_START) &&
            (*it == MARK_END) &&
            ((it.key() - prev.key()) < (uint64_t)length))
        {
            map.remove(prev.key());
            map.remove(it.key());
        }
        ++prev;
        ++it;
    }

    LOG(VB_COMMFLAG, LOG_INFO, "Commercial Map After condense:" );
    for (it = map.begin(); it != map.end(); ++it)
        LOG(VB_COMMFLAG, LOG_INFO, QString("    %1:%2")
                .arg(it.key()).arg(*it));
}

void ClassicCommDetector::ConvertShowMapToCommMap(
    frm_dir_map_t &out, const show_map_t &in)
{
    out.clear();
    if (in.empty())
        return;

    show_map_t::const_iterator sit;
    for (sit = in.begin(); sit != in.end(); ++sit)
    {
        if (*sit == MARK_START)
            out[sit.key()] = MARK_COMM_END;
        else
            out[sit.key()] = MARK_COMM_START;
    }

    frm_dir_map_t::iterator it = out.begin();
    if (it == out.end())
        return;

    switch (out[it.key()])
    {
        case MARK_COMM_END:
            if (it.key() == 0)
                out.remove(0);
            else
                out[0] = MARK_COMM_START;
            break;
        case MARK_COMM_START:
            break;
        default:
            out.remove(0);
            break;
    }
}


/* ideas for this method ported back from comskip.c mods by Jere Jones
 * which are partially mods based on Myth's original commercial skip
 * code written by Chris Pinkham. */

void ClassicCommDetector::CleanupFrameInfo(void)
{
    LOG(VB_COMMFLAG, LOG_INFO, "CommDetect::CleanupFrameInfo()");

    // try to account for noisy signal causing blank frames to be undetected
    if ((m_framesProcessed > (m_fps * 60)) &&
        (m_blankFrameCount < (m_framesProcessed * 0.0004)))
    {
        std::array<int,256> avgHistogram {};
        int minAvg = -1;
        int newThreshold = -1;

        LOG(VB_COMMFLAG, LOG_INFO,
            QString("ClassicCommDetect: Only found %1 blank frames but "
                    "wanted at least %2, rechecking data using higher "
                    "threshold.")
                .arg(m_blankFrameCount)
                .arg((int)(m_framesProcessed * 0.0004)));
        m_blankFrameMap.clear();
        m_blankFrameCount = 0;

        avgHistogram.fill(0);

        for (uint64_t i = 1; i <= m_framesProcessed; i++)
            avgHistogram[std::clamp(m_frameInfo[i].avgBrightness, 0, 255)] += 1;

        for (int i = 1; i <= 255 && minAvg == -1; i++)
            if (avgHistogram[i] > (m_framesProcessed * 0.0004))
                minAvg = i;

        newThreshold = minAvg + 3;
        LOG(VB_COMMFLAG, LOG_INFO,
            QString("Minimum Average Brightness on a frame "
                    "was %1, will use %2 as new threshold")
                .arg(minAvg).arg(newThreshold));

        for (uint64_t i = 1; i <= m_framesProcessed; i++)
        {
            int value = m_frameInfo[i].flagMask;
            m_frameInfo[i].flagMask = value & ~COMM_FRAME_BLANK;

            if (( (m_frameInfo[i].flagMask & COMM_FRAME_BLANK) == 0) &&
                (m_frameInfo[i].avgBrightness < newThreshold))
            {
                m_frameInfo[i].flagMask = value | COMM_FRAME_BLANK;
                m_blankFrameMap[i] = MARK_BLANK_FRAME;
                m_blankFrameCount++;
            }
        }

        LOG(VB_COMMFLAG, LOG_INFO,
            QString("Found %1 blank frames using new value")
                .arg(m_blankFrameCount));
    }

    // try to account for fuzzy logo detection
    for (uint64_t i = 1; i <= m_framesProcessed; i++)
    {
        if ((i < 10) || ((i+10) > m_framesProcessed))
            continue;

        int before = 0;
        for (int offset = 1; offset <= 10; offset++)
            if ((m_frameInfo[i - offset].flagMask & COMM_FRAME_LOGO_PRESENT) != 0)
                before++;

        int after = 0;
        for (int offset = 1; offset <= 10; offset++)
            if ((m_frameInfo[i + offset].flagMask & COMM_FRAME_LOGO_PRESENT) != 0)
                after++;

        int value = m_frameInfo[i].flagMask;
        if (value == -1)
            m_frameInfo[i].flagMask = 0;

        if (value & COMM_FRAME_LOGO_PRESENT)
        {
            if ((before < 4) && (after < 4))
                m_frameInfo[i].flagMask = value & ~COMM_FRAME_LOGO_PRESENT;
        }
        else
        {
            if ((before > 6) && (after > 6))
                m_frameInfo[i].flagMask = value | COMM_FRAME_LOGO_PRESENT;
        }
    }
}

void ClassicCommDetector::GetLogoCommBreakMap(show_map_t &map)
{
    LOG(VB_COMMFLAG, LOG_INFO, "CommDetect::GetLogoCommBreakMap()");

    map.clear();

    bool PrevFrameLogo = false;

    for (uint64_t curFrame = 1 ; curFrame <= m_framesProcessed; curFrame++)
    {
        bool CurrentFrameLogo =
            (m_frameInfo[curFrame].flagMask & COMM_FRAME_LOGO_PRESENT) != 0;

        if (!PrevFrameLogo && CurrentFrameLogo)
            map[curFrame] = MARK_START;
        else if (PrevFrameLogo && !CurrentFrameLogo)
            map[curFrame] = MARK_END;

        PrevFrameLogo = CurrentFrameLogo;
    }
}

void ClassicCommDetector::logoDetectorBreathe()
{
    emit breathe();
}

void ClassicCommDetector::PrintFullMap(
    std::ostream &out, const frm_dir_map_t *comm_breaks, bool verbose) const
{
    if (verbose)
    {
        QByteArray tmp = FrameInfoEntry::GetHeader().toLatin1();
        out << tmp.constData() << " mark" << std::endl;
    }

    for (long long i = 1; i < m_curFrameNumber; i++)
    {
        QMap<long long, FrameInfoEntry>::const_iterator it = m_frameInfo.find(i);
        if (it == m_frameInfo.end())
            continue;

        QByteArray atmp = (*it).toString(i, verbose).toLatin1();
        out << atmp.constData() << " ";
        if (comm_breaks)
        {
            frm_dir_map_t::const_iterator mit = comm_breaks->find(i);
            if (mit != comm_breaks->end())
            {
                QString tmp = (verbose) ?
                    toString((MarkTypes)*mit) : QString::number(*mit);
                atmp = tmp.toLatin1();

                out << atmp.constData();
            }
        }
        out << "\n";
    }

    out << std::flush;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
