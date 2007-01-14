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
    /*
     * strspn(3)-style interface: return the first frame number that does not
     * match "acceptval".
     */
    while (frameno < nframes && match[frameno] == acceptval)
        frameno++;
    return frameno;
}

bool
removeShortBreaks(FrameAnalyzer::FrameMap *breakMap, float fps, int minbreaklen,
    bool verbose)
{
    /*
     * Remove any breaks that are less than "minbreaklen" units long.
     *
     * Return whether or not any breaks were actually removed.
     */
    FrameAnalyzer::FrameMap::Iterator   bb;
    bool                                removed;

    removed = false;

    /* Don't remove the initial commercial break, no matter how short. */
    bb = breakMap->begin();
    if (bb != breakMap->end() && bb.key() == 0)
        ++bb;
    while (bb != breakMap->end())
    {
        if (bb.data() >= minbreaklen)
        {
            ++bb;
            continue;
        }

        /* Don't remove the final commercial break, no matter how short. */
        FrameAnalyzer::FrameMap::Iterator bb1 = bb;
        ++bb;
        if (bb == breakMap->end())
            continue;

        if (verbose)
        {
            long long start = bb1.key();
            long long end = start + bb1.data() - 1;
            VERBOSE(VB_COMMFLAG, QString("Removing break %1-%2 (%3-%4)")
                .arg(frameToTimestamp(start, fps))
                .arg(frameToTimestamp(end, fps))
                .arg(start).arg(end));
        }
        breakMap->remove(bb1);
        removed = true;
    }

    return removed;
}

bool
removeShortSegments(FrameAnalyzer::FrameMap *breakMap, long long nframes,
    float fps, int minseglen, bool verbose)
{
    /*
     * Remove any segments that are less than "minseglen" units long.
     *
     * Return whether or not any segments were actually removed.
     */
    FrameAnalyzer::FrameMap::Iterator   bb, bbnext;
    bool                                removed;

    removed = false;

    /* Handle initial segment. */
    bb = breakMap->begin();
    if (bb != breakMap->end() && bb.key() != 0 && bb.key() < minseglen)
    {
        bbnext = bb;
        ++bbnext;
        if (verbose)
        {
            long long remove1 = bb.key();
            long long remove2 = remove1 + bb.data() - 1;
            long long insert1 = 0;
            long long insert2 = remove2;
            VERBOSE(VB_COMMFLAG, QString("Removing segment %1-%2 (%3-%4),"
                " inserting %5-%6 (%7-%8)")
                .arg(frameToTimestamp(remove1, fps))
                .arg(frameToTimestamp(remove2, fps))
                .arg(remove1).arg(remove2)
                .arg(frameToTimestamp(insert1, fps))
                .arg(frameToTimestamp(insert2, fps))
                .arg(insert1).arg(insert2));
        }
        breakMap->remove(bb);
        breakMap->insert(0, bb.key() + bb.data() - 1);
        bb = bbnext;
        removed = true;
    }
    for (; bb != breakMap->end(); bb = bbnext)
    {
        bbnext = bb;
        ++bbnext;
        long long brkb = bb.key();
        long long segb = brkb + bb.data();
        long long sege = bbnext == breakMap->end() ? nframes : bbnext.key();
        long long seglen = sege - segb;
        if (seglen >= minseglen)
            continue;

        /* Merge break with next break. */
        if (bbnext == breakMap->end())
        {
            if (segb != nframes)
            {
                /* Extend break "bb" to end of recording. */
                if (verbose)
                {
                    long long old1 = brkb;
                    long long old2 = segb - 1;
                    long long new1 = brkb;
                    long long new2 = nframes - 1;
                    VERBOSE(VB_COMMFLAG,
                        QString("Replacing segment %1-%2 (%3-%4)"
                        " with %5-%6 (%7-%8, EOF)")
                        .arg(frameToTimestamp(old1, fps))
                        .arg(frameToTimestamp(old2, fps))
                        .arg(old1).arg(old2)
                        .arg(frameToTimestamp(new1, fps))
                        .arg(frameToTimestamp(new2, fps))
                        .arg(new1).arg(new2));
                }
                breakMap->replace(brkb, nframes - brkb);
                removed = true;
            }
        }
        else
        {
            /* Extend break "bb" to cover "bbnext"; delete "bbnext". */
            if (verbose)
            {
                long long old1 = brkb;
                long long old2 = segb - 1;
                long long new1 = brkb;
                long long new2 = bbnext.key() + bbnext.data() - 1;
                VERBOSE(VB_COMMFLAG,
                    QString("Replacing segment %1-%2 (%3-%4)"
                    " with %5-%6 (%7-%8)")
                    .arg(frameToTimestamp(old1, fps))
                    .arg(frameToTimestamp(old2, fps))
                    .arg(old1).arg(old2)
                    .arg(frameToTimestamp(new1, fps))
                    .arg(frameToTimestamp(new2, fps))
                    .arg(new1).arg(new2));
            }
            breakMap->replace(brkb, bbnext.key() + bbnext.data() - brkb);
            bb = bbnext;
            ++bbnext;
            if (verbose)
            {
                long long start = bb.key();
                long long end = start + bb.data() - 1;
                VERBOSE(VB_COMMFLAG, QString("Removing segment %1-%2 (%3-%4)")
                    .arg(frameToTimestamp(start, fps))
                    .arg(frameToTimestamp(end, fps))
                    .arg(start).arg(end));
            }
            breakMap->remove(bb);
            removed = true;
        }
    }

    return removed;
}

