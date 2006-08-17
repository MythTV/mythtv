#include <math.h>
#include <qdir.h>
#include <qfileinfo.h>

#include "NuppelVideoPlayer.h"

#include "CommDetector.h"
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

int nuppelVideoPlayerInited(QPtrList<FrameAnalyzer> *pass,
        QPtrList<FrameAnalyzer> *finishedAnalyzers,
        QPtrList<FrameAnalyzer> *deadAnalyzers,
        NuppelVideoPlayer *nvp, long long nframes)
{
    QPtrListIterator<FrameAnalyzer>     iifa(*pass);
    FrameAnalyzer                       *fa;
    FrameAnalyzer::analyzeFrameResult   ares;

    for (QPtrListIterator<FrameAnalyzer> jjfa = iifa;
            (fa = iifa.current()); iifa = jjfa)
    {
        ++jjfa;

        ares = fa->nuppelVideoPlayerInited(nvp, nframes);

        if (ares == FrameAnalyzer::ANALYZE_OK ||
                ares == FrameAnalyzer::ANALYZE_ERROR)
        {
            continue;
        }

        if (ares == FrameAnalyzer::ANALYZE_FINISHED)
        {
            pass->remove(fa);
            finishedAnalyzers->append(fa);
            continue;
        }

        if (ares == FrameAnalyzer::ANALYZE_FATAL)
        {
            pass->remove(fa);
            deadAnalyzers->append(fa);
            continue;
        }

        VERBOSE(VB_IMPORTANT, QString("Unexpected return value from"
                    " %1::nuppelVideoPlayerInited: %2")
                .arg(fa->name()).arg(ares));
        return -1;
    }

    return 0;
}

long long processFrame(QPtrList<FrameAnalyzer> *pass,
        QPtrList<FrameAnalyzer> *finishedAnalyzers,
        QPtrList<FrameAnalyzer> *deadAnalyzers,
        const VideoFrame *frame, long long frameno)
{
    QPtrListIterator<FrameAnalyzer>     iifa(*pass);
    FrameAnalyzer                       *fa;
    FrameAnalyzer::analyzeFrameResult   ares;
    long long                           nextFrame, minNextFrame;

    minNextFrame = FrameAnalyzer::ANYFRAME;
    for (QPtrListIterator<FrameAnalyzer> jjfa = iifa;
            (fa = iifa.current()); iifa = jjfa)
    {
        ++jjfa;

        ares = fa->analyzeFrame(frame, frameno, &nextFrame);

        if (ares == FrameAnalyzer::ANALYZE_OK ||
                ares == FrameAnalyzer::ANALYZE_ERROR)
        {
            if (minNextFrame > nextFrame)
                minNextFrame = nextFrame;
            continue;
        }

        if (ares == FrameAnalyzer::ANALYZE_FINISHED)
        {
            pass->remove(fa);
            finishedAnalyzers->append(fa);
            continue;
        }

        if (ares == FrameAnalyzer::ANALYZE_FATAL)
        {
            pass->remove(fa);
            deadAnalyzers->append(fa);
            continue;
        }

        VERBOSE(VB_IMPORTANT, QString("Unexpected return value from"
                    " %1::analyzeFrame: %2")
                .arg(fa->name()).arg(ares));
        pass->remove(fa);
        deadAnalyzers->append(fa);
    }

    if (minNextFrame == FrameAnalyzer::ANYFRAME)
        minNextFrame = FrameAnalyzer::NEXTFRAME;

    if (minNextFrame == FrameAnalyzer::NEXTFRAME)
        minNextFrame = frameno + 1;

    return minNextFrame;
}

int passFinished(QPtrList<FrameAnalyzer> *pass, long long nframes, bool final)
{
    FrameAnalyzer       *fa;

    for (QPtrListIterator<FrameAnalyzer> iifa(*pass);
            (fa = iifa.current()); ++iifa)
        (void)fa->finished(nframes, final);

    return 0;
}

int passReportTime(QPtrList<FrameAnalyzer> *pass)
{
    FrameAnalyzer       *fa;

    for (QPtrListIterator<FrameAnalyzer> iifa(*pass);
            (fa = iifa.current()); ++iifa)
        (void)fa->reportTime();

    return 0;
}

