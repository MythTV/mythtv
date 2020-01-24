// ANSI C headers
#include <cmath>
#include <cerrno>
#include <chrono> // for milliseconds
#include <thread> // for sleep_for

// C++ headers
#include <algorithm>
using namespace std;

// Qt headers
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

// MythTV headers
#include "compat.h"
#include "mythdb.h"
#include "mythlogging.h"
#include "mythplayer.h"
#include "programinfo.h"
#include "channelutil.h"

// Commercial Flagging headers
#include "CommDetector2.h"
#include "CannyEdgeDetector.h"
#include "FrameAnalyzer.h"
#include "PGMConverter.h"
#include "BorderDetector.h"
#include "HistogramAnalyzer.h"
#include "BlankFrameDetector.h"
#include "SceneChangeDetector.h"
#include "TemplateFinder.h"
#include "TemplateMatcher.h"

namespace {

bool stopForBreath(bool isrecording, long long frameno)
{
    return (isrecording && (frameno % 100) == 0) || (frameno % 500) == 0;
}

bool needToReportState(bool showprogress, bool isrecording, long long frameno)
{
    return ((showprogress || isrecording) && (frameno % 100) == 0) ||
        (frameno % 500) == 0;
}

void waitForBuffer(const struct timeval *framestart, int minlag, int flaglag,
        float fps, bool fullspeed)
{
    long usperframe = (long)(1000000.0F / fps);
    struct timeval now {};
    struct timeval elapsed {};

    (void)gettimeofday(&now, nullptr);
    timersub(&now, framestart, &elapsed);

    // Sleep for one frame's worth of time.
    long sleepus = usperframe - elapsed.tv_sec * 1000000 - elapsed.tv_usec;
    if (sleepus <= 0)
        return;

    if (flaglag > minlag)
    {
        // Try to catch up; reduce sleep time.
        if (fullspeed)
            return;
        sleepus /= 4;
    }
    else if (flaglag < minlag)
    {
        // Slow down; increase sleep time
        sleepus = sleepus * 3 / 2;
    }
    std::this_thread::sleep_for(std::chrono::microseconds(sleepus));
}

bool MythPlayerInited(FrameAnalyzerItem &pass,
                      FrameAnalyzerItem &finishedAnalyzers,
                      FrameAnalyzerItem &deadAnalyzers,
                      MythPlayer *player,
                      long long nframes)
{
    auto it = pass.begin();
    while (it != pass.end())
    {
        FrameAnalyzer::analyzeFrameResult ares =
            (*it)->MythPlayerInited(player, nframes);

        if ((FrameAnalyzer::ANALYZE_OK    == ares) ||
            (FrameAnalyzer::ANALYZE_ERROR == ares))
        {
            ++it;
        }
        else if (FrameAnalyzer::ANALYZE_FINISHED == ares)
        {
            finishedAnalyzers.push_back(*it);
            it = pass.erase(it);
        }
        else if (FrameAnalyzer::ANALYZE_FATAL == ares)
        {
            deadAnalyzers.push_back(*it);
            it = pass.erase(it);
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("Unexpected return value from %1::MythPlayerInited: %2")
                .arg((*it)->name()).arg(ares));
            return false;
        }
    }

    return true;
}

