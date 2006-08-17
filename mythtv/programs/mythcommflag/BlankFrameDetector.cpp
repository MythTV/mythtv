#include <math.h>

#include "mythcontext.h"    /* gContext */
#include "NuppelVideoPlayer.h"

#include "CommDetector2.h"
#include "FrameAnalyzer.h"
#include "PGMConverter.h"
#include "BorderDetector.h"
#include "TemplateFinder.h"
#include "BlankFrameDetector.h"

/* Debugging */
#include <qfile.h>

using namespace commDetector2;

namespace {

bool
readData(QString filename, unsigned char *minval, unsigned char *avgval,
        unsigned char *maxval, long long nframes)
{
    QFile qfile(filename);

    if (!qfile.open(IO_ReadOnly))
        return false;

    QTextStream     stream(&qfile);
    long long       ii = 0;

    while (!stream.atEnd())
    {
        unsigned int    first, second, third;

        if (ii >= nframes)
        {
            VERBOSE(VB_COMMFLAG, QString("Too much data in %1")
                    .arg(filename));
            goto error;
        }
        stream >> first >> second >> third;
        minval[ii] = first;
        avgval[ii] = second;
        maxval[ii] = third;
        ii++;
    }
    qfile.close();

    if (ii < nframes)
    {
        VERBOSE(VB_COMMFLAG, QString(
                    "Not enough data in %1 (got %2, expected %3)")
                .arg(filename).arg(ii).arg(nframes));
        return false;
    }
    return true;

error:
    qfile.close();
    return false;
}

bool
writeData(QString filename, unsigned char *minval, unsigned char *avgval,
        unsigned char *maxval, long long nframes)
{
    QFile qfile(filename);

    if (!qfile.open(IO_WriteOnly))
        return false;

    QTextStream     stream(&qfile);
    long long       ii;
    QString         buf;

    for (ii = 0; ii < nframes; ii++)
        stream << buf.sprintf("%3u\t%3u\t%3u\n",
                minval[ii], avgval[ii], maxval[ii]);

    qfile.close();
    return true;
}

bool
isblank(unsigned char avgval,
        unsigned char maxavgblack, unsigned char maxavggrey,
        unsigned char maxval, unsigned char maxgrey)
{
    return avgval <= maxavgblack || avgval <= maxavggrey && maxval <= maxgrey;
}

long long
matchspn(long long nframes, unsigned char *match, long long frameno,
        unsigned char acceptval)
{
    while (frameno < nframes && match[frameno] == acceptval)
        frameno++;
    return frameno;
}

};  /* namespace */

BlankFrameDetector::BlankFrameDetector(PGMConverter *pgmc, BorderDetector *bd,
        TemplateFinder *tf, QString debugdir)
    : FrameAnalyzer()
    , pgmConverter(pgmc)
    , borderDetector(bd)
    , templateFinder(tf)
    , tmpl(NULL)
#ifdef PGM_CONVERT_GREYSCALE
    , maxavgblack(5)
    , maxavggrey(7)
    , maxgrey(24)
#else  /* !PGM_CONVERT_GREYSCALE */
    , maxavgblack(28)
    , maxavggrey(24)
    , maxgrey(36)
#endif /* !PGM_CONVERT_GREYSCALE */
    , minval(NULL)
    , avgval(NULL)
    , maxval(NULL)
    , iscontent(NULL)
    , debugLevel(0)
    , debugdir(debugdir)
#ifdef PGM_CONVERT_GREYSCALE
    , debugdata(debugdir + "/BlankFrameDetector-pgm.txt")
#else  /* !PGM_CONVERT_GREYSCALE */
    , debugdata(debugdir + "/BlankFrameDetector-yuv.txt")
