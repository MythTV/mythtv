// ANSI C headers
#include <cstdlib>
#include <cmath>

// MythTV headers
#include "mythcorecontext.h"    /* gContext */
#include "decodeencode.h"           /* for absLongLong */
#include "NuppelVideoPlayer.h"

// Commercial Flagging headers
#include "CommDetector2.h"
#include "FrameAnalyzer.h"
#include "quickselect.h"
#include "HistogramAnalyzer.h"
#include "BlankFrameDetector.h"
#include "TemplateMatcher.h"

using namespace commDetector2;
using namespace frameAnalyzer;

namespace {

bool
isBlank(unsigned char median, float stddev, unsigned char maxmedian,
        float maxstddev)
{
    return ((median < maxmedian) ||
            ((median == maxmedian) && (stddev <= maxstddev)));
}

int
sort_ascending_uchar(const void *aa, const void *bb)
{
    return *(unsigned char*)aa - *(unsigned char*)bb;
}

int
sort_ascending_float(const void *aa, const void *bb)
{
    float faa = *(float*)aa;
    float fbb = *(float*)bb;
    return faa < fbb ? -1 : faa == fbb ? 0 : 1;
}

bool
pickmedian(const unsigned char medianval,
        unsigned char minval, unsigned char maxval)
{
    return medianval >= minval && medianval <= maxval;
}

void
computeBlankMap(FrameAnalyzer::FrameMap *blankMap, long long nframes,
        const unsigned char *median, const float *stddev,
        const unsigned char *monochromatic)
{
    /*
     * Select a "black" value based on a curve, to deal with varying "black"
     * levels.
     */
    const unsigned char     MINBLANKMEDIAN = 1;
    const unsigned char     MAXBLANKMEDIAN = 96;
    const float             MEDIANPCTILE = 0.95;
    const float             STDDEVPCTILE = 0.85;

    long long       frameno, segb, sege, nblanks;
    long long       blankno, blankno1, blankno2;
    long long       stddevno, stddevno1, stddevno2;
    unsigned char   *blankmedian, maxmedian;
    float           *blankstddev, maxstddev;

    /* Count and select for monochromatic frames. */

    nblanks = 0;
    for (frameno = 0; frameno < nframes; frameno++)
    {
        if (monochromatic[frameno] && pickmedian(median[frameno],
                    MINBLANKMEDIAN, MAXBLANKMEDIAN))
            nblanks++;
    }

    if (!nblanks)
    {
        /* No monochromatic frames. */
        VERBOSE(VB_COMMFLAG,
                "BlankFrameDetector::computeBlankMap: No blank frames.");
        return;
    }

    /* Select percentile values from monochromatic frames. */

    blankmedian = new unsigned char[nblanks];
    blankstddev = new float[nblanks];
    blankno = 0;
    for (frameno = 0; frameno < nframes; frameno++)
    {
        if (monochromatic[frameno] && pickmedian(median[frameno],
                    MINBLANKMEDIAN, MAXBLANKMEDIAN))
        {
            blankmedian[blankno] = median[frameno];
            blankstddev[blankno] = stddev[frameno];
            blankno++;
        }
    }

    qsort(blankmedian, nblanks, sizeof(*blankmedian), sort_ascending_uchar);
    blankno = min(nblanks - 1, (long long)roundf(nblanks * MEDIANPCTILE));
    maxmedian = blankmedian[blankno];

    qsort(blankstddev, nblanks, sizeof(*blankstddev), sort_ascending_float);
    stddevno = min(nblanks - 1, (long long)roundf(nblanks * STDDEVPCTILE));
    maxstddev = blankstddev[stddevno];

    /* Determine effective percentile ranges (for debugging). */

    blankno1 = blankno;
    blankno2 = blankno;
    while (blankno1 > 0 && blankmedian[blankno1] == maxmedian)
        blankno1--;
    if (blankmedian[blankno1] != maxmedian)
        blankno1++;
    while (blankno2 < nblanks && blankmedian[blankno2] == maxmedian)
        blankno2++;
    if (blankno2 == nblanks)
        blankno2--;

    stddevno1 = stddevno;
    stddevno2 = stddevno;
    while (stddevno1 > 0 && blankstddev[stddevno1] == maxstddev)
        stddevno1--;
    if (blankstddev[stddevno1] != maxstddev)
        stddevno1++;
    while (stddevno2 < nblanks && blankstddev[stddevno2] == maxstddev)
        stddevno2++;
    if (stddevno2 == nblanks)
        stddevno2--;

    VERBOSE(VB_COMMFLAG, QString("Blanks selecting"
                " median<=%1 (%2-%3%), stddev<=%4 (%5-%6%)")
            .arg(maxmedian)
            .arg(blankno1 * 100 / nblanks).arg(blankno2 * 100 / nblanks)
            .arg(maxstddev)
            .arg(stddevno1 * 100 / nblanks).arg(stddevno2 * 100 / nblanks));

    delete []blankmedian;
    delete []blankstddev;

    blankMap->clear();
    if (monochromatic[0] && isBlank(median[0], stddev[0], maxmedian, maxstddev))
    {
        segb = 0;
        sege = 0;
    }
    else
    {
        /* Fake up a dummy blank frame for interval calculations. */
        blankMap->insert(0, 0);
        segb = -1;
        sege = -1;
    }
    for (frameno = 1; frameno < nframes; frameno++)
    {
        if (monochromatic[frameno] && isBlank(median[frameno], stddev[frameno],
                    maxmedian, maxstddev))
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
            blankMap->insert(segb, seglen);
        }
    }
    if (sege == frameno - 1)
    {
        /* Possibly ending on blank frames. */
        long long seglen = frameno - segb;
        blankMap->insert(segb, seglen);
    }

