#include <stdio.h>
#include <math.h>

#include "mythcontext.h"    /* gContext */
#include "NuppelVideoPlayer.h"

#include "CommDetector2.h"
#include "FrameAnalyzer.h"
#include "quickselect.h"
#include "HistogramAnalyzer.h"
#include "BlankFrameDetector.h"

using namespace commDetector2;
using namespace frameAnalyzer;

namespace {

struct threshold {
    float           maxmean;
    unsigned int    maxmedian;
    float           maxstddev;
    unsigned int    minblanklen;
};

bool
isBlank(float mean, unsigned char median, float stddev, unsigned int nthresh,
        const struct threshold *thresh, unsigned int *maxblanklen)
{
    bool found = false;

    for (unsigned int ii = 0; ii < nthresh; ii++)
    {
        if (mean <= thresh[ii].maxmean && median <= thresh[ii].maxmedian &&
                stddev <= thresh[ii].maxstddev)
        {
            maxblanklen[ii]++;
            found = true;
        }
    }

    return found;
}

void
computeBlankMap(FrameAnalyzer::FrameMap *blankMap, float fps, long long nframes,
        const unsigned char *blank, const float *mean,
        const unsigned char *median, const float *stddev)
{
    /* Blanks of these sequences are ORed together. */
    const struct threshold thresh[] = {
        /* Digital HDTV ("Bones" on FOX). */
        { 5.0,  5,  0.0,    (unsigned int)roundf(6 / 59.94 * fps) },

        /* SDTV broadcast over digital HDTV ("24" reruns on ABC). */
        { 2.0,  2,  0.26,   (unsigned int)roundf(6 / 59.94 * fps) },
    };
    const unsigned int nthresh = sizeof(thresh)/sizeof(*thresh);

    long long       frameno, segb, sege;
    unsigned int    maxblanklen[nthresh];

    blankMap->clear();
    memset(maxblanklen, 0, sizeof(maxblanklen));
    if (blank[0] && isBlank(mean[0], median[0], stddev[0], nthresh, thresh,
                maxblanklen))
    {
        segb = 0;
        sege = 0;
    }
    else
    {
        /* Fake up a dummy blank frame for interval calculations. */
        (*blankMap)[-1] = 0;
        segb = -1;
        sege = -1;
    }
    for (frameno = 1; frameno < nframes; frameno++)
    {
        if (blank[frameno] && isBlank(mean[frameno], median[frameno],
                    stddev[frameno], nthresh, thresh, maxblanklen))
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
            for (unsigned int ii = 0; ii < nthresh; ii++)
            {
                if (maxblanklen[ii] >= thresh[ii].minblanklen)
                {
                    (*blankMap)[segb] = seglen;
                    break;
                }
            }
            memset(maxblanklen, 0, sizeof(maxblanklen));
        }
    }
    if (sege == frameno - 1)
    {
        /* Possibly ending on blank frames. */
        long long seglen = frameno - segb;
        for (unsigned int ii = 0; ii < nthresh; ii++)
        {
            if (maxblanklen[ii] >= thresh[ii].minblanklen)
            {
                (*blankMap)[segb] = seglen;
                break;
            }
        }
        memset(maxblanklen, 0, sizeof(maxblanklen));
    }

    FrameAnalyzer::FrameMap::Iterator iiblank = blankMap->end();
    --iiblank;
    if (iiblank.key() + iiblank.data() < nframes)
    {
        /*
         * Didn't end on blank frames, so add a dummy blank frame at the
         * end.
         */
        (*blankMap)[nframes] = 0;
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
#ifdef LATER
    static const long long maxbreaktime = breaklen[0].len + breaklen[0].delta;
#endif /* LATER */

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
        long long brkb = iiblank.key();
        brkb = max(0LL, min(nframes - 1, brkb));  /* Dummy frames. */
        if (brkb > 0 && !skipblanks)
            brkb += iiblank.data();             /* Exclude leading blanks. */

        if (!breakMap->empty())
        {
            FrameAnalyzer::FrameMap::const_iterator iibreak = breakMap->end();
            --iibreak;
            long long lastb = iibreak.key();
            long long laste = lastb + iibreak.data();
            long long seglen = brkb - laste;
            if (seglen < MINSEGLEN)
            {
                ++iiblank;
                continue;
            }
        }

        /* Work backwards from end to test maximal candidate break brkments. */
        FrameAnalyzer::FrameMap::const_iterator jjblank = blankMap->end();
        --jjblank;
        while (jjblank != iiblank)
        {
            long long brke = jjblank.key();
            if (brke + jjblank.data() == nframes - 1 || skipblanks)
                brke += jjblank.data();  /* Exclude trailing blanks. */

            long long brklen = brke - brkb;                      /* frames */
            long long brktime = (long long)roundf(brklen / fps); /* seconds */

            /*
             * Test if the brkment length matches any of the candidate
             * commercial break lengths.
             */
            for (unsigned int ii = 0;
                    ii < sizeof(breaklen)/sizeof(*breaklen);
                    ii++)
            {
                if (brktime > breaklen[ii].len + breaklen[ii].delta)
                    break;
                if (abs(brktime - breaklen[ii].len) <= breaklen[ii].delta)
                {
                    (*breakMap)[brkb] = brklen;
                    iiblank = jjblank;
                    goto next;
                }
            }
            --jjblank;
        }
        ++iiblank;
    }

#ifdef LATER
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
#endif /* LATER */
}

};  /* namespace */

