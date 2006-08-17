#include <stdio.h>
#include <math.h>

#include "mythcontext.h"    /* gContext */
#include "NuppelVideoPlayer.h"

#include "CommDetector2.h"
#include "FrameAnalyzer.h"
#include "PGMConverter.h"
#include "BorderDetector.h"
#include "HistogramAnalyzer.h"

using namespace commDetector2;
using namespace frameAnalyzer;

namespace {

bool
readData(QString filename, unsigned long long *histval, long long nframes)
{
    FILE            *fp;
    long long       frameno;

    if (!(fp = fopen(filename, "r")))
        return false;

    for (frameno = 0; frameno < nframes; frameno++)
    {
        int nitems = fscanf(fp, "%llu", &histval[frameno]);
        if (nitems != 1)
        {
            VERBOSE(VB_COMMFLAG, QString(
                        "Not enough data in %1: frame %2")
                    .arg(filename).arg(frameno));
            goto error;
        }
    }
    if (fclose(fp))
        VERBOSE(VB_COMMFLAG, QString("Error closing %1: %2")
                .arg(filename).arg(strerror(errno)));
    return true;

error:
    if (fclose(fp))
        VERBOSE(VB_COMMFLAG, QString("Error closing %1: %2")
                .arg(filename).arg(strerror(errno)));
    return false;
}

bool
writeData(QString filename, unsigned long long *histval, long long nframes)
{
    FILE            *fp;
    long long       frameno;

    if (!(fp = fopen(filename, "w")))
        return false;

    for (frameno = 0; frameno < nframes; frameno++)
        (void)fprintf(fp, "%24llu\n", histval[frameno]);
    if (fclose(fp))
        VERBOSE(VB_COMMFLAG, QString("Error closing %1: %2")
                .arg(filename).arg(strerror(errno)));
    return true;
}

#ifdef LATER
unsigned long long
histogramValue(const HistogramAnalyzer::Histogram *hist)
{
    /*
     * Type needs to be big enough to hold maximum value:
     * HISTOGRAM_MAXCOLOR * HISTOGRAM_MAXCOLOR * 1980 x 1080
     */
    unsigned long long  histval = 0;
    for (unsigned int color = 0;
            color <= HistogramAnalyzer::HISTOGRAM_MAXCOLOR;
            color++)
        histval += color * (*hist)[color];
    return histval;
}
#endif /* LATER */

bool
isBlank(unsigned long long histval, unsigned int maxblack)
{
    return histval <= maxblack;
}

void
computeBlankMap(FrameAnalyzer::FrameMap *blankMap, float fps,
        long long nframes, unsigned long long *histval,
        unsigned int maxblackval)
{
    const int   MINBLACKLEN = (int)roundf(0.34 * fps);

    long long   segb, sege;

    blankMap->clear();

    if (isBlank(histval[0], maxblackval))
    {
        segb = 0;
        sege = 0;
    }
    else
    {
        /* Fake up a dummy blank "segment" at frame 0. */
        (*blankMap)[0] = 1;
        segb = -1;
        sege = -1;
    }
    for (long long frameno = 1; frameno < nframes; frameno++)
    {
        if (isBlank(histval[frameno], maxblackval))
        {
            /* Blank frame. */
            if (sege < frameno - 1)
            {
                /* Start counting. */
                segb = frameno;
                sege = frameno;
            }
            else
            {
                /* Continue counting. */
                sege = frameno;
            }
        }
        else if (sege == frameno - 1)
        {
            /* Transition to non-blank frame. */
            long long seglen = frameno - segb;
            if (seglen >= MINBLACKLEN)
                (*blankMap)[segb] = seglen;
        }
    }
}

void
computeBreakMap(FrameAnalyzer::FrameMap *breakMap,
        const FrameAnalyzer::FrameMap *blankMap, float fps, long long nframes,
        bool skipblanks)
{
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

    /*
     * TUNABLE:
     *
     * Minimum length of "content", to coalesce consecutive commercial breaks
     * (or to avoid too many false consecutive breaks).
     */
    const int   MINSEGLEN = (int)roundf(25 * fps);

    breakMap->clear();
    FrameAnalyzer::FrameMap::const_iterator iiblank = blankMap->begin();
    while (iiblank != blankMap->end())
    {
next:
        long long segb = iiblank.key();
        if (segb > 0 && !skipblanks)
            segb += iiblank.data();      /* Exclude leading blanks. */

        /* Work backwards from end to test maximal candidate break segments. */
        FrameAnalyzer::FrameMap::const_iterator jjblank = blankMap->end();
        --jjblank;
        while (jjblank != iiblank)
        {
            long long sege = jjblank.key();
            if (sege + jjblank.data() == nframes - 1 || skipblanks)
                sege += jjblank.data();  /* Exclude trailing blanks. */

            long long seglen = sege - segb;                      /* frames */
            long long segtime = (long long)roundf(seglen / fps); /* seconds */

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
                    (*breakMap)[segb] = seglen;
                    iiblank = jjblank;
                    goto next;
                }
            }
            --jjblank;
        }
        ++iiblank;
    }

    /*
     * Eliminate false breaks.
     */
    long long segb = 0;
    FrameAnalyzer::FrameMap::Iterator iibreak = breakMap->begin();
    FrameAnalyzer::FrameMap::Iterator iibreak0 = breakMap->end();
    while (iibreak != breakMap->end())
    {
        long long sege = iibreak.key();
        long long seglen = sege - segb;
        if (seglen < MINSEGLEN && iibreak != breakMap->begin())
        {
            /*
             * Too soon since last break ended. If aggregate break (previous
             * break + this break) is reasonably short, merge them together.
             * Otherwise, eliminate this break as a false break.
             */
            if (iibreak0 != breakMap->end())
            {
                long long seglen = sege + iibreak.data() - iibreak0.key();
                long long segtime = (long long)roundf(seglen / fps);
                if (segtime <= maxbreaktime)
                    (*breakMap)[iibreak0.key()] = seglen;   /* Merge. */
            }
            FrameAnalyzer::FrameMap::Iterator jjbreak = iibreak;
            ++iibreak;
            breakMap->remove(jjbreak);
            continue;
        }

        /* Normal break; save cursors and continue. */
        segb = sege + iibreak.data();
        iibreak0 = iibreak;
        ++iibreak;
    }
}

};  /* namespace */