long long processFrame(FrameAnalyzerItem &pass,
                       FrameAnalyzerItem &finishedAnalyzers,
                       FrameAnalyzerItem &deadAnalyzers,
                       const VideoFrame *frame,
                       long long frameno)
{
    long long nextFrame = 0;
    long long minNextFrame = FrameAnalyzer::kAnyFrame;

    auto it = pass.begin();
    while (it != pass.end())
    {
        FrameAnalyzer::analyzeFrameResult ares =
            (*it)->analyzeFrame(frame, frameno, &nextFrame);

        if ((FrameAnalyzer::ANALYZE_OK == ares) ||
            (FrameAnalyzer::ANALYZE_ERROR == ares))
        {
            minNextFrame = std::min(minNextFrame, nextFrame);
            ++it;
        }
        else if (ares == FrameAnalyzer::ANALYZE_FINISHED)
        {
            finishedAnalyzers.push_back(*it);
            it = pass.erase(it);
        }
        else
        {
            if (ares != FrameAnalyzer::ANALYZE_FATAL)
            {
                LOG(VB_GENERAL, LOG_ERR,
                    QString("Unexpected return value from %1::analyzeFrame: %2")
                    .arg((*it)->name()).arg(ares));
            }

            deadAnalyzers.push_back(*it);
            it = pass.erase(it);
        }
    }

    if (minNextFrame == FrameAnalyzer::kAnyFrame)
        minNextFrame = FrameAnalyzer::kNextFrame;

    if (minNextFrame == FrameAnalyzer::kNextFrame)
        minNextFrame = frameno + 1;

    return minNextFrame;
}

int passFinished(FrameAnalyzerItem &pass, long long nframes, bool final)
{
    for (auto & pas : pass)
        (void)pas->finished(nframes, final);

    return 0;
}

int passReportTime(const FrameAnalyzerItem &pass)
{
    for (auto *pas : pass)
        (void)pas->reportTime();

    return 0;
}

bool searchingForLogo(TemplateFinder *tf, const FrameAnalyzerItem &pass)
{
    if (!tf)
        return false;

    auto it = std::find(pass.cbegin(), pass.cend(), tf);
    return it != pass.end();
}

};  // namespace

namespace commDetector2 {

QString debugDirectory(int chanid, const QDateTime& recstartts)
{
    /*
     * Should deleting a recording also delete its associated debug directory?
     */
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT basename"
            " FROM recorded"
            " WHERE chanid = :CHANID"
            "   AND starttime = :STARTTIME"
            ";");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", recstartts);
    if (!query.exec() || !query.next())
    {
        MythDB::DBError("Error in CommDetector2::CommDetector2", query);
        return "";
    }

    ProgramInfo pginfo(chanid, recstartts);

    if (!pginfo.GetChanID())
        return "";

    QString pburl = pginfo.GetPlaybackURL(true);
    if (!pburl.startsWith("/"))
        return "";

    QString basename(query.value(0).toString());
    QString m_debugdir = pburl.section('/',0,-2) + "/" + basename + "-debug";

    return m_debugdir;
}

void createDebugDirectory(const QString& dirname, const QString& comment)
{
    QDir qdir(dirname);
    if (qdir.exists())
    {
        LOG(VB_COMMFLAG, LOG_INFO, QString("%1 using debug directory \"%2\"")
                .arg(comment).arg(dirname));
    }
    else
    {
        if (qdir.mkdir(dirname))
        {
            LOG(VB_COMMFLAG, LOG_INFO,
                QString("%1 created debug directory \"%1\"")
                    .arg(comment).arg(dirname));
        }
        else
        {
            LOG(VB_COMMFLAG, LOG_INFO, QString("%1 failed to create \"%2\": %3")
                    .arg(comment).arg(dirname).arg(strerror(errno)));
        }
    }
}

QString frameToTimestamp(long long frameno, float fps)
{
    int ms = (int)roundf(frameno / fps * 1000);

    int ss = ms / 1000;
    ms %= 1000;
    if (ms >= 500)
        ss++;

    int mm = ss / 60;
    ss %= 60;

    int hh = mm / 60;
    mm %= 60;

    return QString("%1:%2:%3")
        .arg(hh).arg(mm, 2, 10, QChar('0')) .arg(ss, 2, 10, QChar('0'));
}

QString frameToTimestampms(long long frameno, float fps)
{
    int ms = (int)roundf(frameno / fps * 1000);

    int ss = ms / 1000;
    ms %= 1000;

    int mm = ss / 60;
    ss %= 60;

    return QString("%1:%2:%3")
        .arg(mm).arg(ss, 2, 10, QChar(QChar('0'))).arg(ms, 2, 10, QChar(QChar('0')));
}

