// POSIX headers
#include <unistd.h>

// ANSI C headers
#include <cmath>
#include <cerrno>

// C++ headers
#include <algorithm>
using namespace std;

// Qt headers
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
    long usperframe = (long)(1000000.0 / fps);
    struct timeval now, elapsed;
    long sleepus;

    (void)gettimeofday(&now, NULL);
    timersub(&now, framestart, &elapsed);

    // Sleep for one frame's worth of time.
    sleepus = usperframe - elapsed.tv_sec * 1000000 - elapsed.tv_usec;
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
    usleep(sleepus);
}

bool MythPlayerInited(FrameAnalyzerItem &pass,
                      FrameAnalyzerItem &finishedAnalyzers,
                      FrameAnalyzerItem &deadAnalyzers,
                      MythPlayer *player,
                      long long nframes)
{
    FrameAnalyzerItem::iterator it = pass.begin();
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
    long long nextFrame;
    long long minNextFrame = FrameAnalyzer::ANYFRAME;

    FrameAnalyzerItem::iterator it = pass.begin();
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

    if (minNextFrame == FrameAnalyzer::ANYFRAME)
        minNextFrame = FrameAnalyzer::NEXTFRAME;

    if (minNextFrame == FrameAnalyzer::NEXTFRAME)
        minNextFrame = frameno + 1;

    return minNextFrame;
}

int passFinished(FrameAnalyzerItem &pass, long long nframes, bool final)
{
    FrameAnalyzerItem::iterator it = pass.begin();
    for (; it != pass.end(); ++it)
        (void)(*it)->finished(nframes, final);

    return 0;
}

int passReportTime(const FrameAnalyzerItem &pass)
{
    FrameAnalyzerItem::const_iterator it = pass.begin();
    for (; it != pass.end(); ++it)
        (void)(*it)->reportTime();

    return 0;
}

bool searchingForLogo(TemplateFinder *tf, const FrameAnalyzerItem &pass)
{
    if (!tf)
        return false;

    FrameAnalyzerItem::const_iterator it =
        std::find(pass.begin(), pass.end(), tf);

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

    const ProgramInfo pginfo(chanid, recstartts);

    if (!pginfo.GetChanID())
        return "";

    QString pburl = pginfo.GetPlaybackURL(true);
    if (pburl.left(1) != "/")
        return "";

    QString basename(query.value(0).toString());
    QString debugdir = pburl.section('/',0,-2) + "/" + basename + "-debug";

    return debugdir;
}

void createDebugDirectory(QString dirname, QString comment)
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
    int         ms, ss, mm, hh;
    QString     ts;

    ms = (int)roundf(frameno / fps * 1000);

    ss = ms / 1000;
    ms %= 1000;
    if (ms >= 500)
        ss++;

    mm = ss / 60;
    ss %= 60;

    hh = mm / 60;
    mm %= 60;

    return ts.sprintf("%d:%02d:%02d", hh, mm, ss);
}

QString frameToTimestampms(long long frameno, float fps)
{
    int         ms, ss, mm;
    QString     ts;

    ms = (int)roundf(frameno / fps * 1000);

    ss = ms / 1000;
    ms %= 1000;

    mm = ss / 60;
    ss %= 60;

    return ts.sprintf("%d:%02d.%03d", mm, ss, ms);
}

QString strftimeval(const struct timeval *tv)
{
    QString str;
    return str.sprintf("%ld.%06ld", tv->tv_sec, tv->tv_usec);
}

};  /* namespace */

using namespace commDetector2;