HistogramAnalyzer::HistogramAnalyzer(PGMConverter *pgmc, BorderDetector *bd,
        QString debugdir)
    : FrameAnalyzer()
    , pgmConverter(pgmc)
    , borderDetector(bd)
    , histval(NULL)
    , debugLevel(0)
    , debugdir(debugdir)
#ifdef PGM_CONVERT_GREYSCALE
    , debugdata(debugdir + "/HistogramAnalyzer-pgm.txt")
#else  /* !PGM_CONVERT_GREYSCALE */
    , debugdata(debugdir + "/HistogramAnalyzer-yuv.txt")
#endif /* !PGM_CONVERT_GREYSCALE */
    , debug_histval(false)
    , histval_done(false)
{
    memset(&analyze_time, 0, sizeof(analyze_time));
    skipblanks = gContext->GetNumSetting("CommSkipAllBlanks", 1) != 0;

    VERBOSE(VB_COMMFLAG, QString("HistogramAnalyzer: skipblanks=%1")
            .arg(skipblanks ? "true" : "false"));

    /*
     * debugLevel:
     *      0: no debugging
     *      1: cache frame information into debugdir [1 file]
     *      2: extra verbosity [O(nframes)]
     */
    debugLevel = gContext->GetNumSetting("HistogramAnalyzerDebugLevel", 0);

    if (debugLevel >= 1)
        createDebugDirectory(debugdir,
            QString("HistogramAnalyzer debugLevel %1").arg(debugLevel));

    if (debugLevel >= 1)
        debug_histval = true;
}