bool searchingForLogo(TemplateFinder *tf, QPtrList<FrameAnalyzer> *pass)
{
    return tf && pass->find(tf) != -1;
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
    query.exec();
    if (query.size() <= 0 || !query.next())
    {
        MythContext::DBError("Error in CommDetector2::CommDetector2",
                query);
        return "";
    }

    QString recordfileprefix = gContext->GetSetting("RecordFilePrefix");
    QString basename = query.value(0).toString();
    QFileInfo baseinfo(recordfileprefix + "/" + basename);
    QString debugdir = recordfileprefix + "/" + baseinfo.baseName(TRUE) +
        "-debug";

    return debugdir;
}

void createDebugDirectory(QString dirname, QString comment)
{
    QDir qdir(dirname);
    if (qdir.exists())
    {
        VERBOSE(VB_COMMFLAG, QString("%1 using debug directory \"%2\"")
                .arg(comment).arg(dirname));
    }
    else
    {
        if (qdir.mkdir(dirname))
        {
            VERBOSE(VB_COMMFLAG, QString("%1 created debug directory \"%1\"")
                    .arg(comment).arg(dirname));
        }
        else
        {
            VERBOSE(VB_COMMFLAG, QString("%1 failed to create \"%2\": %3")
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

CommDetector2::CommDetector2(enum SkipTypes commDetectMethod_in,
        bool showProgress_in, bool fullSpeed_in, NuppelVideoPlayer* nvp_in,
        int chanid, const QDateTime& startts_in, const QDateTime& endts_in,
        const QDateTime& recstartts_in, const QDateTime& recendts_in)
    : commDetectMethod((enum SkipTypes)(commDetectMethod_in & ~COMM_DETECT_2))
    , showProgress(showProgress_in)
    , fullSpeed(fullSpeed_in)
    , nvp(nvp_in)
    , startts(startts_in)
    , endts(endts_in)
    , recstartts(recstartts_in)
    , recendts(recendts_in)
    , isRecording(QDateTime::currentDateTime() < recendts)
    , sendBreakMapUpdates(false)
    , breakMapUpdateRequested(false)
    , finished(false)
    , logoFinder(NULL)
    , logoMatcher(NULL)
    , blankFrameDetector(NULL)
    , sceneChangeDetector(NULL)
    , debugdir("")
{
    QPtrList<FrameAnalyzer> pass0, pass1;
    PGMConverter            *pgmConverter = NULL;
    BorderDetector          *borderDetector = NULL;
    HistogramAnalyzer       *histogramAnalyzer = NULL;

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
            pass1.append(blankFrameDetector);
        }
    }

#ifdef LATER
    /*
     * "Scene Detection" doesn't seem to be too useful (in the USA); there are
     * just too many false positives from non-commercial cut scenes. But I'll
     * leave the code in here in case someone else finds it useful.
     */

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
            pass1.append(sceneChangeDetector);
        }
    }
#endif /* LATER */

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
                    cannyEdgeDetector, nvp, recstartts.secsTo(recendts),
                    debugdir);
            pass0.append(logoFinder);
        }

        if (!logoMatcher)
        {
            logoMatcher = new TemplateMatcher(pgmConverter, cannyEdgeDetector,
                    logoFinder, debugdir);
            pass1.append(logoMatcher);
        }
    }

    if (histogramAnalyzer && logoFinder)
        histogramAnalyzer->setLogoState(logoFinder);

    /* Aggregate them all together. */
    frameAnalyzers.append(pass0);
    frameAnalyzers.append(pass1);
}

