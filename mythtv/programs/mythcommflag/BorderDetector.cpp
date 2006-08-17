#include <sys/time.h>

#include "avcodec.h"        /* AVPicture */
#include "mythcontext.h"    /* gContext */

#include "CommDetector2.h"
#include "TemplateFinder.h"
#include "BorderDetector.h"

using namespace commDetector2;

BorderDetector::BorderDetector(void)
    : logoFinder(NULL)
    , logo(NULL)
    , frameno(-1)
    , ismonochromatic(false)
    , debugLevel(0)
    , time_reported(false)
{
    debugLevel = gContext->GetNumSetting("BorderDetectorDebugLevel", 0);

    if (debugLevel >= 1)
        VERBOSE(VB_COMMFLAG,
            QString("BorderDetector debugLevel %1").arg(debugLevel));
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

void
BorderDetector::setLogoState(TemplateFinder *finder)
{
    if ((logoFinder = finder) && (logo = logoFinder->getTemplate(
                    &logorr1, &logocc1, &logowidth, &logoheight)))
    {
        logorr2 = logorr1 + logoheight - 1;
        logocc2 = logocc1 + logowidth - 1;

        VERBOSE(VB_COMMFLAG, QString(
                    "BorderDetector::setLogoState: %1x%2@(%3,%4)")
            .arg(logowidth).arg(logoheight).arg(logocc1).arg(logorr1));
    }
}

int
BorderDetector::getDimensions(const AVPicture *pgm, int pgmheight,
        long long _frameno, int *prow, int *pcol, int *pwidth, int *pheight)
{
    /*
     * The basic algorithm is to look for pixels of the same color along all
     * four borders of the frame, working inwards until the pixels cease to be
     * of uniform color. This way, letterboxing/pillarboxing bars can be of any
     * color (varying shades of black-grey).
     *
     * If there is a logo, exclude its area from border detection.
     *
     * Return 0 for normal frames; non-zero for monochromatic frames.
     */

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
    static const unsigned char  MAXRANGE = 8;

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
    const int               HORIZSLOP = max(MAXLINES, pgmwidth * 5 / 1000);

    struct timeval          start, end, elapsed;
    unsigned char           minval, maxval, val;
    int                     rr, cc, minrow, maxrow, mincol, maxcol, rr2, cc2;
    int                     newrow, newcol, newwidth, newheight;
    bool                    top, bottom, left, right;
    int                     range, outliers, lines;

    (void)gettimeofday(&start, NULL);

    newrow = 0;
    newcol = 0;
    newwidth = 0;
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
                if (logo && rr >= logorr1 && rr <= logorr2 &&
                        cc >= logocc1 && cc <= logocc2)
                    continue;   /* Exclude logo area from analysis. */

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
        newwidth = max(0, maxcol - HORIZSLOP - newcol + 1);
        if (newcol > maxcol)
            goto monochromatic_frame;

        /*
         * Find right edge. Keep same minval/maxval as left edge. Also, right
         * pillarbox should be roughly the same size as the left pillarbox.
         */
        lines = 0;
        cc2 = max(newcol, pgmwidth - newcol);
        for (cc = maxcol; cc > cc2; cc--)
        {
            outliers = 0;
            for (rr = minrow; rr <= maxrow; rr++)
            {
                if (logo && rr >= logorr1 && rr <= logorr2 &&
                        cc >= logocc1 && cc <= logocc2)
                    continue;   /* Exclude logo area from analysis. */

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
            goto monochromatic_frame;

        if (top || bottom)
            break;  /* No need to repeat letterboxing check. */

        /* Find top edge. */
        minval = UCHAR_MAX;
        maxval = 0;
        lines = 0;
        cc2 = newcol + newwidth;
        for (rr = minrow; rr <= maxrow; rr++)
        {
            outliers = 0;
            for (cc = newcol; cc < cc2; cc++)
            {
                if (logo && rr >= logorr1 && rr <= logorr2 &&
                        cc >= logocc1 && cc <= logocc2)
                    continue;   /* Exclude logo area from analysis. */

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
        newheight = max(0, maxrow - VERTSLOP - newrow + 1);
        if (newrow > maxrow)
            goto monochromatic_frame;

        /*
         * Find bottom edge. Keep same minval/maxval as top edge. Also, bottom
         * letterbox should be roughly the same size as the top letterbox.
         */
        lines = 0;
        rr2 = max(newrow, pgmheight - newrow);
        cc2 = newcol + newwidth;
        for (rr = maxrow; rr > rr2; rr--)
        {
            outliers = 0;
            for (cc = newcol; cc < cc2; cc++)
            {
                if (logo && rr >= logorr1 && rr <= logorr2 &&
                        cc >= logocc1 && cc <= logocc2)
                    continue;   /* Exclude logo area from analysis. */

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
            goto monochromatic_frame;

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

    frameno = _frameno;
    row = newrow;
    col = newcol;
    width = newwidth;
    height = newheight;
    ismonochromatic = false;
    goto done;

monochromatic_frame:
    frameno = _frameno;
    row = newrow;
    col = newcol;
    width = newwidth;
    height = newheight;
    ismonochromatic = true;

done:
    *prow = row;
    *pcol = col;
    *pwidth = width;
    *pheight = height;

    (void)gettimeofday(&end, NULL);
    timersub(&end, &start, &elapsed);
    timeradd(&analyze_time, &elapsed, &analyze_time);

    return ismonochromatic ? -1 : 0;
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