HistogramAnalyzer::~HistogramAnalyzer(void)
{
    if (histval)
        delete []histval;
    if (iscontent)
        delete []iscontent;
}

enum FrameAnalyzer::analyzeFrameResult
HistogramAnalyzer::nuppelVideoPlayerInited(NuppelVideoPlayer *nvp)
{
    nframes = nvp->GetTotalFrameCount();
    fps = nvp->GetFrameRate();
    npixels = nvp->GetVideoWidth() * nvp->GetVideoHeight();

    VERBOSE(VB_COMMFLAG, QString(
                "HistogramAnalyzer::nuppelVideoPlayerInited %1x%2")
            .arg(nvp->GetVideoWidth())
            .arg(nvp->GetVideoHeight()));

    if (pgmConverter->nuppelVideoPlayerInited(nvp))
	    return ANALYZE_FATAL;

    if (borderDetector->nuppelVideoPlayerInited(nvp))
	    return ANALYZE_FATAL;

    histval = new unsigned long long[nframes];
    memset(histval, 0, nframes * sizeof(*histval));

    iscontent = new unsigned char[nframes];
    memset(iscontent, 0, nframes * sizeof(*iscontent));

    if (debug_histval)
    {
        if (readData(debugdata, histval, nframes))
        {
            VERBOSE(VB_COMMFLAG, QString(
                        "HistogramAnalyzer::nuppelVideoPlayerInited read %1")
                    .arg(debugdata));
            histval_done = true;
            return ANALYZE_FINISHED;
        }
    }
    return ANALYZE_OK;
}

enum FrameAnalyzer::analyzeFrameResult
HistogramAnalyzer::analyzeFrame(const VideoFrame *frame, long long frameno,
        long long *pNextFrame)
{
#ifdef LATER
    /*
     * TUNABLE:
     *
     * Default color value to assign to margin areas of the screen.
     */
    static const unsigned char  DEFAULT = 0;
#endif /* LATER */

    /*
     * TUNABLE:
     *
     * Coverage of each frame. Higher values will allow analysis to proceed
     * faster, but might be less accurate. Lower values will examine more
     * pixels, but will run slower.
     */
    static const int    RINC = 4;
    static const int    CINC = 4;

    const AVPicture     *pgm;
    int                 pgmwidth, pgmheight;
    int                 croprow, cropcol, cropwidth, cropheight;
    int                 rr, cc;
    struct timeval      start, end, elapsed;

    *pNextFrame = NEXTFRAME;

    if (!(pgm = pgmConverter->getImage(frame, frameno, &pgmwidth, &pgmheight)))
        goto error;

    (void)borderDetector->getDimensions(pgm, pgmheight, frameno,
                &croprow, &cropcol, &cropwidth, &cropheight);

    (void)gettimeofday(&start, NULL);

#ifdef LATER

    /* Add margin pixels as default pixel color (black). */
    memset(&histogram, 0, sizeof(histogram));
    histogram[DEFAULT] =
        ((pgmheight - cropheight) / RINC + 1) * (pgmwidth / CINC + 1) +
        ((pgmwidth - cropwidth) / CINC + 1) * (cropheight / RINC + 1);

    if (cropheight && cropwidth)
    {
        for (rr = croprow; rr < croprow + cropheight; rr += RINC)
        {
            for (cc = cropcol; cc < cropcol + cropwidth; cc += CINC)
            {
                unsigned char val = pgm->data[0][rr * pgmwidth + cc];
                histogram[val]++;
            }
        }
    }

    histval[frameno] = histogramValue(&histogram);

#else  /* !LATER */

    histval[frameno] = 0;
    if (cropheight && cropwidth)
    {
        for (rr = croprow; rr < croprow + cropheight; rr += RINC)
        {
            for (cc = cropcol; cc < cropcol + cropwidth; cc += CINC)
            {
                unsigned char val = pgm->data[0][rr * pgmwidth + cc];
                if (val)
                    histval[frameno] += val;
            }
        }
    }

#endif /* !LATER */

    (void)gettimeofday(&end, NULL);
    timersub(&end, &start, &elapsed);
    timeradd(&analyze_time, &elapsed, &analyze_time);

    return ANALYZE_OK;

error:
    VERBOSE(VB_COMMFLAG,
            QString("HistogramAnalyzer::analyzeFrame error at frame %1")
            .arg(frameno));

    return ANALYZE_ERROR;
}