QString strftimeval(const struct timeval *tv)
{
    return QString("%1.%2")
        .arg(tv->tv_sec).arg(tv->tv_usec, 6, 10, QChar(QChar('0')));
}

};  /* namespace */

using namespace commDetector2;

CommDetector2::CommDetector2(
    SkipType           commDetectMethod_in,
    bool               showProgress_in,
    bool               fullSpeed_in,
    MythPlayer        *player_in,
    int                chanid,
    QDateTime          startts_in,
    QDateTime          endts_in,
    QDateTime          recstartts_in,
    QDateTime          recendts_in,
    bool               useDB) :
    m_commDetectMethod((SkipType)(commDetectMethod_in & ~COMM_DETECT_2)),
    m_showProgress(showProgress_in),  m_fullSpeed(fullSpeed_in),
    m_player(player_in),
    m_startts(std::move(startts_in)),       m_endts(std::move(endts_in)),
    m_recstartts(std::move(recstartts_in)), m_recendts(std::move(recendts_in)),
    m_isRecording(MythDate::current() < m_recendts),
    m_debugdir("")
{
    FrameAnalyzerItem        pass0;
    FrameAnalyzerItem        pass1;
    PGMConverter            *pgmConverter = nullptr;
    BorderDetector          *borderDetector = nullptr;
    HistogramAnalyzer       *histogramAnalyzer = nullptr;

    if (useDB)
        m_debugdir = debugDirectory(chanid, m_recstartts);

    /*
     * Look for blank frames to use as delimiters between commercial and
     * non-commercial segments.
     */
    if ((m_commDetectMethod & COMM_DETECT_2_BLANK))
    {
        if (!pgmConverter)
            pgmConverter = new PGMConverter();

        if (!borderDetector)
            borderDetector = new BorderDetector();

        if (!histogramAnalyzer)
        {
            histogramAnalyzer = new HistogramAnalyzer(pgmConverter,
                    borderDetector, m_debugdir);
        }

        if (!m_blankFrameDetector)
        {
            m_blankFrameDetector = new BlankFrameDetector(histogramAnalyzer,
                    m_debugdir);
            pass1.push_back(m_blankFrameDetector);
        }
    }

    /*
     * Look for "scene changes" to use as delimiters between commercial and
     * non-commercial segments.
     */
    if ((m_commDetectMethod & COMM_DETECT_2_SCENE))
    {
        if (!pgmConverter)
            pgmConverter = new PGMConverter();

        if (!borderDetector)
            borderDetector = new BorderDetector();

        if (!histogramAnalyzer)
        {
            histogramAnalyzer = new HistogramAnalyzer(pgmConverter,
                    borderDetector, m_debugdir);
        }

        if (!m_sceneChangeDetector)
        {
            m_sceneChangeDetector = new SceneChangeDetector(histogramAnalyzer,
                    m_debugdir);
            pass1.push_back(m_sceneChangeDetector);
        }
    }

    /*
     * Logo Detection requires two passes. The first pass looks for static
     * areas of the screen to look for logos and generate a template. The
     * second pass matches the template against the frame and computes the
     * closeness of the match.
     */
    if ((m_commDetectMethod & COMM_DETECT_2_LOGO))
    {
        if (!pgmConverter)
            pgmConverter = new PGMConverter();

        if (!borderDetector)
            borderDetector = new BorderDetector();

        auto *cannyEdgeDetector = new CannyEdgeDetector();

        if (!m_logoFinder)
        {
            m_logoFinder = new TemplateFinder(pgmConverter, borderDetector,
                    cannyEdgeDetector, m_player, m_recstartts.secsTo(m_recendts),
                    m_debugdir);
            pass0.push_back(m_logoFinder);
        }

        if (!m_logoMatcher)
        {
            m_logoMatcher = new TemplateMatcher(pgmConverter, cannyEdgeDetector,
                    m_logoFinder, m_debugdir);
            pass1.push_back(m_logoMatcher);
        }
    }

    if (histogramAnalyzer && m_logoFinder)
        histogramAnalyzer->setLogoState(m_logoFinder);

    /* Aggregate them all together. */
    m_frameAnalyzers.push_back(pass0);
    m_frameAnalyzers.push_back(pass1);
}