void CommDetector2::reportState(int elapsedms, long long frameno,
        long long nframes, unsigned int passno, unsigned int npasses)
{
    float fps = elapsedms ? (float)frameno * 1000 / elapsedms : 0;

    /* Assume that 0-th pass is negligible in terms of computational cost. */
    int percentage = passno == 0 ? 0 :
        (passno - 1) * 100 / (npasses - 1) +
        min((long long)100, (frameno * 100 / nframes) / (npasses - 1));

    if (showProgress)
    {
        if (nframes)
        {
            cerr << "\b\b\b\b\b\b\b\b\b\b\b"
                 << QString::number(percentage).rightJustify(3, ' ')
                 << "%/"
                 << QString::number((int)fps).rightJustify(3, ' ')
                 << "fps";
        }
        else
        {
            cerr << "\b\b\b\b\b\b\b\b\b\b\b\b\b"
                 << QString::number(frameno).rightJustify(6, ' ')
                 << "/"
                 << QString::number((int)fps).rightJustify(3, ' ')
                 << "fps";
        }
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
            if (matcher->adjustForBlanks(blankFrameDetector))
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

    nvp->SetNullVideo();

    if (nvp->OpenFile() < 0)
        return false;

    if (!nvp->InitVideo())
    {
        VERBOSE(VB_IMPORTANT,
                "NVP: Unable to initialize video for FlagCommercials.");
        return false;
    }

    nvp->SetCaptionsEnabled(false);

    QTime flagTime;
    flagTime.start();
    
    /* If still recording, estimate the eventual total number of frames. */
    long long nframes = isRecording ?
        (long long)roundf((recstartts.secsTo(recendts) + 5) *
                          nvp->GetFrameRate()) :
            nvp->GetTotalFrameCount();
    bool postprocessing = !isRecording;

    if (showProgress)
    {
        if (nframes)
            cerr << "  0%/      ";
        else
            cerr << "     0/      ";
        cerr.flush();
    }

    QMap<long long, int> lastBreakMap;
    unsigned int passno = 0;
    unsigned int npasses = frameAnalyzers.count();
    for (currentPass = frameAnalyzers.begin();
            currentPass != frameAnalyzers.end();
            ++currentPass, passno++)
    {
        QPtrList<FrameAnalyzer> deadAnalyzers;

        VERBOSE(VB_COMMFLAG, QString(
                    "CommDetector2::go pass %1 of %2 (%3 frames, %4 fps)")
                .arg(passno + 1).arg(npasses)
                .arg(nvp->GetTotalFrameCount())
                .arg(nvp->GetFrameRate(), 0, 'f', 2));

        if (nuppelVideoPlayerInited(&(*currentPass), &finishedAnalyzers,
                    &deadAnalyzers, nvp, nframes))
            return false;

        /*
         * Each pass starts at beginning. VideoFrame.frameNumber appears to be
         * a cumulative counter of total frames "played" rather than an actual
         * index into the file, so maintain the frame index in
         * currentFrameNumber.
         */
        nvp->DiscardVideoFrame(nvp->GetRawVideoFrame(0));
        long long nextFrame = -1;
        currentFrameNumber = 0;
        long long lastLoggedFrame = currentFrameNumber;
        QTime clock;
        struct timeval getframetime;

        if (searchingForLogo(logoFinder, &(*currentPass)))
            emit statusUpdate("Performing Logo Identification");

        clock.start();
        memset(&getframetime, 0, sizeof(getframetime));
        while (!(*currentPass).isEmpty() && !nvp->GetEof())
        {
            struct timeval start, end, elapsed;

            (void)gettimeofday(&start, NULL);
            VideoFrame *currentFrame = nvp->GetRawVideoFrame(nextFrame);
            (void)gettimeofday(&end, NULL);
            timersub(&end, &start, &elapsed);
            timeradd(&getframetime, &elapsed, &getframetime);

            if (stopForBreath(isRecording, currentFrameNumber))
            {
                emit breathe();
                if (m_bStop)
                {
                    nvp->DiscardVideoFrame(currentFrame);
                    return false;
                }
            }

            while (m_bPaused)
            {
                emit breathe();
                sleep(1);
            }

            if (!searchingForLogo(logoFinder, &(*currentPass)) &&
                    needToReportState(showProgress, isRecording,
                        currentFrameNumber))
            {
                reportState(flagTime.elapsed(), currentFrameNumber,
                        nframes, passno, npasses);
            }

            nextFrame = processFrame(&(*currentPass), &finishedAnalyzers,
                    &deadAnalyzers, currentFrame, currentFrameNumber);

            if (currentFrameNumber >= 1 &&
                    nextFrame * 10 / nframes !=
                    currentFrameNumber * 10 / nframes ||
                    nextFrame >= nframes)
            {
                /* Log something every 10%. */
                int elapsed = clock.restart();
                VERBOSE(VB_COMMFLAG, QString(
                            "processFrame %1 of %2 (%3%) - %4 fps")
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
                        recstartts.secsTo(QDateTime::currentDateTime()) -
                        flagTime.elapsed() / 1000, nvp->GetFrameRate(),
                        fullSpeed);
            }

            // sleep a little so we don't use all cpu even if we're niced
            if (!fullSpeed && !isRecording)
                usleep(10000);  // 10ms

            if (sendBreakMapUpdates && (breakMapUpdateRequested ||
                        !(currentFrameNumber % 500)))
            {
                QMap<long long, int> breakMap;

                getCommercialBreakList(breakMap);

                QMap<long long, int>::const_iterator ii, jj;
                ii = breakMap.begin();
                jj = lastBreakMap.begin();
                while (ii != breakMap.end() && jj != breakMap.end())
                {
                    if (ii.key() != jj.key())
                        break;
                    if (ii.data() != jj.data())
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

            nvp->DiscardVideoFrame(currentFrame);

            currentFrameNumber = nextFrame;
        }

        QPtrListIterator<FrameAnalyzer> iifa(finishedAnalyzers);
        FrameAnalyzer *fa;
        while ((fa = iifa.current()))
        {
            finishedAnalyzers.remove(fa);
            (*currentPass).append(fa);
        }

        if (postprocessing)
            currentFrameNumber = nvp->GetTotalFrameCount() - 1;
        if (passFinished(&(*currentPass), currentFrameNumber + 1, true))
            return false;

        VERBOSE(VB_COMMFLAG, QString("NVP Time: GetRawVideoFrame=%1s")
                .arg(strftimeval(&getframetime)));
        if (passReportTime(&(*currentPass)))
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

void CommDetector2::getCommercialBreakList(QMap<long long, int> &marks)
{
    if (!finished)
    {
        for (frameAnalyzerList::iterator pass = frameAnalyzers.begin();
                pass != frameAnalyzers.end();
                ++pass)
        {
            if (*pass == *currentPass && passFinished(&finishedAnalyzers,
                        currentFrameNumber + 1, false))
                return;
            if (passFinished(&(*pass), currentFrameNumber + 1, false))
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
        long long seglen = bb.data();
        long long sege = segb + seglen - 1;

        marks[segb] = MARK_COMM_START;
        marks[sege] = MARK_COMM_END;

        breakframes += seglen;
    }

    /* Report results. */
    const float fps = nvp->GetFrameRate();
    for (QMap<long long, int>::const_iterator iimark = marks.begin();
            iimark != marks.end();
            ++iimark)
    {
        long long   markstart, markend;

        /* Display as 1-based frame numbers. */
        markstart = iimark.key() + 1;   /* MARK_COMM_BEGIN */
        ++iimark;                       /* MARK_COMM_END */
        markend = iimark.key() + 1;

        VERBOSE(VB_COMMFLAG, QString("Break: frame %1-%2 (%3-%4, %5)")
                .arg(markstart, 6).arg(markend, 6)
                .arg(frameToTimestamp(markstart, fps))
                .arg(frameToTimestamp(markend, fps))
                .arg(frameToTimestamp(markend - markstart + 1, fps)));
    }

    const long long nframes = nvp->GetTotalFrameCount();
    VERBOSE(VB_COMMFLAG, QString("Flagged %1 of %2 frames (%3 of %4),"
                " %5% commercials (%6)")
            .arg(currentFrameNumber)
            .arg(nframes)
            .arg(frameToTimestamp(currentFrameNumber, fps))
            .arg(frameToTimestamp(nframes, fps))
            .arg(breakframes * 100 / currentFrameNumber)
            .arg(frameToTimestamp(breakframes, fps)));
}

void CommDetector2::recordingFinished(long long totalFileSize)
{
    CommDetectorBase::recordingFinished(totalFileSize);
    isRecording = false;
    VERBOSE(VB_COMMFLAG, QString("CommDetector2::recordingFinished: %1 bytes")
            .arg(totalFileSize));
}

void CommDetector2::requestCommBreakMapUpdate(void)
{
    if (searchingForLogo(logoFinder, &(*currentPass)))
    {
        VERBOSE(VB_COMMFLAG, "Ignoring request for commBreakMapUpdate;"
                " still doing logo detection");
        return;
    }

    VERBOSE(VB_COMMFLAG, QString("commBreakMapUpdate requested at frame %1")
            .arg(currentFrameNumber + 1));
    sendBreakMapUpdates = true;
    breakMapUpdateRequested = true;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
