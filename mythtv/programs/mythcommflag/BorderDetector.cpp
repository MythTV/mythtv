#include <sys/time.h>

#include "mythconfig.h"

extern "C" {
#include "libavcodec/avcodec.h"        /* AVPicture */
}
#include "mythcorecontext.h"    /* gContext */
#include "compat.h"

#include "CommDetector2.h"
#include "FrameAnalyzer.h"
#include "TemplateFinder.h"
#include "BorderDetector.h"

using namespace frameAnalyzer;
using namespace commDetector2;

BorderDetector::BorderDetector(void)
    : logoFinder(NULL),
      logo(NULL),
      logorow(-1),
      logocol(-1),
      logowidth(-1),
      logoheight(-1),
      frameno(-1),
      row(-1),
      col(-1),
      width(-1),
      height(-1),
      ismonochromatic(false),
      debugLevel(0),
      time_reported(false)
{
    memset(&analyze_time, 0, sizeof(analyze_time));
    debugLevel = gCoreContext->GetNumSetting("BorderDetectorDebugLevel", 0);

    if (debugLevel >= 1)
        LOG(VB_COMMFLAG, LOG_INFO,
            QString("BorderDetector debugLevel %1").arg(debugLevel));
}

int
BorderDetector::MythPlayerInited(const MythPlayer *player)
{
    (void)player;  /* gcc */
    time_reported = false;
    memset(&analyze_time, 0, sizeof(analyze_time));
    return 0;
}