void CommDetector2::reportState(int elapsedms, long long frameno,
        long long nframes, unsigned int passno, unsigned int npasses)
{
    float fps = elapsedms ? (float)frameno * 1000 / elapsedms : 0;
    static int s_prevpercent = -1;

    /* Assume that 0-th pass is negligible in terms of computational cost. */
    int percentage = (passno == 0 || npasses == 1 || nframes == 0) ? 0 :
        (passno - 1) * 100 / (npasses - 1) +
        min((long long)100, (frameno * 100 / nframes) / (npasses - 1));

    if (m_showProgress)
    {
        QString tmp = "";

        if (nframes)
            tmp = QString("\r%1%/ %2fps").arg(percentage,3).arg(fps,6,'f',2);
        else
            tmp = QString("\r%1/ %2fps").arg(frameno,6).arg(fps,6,'f',2);

        QByteArray tmp2 = tmp.toLocal8Bit();
        cerr << tmp2.constData() << "          \r";
        cerr.flush();
    }

    if (nframes)
    {
        emit statusUpdate(QCoreApplication::translate("(mythcommflag)",
            "%1% Completed @ %2 fps.").arg(percentage).arg(fps));
    }
    else
    {
        emit statusUpdate(QCoreApplication::translate("(mythcommflag)",
            "%1 Frames Completed @ %2 fps.").arg(frameno).arg(fps));
    }

    if (percentage % 10 == 0 && s_prevpercent != percentage)
    {
        s_prevpercent = percentage;
        LOG(VB_GENERAL, LOG_INFO, QString("%1%% Completed @ %2 fps.")
            .arg(percentage) .arg(fps));
    }
}

int CommDetector2::computeBreaks(long long nframes)
{
    int trow = 0;
    int tcol = 0;
    int twidth = 0;
    int theight = 0;

    m_breaks.clear();

    TemplateMatcher *matcher = nullptr;
    if (m_logoFinder && m_logoFinder->getTemplate(&trow, &tcol, &twidth, &theight))
        matcher = m_logoMatcher;

    if (matcher && m_blankFrameDetector)
    {
        int cmp = matcher->templateCoverage(nframes, m_finished);
        if (cmp == 0)
        {
            if (matcher->adjustForBlanks(m_blankFrameDetector, nframes))
                return -1;
            if (matcher->computeBreaks(&m_breaks))
                return -1;
        }
        else
        {
            if (cmp > 0 &&
                    m_blankFrameDetector->computeForLogoSurplus(matcher))
                return -1;
            if (cmp < 0 &&
                    BlankFrameDetector::computeForLogoDeficit(matcher))
                return -1;

            if (m_blankFrameDetector->computeBreaks(&m_breaks))
                return -1;
        }
    }
    else if (matcher)
    {
        if (matcher->computeBreaks(&m_breaks))
            return -1;
    }
    else if (m_blankFrameDetector)
    {
        if (m_blankFrameDetector->computeBreaks(&m_breaks))
            return -1;
    }

    return 0;
}