#endif /* !PGM_CONVERT_GREYSCALE */
    , debug_blanks(false)
    , blanks_done(false)
{
    memset(&analyze_time, 0, sizeof(analyze_time));
    skipblanks = gContext->GetNumSetting("CommSkipAllBlanks", 1) != 0;

    VERBOSE(VB_COMMFLAG, QString("BlankFrameDetector:"
                " maxavgblack=%1 maxavggrey=%2 maxgrey=%3 skipblanks=%4")
            .arg(maxavgblack)
            .arg(maxavggrey)
            .arg(maxgrey)
            .arg(skipblanks ? "true" : "false"));

    /*
     * debugLevel:
     *      0: no debugging
     *      1: cache frame information into debugdir [1 file]
     *      2: extra verbosity [O(nframes)]
     */
    debugLevel = gContext->GetNumSetting("BlankFrameDetectorDebugLevel", 0);

    if (debugLevel >= 1)
        createDebugDirectory(debugdir,
            QString("BlankFrameDetector debugLevel %1").arg(debugLevel));

    if (debugLevel >= 1)
        debug_blanks = true;
}

BlankFrameDetector::~BlankFrameDetector(void)
{
    if (minval)
        delete []minval;
    if (avgval)
        delete []avgval;
    if (maxval)
        delete []maxval;
    if (iscontent)
        delete []iscontent;
}

enum FrameAnalyzer::analyzeFrameResult
BlankFrameDetector::nuppelVideoPlayerInited(NuppelVideoPlayer *nvp)
{
    nframes = nvp->GetTotalFrameCount();
    fps = nvp->GetFrameRate();

    VERBOSE(VB_COMMFLAG, QString(
                "BlankFrameDetector::nuppelVideoPlayerInited"));

    if (pgmConverter->nuppelVideoPlayerInited(nvp))
	    return ANALYZE_FATAL;

    /* Check for template. */
    if (templateFinder)
    {
        tmpl = templateFinder->getTemplate(&tmplrow, &tmplcol,
                &tmplwidth, &tmplheight);
    }

    minval = new unsigned char[nframes];
    avgval = new unsigned char[nframes];
    maxval = new unsigned char[nframes];
    iscontent = new unsigned char[nframes];

    if (debug_blanks)
    {
        if (readData(debugdata, minval, avgval, maxval, nframes))
        {
            VERBOSE(VB_COMMFLAG, QString(
                        "BlankFrameDetector::nuppelVideoPlayerInited read %1")
                    .arg(debugdata));
            blanks_done = true;
            return ANALYZE_FINISHED;
        }
    }
    return ANALYZE_OK;
}

enum FrameAnalyzer::analyzeFrameResult
BlankFrameDetector::analyzeFrame(const VideoFrame *frame, long long frameno,
        long long *pNextFrame)
{
    const AVPicture     *pgm;
    int                 pgmwidth, pgmheight;
    int                 croprow, cropcol, cropwidth, cropheight;
    int                 rr, cc;
    unsigned char       framemin, framemax;
    unsigned long long  framesum, npixels;
    struct timeval      start, end, elapsed;

    *pNextFrame = NEXTFRAME;

    if (!(pgm = pgmConverter->getImage(frame, frameno, &pgmwidth, &pgmheight)))
        goto error;

    (void)gettimeofday(&start, NULL);

    (void)borderDetector->getDimensions(pgm, pgmheight, frameno,
                &croprow, &cropcol, &cropwidth, &cropheight);

    if (cropheight && cropwidth)
    {
        framemin = UCHAR_MAX;
        framemax = 0;
        framesum = 0;
        for (rr = croprow; rr < croprow + cropheight; rr++)
        {
            for (cc = cropcol; cc < cropcol + cropwidth; cc++)
            {
                unsigned char   val;

                /*
                 * Exclude template area from blank-frame analysis so that its
                 * pixels don't mess up maxgrey/maxblack values.
                 */
                if (tmpl && rr >= tmplrow && rr < tmplrow + tmplheight &&
                        cc >= tmplcol && cc < tmplcol + tmplwidth)
                    continue;

                val = pgm->data[0][rr * pgmwidth + cc];
                if (framemin > val)
                    framemin = val;
                if (framemax < val)
                    framemax = val;
                framesum += val;
            }
        }
    }

    (void)gettimeofday(&end, NULL);
    timersub(&end, &start, &elapsed);
    timeradd(&analyze_time, &elapsed, &analyze_time);

    npixels = cropheight * cropwidth;
    if (tmpl)
        npixels -= tmplwidth * tmplheight;

    if (npixels > 0)
    {
        minval[frameno] = framemin;
        avgval[frameno] = framesum / npixels;
        maxval[frameno] = framemax;
    }

    return ANALYZE_OK;

error:
    VERBOSE(VB_COMMFLAG,
            QString("BlankFrameDetector::analyzeFrame error at frame %1")
            .arg(frameno));

    return ANALYZE_ERROR;
}