BlankFrameDetector::BlankFrameDetector(HistogramAnalyzer *ha, QString debugdir)
    : FrameAnalyzer()
    , histogramAnalyzer(ha)
    , debugLevel(0)
{
    skipblanks = gContext->GetNumSetting("CommSkipAllBlanks", 1) != 0;

    VERBOSE(VB_COMMFLAG, QString("BlankFrameDetector: skipblanks=%1")
            .arg(skipblanks ? "true" : "false"));

    /*
     * debugLevel:
     *      0: no debugging
     *      2: extra verbosity [O(nframes)]
     */
    debugLevel = gContext->GetNumSetting("BlankFrameDetectorDebugLevel", 0);

    if (debugLevel >= 1)
        createDebugDirectory(debugdir,
            QString("BlankFrameDetector debugLevel %1").arg(debugLevel));
}

BlankFrameDetector::~BlankFrameDetector(void)
{
}

enum FrameAnalyzer::analyzeFrameResult
BlankFrameDetector::nuppelVideoPlayerInited(NuppelVideoPlayer *nvp)
{
    FrameAnalyzer::analyzeFrameResult ares =
        histogramAnalyzer->nuppelVideoPlayerInited(nvp);

    nframes = nvp->GetTotalFrameCount();
    fps = nvp->GetFrameRate();
    npixels = nvp->GetVideoWidth() * nvp->GetVideoHeight();

    VERBOSE(VB_COMMFLAG, QString(
                "BlankFrameDetector::nuppelVideoPlayerInited %1x%2")
            .arg(nvp->GetVideoWidth())
            .arg(nvp->GetVideoHeight()));

    return ares;
}

enum FrameAnalyzer::analyzeFrameResult
BlankFrameDetector::analyzeFrame(const VideoFrame *frame, long long frameno,
        long long *pNextFrame)
{
    *pNextFrame = NEXTFRAME;

    if (histogramAnalyzer->analyzeFrame(frame, frameno) ==
            FrameAnalyzer::ANALYZE_OK)
        return ANALYZE_OK;

    VERBOSE(VB_COMMFLAG,
            QString("BlankFrameDetector::analyzeFrame error at frame %1")
            .arg(frameno));
    return ANALYZE_ERROR;
}

int
BlankFrameDetector::finished(void)
{
    if (histogramAnalyzer->finished())
        return -1;

    /*
     * Identify all sequences of blank frames (blankMap).
     */
    computeBlankMap(&blankMap, fps, nframes, histogramAnalyzer->getBlanks(),
            histogramAnalyzer->getMeans(), histogramAnalyzer->getMedians(),
            histogramAnalyzer->getStdDevs());
    if (debugLevel >= 2)
        frameAnalyzerReportMapms(&blankMap, fps, "BF Blank");

    /*
     * Compute breaks (breakMap).
     */
    computeBreakMap(&breakMap, &blankMap, fps, nframes, skipblanks);
    frameAnalyzerReportMap(&breakMap, fps, "BF Break");

    return 0;
}

int
BlankFrameDetector::computeBreaks(FrameAnalyzer::FrameMap *breaks)
{
    breaks->clear();
    for (FrameAnalyzer::FrameMap::Iterator bb = breakMap.begin();
            bb != breakMap.end();
            ++bb)
    {
        (*breaks)[bb.key()] = bb.data();
    }
    return 0;
}

int
BlankFrameDetector::reportTime(void) const
{
    return histogramAnalyzer->reportTime();
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