bool CommDetector2::go(void)
{
    int minlag = 7; // seconds

    if (m_player->OpenFile() < 0)
        return false;

    if (!m_player->InitVideo())
    {
        LOG(VB_GENERAL, LOG_ERR,
            "NVP: Unable to initialize video for FlagCommercials.");
        return false;
    }

    m_player->EnableSubtitles(false);

    QElapsedTimer totalFlagTime;
    totalFlagTime.start();

    /* If still recording, estimate the eventual total number of frames. */
    long long nframes = m_isRecording ?
        (long long)roundf((m_recstartts.secsTo(m_recendts) + 5) *
                          m_player->GetFrameRate()) :
            m_player->GetTotalFrameCount();
    bool postprocessing = !m_isRecording;

    if (m_showProgress)
    {
        if (nframes)
            cerr << "  0%/      ";
        else
            cerr << "     0/      ";
        cerr.flush();
    }

    frm_dir_map_t lastBreakMap;
    unsigned int passno = 0;
    unsigned int npasses = m_frameAnalyzers.size();
    for (m_currentPass = m_frameAnalyzers.begin();
            m_currentPass != m_frameAnalyzers.end();
            ++m_currentPass, passno++)
    {
        FrameAnalyzerItem deadAnalyzers;

        LOG(VB_COMMFLAG, LOG_INFO,
            QString("CommDetector2::go pass %1 of %2 (%3 frames, %4 fps)")
                .arg(passno + 1).arg(npasses)
                .arg(m_player->GetTotalFrameCount())
                .arg(m_player->GetFrameRate(), 0, 'f', 2));

        if (!MythPlayerInited(
                *m_currentPass, m_finishedAnalyzers, deadAnalyzers, m_player, nframes))
        {
            return false;
        }

        m_player->DiscardVideoFrame(m_player->GetRawVideoFrame(0));
        long long nextFrame = -1;
        m_currentFrameNumber = 0;
        long long lastLoggedFrame = m_currentFrameNumber;
        QElapsedTimer passTime;
        QElapsedTimer clock;
        struct timeval getframetime {};

        m_player->ResetTotalDuration();

        if (searchingForLogo(m_logoFinder, *m_currentPass))
            emit statusUpdate(QCoreApplication::translate("(mythcommflag)",
                "Performing Logo Identification"));

        clock.start();
        passTime.start();
        memset(&getframetime, 0, sizeof(getframetime));
        while (!(*m_currentPass).empty() && m_player->GetEof() == kEofStateNone)
        {
            struct timeval start {};
            struct timeval end {};
            struct timeval elapsedtv {};

            (void)gettimeofday(&start, nullptr);
            bool fetchNext = (nextFrame == m_currentFrameNumber + 1);
            VideoFrame *currentFrame =
                m_player->GetRawVideoFrame(fetchNext ? -1 : nextFrame);
            long long lastFrameNumber = m_currentFrameNumber;
            m_currentFrameNumber = currentFrame->frameNumber + 1;
            (void)gettimeofday(&end, nullptr);
            timersub(&end, &start, &elapsedtv);
            timeradd(&getframetime, &elapsedtv, &getframetime);

            if (nextFrame != -1 && nextFrame == lastFrameNumber + 1 &&
                    m_currentFrameNumber != nextFrame)
            {
                /*
                 * Don't log "Jumped" when we know we're skipping frames (e.g.,
                 * logo detection).
                 */
                LOG(VB_COMMFLAG, LOG_INFO,
                    QString("Jumped from frame %1 to frame %2")
                        .arg(lastFrameNumber).arg(m_currentFrameNumber));
            }

            if (stopForBreath(m_isRecording, m_currentFrameNumber))
            {
                emit breathe();
                if (m_bStop)
                {
                    m_player->DiscardVideoFrame(currentFrame);
                    return false;
                }
            }

            while (m_bPaused)
            {
                emit breathe();
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }

            if (!searchingForLogo(m_logoFinder, *m_currentPass) &&
                    needToReportState(m_showProgress, m_isRecording,
                        m_currentFrameNumber))
            {
                reportState(passTime.elapsed(), m_currentFrameNumber,
                        nframes, passno, npasses);
            }

            nextFrame = processFrame(
                *m_currentPass, m_finishedAnalyzers,
                deadAnalyzers, currentFrame, m_currentFrameNumber);

            if (((m_currentFrameNumber >= 1) && (nframes > 0) &&
                 (((nextFrame * 10) / nframes) !=
                  ((m_currentFrameNumber * 10) / nframes))) ||
                (nextFrame >= nframes))
            {
                /* Log something every 10%. */
                int elapsed = clock.restart();
                LOG(VB_COMMFLAG, LOG_INFO,
                    QString("processFrame %1 of %2 (%3%) - %4 fps")
                        .arg(m_currentFrameNumber)
                        .arg(nframes)
                        .arg((nframes == 0) ? 0 : 
                            (int)roundf(m_currentFrameNumber * 100.0 / nframes))
                        .arg((m_currentFrameNumber - lastLoggedFrame) * 1000 /
                            (elapsed ? elapsed : 1)));
                lastLoggedFrame = m_currentFrameNumber;
            }

            if (m_isRecording)
            {
                waitForBuffer(&start, minlag,
                        m_recstartts.secsTo(MythDate::current()) -
                        totalFlagTime.elapsed() / 1000, m_player->GetFrameRate(),
                        m_fullSpeed);
            }

            // sleep a little so we don't use all cpu even if we're niced
            if (!m_fullSpeed && !m_isRecording)
                std::this_thread::sleep_for(std::chrono::milliseconds(10));

            if (m_sendBreakMapUpdates && (m_breakMapUpdateRequested ||
                        !(m_currentFrameNumber % 500)))
            {
                frm_dir_map_t breakMap;

                GetCommercialBreakList(breakMap);

                auto ii = breakMap.cbegin();
                auto jj = lastBreakMap.cbegin();
                while (ii != breakMap.cend() && jj != lastBreakMap.cend())
                {
                    if (ii.key() != jj.key())
                        break;
                    if (*ii != *jj)
                        break;
                    ++ii;
                    ++jj;
                }
                bool same = ii == breakMap.end() && jj == lastBreakMap.end();
                lastBreakMap = breakMap;

                if (m_breakMapUpdateRequested || !same)
                    emit gotNewCommercialBreakList();

                m_breakMapUpdateRequested = false;
            }

            m_player->DiscardVideoFrame(currentFrame);
        }

        // Save total duration only on the last pass, which hopefully does
        // no skipping.
        if (passno + 1 == npasses)
            m_player->SaveTotalDuration();

        m_currentPass->insert(m_currentPass->end(),
                            m_finishedAnalyzers.begin(),
                            m_finishedAnalyzers.end());
        m_finishedAnalyzers.clear();

        if (postprocessing)
            m_currentFrameNumber = m_player->GetTotalFrameCount() - 1;
        if (passFinished(*m_currentPass, m_currentFrameNumber + 1, true))
            return false;

        LOG(VB_COMMFLAG, LOG_INFO, QString("NVP Time: GetRawVideoFrame=%1s")
                .arg(strftimeval(&getframetime)));
        if (passReportTime(*m_currentPass))
            return false;
    }

    if (m_showProgress)
    {
        if (nframes)
            cerr << "\b\b\b\b\b\b      \b\b\b\b\b\b";
        else
            cerr << "\b\b\b\b\b\b\b\b\b\b\b\b\b             "
                    "\b\b\b\b\b\b\b\b\b\b\b\b\b";
        cerr.flush();
    }

    m_finished = true;
    return true;
}

