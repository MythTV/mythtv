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
        NuppelVideoPlayer *nvp)
{
    QPtrListIterator<FrameAnalyzer>     iifa(*pass);
    FrameAnalyzer                       *fa;
    FrameAnalyzer::analyzeFrameResult   ares;

    while ((fa = iifa.current()))
    {
        ares = fa->nuppelVideoPlayerInited(nvp);

        if (ares == FrameAnalyzer::ANALYZE_OK ||
                ares == FrameAnalyzer::ANALYZE_ERROR)
        {
            ++iifa;
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
    while ((fa = iifa.current()))
    {
        ares = fa->analyzeFrame(frame, frameno, &nextFrame);

        if (ares == FrameAnalyzer::ANALYZE_OK ||
                ares == FrameAnalyzer::ANALYZE_ERROR)
        {
            if (minNextFrame > nextFrame)
                minNextFrame = nextFrame;
            ++iifa;
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

int passFinished(QPtrList<FrameAnalyzer> *pass)
{
    FrameAnalyzer       *fa;

    for (QPtrListIterator<FrameAnalyzer> iifa(*pass);
            (fa = iifa.current()); ++iifa)
        (void)fa->finished();

    for (QPtrListIterator<FrameAnalyzer> iifa(*pass);
            (fa = iifa.current()); ++iifa)
        (void)fa->finished2();

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

bool isContent(QPtrList<FrameAnalyzer> *pass, long long frameno)
{
    /* Compute some function of all analyses on a given frame. */
    unsigned int    score;
    FrameAnalyzer   *fa;

    score = 0;
    for (QPtrListIterator<FrameAnalyzer> iifa(*pass);
            (fa = iifa.current()); ++iifa)
    {
        if (fa->isContent(frameno))
            score++;
    }
    return score > 0;
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
    , debugdir("")
{
    QPtrList<FrameAnalyzer> pass0, pass1;
    PGMConverter            *pgmConverter = NULL;
    BorderDetector          *borderDetector = NULL;
    HistogramAnalyzer       *histogramAnalyzer = NULL;
    TemplateFinder          *logoFinder = NULL;
    TemplateMatcher         *logoMatcher = NULL;

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
            pass1.append(histogramAnalyzer);
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
                    cannyEdgeDetector, nvp, debugdir);
            pass0.append(logoFinder);
        }

        if (!logoMatcher)
        {
            logoMatcher = new TemplateMatcher(pgmConverter, cannyEdgeDetector,
                    logoFinder, debugdir);
            pass1.append(logoMatcher);
        }
    }

    if (logoMatcher && histogramAnalyzer)
    {
        logoMatcher->setHistogramAnalyzer(histogramAnalyzer);
        histogramAnalyzer->setTemplateState(logoFinder);
    }

    /* Aggregate them all together. */
    frameAnalyzers.append(pass0);
    frameAnalyzers.append(pass1);
}

int CommDetector2::buildBuffer(int minbuffer)
{
    bool                    wereRecording = isRecording;
    int                     buffer = minbuffer;
    int                     maxExtra = 0;
    int                     preroll = recstartts.secsTo(startts);

    for (frameAnalyzerList::const_iterator pass = frameAnalyzers.begin();
            pass != frameAnalyzers.end();
            ++pass)
    {
        FrameAnalyzer   *fa;

        if ((*pass).isEmpty())
            continue;

        for (QPtrListIterator<FrameAnalyzer> iifa(*pass);
                (fa = iifa.current()); ++iifa)
        {
            maxExtra = max(maxExtra, fa->extraBuffer(preroll));
        }

        buffer += maxExtra;
    }

    emit statusUpdate("Building Detection Buffer");

    int reclen;
    while (isRecording && (reclen = recstartts.secsTo(
        QDateTime::currentDateTime())) < buffer)
    {
        emit breathe();
        if (m_bStop)
            return -1;
        sleep(2);
    }

    // Don't bother flagging short ~realtime recordings
    if (wereRecording && !isRecording && reclen < buffer)
        return -1;

    return 0;
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

bool CommDetector2::go(void)
{
    int minlag = 7; // seconds

    nvp->SetNullVideo();

    if (buildBuffer(minlag) < 0)
        return false;

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
    
    long long myTotalFrames = recendts < QDateTime::currentDateTime() ?
        nvp->GetTotalFrameCount() :
        (long long)(nvp->GetFrameRate() * recstartts.secsTo(recendts));

    if (showProgress)
    {
        if (myTotalFrames)
            cerr << "  0%/      ";
        else
            cerr << "     0/      ";
        cerr.flush();
    }

    emit breathe();

    unsigned int passno = 0;
    unsigned int npasses = frameAnalyzers.count();
    long long nframes = nvp->GetTotalFrameCount();
    for (frameAnalyzerList::iterator pass = frameAnalyzers.begin();
            pass != frameAnalyzers.end();
            ++pass, passno++)
    {
        QPtrList<FrameAnalyzer> finishedAnalyzers, deadAnalyzers;
        FrameAnalyzer           *fa;

        VERBOSE(VB_COMMFLAG, QString(
                    "CommDetector2::go pass %1 of %2 (%3 frames, %4 fps)")
                .arg(passno + 1).arg(npasses)
                .arg(nvp->GetTotalFrameCount())
                .arg(nvp->GetFrameRate(), 0, 'f', 2));

        if (nuppelVideoPlayerInited(&(*pass), &finishedAnalyzers,
                    &deadAnalyzers, nvp))
            return false;

        /*
         * Each pass starts at beginning. VideoFrame.frameNumber appears to be
         * a cumulative counter of total frames "played" rather than an actual
         * index into the file, so maintain the frame index in
         * currentFrameNumber.
         */
        nvp->DiscardVideoFrame(nvp->GetRawVideoFrame(0));
        long long nextFrame = -1;
        long long currentFrameNumber = 0;
        long long lastLoggedFrame = currentFrameNumber;
        QTime clock;
        struct timeval getframetime;

        clock.start();
        memset(&getframetime, 0, sizeof(getframetime));
        while (currentFrameNumber < nframes && !(*pass).isEmpty() &&
                !nvp->GetEof())
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

            // sleep a little so we don't use all cpu even if we're niced
            if (!fullSpeed && !isRecording)
                usleep(10000);  // 10ms

            if (needToReportState(showProgress, isRecording,
                        currentFrameNumber))
            {
                reportState(flagTime.elapsed(), currentFrameNumber,
                        myTotalFrames, passno, npasses);
            }

            nextFrame = processFrame(&(*pass), &finishedAnalyzers,
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
                        .arg((currentFrameNumber + 1) * 100 / nframes)
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

            nvp->DiscardVideoFrame(currentFrame);

            currentFrameNumber = nextFrame;
        }

        QPtrListIterator<FrameAnalyzer> iifa(finishedAnalyzers);
        while ((fa = iifa.current()))
        {
            finishedAnalyzers.remove(fa);
            (*pass).append(fa);
        }

        if (passFinished(&(*pass)))
            return false;

        VERBOSE(VB_COMMFLAG, QString("NVP Time: GetRawVideoFrame=%1s")
                .arg(strftimeval(&getframetime)));
        if (passReportTime(&(*pass)))
            return false;
    }

    if (showProgress)
    {
        if (myTotalFrames)
            cerr << "\b\b\b\b\b\b      \b\b\b\b\b\b";
        else
            cerr << "\b\b\b\b\b\b\b\b\b\b\b\b\b             "
                    "\b\b\b\b\b\b\b\b\b\b\b\b\b";
        cerr.flush();
    }

    return true;
}

void CommDetector2::getCommercialBreakList(QMap<long long, int> &marks)
{
    /*
     * As a matter of record, the TemplateMatcher is tuned to yield false
     * positives (non-commercials marked as commercials); false negatives
     * (commercials marked as non-commercials) are expected to be rare.
     */
    const long long                 nframes = nvp->GetTotalFrameCount();
    const float                     fps = nvp->GetFrameRate();
    unsigned int                    passno;
    bool                            prevContent, thisContent;
    QPtrList<FrameAnalyzer>         *pass1;
    QMap<long long, int>::Iterator  iimark;

    marks.clear();

    /* XXX: Pass #1 (0-based) contains the image analyses. */

    passno = 0;
    pass1 = NULL;
    for (frameAnalyzerList::iterator pass = frameAnalyzers.begin();
            pass != frameAnalyzers.end();
            ++pass, passno++)
    {
        if (passno == 1)
        {
            pass1 = &(*pass);
            break;
        }
    }

    if (!pass1)
        return;

    /* Create break list. */

    if (!(prevContent = isContent(pass1, 0)))
        marks[0] = MARK_COMM_START;
    for (long long frameno = 1; frameno < nframes; frameno++)
    {
        if ((thisContent = isContent(pass1, frameno)) == prevContent)
            continue;

        if (!thisContent)
        {
            marks[frameno] = MARK_COMM_START;
        }
        else
        {
            if ((iimark = marks.find(frameno - 1)) == marks.end())
                marks[frameno - 1] = MARK_COMM_END;
            else
                marks.remove(iimark);
        }
        prevContent = thisContent;
    }
    if (!prevContent)
        marks[nframes - 1] = MARK_COMM_END;

    /* Report results. */

    for (iimark = marks.begin(); iimark != marks.end(); ++iimark)
    {
        long long   markstart, markend;

        markstart = iimark.key();   /* MARK_COMM_BEGIN */
        ++iimark;                   /* MARK_COMM_END */

        markend = iimark.key();

        VERBOSE(VB_COMMFLAG, QString("Break: frame %1-%2 (%3-%4, %5)")
                .arg(markstart, 6).arg(markend, 6)
                .arg(frameToTimestamp(markstart, fps))
                .arg(frameToTimestamp(markend, fps))
                .arg(frameToTimestamp(markend - markstart + 1, fps)));
    }
}

void CommDetector2::recordingFinished(long long totalFileSize)
{
    CommDetectorBase::recordingFinished(totalFileSize);
    isRecording = false;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