    FrameAnalyzer::FrameMap::Iterator iiblank = blankMap->end();
    --iiblank;
    if (iiblank.key() + *iiblank < nframes)
    {
        /*
         * Didn't end on blank frames, so add a dummy blank frame at the
         * end.
         */
        blankMap->insert(nframes - 1, 0);
    }
}

void
computeBreakMap(FrameAnalyzer::FrameMap *breakMap,
        const FrameAnalyzer::FrameMap *blankMap, float fps, bool skipcommblanks,
        int debugLevel)
{
    /*
     * TUNABLE:
     *
     * Common commercial-break lengths.
     */
    static const struct {
        int     len;    /* seconds */
        int     delta;  /* seconds */
    } breaktype[] = {
        /* Sort by "len". */
        { 15,   2 },
        { 20,   2 },
        { 30,   5 },
        { 60,   5 },
    };
    static const unsigned int   nbreaktypes =
        sizeof(breaktype)/sizeof(*breaktype);

    /*
     * TUNABLE:
     *
     * Shortest non-commercial length, used to coalesce consecutive commercial
     * breaks that are usually identified due to in-commercial cuts.
     */
    static const int MINCONTENTLEN = (int)roundf(10 * fps);

    breakMap->clear();
    for (FrameAnalyzer::FrameMap::const_iterator iiblank = blankMap->begin();
            iiblank != blankMap->end();
            ++iiblank)
    {
        long long brkb = iiblank.key();
        long long iilen = *iiblank;
        long long start = brkb + iilen / 2;

        for (unsigned int ii = 0; ii < nbreaktypes; ii++)
        {
            /* Look for next blank frame that is an acceptable distance away. */
            FrameAnalyzer::FrameMap::const_iterator jjblank = iiblank;
            for (++jjblank; jjblank != blankMap->end(); ++jjblank)
            {
                long long brke = jjblank.key();
                long long jjlen = *jjblank;
                long long end = brke + jjlen / 2;

                long long testlen = (long long)roundf((end - start) / fps);
                if (testlen > breaktype[ii].len + breaktype[ii].delta)
                    break;      /* Too far ahead; break to next break length. */
                if (absLongLong(testlen - breaktype[ii].len)
                        > breaktype[ii].delta)
                    continue;   /* Outside delta range; try next end-blank. */

                /* Mark this commercial break. */
                bool inserted = false;
                for (unsigned int jj = 0;; jj++)
                {
                    long long newbrkb = brkb + jj;
                    if (newbrkb >= brke)
                    {
                        VERBOSE(VB_COMMFLAG,
                            QString("BF [%1,%2] ran out of slots")
                                .arg(brkb).arg(brke - 1));
                        break;
                    }
                    if (breakMap->find(newbrkb) == breakMap->end())
                    {
                        breakMap->insert(newbrkb, brke - newbrkb);
                        inserted = true;
                        break;
                    }
                }
                if (inserted)
                    break;  /* next break type */
            }
        }
    }

    if (debugLevel >= 1)
    {
        frameAnalyzerReportMap(breakMap, fps, "BF Break");
        VERBOSE(VB_COMMFLAG, "BF coalescing overlapping/nearby breaks ...");
    }

    /*
     * Coalesce overlapping or very-nearby breaks (handles cut-scenes within a
     * commercial).
     */
    for (;;)
    {
        bool coalesced = false;
        FrameAnalyzer::FrameMap::iterator iibreak = breakMap->begin();
        while (iibreak != breakMap->end())
        {
            long long iib = iibreak.key();
            long long iie = iib + *iibreak;

            FrameAnalyzer::FrameMap::iterator jjbreak = iibreak;
            ++jjbreak;
            if (jjbreak == breakMap->end())
                break;

            long long jjb = jjbreak.key();
            long long jje = jjb + *jjbreak;

            if (jjb < iib)
            {
                /* (jjb,jje) is behind (iib,iie). */
                ++iibreak;
                continue;
            }

            if (iie + MINCONTENTLEN < jjb)
            {
                /* (jjb,jje) is too far ahead. */
                ++iibreak;
                continue;
            }

            /* Coalesce. */
            if (jje > iie)
            {
                breakMap->remove(iib);             /* overlap */
                breakMap->insert(iib, jje - iib);  /* overlap */
            }
            breakMap->erase(jjbreak);
            coalesced = true;
            iibreak = breakMap->find(iib);
        }
        if (!coalesced)
            break;
    }

    /* Adjust for skipcommblanks configuration. */
    FrameAnalyzer::FrameMap::iterator iibreak = breakMap->begin();
    while (iibreak != breakMap->end())
    {
        long long iib = iibreak.key();
        long long iie = iib + *iibreak;

        if (!skipcommblanks)
        {
            /* Trim leading blanks from commercial break. */
            FrameAnalyzer::FrameMap::const_iterator iiblank =
                blankMap->find(iib);
            FrameAnalyzer::FrameMap::iterator jjbreak = iibreak;
            ++jjbreak;
            iib += *iiblank;
            breakMap->erase(iibreak);
            breakMap->insert(iib, iie - iib);
            iibreak = jjbreak;
        }
        else
        {
            /* Add trailing blanks to commercial break. */
            ++iibreak;
            FrameAnalyzer::FrameMap::const_iterator jjblank =
                blankMap->find(iie);
            iie += *jjblank;
            breakMap->remove(iib);
            breakMap->insert(iib, iie - iib);
        }
    }
}

};  /* namespace */