CommDetector2::CommDetector2(
    enum SkipTypes     commDetectMethod_in,
    bool               showProgress_in,
    bool               fullSpeed_in,
    MythPlayer        *player_in,
    int                chanid,
    const QDateTime   &startts_in,
    const QDateTime   &endts_in,
    const QDateTime   &recstartts_in,
    const QDateTime   &recendts_in,
    bool               useDB) :
    commDetectMethod((enum SkipTypes)(commDetectMethod_in & ~COMM_DETECT_2)),
    showProgress(showProgress_in),  fullSpeed(fullSpeed_in),
    player(player_in),
    startts(startts_in),            endts(endts_in),
    recstartts(recstartts_in),      recendts(recendts_in),
    isRecording(MythDate::current() < recendts),
    sendBreakMapUpdates(false),     breakMapUpdateRequested(false),
    finished(false),                currentFrameNumber(0),
    logoFinder(NULL),               logoMatcher(NULL),
    blankFrameDetector(NULL),       sceneChangeDetector(NULL),
    debugdir("")
{
    FrameAnalyzerItem        pass0, pass1;
    PGMConverter            *pgmConverter = NULL;
    BorderDetector          *borderDetector = NULL;
    HistogramAnalyzer       *histogramAnalyzer = NULL;

    if (useDB)
        debugdir = debugDirectory(chanid, recstartts);

    /*
     * Look for blank frames to use as delimiters between commercial and
     * non-commercial segments.
     */
    if ((commDetectMethod & COMM_DETECT_2_BLANK))
    {
        if (!pgmConverter)
            pgmConverter = new PGMConverter();

        if (!borderDetector)
            borderDetector = new BorderDetector();

        if (!histogramAnalyzer)
        {
            histogramAnalyzer = new HistogramAnalyzer(pgmConverter,
                    borderDetector, debugdir);
        }

        if (!blankFrameDetector)
        {
            blankFrameDetector = new BlankFrameDetector(histogramAnalyzer,
                    debugdir);
            pass1.push_back(blankFrameDetector);
        }
    }

    /*
     * Look for "scene changes" to use as delimiters between commercial and
     * non-commercial segments.
     */
    if ((commDetectMethod & COMM_DETECT_2_SCENE))
    {
        if (!pgmConverter)
            pgmConverter = new PGMConverter();

        if (!borderDetector)
            borderDetector = new BorderDetector();

        if (!histogramAnalyzer)
        {
            histogramAnalyzer = new HistogramAnalyzer(pgmConverter,
                    borderDetector, debugdir);
        }

        if (!sceneChangeDetector)
        {
            sceneChangeDetector = new SceneChangeDetector(histogramAnalyzer,
                    debugdir);
            pass1.push_back(sceneChangeDetector);
        }
    }

    /*
     * Logo Detection requires two passes. The first pass looks for static
     * areas of the screen to look for logos and generate a template. The
     * second pass matches the template against the frame and computes the
     * closeness of the match.
     */
    if ((commDetectMethod & COMM_DETECT_2_LOGO))
    {
        CannyEdgeDetector       *cannyEdgeDetector = NULL;

        if (!pgmConverter)
            pgmConverter = new PGMConverter();

        if (!borderDetector)
            borderDetector = new BorderDetector();

        if (!cannyEdgeDetector)
            cannyEdgeDetector = new CannyEdgeDetector();

        if (!logoFinder)
        {
            logoFinder = new TemplateFinder(pgmConverter, borderDetector,
                    cannyEdgeDetector, player, recstartts.secsTo(recendts),
                    debugdir);
            pass0.push_back(logoFinder);
        }

        if (!logoMatcher)
        {
            logoMatcher = new TemplateMatcher(pgmConverter, cannyEdgeDetector,
                    logoFinder, debugdir);
            pass1.push_back(logoMatcher);
        }
    }

    if (histogramAnalyzer && logoFinder)
        histogramAnalyzer->setLogoState(logoFinder);

    /* Aggregate them all together. */
    frameAnalyzers.push_back(pass0);
    frameAnalyzers.push_back(pass1);
}

