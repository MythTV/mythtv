#include <sys/time.h>
#include <algorithm>

#include "libmythbase/mythconfig.h"

extern "C" {
#include "libavcodec/avcodec.h"        /* AVFrame */
}

// MythTV
#include "libmythbase/compat.h"
#include "libmythbase/mythchrono.h"
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythlogging.h"

// Commercial Flagging headers
#include "BorderDetector.h"
#include "CommDetector2.h"
#include "FrameAnalyzer.h"
#include "TemplateFinder.h"

using namespace frameAnalyzer;
using namespace commDetector2;

BorderDetector::BorderDetector(void)
{
    m_debugLevel = gCoreContext->GetNumSetting("BorderDetectorDebugLevel", 0);

    if (m_debugLevel >= 1)
        LOG(VB_COMMFLAG, LOG_INFO,
            QString("BorderDetector debugLevel %1").arg(m_debugLevel));
}

int
BorderDetector::MythPlayerInited([[maybe_unused]] const MythPlayer *player)
{
    m_timeReported = false;
    memset(&m_analyzeTime, 0, sizeof(m_analyzeTime));
    return 0;
}

void
BorderDetector::setLogoState(TemplateFinder *finder)
{
    m_logoFinder = finder;
    if (m_logoFinder == nullptr)
        return;
    m_logo = m_logoFinder->getTemplate(&m_logoRow, &m_logoCol,
                                       &m_logoWidth, &m_logoHeight);
    if (m_logo == nullptr)
        return;
    LOG(VB_COMMFLAG, LOG_INFO,
        QString("BorderDetector::setLogoState: %1x%2@(%3,%4)")
        .arg(m_logoWidth).arg(m_logoHeight).arg(m_logoCol).arg(m_logoRow));
}