FrameAnalyzer::FrameMap::const_iterator
blankMapSearchForwards(const FrameAnalyzer::FrameMap *blankMap, long long mark,
    long long markend)
{
    /*
     * Search forwards to find the earliest blank block "blank" such that
     *
     *          mark <= blankbegin < markend
     *
     * Return blankMap->constEnd() if there is no such blank block.
     */
    FrameAnalyzer::FrameMap::const_iterator blank = blankMap->constBegin();

    if (blank != blankMap->constEnd() && !blank.key() && !blank.data())
        ++blank;   /* Skip faked-up dummy frame-0 blank frame. */
    for (; blank != blankMap->constEnd(); ++blank)
    {
        const long long bb = blank.key();
        const long long ee = bb + blank.data();
        if (mark <= ee)
        {
            if (bb < markend)
                return blank;
            break;
        }
    }
    return blankMap->constEnd();
}

FrameAnalyzer::FrameMap::const_iterator
blankMapSearchBackwards(const FrameAnalyzer::FrameMap *blankMap,
    long long markbegin, long long mark)
{
    /*
     * Search backards to find the latest blank block "blank" such that
     *
     *          markbegin <= blankend < mark
     *
     * Return blankMap->constEnd() if there is no such blank block.
     */
    FrameAnalyzer::FrameMap::const_iterator blank = blankMap->constEnd();
    for (--blank; blank != blankMap->constBegin(); --blank)
    {
        const long long bb = blank.key();
        const long long ee = bb + blank.data();
        if (bb < mark)
        {
            if (markbegin <= ee)
                return blank;
            break;
        }
    }
    return blankMap->constEnd();
}

struct histogram {
    float           pctile;
    unsigned int    framecnt;
};

float
range_area(const struct histogram *histogram, int nhist, int iihist,
        float start, float end)
{
    /* Return the integrated area under the curve of the plotted histogram. */
    const float     width = end - start;
    unsigned int    sum, nsamples;

    sum = 0;
    nsamples = 0;
    if (end <= histogram[iihist].pctile)
    {
        iihist--;
        for (;;)
        {
            if (iihist < 0)
                break;
            if (histogram[iihist].pctile < start)
                break;
            sum += histogram[iihist].framecnt;
            iihist--;
            nsamples++;
        }
    }
    else
    {
        for (;;)
        {
            if (iihist >= nhist)
                break;
            if (histogram[iihist].pctile >= end)
                break;
            sum += histogram[iihist].framecnt;
            iihist++;
            nsamples++;
        }
    }
    if (!nsamples)
        return 0;
    return sum / nsamples * width;
}

