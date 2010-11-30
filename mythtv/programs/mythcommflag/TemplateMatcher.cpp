// POSIX headers
#include <sys/time.h>      /* gettimeofday */

// ANSI C headers
#include <cstdlib>
#include <cmath>

// C++ headers
#include <algorithm>
using namespace std;

// Qt headers
#include <QFile>
#include <QFileInfo>

// MythTV headers
#include "mythplayer.h"
#include "mythcorecontext.h"

// Commercial Flagging headers
#include "CommDetector2.h"
#include "FrameAnalyzer.h"
#include "pgm.h"
#include "PGMConverter.h"
#include "EdgeDetector.h"
#include "BlankFrameDetector.h"
#include "TemplateFinder.h"
#include "TemplateMatcher.h"

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

    QByteArray fname = filename.toLocal8Bit();
    if (!(fp = fopen(fname.constData(), "r")))
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

    QByteArray fname = filename.toLocal8Bit();
    if (!(fp = fopen(fname.constData(), "w")))
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

unsigned int
range_area(const unsigned short *freq, unsigned short start, unsigned short end)
{
    /* Return the integrated area under the curve of the plotted histogram. */
    const unsigned short    width = end - start;
    unsigned short          matchcnt;
    unsigned int            sum, nsamples;

    sum = 0;
    nsamples = 0;
    for (matchcnt = start; matchcnt < end; matchcnt++)
    {
        if (freq[matchcnt])
        {
            sum += freq[matchcnt];
            nsamples++;
        }
    }
    if (!nsamples)
        return 0;
    return width * sum / nsamples;
}

unsigned short
pick_mintmpledges(const unsigned short *matches, long long nframes)
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
    static const float  LEFTWIDTH = 0.04;
    static const float  MIDDLEWIDTH = 0.04;
    static const float  RIGHTWIDTH = 0.04;

    static const float  MATCHSTART = 0.20;
    static const float  MATCHEND = 0.80;

    unsigned short      matchrange, matchstart, matchend;
    unsigned short      leftwidth, middlewidth, rightwidth;
    unsigned short      *sorted, minmatch, maxmatch, *freq;
    int                 nfreq, matchcnt, local_minimum;
    unsigned int        maxdelta;

    sorted = new unsigned short[nframes];
    memcpy(sorted, matches, nframes * sizeof(*matches));
    qsort(sorted, nframes, sizeof(*sorted), sort_ascending);
    minmatch = sorted[0];
    maxmatch = sorted[nframes - 1];
    matchrange = maxmatch - minmatch;
    /* degenerate minmatch==maxmatch case is gracefully handled */

    leftwidth = (unsigned short)(LEFTWIDTH * matchrange);
    middlewidth = (unsigned short)(MIDDLEWIDTH * matchrange);
    rightwidth = (unsigned short)(RIGHTWIDTH * matchrange);

    nfreq = maxmatch + 1;
    freq = new unsigned short[nfreq];
    memset(freq, 0, nfreq * sizeof(*freq));
    for (long long frameno = 0; frameno < nframes; frameno++)
        freq[matches[frameno]]++;   /* freq[<matchcnt>] = <framecnt> */

    matchstart = minmatch + (unsigned short)(MATCHSTART * matchrange);
    matchend = minmatch + (unsigned short)(MATCHEND * matchrange);

    local_minimum = matchstart;
    maxdelta = 0;
    for (matchcnt = matchstart + leftwidth + middlewidth / 2;
            matchcnt < matchend - rightwidth - middlewidth / 2;
            matchcnt++)
    {
        unsigned short  p0, p1, p2, p3;
        unsigned int    leftscore, middlescore, rightscore;

        p0 = matchcnt - leftwidth - middlewidth / 2;
        p1 = p0 + leftwidth;
        p2 = p1 + middlewidth;
        p3 = p2 + rightwidth;

        leftscore = range_area(freq, p0, p1);
        middlescore = range_area(freq, p1, p2);
        rightscore = range_area(freq, p2, p3);
        if (middlescore < leftscore && middlescore < rightscore)
        {
            unsigned int delta = (leftscore - middlescore) +
                (rightscore - middlescore);
            if (delta > maxdelta)
            {
                local_minimum = matchcnt;
                maxdelta = delta;
            }
        }
    }

    VERBOSE(VB_COMMFLAG, QString("pick_mintmpledges minmatch=%1 maxmatch=%2"
                " matchstart=%3 matchend=%4 widths=%5,%6,%7 local_minimum=%8")
            .arg(minmatch).arg(maxmatch).arg(matchstart).arg(matchend)
            .arg(leftwidth).arg(middlewidth).arg(rightwidth)
            .arg(local_minimum));

    delete []freq;
    delete []sorted;
    return local_minimum;
}

};  /* namespace */

