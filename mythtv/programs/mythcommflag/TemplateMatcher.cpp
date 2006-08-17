#include <math.h>

#include "NuppelVideoPlayer.h"
#include "avcodec.h"        /* AVPicture */

#include "CommDetector2.h"
#include "pgm.h"
#include "PGMConverter.h"
#include "EdgeDetector.h"
#include "TemplateFinder.h"
#include "TemplateMatcher.h"

/* Debugging */
#include <qfile.h>
#include <qfileinfo.h>

using namespace commDetector2;

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
    QFile qfile(filename);

    if (!qfile.open(IO_ReadOnly))
        return false;

    QTextStream     stream(&qfile);
    long long       ii = 0;
    bool            ok;

    while (!stream.atEnd())
    {
        if (ii >= nframes)
        {
            VERBOSE(VB_COMMFLAG, QString("Too much data in %1")
                    .arg(filename));
            goto error;
        }
        matches[ii++] = stream.readLine().toUShort(&ok);
        if (!ok)
        {
            VERBOSE(VB_COMMFLAG, QString("Error reading %1 at line %2")
                    .arg(filename).arg(ii - 1));
            goto error;
        }
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
writeMatches(QString filename, unsigned short *matches, long long nframes)
{
    QFile qfile(filename);

    if (!qfile.open(IO_WriteOnly))
        return false;

    QTextStream     stream(&qfile);
    long long       ii;

    for (ii = 0; ii < nframes; ii++)
        stream << matches[ii] << "\n";

    qfile.close();
    return true;
}

int 
finishedDebug(PGMConverter *pgmConverter, EdgeDetector *edgeDetector,
        NuppelVideoPlayer *nvp,
        long long nframes, const unsigned char *match,
        const unsigned short *matches, AVPicture *overlay, AVPicture *cropped,
        int tmplrow, int tmplcol, int tmplwidth, int tmplheight,
        bool debug_frames, QString debugdir)
{
    static const int    FRAMESGMPCTILE = 70; /* TemplateMatcher::analyzeFrame */
    const int           width = overlay->linesize[0];
    const int           height = nvp->GetVideoHeight();

    long long           frameno, startframe;
    unsigned short      score, low, high;

    score = matches[0];

    low = score;
    high = score;
    startframe = 0;

    for (frameno = 1; frameno < nframes; frameno++)
    {
        score = matches[frameno];

        if (match[frameno - 1] == match[frameno])
        {
            if (score < low)
                low = score;
            if (score > high)
                high = score;
            if (frameno < nframes - 1)
                continue;
        }

        VERBOSE(VB_COMMFLAG, QString("Frame %1-%2: %3 L-H: %4-%5 (%6)")
                .arg(startframe, 6).arg(frameno - 1, 6)
                .arg(match[frameno - 1] ? "logo        " : "     no-logo")
                .arg(low, 4).arg(high, 4).arg(frameno - startframe, 5));

        low = score;
        high = score;
        startframe = frameno;

        if (debug_frames)
        {
            VideoFrame          *frame;
            const AVPicture     *pgm;
            const AVPicture     *edges;
            int                 pgmwidth, pgmheight;

            frame = nvp->GetRawVideoFrame(frameno);

            if (!(pgm = pgmConverter->getImage(frame, frameno,
                            &pgmwidth, &pgmheight)))
                continue;

            if (pgm_crop(cropped, pgm, pgmheight, tmplrow, tmplcol,
                        tmplwidth, tmplheight))
                continue;

            if (!(edges = edgeDetector->detectEdges(cropped, tmplheight,
                            FRAMESGMPCTILE)))
                continue;

            QString basefilename;
            basefilename.sprintf("%s/tm-%05lld-%d",
                    debugdir.ascii(), frameno, score);
            QFile tfile;

            /* Compose template over frame. Write out and convert to JPG. */

            QString basename = basefilename;
            if (!match[frameno])
                basename += "-c";

            QString jpgfilename(basename + ".jpg");
            QFileInfo jpgfi(jpgfilename);
            QFile pgmfile(basename + ".pgm");

            QString edgefilename(basefilename + "-edges.jpg");
            QFileInfo jpgedges(edgefilename);
            QFile pgmedges(basefilename + "-edges.pgm");

            if (pgm_overlay(overlay, pgm, height, tmplrow, tmplcol,
                        edges, tmplheight))
                continue;

            if (!jpgfi.exists())
            {
                if (!pgmfile.exists() && pgm_write(overlay->data[0],
                            width, height, pgmfile.name().ascii()))
                    continue;
                if (myth_system(QString(
                                "convert -quality 50 -resize 192x144 %1 %2")
                            .arg(pgmfile.name()).arg(jpgfi.filePath())))
                    continue;
            }

            if (!jpgedges.exists())
            {
                if (!pgmedges.exists() && pgm_write(edges->data[0],
                            tmplwidth, tmplheight, pgmedges.name().ascii()))
                    continue;
                if (myth_system(QString(
                                "convert -quality 50 -resize 192x144 %1 %2")
                            .arg(pgmedges.name()).arg(jpgedges.filePath())))
                    continue;
            }

            /* Delete any existing PGM files to save disk space. */
            tfile.setName(basefilename + ".pgm");
            if (tfile.exists() && !tfile.remove())
            {
                VERBOSE(VB_COMMFLAG, QString("Error removing %1 (%2)")
                        .arg(tfile.name()).arg(strerror(errno)));
                continue;
            }
            tfile.setName(basefilename + "-c.pgm");
            if (tfile.exists() && !tfile.remove())
            {
                VERBOSE(VB_COMMFLAG, QString("Error removing %1 (%2)")
                        .arg(tfile.name()).arg(strerror(errno)));
                continue;
            }
            tfile.setName(basefilename + "-edges.pgm");
            if (tfile.exists() && !tfile.remove())
            {
                VERBOSE(VB_COMMFLAG, QString("Error removing %1 (%2)")
                        .arg(tfile.name()).arg(strerror(errno)));
                continue;
            }
        }
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
    , iscontent(NULL)
    , debugLevel(0)
    , debugdir(debugdir)
#ifdef PGM_CONVERT_GREYSCALE
    , debugdata(debugdir + "/TemplateMatcher-pgm.txt")
#else  /* !PGM_CONVERT_GREYSCALE */
    , debugdata(debugdir + "/TemplateMatcher-yuv.txt")
#endif /* !PGM_CONVERT_GREYSCALE */
    , debug_frames(false)
    , nvp(NULL)
    , debug_matches(false)
    , matches_done(false)
{
    memset(&cropped, 0, sizeof(cropped));
    memset(&overlay, 0, sizeof(overlay));
    memset(&analyze_time, 0, sizeof(analyze_time));

    /*
     * debugLevel:
     *      0: no debugging
     *      1: cache frame edge counts into debugdir [1 file]
     *      2: extra verbosity [O(nframes)]
     *      3: dump overlay frames into debugdir [O(nframes)]
     */
    debugLevel = gContext->GetNumSetting("TemplateMatcherDebugLevel", 0);

    if (debugLevel >= 1)
        createDebugDirectory(debugdir,
            QString("TemplateMatcher debugLevel %1").arg(debugLevel));

    if (debugLevel >= 1)
        debug_matches = true;

    if (debugLevel >= 3)
        debug_frames = true;
}

TemplateMatcher::~TemplateMatcher(void)
{
    avpicture_free(&overlay);

    if (matches)
        delete []matches;
    if (match)
        delete []match;
    if (iscontent)
        delete []iscontent;
    avpicture_free(&cropped);
}

enum FrameAnalyzer::analyzeFrameResult
TemplateMatcher::nuppelVideoPlayerInited(NuppelVideoPlayer *_nvp)
{
    /*
     * TUNABLE:
     *
     * Percent of template's edges that must be covered by candidate frame's
     * test area to be considered a match.
     *
     * Higher values have tighter matching requirements, but can cause
     * legitimate template-matching frames to be passed over (for example, if
     * the template is fading into or out of existence in a sequence of
     * frames).
     *
     * Lower values relax matching requirements, but can yield false
     * identification of template-matching frames when the scene just happens
     * to have lots of edges in the same region of the frame.
     */
    const float     MINMATCHPCT = 0.559670;

    const int       width = _nvp->GetVideoWidth();
    const int       height = _nvp->GetVideoHeight();

    int             tmpledges;

    nvp = _nvp;
    nframes = nvp->GetTotalFrameCount();
    fps = nvp->GetFrameRate();

    if (!(tmpl = templateFinder->getTemplate(&tmplrow, &tmplcol,
                    &tmplwidth, &tmplheight)))
    {
        VERBOSE(VB_COMMFLAG, QString("TemplateMatcher::nuppelVideoPlayerInited:"
                    " no template"));
        return ANALYZE_FATAL;
    }

    tmpledges = pgm_set(tmpl, tmplheight);
    mintmpledges = (int)roundf(tmpledges * MINMATCHPCT);

    VERBOSE(VB_COMMFLAG, QString("TemplateMatcher::nuppelVideoPlayerInited "
                "%1x%2@(%3,%4), %5 edge pixels (want %6)")
            .arg(tmplwidth).arg(tmplheight).arg(tmplcol).arg(tmplrow)
            .arg(tmpledges).arg(mintmpledges));

    if (debug_frames)
    {
        if (avpicture_alloc(&overlay, PIX_FMT_GRAY8, width, height))
        {
            VERBOSE(VB_COMMFLAG, QString(
                        "TemplateMatcher::nuppelVideoPlayerInited "
                    "avpicture_alloc overlay (%1x%2) failed")
                    .arg(width).arg(height));
            return ANALYZE_FATAL;
        }
    }

    if (avpicture_alloc(&cropped, PIX_FMT_GRAY8, tmplwidth, tmplheight))
    {
        VERBOSE(VB_COMMFLAG, QString("TemplateMatcher::nuppelVideoPlayerInited "
                "avpicture_alloc cropped (%1x%2) failed").
                arg(tmplwidth).arg(tmplheight));
        goto free_overlay;
    }

    if (pgmConverter->nuppelVideoPlayerInited(nvp))
        goto free_cropped;

    matches = new unsigned short[nframes];
    memset(matches, 0, nframes * sizeof(*matches));

    match = new unsigned char[nframes];
    iscontent = new unsigned char[nframes];

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
free_overlay:
    avpicture_free(&overlay);
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
    const int           FRAMESGMPCTILE = 70;    /* sync with finishedDebug */

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
            .arg(frameno).arg(nframes));
    return ANALYZE_ERROR;
}

int
TemplateMatcher::finished(void)
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

    long long       ii, segb, sege, seglen, brkb, brke, brklen;
    QString         analyzestr;
    QMap<long long, long long>  breakSegment; /* frameno => len */
    QMap<long long, long long>::Iterator bb, bb1;

    if (pgmConverter->finished())
        goto error;

    if (!matches_done && debug_matches)
    {
        if (writeMatches(debugdata, matches, nframes))
        {
            VERBOSE(VB_COMMFLAG, QString("TemplateMatcher::finished wrote %1")
                    .arg(debugdata));
            matches_done = true;
        }
    }

    for (ii = 0; ii < nframes; ii++)
        match[ii] = matches[ii] >= mintmpledges ? 1 : 0;

    if (debugLevel >= 2)
    {
        if (finishedDebug(pgmConverter, edgeDetector, nvp,
                    nframes, match, matches, &overlay, &cropped,
                    tmplrow, tmplcol, tmplwidth, tmplheight,
                    debug_frames, debugdir))
            goto error;
    }

    /*
     * Construct breaks.
     */
    brkb = 0;
    while (brkb < nframes)
    {
        /* Find break. */
        if (match[brkb])
            brkb = matchspn(nframes, match, brkb, match[brkb]);

        brke = matchspn(nframes, match, brkb, match[brkb]);
        brklen = brke - brkb;
        breakSegment[brkb] = brklen;

        brkb = brke;
    }

    /*
     * Eliminate false breaks.
     */
    bb = breakSegment.begin();
    while (bb != breakSegment.end())
    {
        if (bb.data() >= MINBREAKLEN)
        {
            ++bb;
            continue;
        }

        bb1 = bb;
        ++bb;
        breakSegment.remove(bb1);
    }

    /*
     * Eliminate false segments.
     */
    segb = 0;
    bb = breakSegment.begin();
    while (bb != breakSegment.end())
    {
        brkb = bb.key();
        seglen = brkb - segb;
        if (seglen >= MINSEGLEN)
        {
            segb = brkb + bb.data();
            ++bb;
            continue;
        }

        /* Merge break with next break. */
        bb1 = bb;
        ++bb1;
        if (bb1 == breakSegment.end())
        {
            /* Extend break to end of recording. */
            breakSegment[brkb] = nframes - brkb;
        }
        else
        {
            /* Extend break to cover next break; delete next break. */
            breakSegment[brkb] = bb1.key() + bb1.data() - brkb;  /* bb */
            bb = bb1;
            ++bb1;
            breakSegment.remove(bb);
        }
        bb = bb1;
    }

    /*
     * Report breaks.
     */
    bb = breakSegment.begin();
    while (bb != breakSegment.end())
    {
        brkb = bb.key();
        brklen = bb.data();
        brke = brkb + brklen;

        VERBOSE(VB_COMMFLAG, QString("TM Break: frame %1-%2 (%3-%4, %5)")
                .arg(brkb, 6).arg(brke - 1, 6)
                .arg(frameToTimestamp(brkb, fps))
                .arg(frameToTimestamp(brke - 1, fps))
                .arg(frameToTimestamp(brklen, fps)));
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

    VERBOSE(VB_COMMFLAG, QString("TM Time: analyze=%1s")
            .arg(analyzestr.sprintf("%ld.%06ld",
                    analyze_time.tv_sec, analyze_time.tv_usec)));

    return 0;

error:
    return -1;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