void
BorderDetector::setLogoState(TemplateFinder *finder)
{
    if ((logoFinder = finder) && (logo = logoFinder->getTemplate(
                    &logorow, &logocol, &logowidth, &logoheight)))
    {
        LOG(VB_COMMFLAG, LOG_INFO,
            QString("BorderDetector::setLogoState: %1x%2@(%3,%4)")
                .arg(logowidth).arg(logoheight).arg(logocol).arg(logorow));
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
     * TUNABLES:
     *
     * Higher values mean more tolerance for noise (e.g., analog recordings).
     * However, in the absence of noise, content/logos can be cropped away from
     * analysis.
     *
     * Lower values mean less tolerance for noise. In a noisy recording, the
     * transition between pillarbox/letterbox black to content color will be
     * detected as an edge, and thwart logo edge detection. In the absence of
     * noise, content/logos will be more faithfully analyzed.
     */

    /*
     * TUNABLE: The maximum range of values allowed for
     * letterboxing/pillarboxing bars. Usually the bars are black (0x00), but
     * sometimes they are grey (0x71). Sometimes the letterboxing and
     * pillarboxing (when one is embedded inside the other) are different
     * colors.
     */
    static const unsigned char  MAXRANGE = 32;

    /*
     * TUNABLE: The maximum number of consecutive rows or columns with too many
     * outlier points that may be scanned before declaring the existence of a
     * border.
     */
    static const int        MAXLINES = 2;

    const int               pgmwidth = pgm->linesize[0];

    /*
     * TUNABLE: The maximum number of outlier points in a single row or column
     * with grey values outside of MAXRANGE before declaring the existence of a
     * border.
     */
    const int               MAXOUTLIERS = pgmwidth * 12 / 1000;

    /*
     * TUNABLE: Margins to avoid noise at the extreme edges of the signal
     * (VBI?). (Really, just a special case of VERTSLOP and HORIZSLOP, below.)
     */
    const int               VERTMARGIN = max(2, pgmheight * 1 / 60);
    const int               HORIZMARGIN = max(2, pgmwidth * 1 / 80);

    /*
     * TUNABLE: Slop to accommodate any jagged letterboxing/pillarboxing edges,
     * or noise between edges and content. (Really, a more general case of
     * VERTMARGIN and HORIZMARGIN, above.)
     */
    const int               VERTSLOP = max(MAXLINES, pgmheight * 1 / 120);
    const int               HORIZSLOP = max(MAXLINES, pgmwidth * 1 / 160);

    struct timeval          start, end, elapsed;
    unsigned char           minval, maxval, val;
    int                     rr, cc, minrow, mincol, maxrow1, maxcol1, saved;
    int                     newrow, newcol, newwidth, newheight;
    bool                    top, bottom, left, right, inrange;
    int                     range, outliers, lines;

    (void)gettimeofday(&start, NULL);

    if (_frameno != UNCACHED && _frameno == frameno)
        goto done;

    top = false;
    bottom = false;
    left = false;
    right = false;

    minrow = VERTMARGIN;
    maxrow1 = pgmheight - VERTMARGIN;   /* maxrow + 1 */

    mincol = HORIZMARGIN;
    maxcol1 = pgmwidth - HORIZMARGIN;   /* maxcol + 1 */

    newrow = minrow - 1;
    newcol = mincol - 1;
    newwidth = maxcol1 + 1 - mincol;
    newheight = maxrow1 + 1 - minrow;

    for (;;)
    {
        /* Find left edge. */
        left = false;
        minval = UCHAR_MAX;
        maxval = 0;
        lines = 0;
        saved = mincol;
        for (cc = mincol; cc < maxcol1; cc++)
        {
            outliers = 0;
            inrange = true;
            for (rr = minrow; rr < maxrow1; rr++)
            {
                if (logo && rrccinrect(rr, cc, logorow, logocol,
                            logowidth, logoheight))
                    continue;   /* Exclude logo area from analysis. */

                val = pgm->data[0][rr * pgmwidth + cc];
                range = max(maxval, val) - min(minval, val) + 1;
                if (range > MAXRANGE)
                {
                    if (outliers++ < MAXOUTLIERS)
                        continue;   /* Next row. */
                    inrange = false;
                    if (lines++ < MAXLINES)
                        break;  /* Next column. */
                    goto found_left;
                }
                if (val < minval)
                    minval = val;
                if (val > maxval)
                    maxval = val;
            }
            if (inrange)
            {
                saved = cc;
                lines = 0;
            }
        }
found_left:
        if (newcol != saved + 1 + HORIZSLOP)
        {
            newcol = min(maxcol1, saved + 1 + HORIZSLOP);
            newwidth = max(0, maxcol1 - newcol);
            left = true;
        }

        if (!newwidth)
            goto monochromatic_frame;

        mincol = newcol;

        /*
         * Find right edge. Keep same minval/maxval (pillarboxing colors) as
         * left edge.
         */
        right = false;
        lines = 0;
        saved = maxcol1 - 1;
        for (cc = maxcol1 - 1; cc >= mincol; cc--)
        {
            outliers = 0;
            inrange = true;
            for (rr = minrow; rr < maxrow1; rr++)
            {
                if (logo && rrccinrect(rr, cc, logorow, logocol,
                            logowidth, logoheight))
                    continue;   /* Exclude logo area from analysis. */

                val = pgm->data[0][rr * pgmwidth + cc];
                range = max(maxval, val) - min(minval, val) + 1;
                if (range > MAXRANGE)
                {
                    if (outliers++ < MAXOUTLIERS)
                        continue;   /* Next row. */
                    inrange = false;
                    if (lines++ < MAXLINES)
                        break;  /* Next column. */
                    goto found_right;
                }
                if (val < minval)
                    minval = val;
                if (val > maxval)
                    maxval = val;
            }
            if (inrange)
            {
                saved = cc;
                lines = 0;
            }
        }
found_right:
        if (newwidth != saved - mincol - HORIZSLOP)
        {
            newwidth = max(0, saved - mincol - HORIZSLOP);
            right = true;
        }

        if (!newwidth)
            goto monochromatic_frame;

        if (top || bottom)
            break;  /* Do not repeat letterboxing check. */

        maxcol1 = mincol + newwidth;

        /* Find top edge. */
        top = false;
        minval = UCHAR_MAX;
        maxval = 0;
        lines = 0;
        saved = minrow;
        for (rr = minrow; rr < maxrow1; rr++)
        {
            outliers = 0;
            inrange = true;
            for (cc = mincol; cc < maxcol1; cc++)
            {
                if (logo && rrccinrect(rr, cc, logorow, logocol,
                            logowidth, logoheight))
                    continue;   /* Exclude logo area from analysis. */

                val = pgm->data[0][rr * pgmwidth + cc];
                range = max(maxval, val) - min(minval, val) + 1;
                if (range > MAXRANGE)
                {
                    if (outliers++ < MAXOUTLIERS)
                        continue;   /* Next column. */
                    inrange = false;
                    if (lines++ < MAXLINES)
                        break;  /* Next row. */
                    goto found_top;
                }
                if (val < minval)
                    minval = val;
                if (val > maxval)
                    maxval = val;
            }
            if (inrange)
            {
                saved = rr;
                lines = 0;
            }
        }
found_top:
        if (newrow != saved + 1 + VERTSLOP)
        {
            newrow = min(maxrow1, saved + 1 + VERTSLOP);
            newheight = max(0, maxrow1 - newrow);
            top = true;
        }

        if (!newheight)
            goto monochromatic_frame;

        minrow = newrow;

        /* Find bottom edge. Keep same minval/maxval as top edge. */
        bottom = false;
        lines = 0;
        saved = maxrow1 - 1;
        for (rr = maxrow1 - 1; rr >= minrow; rr--)
        {
            outliers = 0;
            inrange = true;
            for (cc = mincol; cc < maxcol1; cc++)
            {
                if (logo && rrccinrect(rr, cc, logorow, logocol,
                            logowidth, logoheight))
                    continue;   /* Exclude logo area from analysis. */

                val = pgm->data[0][rr * pgmwidth + cc];
                range = max(maxval, val) - min(minval, val) + 1;
                if (range > MAXRANGE)
                {
                    if (outliers++ < MAXOUTLIERS)
                        continue;   /* Next column. */
                    inrange = false;
                    if (lines++ < MAXLINES)
                        break;  /* Next row. */
                    goto found_bottom;
                }
                if (val < minval)
                    minval = val;
                if (val > maxval)
                    maxval = val;
            }
            if (inrange)
            {
                saved = rr;
                lines = 0;
            }
        }
found_bottom:
        if (newheight != saved - minrow - VERTSLOP)
        {
            newheight = max(0, saved - minrow - VERTSLOP);
            bottom = true;
        }

        if (!newheight)
            goto monochromatic_frame;

        if (left || right)
            break;  /* Do not repeat pillarboxing check. */

        maxrow1 = minrow + newheight;
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
        LOG(VB_COMMFLAG, LOG_INFO, QString("BD Time: analyze=%1s")
                .arg(strftimeval(&analyze_time)));
        time_reported = true;
    }
    return 0;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