void CommDetector2::reportState(int elapsedms, long long frameno,
        long long nframes, unsigned int passno, unsigned int npasses)
{
    float fps = elapsedms ? (float)frameno * 1000 / elapsedms : 0;
    int prevpercent = -1;

    /* Assume that 0-th pass is negligible in terms of computational cost. */
    int percentage = passno == 0 ? 0 :
        (passno - 1) * 100 / (npasses - 1) +
        min((long long)100, (frameno * 100 / nframes) / (npasses - 1));

    if (showProgress)
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
        emit statusUpdate(QObject::tr("%1% Completed @ %2 fps.")
                .arg(percentage).arg(fps));
    }
    else
    {
        emit statusUpdate(QObject::tr("%1 Frames Completed @ %2 fps.")
                .arg(frameno).arg(fps));
    }

    if (percentage % 10 == 0 && prevpercent != percentage)
    {
        prevpercent = percentage;
        LOG(VB_GENERAL, LOG_INFO, QString("%1%% Completed @ %2 fps.")
            .arg(percentage) .arg(fps));
    }
}

int CommDetector2::computeBreaks(long long nframes)
{
    int             trow, tcol, twidth, theight;
    TemplateMatcher *matcher;

    breaks.clear();

    matcher = logoFinder &&
        logoFinder->getTemplate(&trow, &tcol, &twidth, &theight) ? logoMatcher :
        NULL;

    if (matcher && blankFrameDetector)
    {
        int cmp;
        if (!(cmp = matcher->templateCoverage(nframes, finished)))
        {
            if (matcher->adjustForBlanks(blankFrameDetector, nframes))
                return -1;
            if (matcher->computeBreaks(&breaks))
                return -1;
        }
        else
        {
            if (cmp > 0 &&
                    blankFrameDetector->computeForLogoSurplus(matcher))
                return -1;
            else if (cmp < 0 &&
                    blankFrameDetector->computeForLogoDeficit(matcher))
                return -1;

            if (blankFrameDetector->computeBreaks(&breaks))
                return -1;
        }
    }
    else if (matcher)
    {
        if (matcher->computeBreaks(&breaks))
            return -1;
    }
    else if (blankFrameDetector)
    {
        if (blankFrameDetector->computeBreaks(&breaks))
            return -1;
    }

    return 0;
}