void CommDetector2::GetCommercialBreakList(frm_dir_map_t &marks)
{
    if (!m_finished)
    {
        for (auto & analyzer : m_frameAnalyzers)
        {
            if (analyzer == *m_currentPass &&
                passFinished(m_finishedAnalyzers, m_currentFrameNumber + 1, false))
            {
                return;
            }

            if (passFinished(analyzer, m_currentFrameNumber + 1, false))
                return;
        }
    }

    if (computeBreaks(m_currentFrameNumber + 1))
        return;

    marks.clear();

    /* Create break list. */
    long long breakframes = 0;
    for (auto bb = m_breaks.begin(); bb != m_breaks.end(); ++bb)
    {
        long long segb = bb.key();
        long long seglen = *bb;
        long long sege = segb + seglen - 1;

        if (segb < sege)
        {
            marks[segb] = MARK_COMM_START;
            marks[sege] = MARK_COMM_END;

            breakframes += seglen;
        }
    }

    /* Report results. */
    const float fps = m_player->GetFrameRate();
    for (frm_dir_map_t::const_iterator iimark = marks.begin();
            iimark != marks.end();
            ++iimark)
    {
        /* Display as 1-based frame numbers. */
        long long markstart = iimark.key() + 1;   /* MARK_COMM_BEGIN */
        ++iimark;                       /* MARK_COMM_END */
        if (iimark == marks.end())
            break;
        long long markend = iimark.key() + 1;

        LOG(VB_COMMFLAG, LOG_INFO, QString("Break: frame %1-%2 (%3-%4, %5)")
                .arg(markstart, 6).arg(markend, 6)
                .arg(frameToTimestamp(markstart, fps))
                .arg(frameToTimestamp(markend, fps))
                .arg(frameToTimestamp(markend - markstart + 1, fps)));
    }

    const long long nframes = m_player->GetTotalFrameCount();
    LOG(VB_COMMFLAG, LOG_INFO,
        QString("Flagged %1 of %2 frames (%3 of %4), %5% commercials (%6)")
            .arg(m_currentFrameNumber + 1).arg(nframes)
            .arg(frameToTimestamp(m_currentFrameNumber + 1, fps))
            .arg(frameToTimestamp(nframes, fps))
            .arg(breakframes * 100 / m_currentFrameNumber)
            .arg(frameToTimestamp(breakframes, fps)));
}