BlankFrameDetector::BlankFrameDetector(HistogramAnalyzer *ha, QString debugdir)
    : FrameAnalyzer()
    , histogramAnalyzer(ha)
    , fps(0.0f)
    , debugLevel(0)
{
    skipcommblanks = gCoreContext->GetNumSetting("CommSkipAllBlanks", 1) != 0;

    VERBOSE(VB_COMMFLAG, QString("BlankFrameDetector: skipcommblanks=%1")
            .arg(skipcommblanks ? "true" : "false"));

    /*
     * debugLevel:
     *      0: no debugging
     *      2: extra verbosity [O(nframes)]
     */
    debugLevel = gCoreContext->GetNumSetting("BlankFrameDetectorDebugLevel", 0);

    if (debugLevel >= 1)
        createDebugDirectory(debugdir,
            QString("BlankFrameDetector debugLevel %1").arg(debugLevel));
}

enum FrameAnalyzer::analyzeFrameResult
BlankFrameDetector::nuppelVideoPlayerInited(NuppelVideoPlayer *nvp,
        long long nframes)
{
    FrameAnalyzer::analyzeFrameResult ares =
        histogramAnalyzer->nuppelVideoPlayerInited(nvp, nframes);

    fps = nvp->GetFrameRate();

    QSize video_disp_dim = nvp->GetVideoSize();

    VERBOSE(VB_COMMFLAG, QString(
                "BlankFrameDetector::nuppelVideoPlayerInited %1x%2")
            .arg(video_disp_dim.width())
            .arg(video_disp_dim.height()));

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
BlankFrameDetector::finished(long long nframes, bool final)
{
    if (histogramAnalyzer->finished(nframes, final))
        return -1;

    VERBOSE(VB_COMMFLAG, QString("BlankFrameDetector::finished(%1)")
            .arg(nframes));

    /* Identify all sequences of blank frames (blankMap). */
    computeBlankMap(&blankMap, nframes,
            histogramAnalyzer->getMedians(), histogramAnalyzer->getStdDevs(),
            histogramAnalyzer->getMonochromatics());
    if (debugLevel >= 2)
        frameAnalyzerReportMapms(&blankMap, fps, "BF Blank");

    return 0;
}

int
BlankFrameDetector::computeForLogoSurplus(
        const TemplateMatcher *templateMatcher)
{
    /*
     * See TemplateMatcher::templateCoverage; some commercial breaks have
     * logos. Conversely, any logo breaks are probably really breaks, so prefer
     * those over blank-frame-calculated breaks.
     */
    const FrameAnalyzer::FrameMap *logoBreakMap = templateMatcher->getBreaks();

    /* TUNABLE: see TemplateMatcher::adjustForBlanks */
    const int       MAXBLANKADJUSTMENT = (int)roundf(5 * fps);  /* frames */

    VERBOSE(VB_COMMFLAG, "BlankFrameDetector adjusting for logo surplus");

    /*
     * For each logo break, find the blank frames closest to its beginning and
     * end. This helps properly support CommSkipAllBlanks.
     */
    for (FrameAnalyzer::FrameMap::const_iterator ii =
                logoBreakMap->constBegin();
            ii != logoBreakMap->constEnd();
            ++ii)
    {
        /* Get bounds of beginning of logo break. */
        long long iikey = ii.key();
        long long iibb = iikey - MAXBLANKADJUSTMENT;
        long long iiee = iikey + MAXBLANKADJUSTMENT;
        FrameAnalyzer::FrameMap::Iterator jjfound = blankMap.end();

        /* Look for a blank frame near beginning of logo break. */
        for (FrameAnalyzer::FrameMap::Iterator jj = blankMap.begin();
                jj != blankMap.end();
                ++jj)
        {
            long long jjbb = jj.key();
            long long jjee = jjbb + *jj;

            if (iiee < jjbb)
                break;      /* No nearby blank frames. */

            if (jjee < iibb)
                continue;   /* Too early; keep looking. */

            jjfound = jj;
            if (iikey <= jjbb)
            {
                /*
                 * Prefer the first blank frame beginning after the logo break
                 * begins.
                 */
                break;
            }
        }

        /* Adjust blank frame to begin with logo break beginning. */
        if (jjfound != blankMap.end())
        {
            long long jjee = jjfound.key() + *jjfound;
            blankMap.erase(jjfound);
            if (jjee <= iikey)
            {
                /* Move blank frame to beginning of logo break. */
                blankMap.remove(iikey);
                blankMap.insert(iikey, 1);
            }
            else
            {
                /* Adjust blank frame to begin with logo break. */
                blankMap.insert(iikey, jjee - iikey);
            }
        }

        /* Get bounds of end of logo break. */
        long long kkkey = ii.key() + *ii;
        long long kkbb = kkkey - MAXBLANKADJUSTMENT;
        long long kkee = kkkey + MAXBLANKADJUSTMENT;
        FrameAnalyzer::FrameMap::Iterator mmfound = blankMap.end();

        /* Look for a blank frame near end of logo break. */
        for (FrameAnalyzer::FrameMap::Iterator mm = blankMap.begin();
                mm != blankMap.end();
                ++mm)
        {
            long long mmbb = mm.key();
            long long mmee = mmbb + *mm;

            if (kkee < mmbb)
                break;      /* No nearby blank frames. */

            if (mmee < kkbb)
                continue;   /* Too early; keep looking. */

            /* Prefer the last blank frame ending before the logo break ends. */
            if (mmee < kkkey || mmfound == blankMap.end())
                mmfound = mm;
            if (mmee >= kkkey)
                break;
        }

        /* Adjust blank frame to end with logo break end. */
        if (mmfound != blankMap.end())
        {
            long long mmbb = mmfound.key();
            if (mmbb < kkkey)
            {
                /* Adjust blank frame to end with logo break. */
                blankMap.remove(mmbb);
                blankMap.insert(mmbb, kkkey - mmbb);
            }
            else
            {
                /* Move blank frame to end of logo break. */
                blankMap.erase(mmfound);
                blankMap.remove(kkkey - 1);
                blankMap.insert(kkkey - 1, 1);
            }
        }
    }

    /*
     * Compute breaks (breakMap).
     */
    computeBreakMap(&breakMap, &blankMap, fps, skipcommblanks, debugLevel);

    /*
     * Expand blank-frame breaks to fully include overlapping logo breaks.
     * Fully include logo breaks that don't include any blank-frame breaks.
     */
    for (FrameAnalyzer::FrameMap::const_iterator ii =
                logoBreakMap->constBegin();
            ii != logoBreakMap->constEnd();
            ++ii)
    {
        long long iibb = ii.key();
        long long iiee = iibb + *ii;
        bool overlap = false;

        for (FrameAnalyzer::FrameMap::Iterator jj = breakMap.begin();
                jj != breakMap.end();
            )
        {
            long long jjbb = jj.key();
            long long jjee = jjbb + *jj;
            FrameAnalyzer::FrameMap::Iterator jjnext = jj;
            ++jjnext;

            if (iiee < jjbb)
            {
                if (!overlap)
                {
                    /* Fully incorporate logo break */
                    breakMap.insert(iibb, iiee - iibb);
                }
                break;
            }

            if (iibb < jjbb && jjbb < iiee)
            {
                /* End of logo break includes beginning of blank-frame break. */
                overlap = true;
                breakMap.erase(jj);
                breakMap.insert(iibb, max(iiee, jjee) - iibb);
            }
            else if (jjbb < iibb && iibb < jjee)
            {
                /* End of blank-frame break includes beginning of logo break. */
                overlap = true;
                if (jjee < iiee)
                {
                    breakMap.remove(jjbb);
                    breakMap.insert(jjbb, iiee - jjbb);
                }
            }

            jj = jjnext;
        }
    }

    frameAnalyzerReportMap(&breakMap, fps, "BF Break");
    return 0;
}

int
BlankFrameDetector::computeForLogoDeficit(
        const TemplateMatcher *templateMatcher)
{
    (void)templateMatcher;  /* gcc */

    VERBOSE(VB_COMMFLAG, "BlankFrameDetector adjusting for"
            " too little logo coverage (unimplemented)");
    return 0;
}

int
BlankFrameDetector::computeBreaks(FrameAnalyzer::FrameMap *breaks)
{
    if (breakMap.empty())
    {
        /* Compute breaks (breakMap). */
        computeBreakMap(&breakMap, &blankMap, fps, skipcommblanks, debugLevel);
        frameAnalyzerReportMap(&breakMap, fps, "BF Break");
    }

    breaks->clear();
    for (FrameAnalyzer::FrameMap::Iterator bb = breakMap.begin();
            bb != breakMap.end();
            ++bb)
        breaks->insert(bb.key(), *bb);

    return 0;
}

int
BlankFrameDetector::reportTime(void) const
{
    return histogramAnalyzer->reportTime();
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