bool CommDetector2::go(void)
{
    int minlag = 7; // seconds

    if (player->OpenFile() < 0)
        return false;

    if (!player->InitVideo())
    {
        LOG(VB_GENERAL, LOG_ERR,
            "NVP: Unable to initialize video for FlagCommercials.");
        return false;
    }

    player->EnableSubtitles(false);

    QTime totalFlagTime;
    totalFlagTime.start();

    /* If still recording, estimate the eventual total number of frames. */
    long long nframes = isRecording ?
        (long long)roundf((recstartts.secsTo(recendts) + 5) *
                          player->GetFrameRate()) :
            player->GetTotalFrameCount();
    bool postprocessing = !isRecording;

    if (showProgress)
    {
        if (nframes)
            cerr << "  0%/      ";
        else
            cerr << "     0/      ";
        cerr.flush();
    }

    frm_dir_map_t lastBreakMap;
    unsigned int passno = 0;
    unsigned int npasses = frameAnalyzers.size();
    for (currentPass = frameAnalyzers.begin();
            currentPass != frameAnalyzers.end();
            ++currentPass, passno++)
    {
        FrameAnalyzerItem deadAnalyzers;

        LOG(VB_COMMFLAG, LOG_INFO,
            QString("CommDetector2::go pass %1 of %2 (%3 frames, %4 fps)")
                .arg(passno + 1).arg(npasses)
                .arg(player->GetTotalFrameCount())
                .arg(player->GetFrameRate(), 0, 'f', 2));

        if (!MythPlayerInited(
                *currentPass, finishedAnalyzers, deadAnalyzers, player, nframes))
        {
            return false;
        }

        player->DiscardVideoFrame(player->GetRawVideoFrame(0));
        long long nextFrame = -1;
        currentFrameNumber = 0;
        long long lastLoggedFrame = currentFrameNumber;
        QTime passTime, clock;
        struct timeval getframetime;

        player->ResetTotalDuration();

        if (searchingForLogo(logoFinder, *currentPass))
            emit statusUpdate(QObject::tr("Performing Logo Identification"));

        clock.start();
        passTime.start();
        memset(&getframetime, 0, sizeof(getframetime));
        while (!(*currentPass).empty() && player->GetEof() == kEofStateNone)
        {
            struct timeval start, end, elapsedtv;

            (void)gettimeofday(&start, NULL);
            VideoFrame *currentFrame = player->GetRawVideoFrame(nextFrame);
            long long lastFrameNumber = currentFrameNumber;
            currentFrameNumber = currentFrame->frameNumber;
            (void)gettimeofday(&end, NULL);
            timersub(&end, &start, &elapsedtv);
            timeradd(&getframetime, &elapsedtv, &getframetime);

            if (nextFrame != -1 && nextFrame == lastFrameNumber + 1 &&
                    currentFrameNumber != nextFrame)
            {
                /*
                 * Don't log "Jumped" when we know we're skipping frames (e.g.,
                 * logo detection).
                 */
                LOG(VB_COMMFLAG, LOG_INFO,
                    QString("Jumped from frame %1 to frame %2")
                        .arg(lastFrameNumber).arg(currentFrameNumber));
            }

            if (stopForBreath(isRecording, currentFrameNumber))
            {
                emit breathe();
                if (m_bStop)
                {
                    player->DiscardVideoFrame(currentFrame);
                    return false;
                }
            }

            while (m_bPaused)
            {
                emit breathe();
                sleep(1);
            }

            if (!searchingForLogo(logoFinder, *currentPass) &&
                    needToReportState(showProgress, isRecording,
                        currentFrameNumber))
            {
                reportState(passTime.elapsed(), currentFrameNumber,
                        nframes, passno, npasses);
            }

            nextFrame = processFrame(
                *currentPass, finishedAnalyzers,
                deadAnalyzers, currentFrame, currentFrameNumber);

            if (((currentFrameNumber >= 1) &&
                 (((nextFrame * 10) / nframes) !=
                  ((currentFrameNumber * 10) / nframes))) ||
                (nextFrame >= nframes))
            {
                /* Log something every 10%. */
                int elapsed = clock.restart();
                LOG(VB_COMMFLAG, LOG_INFO,
                    QString("processFrame %1 of %2 (%3%) - %4 fps")
                        .arg(currentFrameNumber)
                        .arg(nframes)
                        .arg((int)roundf(currentFrameNumber * 100.0 / nframes))
                        .arg((currentFrameNumber - lastLoggedFrame) * 1000 /
                            elapsed));
                lastLoggedFrame = currentFrameNumber;
            }

            if (isRecording)
            {
                waitForBuffer(&start, minlag,
                        recstartts.secsTo(MythDate::current()) -
                        totalFlagTime.elapsed() / 1000, player->GetFrameRate(),
                        fullSpeed);
            }

            // sleep a little so we don't use all cpu even if we're niced
            if (!fullSpeed && !isRecording)
                usleep(10000);  // 10ms

            if (sendBreakMapUpdates && (breakMapUpdateRequested ||
                        !(currentFrameNumber % 500)))
            {
                frm_dir_map_t breakMap;

                GetCommercialBreakList(breakMap);

                frm_dir_map_t::const_iterator ii, jj;
                ii = breakMap.begin();
                jj = lastBreakMap.begin();
                while (ii != breakMap.end() && jj != lastBreakMap.end())
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

                if (breakMapUpdateRequested || !same)
                    emit gotNewCommercialBreakList();

                breakMapUpdateRequested = false;
            }

            player->DiscardVideoFrame(currentFrame);
        }

        // Save total duration only on the last pass, which hopefully does
        // no skipping.
        if (passno + 1 == npasses)
            player->SaveTotalDuration();

        currentPass->insert(currentPass->end(),
                            finishedAnalyzers.begin(),
                            finishedAnalyzers.end());
        finishedAnalyzers.clear();

        if (postprocessing)
            currentFrameNumber = player->GetTotalFrameCount() - 1;
        if (passFinished(*currentPass, currentFrameNumber + 1, true))
            return false;

        LOG(VB_COMMFLAG, LOG_INFO, QString("NVP Time: GetRawVideoFrame=%1s")
                .arg(strftimeval(&getframetime)));
        if (passReportTime(*currentPass))
            return false;
    }

    if (showProgress)
    {
        if (nframes)
            cerr << "\b\b\b\b\b\b      \b\b\b\b\b\b";
        else
            cerr << "\b\b\b\b\b\b\b\b\b\b\b\b\b             "
                    "\b\b\b\b\b\b\b\b\b\b\b\b\b";
        cerr.flush();
    }

    finished = true;
    return true;
}