void CommDetector2::recordingFinished(long long totalFileSize)
{
    CommDetectorBase::recordingFinished(totalFileSize);
    m_isRecording = false;
    LOG(VB_COMMFLAG, LOG_INFO,
        QString("CommDetector2::recordingFinished: %1 bytes")
            .arg(totalFileSize));
}

void CommDetector2::requestCommBreakMapUpdate(void)
{
    if (searchingForLogo(m_logoFinder, *m_currentPass))
    {
        LOG(VB_COMMFLAG, LOG_INFO, "Ignoring request for commBreakMapUpdate; "
                                   "still doing logo detection");
        return;
    }

    LOG(VB_COMMFLAG, LOG_INFO, 
        QString("commBreakMapUpdate requested at frame %1")
            .arg(m_currentFrameNumber + 1));
    m_sendBreakMapUpdates = true;
    m_breakMapUpdateRequested = true;
}

static void PrintReportMap(ostream &out,
                           const FrameAnalyzer::FrameMap &frameMap)
{
    FrameAnalyzer::FrameMap::const_iterator it = frameMap.begin();
    for (; it != frameMap.end(); ++it)
    {
        /*
         * QMap'd as 0-based index, but display as 1-based index to match "Edit
         * Recording" OSD.
         */

        long long bb = it.key() + 1;
        long long ee = (*it) ? (bb + *it) : 1;
        QString tmp = QString("%1: %2").arg(bb, 10).arg(ee - 1, 10);

        out << qPrintable(tmp) << "\n";
    }
    out << flush;
}

void CommDetector2::PrintFullMap(
    ostream &out, const frm_dir_map_t */*comm_breaks*/, bool /*verbose*/) const
{
    FrameAnalyzer::FrameMap logoMap;
    FrameAnalyzer::FrameMap blankMap;
    FrameAnalyzer::FrameMap blankBreakMap;
    FrameAnalyzer::FrameMap sceneMap;
    if (m_logoFinder)
        logoMap = m_logoFinder->GetMap(0);

    if (m_blankFrameDetector)
    {
        blankBreakMap = m_blankFrameDetector->GetMap(0);
        blankMap      = m_blankFrameDetector->GetMap(1);
    }

    if (m_sceneChangeDetector)
        sceneMap = m_sceneChangeDetector->GetMap(0);

    out << "Logo Break Map" << endl;
    PrintReportMap(out, logoMap);
    out << "Blank Break Map" << endl;
    PrintReportMap(out, blankBreakMap);
    out << "Blank Map" << endl;
    PrintReportMap(out, blankMap);
    out << "Scene Break Map" << endl;
    PrintReportMap(out, sceneMap);
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