TemplateMatcher::TemplateMatcher(PGMConverter *pgmc, EdgeDetector *ed,
                                 TemplateFinder *tf, QString debugdir) :
    FrameAnalyzer(),      pgmConverter(pgmc),
    edgeDetector(ed),     templateFinder(tf),
    tmpl(0),
    tmplrow(-1),          tmplcol(-1),
    tmplwidth(-1),        tmplheight(-1),
    matches(NULL),        match(NULL),
    fps(0.0f),
    debugLevel(0),        debugdir(debugdir),
#ifdef PGM_CONVERT_GREYSCALE
    debugdata(debugdir + "/TemplateMatcher-pgm.txt"),
#else  /* !PGM_CONVERT_GREYSCALE */
    debugdata(debugdir + "/TemplateMatcher-yuv.txt"),
#endif /* !PGM_CONVERT_GREYSCALE */
    player(NULL),
    debug_matches(false), debug_removerunts(false),
    matches_done(false)
{
    memset(&cropped, 0, sizeof(cropped));
    memset(&analyze_time, 0, sizeof(analyze_time));

    /*
     * debugLevel:
     *      0: no debugging
     *      1: cache frame edge counts into debugdir [1 file]
     *      2: extra verbosity [O(nframes)]
     */
    debugLevel = gCoreContext->GetNumSetting("TemplateMatcherDebugLevel", 0);

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
    if (matches)
        delete []matches;
    if (match)
        delete []match;
    avpicture_free(&cropped);
}