void CommDetector2::GetCommercialBreakList(frm_dir_map_t &marks)
{
    if (!finished)
    {
        for (FrameAnalyzerList::iterator pass = frameAnalyzers.begin();
             pass != frameAnalyzers.end(); ++pass)
        {
            if (*pass == *currentPass &&
                passFinished(finishedAnalyzers, currentFrameNumber + 1, false))
            {
                return;
            }

            if (passFinished(*pass, currentFrameNumber + 1, false))
                return;
        }
    }

    if (computeBreaks(currentFrameNumber + 1))
        return;

    marks.clear();

    /* Create break list. */
    long long breakframes = 0;
    for (FrameAnalyzer::FrameMap::Iterator bb = breaks.begin();
            bb != breaks.end();
            ++bb)
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
    const float fps = player->GetFrameRate();
    for (frm_dir_map_t::const_iterator iimark = marks.begin();
            iimark != marks.end();
            ++iimark)
    {
        long long   markstart, markend;

        /* Display as 1-based frame numbers. */
        markstart = iimark.key() + 1;   /* MARK_COMM_BEGIN */
        ++iimark;                       /* MARK_COMM_END */
        if (iimark == marks.end())
            break;
        markend = iimark.key() + 1;

        LOG(VB_COMMFLAG, LOG_INFO, QString("Break: frame %1-%2 (%3-%4, %5)")
                .arg(markstart, 6).arg(markend, 6)
                .arg(frameToTimestamp(markstart, fps))
                .arg(frameToTimestamp(markend, fps))
                .arg(frameToTimestamp(markend - markstart + 1, fps)));
    }

    const long long nframes = player->GetTotalFrameCount();
    LOG(VB_COMMFLAG, LOG_INFO,
        QString("Flagged %1 of %2 frames (%3 of %4), %5% commercials (%6)")
            .arg(currentFrameNumber + 1).arg(nframes)
            .arg(frameToTimestamp(currentFrameNumber + 1, fps))
            .arg(frameToTimestamp(nframes, fps))
            .arg(breakframes * 100 / currentFrameNumber)
            .arg(frameToTimestamp(breakframes, fps)));
}

void CommDetector2::recordingFinished(long long totalFileSize)
{
    CommDetectorBase::recordingFinished(totalFileSize);
    isRecording = false;
    LOG(VB_COMMFLAG, LOG_INFO,
        QString("CommDetector2::recordingFinished: %1 bytes")
            .arg(totalFileSize));
}

void CommDetector2::requestCommBreakMapUpdate(void)
{
    if (searchingForLogo(logoFinder, *currentPass))
    {
        LOG(VB_COMMFLAG, LOG_INFO, "Ignoring request for commBreakMapUpdate; "
                                   "still doing logo detection");
        return;
    }

    LOG(VB_COMMFLAG, LOG_INFO, 
        QString("commBreakMapUpdate requested at frame %1")
            .arg(currentFrameNumber + 1));
    sendBreakMapUpdates = true;
    breakMapUpdateRequested = true;
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
    ostream &out, const frm_dir_map_t *comm_breaks, bool verbose) const
{
    FrameAnalyzer::FrameMap logoMap, blankMap, blankBreakMap, sceneMap;
    if (logoFinder)
        logoMap = logoFinder->GetMap(0);

    if (blankFrameDetector)
    {
        blankBreakMap = blankFrameDetector->GetMap(0);
        blankMap      = blankFrameDetector->GetMap(1);
    }

    if (sceneChangeDetector)
        sceneMap = sceneChangeDetector->GetMap(0);

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
