#include <stdio.h>
#include <math.h>

#include "NuppelVideoPlayer.h"
#include "avcodec.h"        /* AVPicture */

#include "CommDetector2.h"
#include "FrameAnalyzer.h"
#include "pgm.h"
#include "PGMConverter.h"
#include "EdgeDetector.h"
#include "BlankFrameDetector.h"
#include "TemplateFinder.h"
#include "TemplateMatcher.h"

/* Debugging */
#include <qfile.h>
#include <qfileinfo.h>

using namespace commDetector2;
using namespace frameAnalyzer;

namespace {

int
pgm_set(const AVPicture *pict, int height)
{
    const int   width = pict->linesize[0];
    const int   size = height * width;
    int         score, ii;

    score = 0;
    for (ii = 0; ii < size; ii++)
        if (pict->data[0][ii])
            score++;
    return score;
}

int
pgm_match(const AVPicture *tmpl, const AVPicture *test, int height,
        int radius, unsigned short *pscore)
{
    /* Return the number of matching "edge" and non-edge pixels. */
    const int       width = tmpl->linesize[0];
    int             score, rr, cc;

    if (width != test->linesize[0])
    {
        VERBOSE(VB_COMMFLAG, QString("pgm_match widths don't match: %1 != %2")
                .arg(width).arg(test->linesize[0]));
        return -1;
    }

    score = 0;
    for (rr = 0; rr < height; rr++)
    {
        for (cc = 0; cc < width; cc++)
        {
            int r2min, r2max, r2, c2min, c2max, c2;

            if (!tmpl->data[0][rr * width + cc])
                continue;

            r2min = max(0, rr - radius);
            r2max = min(height, rr + radius);

            c2min = max(0, cc - radius);
            c2max = min(width, cc + radius);

            for (r2 = r2min; r2 <= r2max; r2++)
            {
                for (c2 = c2min; c2 <= c2max; c2++)
                {
                    if (test->data[0][r2 * width + c2])
                    {
                        score++;
                        goto next_pixel;
                    }
                }
            }
next_pixel:
            ;
        }
    }

    *pscore = score;
    return 0;
}

bool
readMatches(QString filename, unsigned short *matches, long long nframes)
{
    FILE        *fp;
    long long   frameno;

    if (!(fp = fopen(filename, "r")))
        return false;

    for (frameno = 0; frameno < nframes; frameno++)
    {
        int nitems = fscanf(fp, "%hu", &matches[frameno]);
        if (nitems != 1)
        {
            VERBOSE(VB_COMMFLAG, QString("Not enough data in %1: frame %2")
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
writeMatches(QString filename, unsigned short *matches, long long nframes)
{
    FILE        *fp;
    long long   frameno;

    if (!(fp = fopen(filename, "w")))
        return false;

    for (frameno = 0; frameno < nframes; frameno++)
        (void)fprintf(fp, "%hu\n", matches[frameno]);

    if (fclose(fp))
        VERBOSE(VB_COMMFLAG, QString("Error closing %1: %2")
                .arg(filename).arg(strerror(errno)));
    return true;
}

int
finishedDebug(long long nframes, const unsigned short *matches,
        const unsigned char *match)
{
    unsigned short  low, high, score;
    long long       startframe;

    score = matches[0];
    low = score;
    high = score;
    startframe = 0;

    for (long long frameno = 1; frameno < nframes; frameno++)
    {
        score = matches[frameno];
        if (match[frameno - 1] == match[frameno])
        {
            if (score < low)
                low = score;
            if (score > high)
                high = score;
            continue;
        }

        VERBOSE(VB_COMMFLAG, QString("Frame %1-%2: %3 L-H: %4-%5 (%6)")
                .arg(startframe, 6).arg(frameno - 1, 6)
                .arg(match[frameno - 1] ? "logo        " : "     no-logo")
                .arg(low, 4).arg(high, 4).arg(frameno - startframe, 5));

        low = score;
        high = score;
        startframe = frameno;
    }

    return 0;
}

int
sort_ascending(const void *aa, const void *bb)
{
    return *(unsigned short*)aa - *(unsigned short*)bb;
}

long long
matchspn(long long nframes, unsigned char *match, long long frameno,
        unsigned char acceptval)
{
    /* Return the first frame that does not match "acceptval". */
    while (frameno < nframes && match[frameno] == acceptval)
        frameno++;
    return frameno;
}

};  /* namespace */

TemplateMatcher::TemplateMatcher(PGMConverter *pgmc, EdgeDetector *ed,
        TemplateFinder *tf, QString debugdir)
    : FrameAnalyzer()
    , pgmConverter(pgmc)
    , edgeDetector(ed)
    , templateFinder(tf)
    , matches(NULL)
    , match(NULL)
    , tmatches(NULL)
    , debugLevel(0)
    , debugdir(debugdir)
#ifdef PGM_CONVERT_GREYSCALE
    , debugdata(debugdir + "/TemplateMatcher-pgm.txt")
#else  /* !PGM_CONVERT_GREYSCALE */
    , debugdata(debugdir + "/TemplateMatcher-yuv.txt")
#endif /* !PGM_CONVERT_GREYSCALE */
    , nvp(NULL)
    , debug_matches(false)
    , matches_done(false)
{
    memset(&cropped, 0, sizeof(cropped));
    memset(&analyze_time, 0, sizeof(analyze_time));

    /*
     * debugLevel:
     *      0: no debugging
     *      1: cache frame edge counts into debugdir [1 file]
     *      2: extra verbosity [O(nframes)]
     */
    debugLevel = gContext->GetNumSetting("TemplateMatcherDebugLevel", 0);

    if (debugLevel >= 1)
        createDebugDirectory(debugdir,
            QString("TemplateMatcher debugLevel %1").arg(debugLevel));

    if (debugLevel >= 1)
        debug_matches = true;
}

TemplateMatcher::~TemplateMatcher(void)
{
    if (tmatches)
        delete []tmatches;
    if (matches)
        delete []matches;
    if (match)
        delete []match;
    avpicture_free(&cropped);
}

enum FrameAnalyzer::analyzeFrameResult
TemplateMatcher::nuppelVideoPlayerInited(NuppelVideoPlayer *_nvp,
        long long nframes)
{
    nvp = _nvp;
    fps = nvp->GetFrameRate();

    if (!(tmpl = templateFinder->getTemplate(&tmplrow, &tmplcol,
                    &tmplwidth, &tmplheight)))
    {
        VERBOSE(VB_COMMFLAG, QString("TemplateMatcher::nuppelVideoPlayerInited:"
                    " no template"));
        return ANALYZE_FATAL;
    }

    if (avpicture_alloc(&cropped, PIX_FMT_GRAY8, tmplwidth, tmplheight))
    {
        VERBOSE(VB_COMMFLAG, QString("TemplateMatcher::nuppelVideoPlayerInited "
                "avpicture_alloc cropped (%1x%2) failed").
                arg(tmplwidth).arg(tmplheight));
        return ANALYZE_FATAL;
    }

    if (pgmConverter->nuppelVideoPlayerInited(nvp))
        goto free_cropped;

    matches = new unsigned short[nframes];
    tmatches = new unsigned short[nframes];
    memset(matches, 0, nframes * sizeof(*matches));

    match = new unsigned char[nframes];

    if (debug_matches)
    {
        if (readMatches(debugdata, matches, nframes))
        {
            VERBOSE(VB_COMMFLAG, QString(
                        "TemplateMatcher::nuppelVideoPlayerInited read %1")
                    .arg(debugdata));
            matches_done = true;
        }
    }

    if (matches_done)
        return ANALYZE_FINISHED;

    return ANALYZE_OK;

free_cropped:
    avpicture_free(&cropped);
    return ANALYZE_FATAL;
}

enum FrameAnalyzer::analyzeFrameResult
TemplateMatcher::analyzeFrame(const VideoFrame *frame, long long frameno,
        long long *pNextFrame)
{
    /*
     * TUNABLE:
     *
     * The matching area should be a lot smaller than the area used by
     * TemplateFinder, so use a smaller percentile than the TemplateFinder
     * (intensity requirements to be considered an "edge" are lower, because
     * there should be less pollution from non-template edges).
     *
     * Higher values mean fewer edge pixels in the candidate template area;
     * occluded or faint templates might be missed.
     *
     * Lower values mean more edge pixels in the candidate template area;
     * non-template edges can be picked up and cause false identification of
     * matches.
     */
    const int           FRAMESGMPCTILE = 70;

    /*
     * TUNABLE:
     *
     * The per-pixel search radius for matching the template. Meant to deal
     * with template edges that might minimally slide around (such as for
     * animated lighting effects).
     *
     * Higher values will pick up more pixels as matching the template
     * (possibly false matches).
     *
     * Lower values will require more exact template matches, possibly missing
     * true matches.
     *
     * The TemplateMatcher accumulates all its state in the "matches" array to
     * be processed later by TemplateMatcher::finished.
     */
    const int           JITTER_RADIUS = 0;

    const AVPicture     *pgm;
    const AVPicture     *edges;
    int                 pgmwidth, pgmheight;
    struct timeval      start, end, elapsed;

    *pNextFrame = NEXTFRAME;

    if (!(pgm = pgmConverter->getImage(frame, frameno, &pgmwidth, &pgmheight)))
        goto error;

    (void)gettimeofday(&start, NULL);

    if (pgm_crop(&cropped, pgm, pgmheight, tmplrow, tmplcol,
                tmplwidth, tmplheight))
        goto error;

    if (!(edges = edgeDetector->detectEdges(&cropped, tmplheight,
                    FRAMESGMPCTILE)))
        goto error;

    if (pgm_match(tmpl, edges, tmplheight, JITTER_RADIUS, &matches[frameno]))
        goto error;

    (void)gettimeofday(&end, NULL);
    timersub(&end, &start, &elapsed);
    timeradd(&analyze_time, &elapsed, &analyze_time);

    return ANALYZE_OK;

error:
    VERBOSE(VB_COMMFLAG, QString(
                "TemplateMatcher::analyzeFrame error at frame %1 of %2")
            .arg(frameno));
    return ANALYZE_ERROR;
}

int
TemplateMatcher::finished(long long nframes, bool final)
{
    /*
     * TUNABLE:
     *
     * Eliminate false negatives and false positives by eliminating segments
     * shorter than these periods.
     *
     * Higher values could eliminate real breaks entirely.
     * Lower values can yield more false "short" breaks.
     */
    const int       MINBREAKLEN = (int)roundf(25 * fps);  /* frames */
    const int       MINSEGLEN = (int)roundf(25 * fps);    /* frames */

    /*
     * TUNABLE:
     *
     * The percentile of template matching frames to mark as "matching".
     *
     * Higher values require closer matches, and could miss true matches.
     * Lower values can yield false content.
     */
    static const float  MATCHPCTILE = 0.47;

    int                                 tmpledges, mintmpledges, ii;
    long long                           segb, brkb;
    FrameAnalyzer::FrameMap::Iterator   bb;

    if (!matches_done && debug_matches)
    {
        if (final && writeMatches(debugdata, matches, nframes))
        {
            VERBOSE(VB_COMMFLAG, QString("TemplateMatcher::finished wrote %1")
                    .arg(debugdata));
            matches_done = true;
        }
    }

    tmpledges = pgm_set(tmpl, tmplheight);
    memcpy(tmatches, matches, nframes * sizeof(*matches));
    qsort(tmatches, nframes, sizeof(*tmatches), sort_ascending);
    ii = (int)((nframes - 1) * MATCHPCTILE);
    mintmpledges = tmatches[ii];
    for (; ii > 0; ii--) {
        if (tmatches[ii - 1] != mintmpledges)
            break;
    }

    VERBOSE(VB_COMMFLAG, QString("TemplateMatcher::finished "
                "%1x%2@(%3,%4), %5 edge pixels "
                "(want %6 pctile=%7, got pctile=%8)")
            .arg(tmplwidth).arg(tmplheight).arg(tmplcol).arg(tmplrow)
            .arg(tmpledges).arg(mintmpledges).arg(MATCHPCTILE, 0, 'f', 6)
            .arg((float)ii / nframes, 0, 'f', 6));

    for (ii = 0; ii < nframes; ii++)
        match[ii] = matches[ii] >= mintmpledges ? 1 : 0;

    if (debugLevel >= 2)
    {
        if (final && finishedDebug(nframes, matches, match))
            goto error;
    }

    /*
     * Construct breaks.
     */
    breakMap.clear();
    brkb = 0;
    while (brkb < nframes)
    {
        /* Find break. */
        if (match[brkb] &&
                (brkb = matchspn(nframes, match, brkb, match[brkb])) == nframes)
            break;

        long long brke = matchspn(nframes, match, brkb, match[brkb]);
        long long brklen = brke - brkb;
        breakMap[brkb] = brklen;

        brkb = brke;
    }

    /*
     * Eliminate false breaks (but allow short breaks if they start at the very
     * beginning or end at the very end).
     */
    bb = breakMap.begin();
    if (bb != breakMap.end() && bb.key() == 0)
        ++bb;
    while (bb != breakMap.end())
    {
        if (bb.data() >= MINBREAKLEN)
        {
            ++bb;
            continue;
        }

        FrameAnalyzer::FrameMap::Iterator bb1 = bb;
        ++bb;
        if (bb == breakMap.end())
            continue;

        breakMap.remove(bb1);
    }

    /*
     * Eliminate false segments.
     */
    segb = 0;
    bb = breakMap.begin();
    if (bb != breakMap.end() && bb.key() == 0)
        ++bb;
    while (bb != breakMap.end())
    {
        brkb = bb.key();
        long long seglen = brkb - segb;
        if (seglen >= MINSEGLEN)
        {
            segb = brkb + bb.data();
            ++bb;
            continue;
        }

        /* Merge break with next break. */
        FrameAnalyzer::FrameMap::Iterator bb1 = bb;
        ++bb1;
        if (bb1 == breakMap.end())
        {
            /* Extend break to end of recording. */
            breakMap[brkb] = nframes - brkb;
        }
        else
        {
            /* Extend break to cover next break; delete next break. */
            breakMap[brkb] = bb1.key() + bb1.data() - brkb;  /* bb */
            bb = bb1;
            ++bb1;
            breakMap.remove(bb);
        }
        bb = bb1;
    }

    /*
     * Report breaks.
     */
    frameAnalyzerReportMap(&breakMap, fps, "TM Break");

    return 0;

error:
    return -1;
}

int
TemplateMatcher::reportTime(void) const
{
    if (pgmConverter->reportTime())
        return -1;

    VERBOSE(VB_COMMFLAG, QString("TM Time: analyze=%1s")
            .arg(strftimeval(&analyze_time)));
    return 0;
}

int
TemplateMatcher::templateCoverage(long long nframes, bool final) const
{
    /*
     * Return <0 for too little logo coverage (some content has no logo), 0 for
     * "correct" coverage, and >0 for too much coverage (some commercials have
     * logos).
     */

    /*
     * TUNABLE:
     *
     * Expect 20%-45% of total length to be commercials.
     */
    const int       MINBREAKS = nframes * 20 / 100;
    const int       MAXBREAKS = nframes * 45 / 100;

    const long long brklen = frameAnalyzerMapSum(&breakMap);
    const bool good = brklen >= MINBREAKS && brklen <= MAXBREAKS;

    if (debugLevel >= 1)
    {
        if (!tmpl)
        {
            VERBOSE(VB_COMMFLAG, QString("TemplateMatcher: no template"
                        " (wanted %2-%3%)")
                    .arg(100 * MINBREAKS / nframes)
                    .arg(100 * MAXBREAKS / nframes));
        }
        else if (!final)
        {
            VERBOSE(VB_COMMFLAG, QString("TemplateMatcher has %1% breaks"
                        " (real-time flagging)")
                    .arg(100 * brklen / nframes));
        }
        else if (good)
        {
            VERBOSE(VB_COMMFLAG, QString("TemplateMatcher has %1% breaks")
                    .arg(100 * brklen / nframes));
        }
        else
        {
            VERBOSE(VB_COMMFLAG, QString("TemplateMatcher has %1% breaks"
                        " (wanted %2-%3%)")
                    .arg(100 * brklen / nframes)
                    .arg(100 * MINBREAKS / nframes)
                    .arg(100 * MAXBREAKS / nframes));
        }
    }

    if (!final)
        return 0;   /* real-time flagging */

    return brklen < MINBREAKS ? 1 : brklen <= MAXBREAKS ? 0 : -1;
}

int
TemplateMatcher::adjustForBlanks(const BlankFrameDetector *blankFrameDetector)
{
    /*
     * TUNABLE:
     *
     * When logos are a good indicator of commercial breaks, allow blank frames
     * to adjust logo breaks. Use BlankFrameDetector information to adjust
     * beginnings and endings of existing TemplateMatcher breaks. Allow logos
     * to deviate by up to MAXBLANKADJUSTMENT frames before/after the break
     * actually begins/ends.
     */
    const int       MAXBLANKADJUSTMENT = (int)roundf(120 * fps);  /* 120 sec */
    const bool      skipCommBlanks = blankFrameDetector->getSKipCommBlanks();

    const FrameAnalyzer::FrameMap *blankMap = blankFrameDetector->getBlanks();

    VERBOSE(VB_COMMFLAG, QString("TemplateMatcher adjusting for blanks"));

    FrameAnalyzer::FrameMap::Iterator ii = breakMap.begin();
    while (ii != breakMap.end())
    {
        FrameAnalyzer::FrameMap::Iterator iinext = ii;
        ++iinext;

        /* Get bounds of beginning of logo break. */
        long long brkb = ii.key();
        long long brkbmax = brkb + MAXBLANKADJUSTMENT;
        FrameAnalyzer::FrameMap::const_iterator jjfound = blankMap->constEnd();

        if (ii.key() > 0)
        {
            /*
             * Look for first blank frames ending on/after beginning of logo
             * break.
             */
            FrameAnalyzer::FrameMap::const_iterator jj = blankMap->constBegin();
            if (jj != blankMap->constEnd() && !jj.key() && !jj.data())
                ++jj;   /* Skip faked-up dummy frame-0 blank frame. */
            for (; jj != blankMap->constEnd(); ++jj)
            {
                long long jjbb = jj.key();
                long long jjee = jjbb + jj.data();

                if (brkb <= jjee)
                {
                    if (jjbb <= brkbmax)
                        jjfound = jj;
                    break;
                }
            }
        }

        /* Get bounds of end of logo break. */
        long long brke = ii.key() + ii.data();
        long long brkemin = brke - MAXBLANKADJUSTMENT;

        /*
         * Look for last blank frames beginning before/on end of logo break.
         * (Going backwards, look for the "first" blank frames beginning
         * before/on end of logo break.)
         */
        FrameAnalyzer::FrameMap::const_iterator kkfound = blankMap->constEnd();
        FrameAnalyzer::FrameMap::const_iterator kk = blankMap->constEnd();
        --kk;
        for (; kk != jjfound && kk != blankMap->constBegin(); --kk)
        {
            long long kkbb = kk.key();
            long long kkee = kkbb + kk.data();

            if (kkbb <= brke)
            {
                if (brkemin <= kkee)
                    kkfound = kk;
                break;
            }
        }

        /*
         * Adjust breakMap for blank frames.
         */
        long long start = ii.key();
        long long end = start + ii.data();
        if (jjfound != blankMap->constEnd())
        {
            start = jjfound.key();
            if (!skipCommBlanks)
                start += jjfound.data();
        }
        if (kkfound != blankMap->constEnd())
        {
            end = kkfound.key();
            if (skipCommBlanks)
                end += kkfound.data();
        }
        if (start != ii.key())
            breakMap.remove(ii);
        breakMap[start] = end - start;

        ii = iinext;
    }

    /*
     * Report breaks.
     */
    frameAnalyzerReportMap(&breakMap, fps, "TM Break");
    return 0;
}

int
TemplateMatcher::computeBreaks(FrameAnalyzer::FrameMap *breaks)
{
    breaks->clear();
    for (FrameAnalyzer::FrameMap::Iterator bb = breakMap.begin();
            bb != breakMap.end();
            ++bb)
    {
        breaks->insert(bb.key(), bb.data());
    }
    return 0;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