void
BorderDetector::getDimensionsReal(const AVFrame *pgm, int pgmheight,
        long long _frameno)
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
    static constexpr unsigned char kMaxRange = 32;

    /*
     * TUNABLE: The maximum number of consecutive rows or columns with too many
     * outlier points that may be scanned before declaring the existence of a
     * border.
     */
    static constexpr int    kMaxLines = 2;

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
    const int               VERTMARGIN = std::max(2, pgmheight * 1 / 60);
    const int               HORIZMARGIN = std::max(2, pgmwidth * 1 / 80);

    /*
     * TUNABLE: Slop to accommodate any jagged letterboxing/pillarboxing edges,
     * or noise between edges and content. (Really, a more general case of
     * VERTMARGIN and HORIZMARGIN, above.)
     */
    const int               VERTSLOP = std::max(kMaxLines, pgmheight * 1 / 120);
    const int               HORIZSLOP = std::max(kMaxLines, pgmwidth * 1 / 160);

    int minrow    = VERTMARGIN;
    int mincol    = HORIZMARGIN;
    int maxrow1   = pgmheight - VERTMARGIN;   /* maxrow + 1 */
    int maxcol1   = pgmwidth - HORIZMARGIN;   /* maxcol + 1 */
    int newrow    = minrow - 1;
    int newcol    = mincol - 1;
    int newwidth  = maxcol1 + 1 - mincol;
    int newheight = maxrow1 + 1 - minrow;
    bool top    = false;
    bool bottom = false;
    bool monochromatic = false;

    try
    {
        for (;;)
        {
            /* Find left edge. */
            bool left = false;
            uchar minval = UCHAR_MAX;
            uchar maxval = 0;
            int lines = 0;
            int saved = mincol;
            bool found = false;
            for (int cc = mincol; !found && cc < maxcol1; cc++)
            {
                int outliers = 0;
                bool inrange = true;
                for (int rr = minrow; rr < maxrow1; rr++)
                {
                    if (m_logo && rrccinrect(rr, cc, m_logoRow, m_logoCol,
                                m_logoWidth, m_logoHeight))
                        continue;   /* Exclude logo area from analysis. */

                    uchar val = pgm->data[0][(rr * pgmwidth) + cc];
                    int range = std::max(maxval, val) - std::min(minval, val) + 1;
                    if (range > kMaxRange)
                    {
                        if (outliers++ < MAXOUTLIERS)
                            continue;   /* Next row. */
                        inrange = false;
                        if (lines++ >= kMaxLines)
                            found = true;
                        break;  /* Next column. */
                    }
                    minval = std::min(val, minval);
                    maxval = std::max(val, maxval);
                }
                if (inrange)
                {
                    saved = cc;
                    lines = 0;
                }
            }
            if (newcol != saved + 1 + HORIZSLOP)
            {
                newcol = std::min(maxcol1, saved + 1 + HORIZSLOP);
                newwidth = std::max(0, maxcol1 - newcol);
                left = true;
            }

            if (!newwidth)
                throw "monochromatic";

            mincol = newcol;

            /*
             * Find right edge. Keep same minval/maxval (pillarboxing colors) as
             * left edge.
             */
            bool right = false;
            lines = 0;
            saved = maxcol1 - 1;
            found = false;
            for (int cc = maxcol1 - 1; !found && cc >= mincol; cc--)
            {
                int outliers = 0;
                bool inrange = true;
                for (int rr = minrow; rr < maxrow1; rr++)
                {
                    if (m_logo && rrccinrect(rr, cc, m_logoRow, m_logoCol,
                                m_logoWidth, m_logoHeight))
                        continue;   /* Exclude logo area from analysis. */

                    uchar val = pgm->data[0][(rr * pgmwidth) + cc];
                    int range = std::max(maxval, val) - std::min(minval, val) + 1;
                    if (range > kMaxRange)
                    {
                        if (outliers++ < MAXOUTLIERS)
                            continue;   /* Next row. */
                        inrange = false;
                        if (lines++ >= kMaxLines)
                            found = true;
                        break;  /* Next column. */
                    }
                    minval = std::min(val, minval);
                    maxval = std::max(val, maxval);
                }
                if (inrange)
                {
                    saved = cc;
                    lines = 0;
                }
            }
            if (newwidth != saved - mincol - HORIZSLOP)
            {
                newwidth = std::max(0, saved - mincol - HORIZSLOP);
                right = true;
            }

            if (!newwidth)
                throw "monochromatic";

            if (top || bottom)
                break;  /* Do not repeat letterboxing check. */

            maxcol1 = mincol + newwidth;

            /* Find top edge. */
            top = false;
            minval = UCHAR_MAX;
            maxval = 0;
            lines = 0;
            saved = minrow;
            found = false;
            for (int rr = minrow; !found && rr < maxrow1; rr++)
            {
                int outliers = 0;
                bool inrange = true;
                for (int cc = mincol; cc < maxcol1; cc++)
                {
                    if (m_logo && rrccinrect(rr, cc, m_logoRow, m_logoCol,
                                m_logoWidth, m_logoHeight))
                        continue;   /* Exclude logo area from analysis. */

                    uchar val = pgm->data[0][(rr * pgmwidth) + cc];
                    int range = std::max(maxval, val) - std::min(minval, val) + 1;
                    if (range > kMaxRange)
                    {
                        if (outliers++ < MAXOUTLIERS)
                            continue;   /* Next column. */
                        inrange = false;
                        if (lines++ >= kMaxLines)
                            found = true;
                        break;  /* Next row. */
                    }
                    minval = std::min(val, minval);
                    maxval = std::max(val, maxval);
                }
                if (inrange)
                {
                    saved = rr;
                    lines = 0;
                }
            }
            if (newrow != saved + 1 + VERTSLOP)
            {
                newrow = std::min(maxrow1, saved + 1 + VERTSLOP);
                newheight = std::max(0, maxrow1 - newrow);
                top = true;
            }

            if (!newheight)
                throw "monochromatic";

            minrow = newrow;

            /* Find bottom edge. Keep same minval/maxval as top edge. */
            bottom = false;
            lines = 0;
            saved = maxrow1 - 1;
            found = true;
            for (int rr = maxrow1 - 1; !found && rr >= minrow; rr--)
            {
                int outliers = 0;
                bool inrange = true;
                for (int cc = mincol; cc < maxcol1; cc++)
                {
                    if (m_logo && rrccinrect(rr, cc, m_logoRow, m_logoCol,
                                m_logoWidth, m_logoHeight))
                        continue;   /* Exclude logo area from analysis. */

                    uchar val = pgm->data[0][(rr * pgmwidth) + cc];
                    int range = std::max(maxval, val) - std::min(minval, val) + 1;
                    if (range > kMaxRange)
                    {
                        if (outliers++ < MAXOUTLIERS)
                            continue;   /* Next column. */
                        inrange = false;
                        if (lines++ >= kMaxLines)
                            found = true;
                        break;  /* Next row. */
                    }
                    minval = std::min(val, minval);
                    maxval = std::max(val, maxval);
                }
                if (inrange)
                {
                    saved = rr;
                    lines = 0;
                }
            }
            if (newheight != saved - minrow - VERTSLOP)
            {
                newheight = std::max(0, saved - minrow - VERTSLOP);
                bottom = true;
            }

            if (!newheight)
                throw "monochromatic";

            if (left || right)
                break;  /* Do not repeat pillarboxing check. */

            maxrow1 = minrow + newheight;
        }
    }
    catch (char const* e)
    {
        monochromatic = true;
    }

    m_frameNo = _frameno;
    m_row = newrow;
    m_col = newcol;
    m_width = newwidth;
    m_height = newheight;
    m_isMonochromatic = monochromatic;
}

int
BorderDetector::getDimensions(const AVFrame *pgm, int pgmheight,
        long long _frameno, int *prow, int *pcol, int *pwidth, int *pheight)
{
    auto start = nowAsDuration<std::chrono::microseconds>();

    if (_frameno == kUncached || _frameno != m_frameNo)
        getDimensionsReal(pgm, pgmheight, _frameno);

    *prow = m_row;
    *pcol = m_col;
    *pwidth = m_width;
    *pheight = m_height;

    auto end = nowAsDuration<std::chrono::microseconds>();
    m_analyzeTime += (end - start);

    return m_isMonochromatic ? -1 : 0;
}

int
BorderDetector::reportTime(void)
{
    if (!m_timeReported)
    {
        LOG(VB_COMMFLAG, LOG_INFO, QString("BD Time: analyze=%1s")
                .arg(strftimeval(m_analyzeTime)));
        m_timeReported = true;
    }
    return 0;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