unsigned short
matches_threshold(const unsigned short *matches, long long nframes,
        float *ppctile)
{
    /*
     * Most frames either match the template very well, or don't match
     * very well at all. This allows us to assume a bimodal
     * distribution of the values in a frequency histogram of the
     * "matches" array.
     *
     * Return a local minima between the two modes representing the
     * threshold value that decides whether or not a frame matches the
     * template.
     *
     * See "mythcommflag-analyze" and its output.
     */

    /*
     * TUNABLE:
     *
     * Given a point to test, require the integrated area to the left
     * of the point to be greater than some (larger) area to the right
     * of the point.
     */
    static const float  LEFTWINDOW  = 0.025;   /* locality of local minimum. */
    static const float  RIGHTWINDOW = 0.100;   /* locality of local minimum. */
    unsigned short      *sorted, minmatch, maxmatch, *freq, mintmpledges;
    int                 matchrange, nfreq, nhist, iihist;
    struct histogram    *histogram;

    sorted = new unsigned short[nframes];
    memcpy(sorted, matches, nframes * sizeof(*matches));
    qsort(sorted, nframes, sizeof(*sorted), sort_ascending);
    minmatch = sorted[0];
    maxmatch = sorted[nframes - 1];
    matchrange = maxmatch - minmatch;

    nfreq = maxmatch + 1;
    freq = new unsigned short[nfreq];
    memset(freq, 0, nfreq * sizeof(*freq));
    for (long long frameno = 0; frameno < nframes; frameno++)
        freq[matches[frameno]]++;   /* freq[<matchcnt>] = <framecnt> */

    nhist = maxmatch - minmatch + 1;
    histogram = new struct histogram[nhist];
    memset(histogram, 0, nhist * sizeof(*histogram));

    for (int matchcnt = minmatch, iihist = 0;
            matchcnt <= maxmatch;
            matchcnt++, iihist++)
    {
        histogram[iihist].pctile = (float)(matchcnt - minmatch) / matchrange;
        histogram[iihist].framecnt = freq[matchcnt];
    }

    /* Initialize "iihist" to accommodate the RIGHTWINDOW. */
    for (iihist = nhist - 2; iihist >= 0; iihist--)
    {
        if (histogram[iihist].pctile < 1 - RIGHTWINDOW)
            break;
    }

    for (;;)
    {
        float left, right;
        if (iihist < 0)
        {
            iihist = 0;
            break;
        }
        if (histogram[iihist].pctile < LEFTWINDOW)
            break;
        left = range_area(histogram, nhist, iihist,
                histogram[iihist].pctile - LEFTWINDOW,
                histogram[iihist].pctile);
        right = range_area(histogram, nhist, iihist,
                histogram[iihist].pctile,
                histogram[iihist].pctile + RIGHTWINDOW);
        if (left > right)
            break;
        iihist--;
    }

    *ppctile = histogram[iihist].pctile;
    mintmpledges = sorted[(int)(*ppctile * nframes)];
    delete []freq;
    delete []sorted;
    return mintmpledges;
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
    , debug_removerunts(false)
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
    {
        createDebugDirectory(debugdir,
            QString("TemplateMatcher debugLevel %1").arg(debugLevel));
        debug_matches = true;
        if (debugLevel >= 2)
            debug_removerunts = true;
    }
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
     * Eliminate false negatives and false positives by eliminating breaks and
     * segments shorter than these periods, subject to maximum bounds.
     *
     * Higher values could eliminate real breaks or segments entirely.
     * Lower values can yield more false "short" breaks or segments.
     */
    const int       MINBREAKLEN = (int)roundf(45 * fps);  /* frames */
    const int       MINSEGLEN = (int)roundf(120 * fps);    /* frames */

    int                                 tmpledges, mintmpledges;
    float                               matchpctile;
    long long                           brkb;
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
    mintmpledges = matches_threshold(matches, nframes, &matchpctile);

    VERBOSE(VB_COMMFLAG, QString("TemplateMatcher::finished %1x%2@(%3,%4),"
                " %5 edge pixels, want %6 (pctile=%7)")
            .arg(tmplwidth).arg(tmplheight).arg(tmplcol).arg(tmplrow)
            .arg(tmpledges).arg(mintmpledges).arg(matchpctile, 0, 'f', 6));

    for (long long ii = 0; ii < nframes; ii++)
        match[ii] = matches[ii] >= mintmpledges ? 1 : 0;

    if (debugLevel >= 2)
    {
        if (final && finishedDebug(nframes, matches, match))
            goto error;
    }

    /*
     * Construct breaks; find the first logo break.
     */
    breakMap.clear();
    brkb = match[0] ? matchspn(nframes, match, 0, match[0]) : 0;
    while (brkb < nframes)
    {
        /* Skip over logo-less frames; find the next logo frame (brke). */
        long long brke = matchspn(nframes, match, brkb, match[brkb]);
        long long brklen = brke - brkb;
        breakMap.insert(brkb, brklen);

        /* Find the next logo break. */
        brkb = matchspn(nframes, match, brke, match[brke]);
    }

    /* Clean up the "noise". */
    for (;;)
    {
        bool f1 = removeShortBreaks(&breakMap, fps, MINBREAKLEN,
            debug_removerunts);
        bool f2 = removeShortSegments(&breakMap, nframes, fps, MINSEGLEN,
            debug_removerunts);
        if (!f1 && !f2)
            break;
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
TemplateMatcher::adjustForBlanks(const BlankFrameDetector *blankFrameDetector,
    long long nframes)
{
    const bool skipCommBlanks = blankFrameDetector->getSkipCommBlanks();
    const FrameAnalyzer::FrameMap *blankMap = blankFrameDetector->getBlanks();

    /*
     * TUNABLE:
     *
     * Tune the search for blank frames near appearances and disappearances of
     * the template.
     *
     * TEMPLATE_DISAPPEARS_EARLY: Handle the scenario where the template can
     * disappear "early" before switching to commercial. Delay the beginning of
     * the commercial break by searching forwards to find a blank frame.
     *
     *      Setting this value too low can yield false positives. If template
     *      does indeed disappear "early" (common in US broadcast), the end of
     *      the broadcast segment can be misidentified as part of the beginning
     *      of the commercial break.
     *
     *      Setting this value too high can yield or worsen false negatives. If
     *      the template presence extends at all into the commercial break,
     *      immediate cuts to commercial without any intervening blank frames
     *      can cause the broadcast to "continue" even further into the
     *      commercial break.
     *
     * TEMPLATE_DISAPPEARS_LATE: If TEMPLATE_DISAPPEARS_EARLY doesn't find
     * anything, handle the scenario where the template disappears "late" after
     * having already switched to commercial (template presence extends into
     * commercial break). Accelerate the beginning of the commercial break by
     * searching backwards to find a blank frame.
     *
     *      Setting this value too low can yield false negatives. If the
     *      template does extend deep into the commercial break, the first part
     *      of the commercial break can be misidentifed as part of the
     *      broadcast.
     *
     *      Setting this value too high can yield or worsen false positives. If
     *      the template disappears extremely early (too early for
     *      TEMPLATE_DISAPPEARS_EARLY), blank frames before the template
     *      disappears can cause even more of the end of the broadcast segment
     *      to be misidentified as the first part of the commercial break.
     *
     * TEMPLATE_REAPPEARS_LATE: Handle the scenario where the template
     * reappears "late" after having already returned to the broadcast.
     * Accelerate the beginning of the broadcast by searching backwards to find
     * a blank frame.
     *
     *      Setting this value too low can yield false positives. If the
     *      template does indeed reappear "late" (common in US broadcast), the
     *      beginning of the broadcast segment can be misidentified as the end
     *      of the commercial break.
     *
     *      Setting this value too high can yield or worsen false negatives. If
     *      the template actually reappears early, blank frames before the
     *      template reappears can cause even more of the end of the commercial
     *      break to be misidentified as the first part of the broadcast break.
     *
     * TEMPLATE_REAPPEARS_EARLY: If TEMPLATE_REAPPEARS_LATE doesn't find
     * anything, handle the scenario where the template reappears "early"
     * before resuming the broadcast. Delay the beginning of the broadcast by
     * searching forwards to find a blank frame.
     *
     *      Setting this value too low can yield false negatives. If the
     *      template does reappear "early", the the last part of the commercial
     *      break can be misidentified as part of the beginning of the
     *      following broadcast segment.
     *
     *      Setting this value too high can yield or worsen false positives. If
     *      the template reappears extremely late (too late for
     *      TEMPLATE_REAPPEARS_LATE), blank frames after the template reappears
     *      can cause even more of the broadcast segment can be misidentified
     *      as part of the end of the commercial break.
     */
    const int BLANK_NEARBY = (int)roundf(0.5 * fps);
    const int TEMPLATE_DISAPPEARS_EARLY = (int)roundf(25 * fps);
    const int TEMPLATE_DISAPPEARS_LATE = (int)roundf(0 * fps);
    const int TEMPLATE_REAPPEARS_LATE = (int)roundf(25 * fps);
    const int TEMPLATE_REAPPEARS_EARLY = (int)roundf(0 * fps);

    VERBOSE(VB_COMMFLAG, QString("TemplateMatcher adjusting for blanks"));

    FrameAnalyzer::FrameMap::Iterator ii = breakMap.begin();
    while (ii != breakMap.end())
    {
        FrameAnalyzer::FrameMap::Iterator iinext = ii;
        ++iinext;

        /*
         * Where the template disappears, look for nearby blank frames. Prefer
         * to assume that the template disappears early before the commercial
         * break starts. Only adjust beginning-of-breaks that are not at the
         * very beginning.
         */
        const long long brkb = ii.key();
        const long long brke = brkb + ii.data();
        FrameAnalyzer::FrameMap::const_iterator jj = blankMap->constEnd();
        if (brkb > 0)
        {
            jj = blankMapSearchForwards(blankMap,
                max(0LL, brkb - BLANK_NEARBY),
                min(brke, min(nframes, brkb + TEMPLATE_DISAPPEARS_EARLY)));
        }
        if (jj == blankMap->constEnd())
        {
            jj = blankMapSearchBackwards(blankMap,
                max(0LL, brkb - TEMPLATE_DISAPPEARS_LATE),
                min(brke, min(nframes, brkb + BLANK_NEARBY)));
        }
        long long newbrkb = brkb;
        if (jj != blankMap->constEnd())
        {
            newbrkb = jj.key();
            if (!skipCommBlanks)
                newbrkb += jj.data();
        }

        /*
         * Where the template reappears, look for nearby blank frames. Prefer
         * to assume that the template reappears late after the broadcast has
         * already resumed.
         */
        FrameAnalyzer::FrameMap::const_iterator kk = blankMapSearchBackwards(
            blankMap,
            max(newbrkb, brke - TEMPLATE_REAPPEARS_LATE),
            max(newbrkb, min(nframes, brke + BLANK_NEARBY)));
        if (kk == blankMap->constEnd())
        {
            kk = blankMapSearchForwards(blankMap,
                max(newbrkb, brke - BLANK_NEARBY),
                max(newbrkb, min(nframes, brke + TEMPLATE_REAPPEARS_EARLY)));
        }
        long long newbrke = brke;
        if (kk != blankMap->constEnd())
        {
            newbrke = kk.key();
            if (skipCommBlanks)
                newbrke += kk.data();
        }

        /*
         * Adjust breakMap for blank frames.
         */
        long long newbrklen = newbrke - newbrkb;
        if (newbrkb != brkb)
        {
            breakMap.remove(ii);
            if (newbrkb < nframes && newbrklen)
                breakMap.insert(newbrkb, newbrklen);
        }
        else if (newbrke != brke)
        {
            if (newbrklen)
                breakMap.replace(newbrkb, newbrklen);
            else
                breakMap.remove(ii);
        }

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