int
HistogramAnalyzer::finished(void)
{
    /*
     * TUNABLE:
     *
     * Maximum histogram value for a frame to be considered a blank frame. This
     * is equivalent to expressing this value as some average greyscale value
     * in [0..255] and averaging out all histogram values. This way we get
     * finer precision, as well as not having to perform all the unnecessary
     * averaging calculations.
     */
    const unsigned long long maxhistval = npixels * HISTOGRAM_MAXCOLOR;
    const unsigned int MAXBLACKVAL = (unsigned int)(0.002 * maxhistval);

    if (!histval_done && debug_histval)
    {
        if (writeData(debugdata, histval, nframes))
        {
            VERBOSE(VB_COMMFLAG, QString(
                        "HistogramAnalyzer::finished wrote %1")
                    .arg(debugdata));
            histval_done = true;
        }
    }

#if 0
    /*
     * Report histogram summaries with some context.
     */
    if (debugLevel >= 2)
    {
        for (long long frameno = 0; frameno < nframes; frameno++)
        {
            bool isblank = false;

            for (long long ii = frameno - 10; ii < frameno + 10; ii++)
            {
                if (isBlank(histval[ii], MAXBLACKVAL))
                {
                    isblank = true;
                    break;
                }
            }
            if (isblank)
                VERBOSE(VB_COMMFLAG, QString("HA Frame %1 (%2): %3 (%4)")
                        .arg(frameno, 6)
                        .arg(frameToTimestamp(frameno, fps))
                        .arg(histval[frameno], 8)
                        .arg((float)histval[frameno] / maxhistval, 8, 'f', 6));
        }
    }
#endif

    /*
     * Identify all sequences of blank frames (blankMap).
     */
    computeBlankMap(&blankMap, fps, nframes, histval, MAXBLACKVAL);
    if (debugLevel >= 2)
        frameAnalyzerReportMapms(&blankMap, fps, "HA Blank");

    /*
     * Compute breaks (breakMap).
     */
    computeBreakMap(&breakMap, &blankMap, fps, nframes, skipblanks);
    frameAnalyzerReportMap(&breakMap, fps, "HA Break");

    /*
     * Initialize isContent state.
     */
    long long segb = 0;
    for (FrameAnalyzer::FrameMap::const_iterator bb = breakMap.begin();
            bb != breakMap.end();
            ++bb)
    {
        long long sege = bb.key();
        for (long long ii = segb; ii < sege; ii++)
            iscontent[ii] = 1;
        segb = sege + bb.data();
    }
    for (long long ii = segb; ii < nframes; ii++)
        iscontent[ii] = 1;

    return 0;
}

int
HistogramAnalyzer::reportTime(void) const
{
    if (pgmConverter->reportTime())
        return -1;

    if (borderDetector->reportTime())
        return -1;

    VERBOSE(VB_COMMFLAG, QString("HA Time: analyze=%1s")
            .arg(strftimeval(&analyze_time)));
    return 0;
}

void
HistogramAnalyzer::setTemplateState(TemplateFinder *tf)
{
    templateFinder = tf;
}

void
HistogramAnalyzer::clear(void)
{
    VERBOSE(VB_COMMFLAG, "HistogramAnalyzer::clear");
    breakMap.clear();
    memset(iscontent, 0, nframes * sizeof(*iscontent));
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