enum FrameAnalyzer::analyzeFrameResult
TemplateMatcher::MythPlayerInited(MythPlayer *_player,
        long long nframes)
{
    player = _player;
    fps = player->GetFrameRate();

    if (!(tmpl = templateFinder->getTemplate(&tmplrow, &tmplcol,
                    &tmplwidth, &tmplheight)))
    {
        VERBOSE(VB_COMMFLAG, QString("TemplateMatcher::MythPlayerInited:"
                    " no template"));
        return ANALYZE_FATAL;
    }

    if (avpicture_alloc(&cropped, PIX_FMT_GRAY8, tmplwidth, tmplheight))
    {
        VERBOSE(VB_COMMFLAG, QString("TemplateMatcher::MythPlayerInited "
                "avpicture_alloc cropped (%1x%2) failed").
                arg(tmplwidth).arg(tmplheight));
        return ANALYZE_FATAL;
    }

    if (pgmConverter->MythPlayerInited(player))
        goto free_cropped;

    matches = new unsigned short[nframes];
    memset(matches, 0, nframes * sizeof(*matches));

    match = new unsigned char[nframes];

    if (debug_matches)
    {
        if (readMatches(debugdata, matches, nframes))
        {
            VERBOSE(VB_COMMFLAG, QString(
                        "TemplateMatcher::MythPlayerInited read %1")
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
    const int       MINSEGLEN = (int)roundf(105 * fps);    /* frames */

    int                                 tmpledges, mintmpledges;
    int                                 minbreaklen, minseglen;
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
    mintmpledges = pick_mintmpledges(matches, nframes);

    VERBOSE(VB_COMMFLAG, QString("TemplateMatcher::finished %1x%2@(%3,%4),"
                " %5 edge pixels, want %6")
            .arg(tmplwidth).arg(tmplheight).arg(tmplcol).arg(tmplrow)
            .arg(tmpledges).arg(mintmpledges));

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
    minbreaklen = 1;
    minseglen = 1;
    for (;;)
    {
        bool f1 = false;
        bool f2 = false;
        if (minbreaklen <= MINBREAKLEN)
        {
            f1 = removeShortBreaks(&breakMap, fps, minbreaklen,
                    debug_removerunts);
            minbreaklen++;
        }
        if (minseglen <= MINSEGLEN)
        {
            f2 = removeShortSegments(&breakMap, nframes, fps, minseglen,
                    debug_removerunts);
            minseglen++;
        }
        if (minbreaklen > MINBREAKLEN && minseglen > MINSEGLEN)
            break;
        if (debug_removerunts && (f1 || f2))
            frameAnalyzerReportMap(&breakMap, fps, "** TM Break");
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
     * TEMPLATE_DISAPPEARS_LATE: Handle the scenario where the template
     * disappears "late" after having already switched to commercial (template
     * presence extends into commercial break). Accelerate the beginning of the
     * commercial break by searching backwards to find a blank frame.
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
     * TEMPLATE_REAPPEARS_EARLY: Handle the scenario where the template
     * reappears "early" before resuming the broadcast. Delay the beginning of
     * the broadcast by searching forwards to find a blank frame.
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
    const int TEMPLATE_REAPPEARS_LATE = (int)roundf(35 * fps);
    const int TEMPLATE_REAPPEARS_EARLY = (int)roundf(1.5 * fps);

    VERBOSE(VB_COMMFLAG, QString("TemplateMatcher adjusting for blanks"));

    FrameAnalyzer::FrameMap::Iterator ii = breakMap.begin();
    long long prevbrke = 0;
    while (ii != breakMap.end())
    {
        FrameAnalyzer::FrameMap::Iterator iinext = ii;
        ++iinext;

        /*
         * Where the template disappears, look for nearby blank frames. Only
         * adjust beginning-of-breaks that are not at the very beginning. Do
         * not search into adjacent breaks.
         */
        const long long brkb = ii.key();
        const long long brke = brkb + *ii;
        FrameAnalyzer::FrameMap::const_iterator jj = blankMap->constEnd();
        if (brkb > 0)
        {
            jj = frameMapSearchForwards(blankMap,
                max(prevbrke,
                    brkb - max(BLANK_NEARBY, TEMPLATE_DISAPPEARS_LATE)),
                min(brke,
                    brkb + max(BLANK_NEARBY, TEMPLATE_DISAPPEARS_EARLY)));
        }
        long long newbrkb = brkb;
        if (jj != blankMap->constEnd())
        {
            newbrkb = jj.key();
            long long adj = *jj / 2;
            if (adj > MAX_BLANK_FRAMES)
                adj = MAX_BLANK_FRAMES;
            newbrkb += adj;
        }

        /*
         * Where the template reappears, look for nearby blank frames. Do not
         * search into adjacent breaks.
         */
        FrameAnalyzer::FrameMap::const_iterator kk = frameMapSearchBackwards(
            blankMap,
            max(newbrkb,
                brke - max(BLANK_NEARBY, TEMPLATE_REAPPEARS_LATE)),
            min(iinext == breakMap.end() ? nframes : iinext.key(),
                brke + max(BLANK_NEARBY, TEMPLATE_REAPPEARS_EARLY)));
        long long newbrke = brke;
        if (kk != blankMap->constEnd())
        {
            newbrke = kk.key();
            long long adj = *kk;
            newbrke += adj;
            adj /= 2;
            if (adj > MAX_BLANK_FRAMES)
                adj = MAX_BLANK_FRAMES;
            newbrke -= adj;
        }

        /*
         * Adjust breakMap for blank frames.
         */
        long long newbrklen = newbrke - newbrkb;
        if (newbrkb != brkb)
        {
            breakMap.erase(ii);
            if (newbrkb < nframes && newbrklen)
                breakMap.insert(newbrkb, newbrklen);
        }
        else if (newbrke != brke)
        {
            if (newbrklen)
            {
                breakMap.remove(newbrkb);
                breakMap.insert(newbrkb, newbrklen);
            }
            else
                breakMap.erase(ii);
        }

        prevbrke = newbrke;
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
        breaks->insert(bb.key(), *bb);
    }
    return 0;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