int
BlankFrameDetector::finished(void)
{
    /*
     * TUNABLE:
     *
     * Set a minimum duration length on sequences of blank frames.
     */
#ifdef PGM_CONVERT_GREYSCALE
    const int       MINBLACKLEN = (int)roundf(0.34 * fps);  /* frames */
    const int       MINGREYLEN = (int)roundf(0.50 * fps);   /* frames */
#else  /* !PGM_CONVERT_GREYSCALE */
    const int       MINBLACKLEN = (int)roundf(0.20 * fps);  /* frames */
    const int       MINGREYLEN = (int)roundf(0.26 * fps);   /* frames */
#endif /* !PGM_CONVERT_GREYSCALE */

    /*
     * TUNABLE:
     *
     * Minimum length of "content", to coalesce consecutive commercial breaks
     * (or to avoid too many false consecutive breaks).
     */
    const int       MINSEGLEN = (int)roundf(25 * fps);      /* frames */

    /*
     * TUNABLE:
     *
     * Common commercial-break lengths.
     */
    static const struct {
        int     len;
        int     delta;
    } breaklen[] = {
        { 270, 15 },
        { 240, 15 },
        { 210, 15 },
        { 180, 15 },
        { 150, 15 },
        { 120, 15 },
        { 105, 15 },
        {  90, 15 },
        {  75, 15 },
        {  60, 15 },
        {  45, 15 },
        {  30, 15 },
        {  15, 15 },
    };
    static const long long maxbreaktime = breaklen[0].len + breaklen[0].delta;

    long long                   segb, sege, seglen, segtime;
    long long                   blacklen, maxblacklen;
    QString                     analyzestr;
    QMap<long long, long long>  blankSegment, breakSegment; /* frameno => len */
    QMap<long long, long long>::Iterator bb, bb1, cc;

    if (pgmConverter->finished())
        return -1;

    if (!blanks_done && debug_blanks)
    {
        if (writeData(debugdata, minval, avgval, maxval, nframes))
        {
            VERBOSE(VB_COMMFLAG, QString(
                        "BlankFrameDetector::finished wrote %1")
                    .arg(debugdata));
            blanks_done = true;
        }
    }

    /*
     * Identify all sequences of blank frames (blankSegment).
     */
    if (isblank(avgval[0], maxavgblack, maxavggrey, maxval[0], maxgrey))
    {
        segb = 0;
        sege = 0;
        blacklen = avgval[0] <= maxavgblack ? 1 : 0;
        maxblacklen = blacklen;
    }
    else
    {
        /* Fake up a blank "segment" at frame 0. */
        blankSegment[0] = 1;
        segb = -1;
        sege = -1;
    }
    for (long long ii = 1; ii < nframes; ii++)
    {
        if (isblank(avgval[ii], maxavgblack, maxavggrey, maxval[ii], maxgrey))
        {
            /* Blank frame. */
            if (sege < ii - 1)
            {
                /* Start counting. */
                segb = ii;
                sege = ii;
            }
            else
            {
                /* Continue counting. */
                sege = ii;
            }

            /* Black frames (very dark) vs. grey frames (somewhat dark). */
            if (avgval[ii] <= maxavgblack)
            {
                blacklen++;
                if (blacklen > maxblacklen)
                    maxblacklen = blacklen;
            }
            else
            {
                blacklen = 0;
            }
        }
        else if (sege == ii - 1)
        {
            /*
             * Transition to non-blank frame; save sequence if it is long
             * enough.
             */
            seglen = ii - segb;
            if (maxblacklen >= MINBLACKLEN || seglen >= MINGREYLEN)
                blankSegment[segb] = seglen;
            blacklen = 0;
            maxblacklen = 0;
        }
    }

    /*
     * Report blanks.
     */
    if (debugLevel >= 2)
    {
        for (bb = blankSegment.begin(); bb != blankSegment.end(); ++bb)
        {
            segb = bb.key();
            seglen = bb.data();
            sege = segb + seglen - 1;

            VERBOSE(VB_COMMFLAG, QString("Blanks: frame %1-%2 (%3-%4, %5s)")
                    .arg(segb, 6).arg(sege, 6)
                    .arg(frameToTimestamp(segb, fps))
                    .arg(frameToTimestamp(sege, fps))
                    .arg(seglen / fps, 0, 'f', 3));
        }
    }

    /*
     * Compute breaks.
     */
    bb = blankSegment.begin();
    while (bb != blankSegment.end())
    {
next:
        segb = bb.key();
        if (segb > 0 && !skipblanks)
            segb += bb.data();      /* Exclude leading blanks. */

        /* Work backwards from end to test maximal candidate break segments. */
        cc = blankSegment.end();
        --cc;
        while (cc != bb)
        {
            sege = cc.key();
            if (sege + cc.data() == nframes - 1 || skipblanks)
                sege += cc.data();  /* Exclude trailing blanks. */

            seglen = sege - segb;                       /* frames */
            segtime = (long long)roundf(seglen / fps);  /* seconds */

            /*
             * Test if the segment length matches any of the candidate
             * commercial break lengths.
             */
            for (unsigned int ii = 0;
                    ii < sizeof(breaklen)/sizeof(*breaklen);
                    ii++)
            {
                if (segtime > breaklen[ii].len + breaklen[ii].delta)
                    break;
                if (abs(segtime - breaklen[ii].len) <= breaklen[ii].delta)
                {
                    breakSegment[segb] = seglen;
                    bb = cc;
                    goto next;
                }
            }

            --cc;
        }

        ++bb;
    }

    /*
     * Eliminate false breaks.
     */
    segb = 0;
    bb = breakSegment.begin();
    bb1 = breakSegment.end();
    while (bb != breakSegment.end())
    {
        sege = bb.key();
        seglen = sege - segb;
        if (seglen < MINSEGLEN && bb != breakSegment.begin())
        {
            /*
             * Too soon since last break ended. If aggregate break (previous
             * break + this break) is reasonably short, merge them together.
             * Otherwise, eliminate this break as a false break.
             */
            if (bb1 != breakSegment.end())
            {
                seglen = sege + bb.data() - bb1.key();
                segtime = (long long)roundf(seglen / fps);
                if (segtime <= maxbreaktime)
                    breakSegment[bb1.key()] = seglen;   /* Merge. */
            }
            cc = bb;
            ++bb;
            breakSegment.remove(cc);
            continue;
        }

        /* Normal break; save cursors and continue. */
        segb = sege + bb.data();
        bb1 = bb;
        ++bb;
    }

    /*
     * Report breaks.
     */
    bb = breakSegment.begin();
    while (bb != breakSegment.end())
    {
        segb = bb.key();
        sege = segb + bb.data();
        seglen = sege - segb;

        VERBOSE(VB_COMMFLAG, QString("BF Break: frame %1-%2 (%3-%4, %5)")
                .arg(segb, 6).arg(sege - 1, 6)
                .arg(frameToTimestamp(segb, fps))
                .arg(frameToTimestamp(sege - 1, fps))
                .arg(frameToTimestamp(seglen, fps)));
        ++bb;
    }

    /*
     * Initialize isContent state.
     */
    segb = 0;
    for (bb = breakSegment.begin(); bb != breakSegment.end(); ++bb)
    {
        sege = bb.key();
        for (long long ii = segb; ii < sege; ii++)
            iscontent[ii] = 1;
        segb = sege + bb.data();
    }
    for (long long ii = segb; ii < nframes; ii++)
        iscontent[ii] = 1;

    VERBOSE(VB_COMMFLAG, QString("BF Time: analyze=%1s")
            .arg(analyzestr.sprintf("%ld.%06ld",
                    analyze_time.tv_sec, analyze_time.tv_usec)));

    return 0;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
