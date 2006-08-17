#include <sys/time.h>

#include "avcodec.h"        /* AVPicture */
#include "mythcontext.h"    /* gContext */

#include "CommDetector2.h"
#include "BorderDetector.h"

using namespace commDetector2;

BorderDetector::BorderDetector(void)
    : frameno(-1)
    , debugLevel(0)
    , time_reported(false)
{
    debugLevel = gContext->GetNumSetting("BorderDetectorDebugLevel", 0);
}

BorderDetector::~BorderDetector(void)
{
}

int
BorderDetector::nuppelVideoPlayerInited(const NuppelVideoPlayer *nvp)
{
    (void)nvp;  /* gcc */
    time_reported = false;
    memset(&analyze_time, 0, sizeof(analyze_time));
    return 0;
}

int
BorderDetector::getDimensions(const AVPicture *pgm, int pgmheight,
        long long _frameno, int *prow, int *pcol, int *pwidth, int *pheight)
{
    /*
     * TUNABLE:
     *
     * The maximum range of values allowed for letterboxing/pillarboxing bars.
     * Usually the bars are black (0x00), but sometimes they are grey (0x71).
     * Sometimes the letterboxing and pillarboxing (when one is embedded inside
     * the other) are different colors.
     *
     * Higher values mean more tolerance for noise. This means that
     * letterboxing/pillarboxing might be over-cropped, which might cause faint
     * template edges to also be cropped out of consideration.
     *
     * Lower values mean less tolerance for noise, which means that
     * letterboxing/pillarboxing might not be detected. If this happens, the
     * letterboxing/pillarboxing edges will make their way into the
     * edge-detection phase where they are likely to take edge pixels away from
     * possible template edges.
     */
    static const unsigned char  MAXRANGE = 32;

    /*
     * TUNABLE:
     *
     * The maximum number of consecutive rows or columns with too many outlier
     * points that may be scanned before declaring the existence of a border.
     *
     * Higher values mean more tolerance for noise, but content might be
     * cropped.
     *
     * Lower values mean less tolerance for noise, but noise might interfere
     * with letterbox/pillarbox detection.
     */
    static const int        MAXLINES = 2;

    const int               pgmwidth = pgm->linesize[0];

    /*
     * TUNABLE:
     *
     * The maximum number of outlier points in a single row or column with grey
     * values outside of MAXRANGE before declaring the existence of a border.
     *
     * Higher values mean more tolerance for noise, but content might be
     * cropped.
     *
     * Lower values mean less tolerance for noise, but noise might interfere
     * with letterbox/pillarbox detection.
     */
    const int               MAXOUTLIERS = pgmwidth * 25 / 1000;

    /*
     * TUNABLE:
     *
     * Margins to avoid noise at the extreme edges of the signal (VBI?).
     */
    const int               VERTMARGIN = max(2, pgmheight * 1 / 60);
    const int               HORIZMARGIN = max(2, pgmwidth * 125 / 10000);

    /*
     * TUNABLE:
     *
     * Slop to accommodate any jagged letterboxing/pillarboxing edges.
     */
    const int               VERTSLOP = max(MAXLINES, pgmheight * 1 / 120);
    const int               HORIZSLOP = max(MAXLINES, pgmwidth * 125 / 20000);

    struct timeval          start, end, elapsed;
    unsigned char           minval, maxval, val;
    int                     rr, cc, minrow, maxrow, mincol, maxcol;
    int                     newrow, newcol, newwidth, newheight;
    bool                    top, bottom, left, right;
    int                     range, outliers, lines;

    (void)gettimeofday(&start, NULL);

    newrow = 0;
    newheight = 0;

    if (_frameno != UNCACHED && _frameno == frameno)
        goto done;

    top = false;
    bottom = false;
    left = false;
    right = false;

    minrow = VERTMARGIN;
    maxrow = pgmheight - VERTMARGIN - 1;

    mincol = HORIZMARGIN;
    maxcol = pgmwidth - HORIZMARGIN - 1;

    for (;;)
    {
        /* Find left edge. */
        minval = UCHAR_MAX;
        maxval = 0;
        lines = 0;
        for (cc = mincol; cc <= maxcol; cc++)
        {
            outliers = 0;
            for (rr = minrow; rr <= maxrow; rr++)
            {
                val = pgm->data[0][rr * pgmwidth + cc];
                range = max(maxval, val) - min(minval, val);
                if (range > MAXRANGE)
                {
                    if (outliers++ < MAXOUTLIERS)
                        continue;
                    if (lines++ < MAXLINES)
                        break;
                    if (cc != mincol)
                        left = true;
                    goto found_left;
                }
                if (val < minval)
                    minval = val;
                if (val > maxval)
                    maxval = val;
            }
        }
found_left:
        newcol = cc + HORIZSLOP;
        newwidth = maxcol - HORIZSLOP - newcol + 1;
        if (newcol > maxcol)
            goto blank_frame;

        /* Find right edge (keep same minval/maxval as left edge). */
        lines = 0;
        for (cc = maxcol; cc > newcol; cc--)
        {
            outliers = 0;
            for (rr = minrow; rr <= maxrow; rr++)
            {
                val = pgm->data[0][rr * pgmwidth + cc];
                range = max(maxval, val) - min(minval, val);
                if (range > MAXRANGE)
                {
                    if (outliers++ < MAXOUTLIERS)
                        continue;
                    if (lines++ < MAXLINES)
                        break;
                    if (cc != maxcol)
                        right = true;
                    goto found_right;
                }
                if (val < minval)
                    minval = val;
                if (val > maxval)
                    maxval = val;
            }
        }
found_right:
        newwidth = max(0, cc - HORIZSLOP - newcol + 1);

        if (!newwidth)
            goto blank_frame;

        if (top || bottom)
            break;  /* No need to repeat letterboxing check. */

        /* Find top edge. */
        minval = UCHAR_MAX;
        maxval = 0;
        lines = 0;
        for (rr = minrow; rr <= maxrow; rr++)
        {
            outliers = 0;
            for (cc = newcol; cc < newcol + newwidth; cc++)
            {
                val = pgm->data[0][rr * pgmwidth + cc];
                range = max(maxval, val) - min(minval, val);
                if (range > MAXRANGE)
                {
                    if (outliers++ < MAXOUTLIERS)
                        continue;
                    if (lines++ < MAXLINES)
                        break;
                    if (rr != minrow)
                        top = true;
                    goto found_top;
                }
                if (val < minval)
                    minval = val;
                if (val > maxval)
                    maxval = val;
            }
        }
found_top:
        newrow = rr + VERTSLOP;
        newheight = maxrow - VERTSLOP - newrow + 1;
        if (newrow > maxrow)
            goto blank_frame;

        /* Find bottom edge (keep same minval/maxval as top edge). */
        lines = 0;
        for (rr = maxrow; rr > newrow; rr--)
        {
            outliers = 0;
            for (cc = newcol; cc < newcol + newwidth; cc++)
            {
                val = pgm->data[0][rr * pgmwidth + cc];
                range = max(maxval, val) - min(minval, val);
                if (range > MAXRANGE)
                {
                    if (outliers++ < MAXOUTLIERS)
                        continue;
                    if (lines++ < MAXLINES)
                        break;
                    if (rr != maxrow)
                        bottom = true;
                    goto found_bottom;
                }
                if (val < minval)
                    minval = val;
                if (val > maxval)
                    maxval = val;
            }
        }
found_bottom:
        newheight = max(0, rr - VERTSLOP - newrow + 1);

        if (!newheight)
            goto blank_frame;

        if (left || right)
            break;  /* No need to repeat pillarboxing check. */

        if (top || bottom)
        {
            /*
             * Letterboxing was found; repeat loop to look for embedded
             * pillarboxing.
             */
            minrow = newrow;
            maxrow = newrow + newheight - 1;
            continue;
        }

        /* No pillarboxing or letterboxing. */
        break;
    }

    if (debugLevel >= 1)
    {
        if (row != newrow || col != newcol ||
            width != newwidth || height != newheight)
        {
            VERBOSE(VB_COMMFLAG, QString("Frame %1: %2x%3@(%4,%5)")
                    .arg(_frameno, 5)
                    .arg(newwidth).arg(newheight)
                    .arg(newcol).arg(newrow));
        }
    }

    frameno = _frameno;
    row = newrow;
    col = newcol;
    width = newwidth;
    height = newheight;

done:
    *prow = row;
    *pcol = col;
    *pwidth = width;
    *pheight = height;

    (void)gettimeofday(&end, NULL);
    timersub(&end, &start, &elapsed);
    timeradd(&analyze_time, &elapsed, &analyze_time);

    return 0;

blank_frame:
    row = VERTMARGIN;
    col = HORIZMARGIN;
    width = pgmwidth - 2 * HORIZMARGIN;
    height = pgmheight - 2 * VERTMARGIN;
    if (newwidth && (left || right))
    {
        col = newcol;
        width = newwidth;
    }
    if (newheight && (top || bottom))
    {
        row = newrow;
        height = newheight;
    }

    frameno = _frameno;
    *prow = row;
    *pcol = col;
    *pwidth = width;
    *pheight = height;

    if (debugLevel >= 1)
    {
        VERBOSE(VB_COMMFLAG, QString("Frame %1: %2x%3@(%4,%5): blank")
                .arg(_frameno, 5)
                .arg(maxcol - mincol + 1).arg(maxrow - minrow + 1)
                .arg(mincol).arg(minrow));
    }

    (void)gettimeofday(&end, NULL);
    timersub(&end, &start, &elapsed);
    timeradd(&analyze_time, &elapsed, &analyze_time);

    return -1;  /* Blank frame. */
}

int
BorderDetector::reportTime(void)
{
    if (!time_reported)
    {
        VERBOSE(VB_COMMFLAG, QString("BD Time: analyze=%1s")
                .arg(strftimeval(&analyze_time)));
        time_reported = true;
    }
    return 0;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
