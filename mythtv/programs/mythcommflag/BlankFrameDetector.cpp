// C++ headers
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <utility>

// MythTV headers
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythlogging.h"
#include "libmythtv/mythplayer.h"

// Commercial Flagging headers
#include "BlankFrameDetector.h"
#include "CommDetector2.h"
#include "FrameAnalyzer.h"
#include "HistogramAnalyzer.h"
#include "TemplateMatcher.h"
#include "quickselect.h"

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
    if (faa < fbb)
        return -1;
    if (faa == fbb)
        return 0;
    return 1;
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

    long long frameno = 1;
    long long segb = 0;
    long long sege = 0;

    /* Count and select for monochromatic frames. */

    long long nblanks = 0;
    for (frameno = 0; frameno < nframes; frameno++)
    {
        if (monochromatic[frameno] && pickmedian(median[frameno],
                    MINBLANKMEDIAN, MAXBLANKMEDIAN))
            nblanks++;
    }

    if (nblanks <= 0)
    {
        /* No monochromatic frames. */
        LOG(VB_COMMFLAG, LOG_INFO,
            "BlankFrameDetector::computeBlankMap: No blank frames.");
        return;
    }

    /* Select percentile values from monochromatic frames. */

    auto *blankmedian = new unsigned char[nblanks];
    auto *blankstddev = new float[nblanks];
    long long blankno = 0;
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
    blankno = std::min(nblanks - 1, (long long)roundf(nblanks * MEDIANPCTILE));
    if (blankno <= 0)
    {
        delete []blankmedian;
        delete []blankstddev;
        LOG(VB_COMMFLAG, LOG_INFO,
            "BlankFrameDetector::computeBlankMap: No blank frames. (2)");
        return;
    }
    uchar maxmedian = blankmedian[blankno];

    qsort(blankstddev, nblanks, sizeof(*blankstddev), sort_ascending_float);
    long long stddevno = std::min(nblanks - 1, (long long)roundf(nblanks * STDDEVPCTILE));
    float maxstddev = blankstddev[stddevno];

    /* Determine effective percentile ranges (for debugging). */

    long long blankno1 = blankno;
    long long blankno2 = blankno;
    while (blankno1 > 0 && blankmedian[blankno1] == maxmedian)
        blankno1--;
    if (blankmedian[blankno1] != maxmedian)
        blankno1++;
    while (blankno2 < nblanks && blankmedian[blankno2] == maxmedian)
        blankno2++;
    if (blankno2 == nblanks)
        blankno2--;

    long long stddevno1 = stddevno;
    long long stddevno2 = stddevno;
    while (stddevno1 > 0 && blankstddev[stddevno1] == maxstddev)
        stddevno1--;
    if (blankstddev[stddevno1] != maxstddev)
        stddevno1++;
    while (stddevno2 < nblanks && blankstddev[stddevno2] == maxstddev)
        stddevno2++;
    if (stddevno2 == nblanks)
        stddevno2--;

    LOG(VB_COMMFLAG, LOG_INFO,
        QString("Blanks selecting median<=%1 (%2-%3%), stddev<=%4 (%5-%6%)")
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
        const FrameAnalyzer::FrameMap *blankMap, float fps,
        int debugLevel)
{
    /*
     * TUNABLE:
     *
     * Common commercial-break lengths.
     */
    struct breakType {
        std::chrono::seconds m_len;
        std::chrono::seconds m_delta;
    };
    static constexpr std::array<const breakType,4> kBreakType {{
        /* Sort by "len". */
        { 15s,   2s },
        { 20s,   2s },
        { 30s,   5s },
        { 60s,   5s },
    }};

    /*
     * TUNABLE:
     *
     * Shortest non-commercial length, used to coalesce consecutive commercial
     * breaks that are usually identified due to in-commercial cuts.
     */
    static const int kMinContentLen = (int)roundf(10 * fps);

    breakMap->clear();
    for (FrameAnalyzer::FrameMap::const_iterator iiblank = blankMap->begin();
            iiblank != blankMap->end();
            ++iiblank)
    {
        long long brkb = iiblank.key();
        long long iilen = *iiblank;
        long long start = brkb + (iilen / 2);

        for (const auto& type : kBreakType)
        {
            /* Look for next blank frame that is an acceptable distance away. */
            FrameAnalyzer::FrameMap::const_iterator jjblank = iiblank;
            for (++jjblank; jjblank != blankMap->end(); ++jjblank)
            {
                long long brke = jjblank.key();
                long long jjlen = *jjblank;
                long long end = brke + (jjlen / 2);

                auto testlen = std::chrono::seconds(lroundf((end - start) / fps));
                if (testlen > type.m_len + type.m_delta)
                    break;      /* Too far ahead; break to next break length. */

                std::chrono::seconds delta = testlen - type.m_len;
                delta = std::chrono::abs(delta);

                if (delta > type.m_delta)
                    continue;   /* Outside delta range; try next end-blank. */

                /* Mark this commercial break. */
                bool inserted = false;
                for (unsigned int jj = 0;; jj++)
                {
                    long long newbrkb = brkb + jj;
                    if (newbrkb >= brke)
                    {
                        LOG(VB_COMMFLAG, LOG_INFO,
                            QString("BF [%1,%2] ran out of slots")
                                .arg(brkb).arg(brke - 1));
                        break;
                    }
                    if (!breakMap->contains(newbrkb))
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
        LOG(VB_COMMFLAG, LOG_INFO,
            "BF coalescing overlapping/nearby breaks ...");
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

            if (iie + kMinContentLen < jjb)
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

    /* Adjust for blank intervals. */
    FrameAnalyzer::FrameMap::iterator iibreak = breakMap->begin();
    while (iibreak != breakMap->end())
    {
        long long iib = iibreak.key();
        long long iie = iib + *iibreak;
        iibreak = breakMap->erase(iibreak);

        /* Trim leading blanks from commercial break. */
        auto iter = blankMap->find(iib);
        if (iter == blankMap->end())
            break;
        long long addb = *iter;
        addb = addb / 2;
        addb = std::min<long long>(addb, MAX_BLANK_FRAMES);
        iib += addb;
        /* Add trailing blanks to commercial break. */
        iter = blankMap->find(iib);
        if (iter == blankMap->end())
            break;
        long long adde = *iter;
        iie += adde;
        long long sube = adde / 2;
        sube = std::min<long long>(sube, MAX_BLANK_FRAMES);
        iie -= sube;
        breakMap->insert(iib, iie - iib);
    }
}

};  /* namespace */

BlankFrameDetector::BlankFrameDetector(std::shared_ptr<HistogramAnalyzer>ha,
                                       const QString &debugdir)
    : m_histogramAnalyzer(std::move(ha))
{
    /*
     * debugLevel:
     *      0: no debugging
     *      2: extra verbosity [O(nframes)]
     */
    m_debugLevel = gCoreContext->GetNumSetting("BlankFrameDetectorDebugLevel", 0);

    if (m_debugLevel >= 1)
        createDebugDirectory(debugdir,
            QString("BlankFrameDetector debugLevel %1").arg(m_debugLevel));
}

enum FrameAnalyzer::analyzeFrameResult
BlankFrameDetector::MythPlayerInited(MythPlayer *player, long long nframes)
{
    FrameAnalyzer::analyzeFrameResult ares =
        m_histogramAnalyzer->MythPlayerInited(player, nframes);

    m_fps = player->GetFrameRate();

    QSize video_disp_dim = player->GetVideoSize();

    LOG(VB_COMMFLAG, LOG_INFO,
        QString("BlankFrameDetector::MythPlayerInited %1x%2")
            .arg(video_disp_dim.width())
            .arg(video_disp_dim.height()));

    return ares;
}

enum FrameAnalyzer::analyzeFrameResult
BlankFrameDetector::analyzeFrame(const MythVideoFrame *frame, long long frameno,
        long long *pNextFrame)
{
    *pNextFrame = kNextFrame;

    if (m_histogramAnalyzer->analyzeFrame(frame, frameno) ==
            FrameAnalyzer::ANALYZE_OK)
        return ANALYZE_OK;

    LOG(VB_COMMFLAG, LOG_INFO,
        QString("BlankFrameDetector::analyzeFrame error at frame %1")
            .arg(frameno));
    return ANALYZE_ERROR;
}

int
BlankFrameDetector::finished(long long nframes, bool final)
{
    if (m_histogramAnalyzer->finished(nframes, final))
        return -1;

    LOG(VB_COMMFLAG, LOG_INFO, QString("BlankFrameDetector::finished(%1)")
            .arg(nframes));

    /* Identify all sequences of blank frames (blankMap). */
    computeBlankMap(&m_blankMap, nframes,
            m_histogramAnalyzer->getMedians(), m_histogramAnalyzer->getStdDevs(),
            m_histogramAnalyzer->getMonochromatics());
    if (m_debugLevel >= 2)
        frameAnalyzerReportMapms(&m_blankMap, m_fps, "BF Blank");

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
    const int       MAXBLANKADJUSTMENT = (int)roundf(5 * m_fps);  /* frames */

    LOG(VB_COMMFLAG, LOG_INFO, "BlankFrameDetector adjusting for logo surplus");

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
        FrameAnalyzer::FrameMap::Iterator jjfound = m_blankMap.end();

        /* Look for a blank frame near beginning of logo break. */
        for (auto jj = m_blankMap.begin(); jj != m_blankMap.end(); ++jj)
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
        if (jjfound != m_blankMap.end())
        {
            long long jjee = jjfound.key() + *jjfound;
            m_blankMap.erase(jjfound);
            if (jjee <= iikey)
            {
                /* Move blank frame to beginning of logo break. */
                m_blankMap.remove(iikey);
                m_blankMap.insert(iikey, 1);
            }
            else
            {
                /* Adjust blank frame to begin with logo break. */
                m_blankMap.insert(iikey, jjee - iikey);
            }
        }

        /* Get bounds of end of logo break. */
        long long kkkey = ii.key() + *ii;
        long long kkbb = kkkey - MAXBLANKADJUSTMENT;
        long long kkee = kkkey + MAXBLANKADJUSTMENT;
        FrameAnalyzer::FrameMap::Iterator mmfound = m_blankMap.end();

        /* Look for a blank frame near end of logo break. */
        for (auto mm = m_blankMap.begin(); mm != m_blankMap.end(); ++mm)
        {
            long long mmbb = mm.key();
            long long mmee = mmbb + *mm;

            if (kkee < mmbb)
                break;      /* No nearby blank frames. */

            if (mmee < kkbb)
                continue;   /* Too early; keep looking. */

            /* Prefer the last blank frame ending before the logo break ends. */
            if (mmee < kkkey || mmfound == m_blankMap.end())
                mmfound = mm;
            if (mmee >= kkkey)
                break;
        }

        /* Adjust blank frame to end with logo break end. */
        if (mmfound != m_blankMap.end())
        {
            long long mmbb = mmfound.key();
            if (mmbb < kkkey)
            {
                /* Adjust blank frame to end with logo break. */
                m_blankMap.remove(mmbb);
                m_blankMap.insert(mmbb, kkkey - mmbb);
            }
            else
            {
                /* Move blank frame to end of logo break. */
                m_blankMap.erase(mmfound);
                m_blankMap.remove(kkkey - 1);
                m_blankMap.insert(kkkey - 1, 1);
            }
        }
    }

    /*
     * Compute breaks (breakMap).
     */
    computeBreakMap(&m_breakMap, &m_blankMap, m_fps, m_debugLevel);

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

        for (auto jj = m_breakMap.begin(); jj != m_breakMap.end(); )
        {
            long long jjbb = jj.key();
            long long jjee = jjbb + *jj;

            if (iiee < jjbb)
            {
                if (!overlap)
                {
                    /* Fully incorporate logo break */
                    m_breakMap.insert(iibb, iiee - iibb);
                }
                break;
            }

            if (iibb < jjbb && jjbb < iiee)
            {
                /* End of logo break includes beginning of blank-frame break. */
                overlap = true;
                jj = m_breakMap.erase(jj);
                m_breakMap.insert(iibb, std::max(iiee, jjee) - iibb);
                continue;
            }
            if (jjbb < iibb && iibb < jjee)
            {
                /* End of blank-frame break includes beginning of logo break. */
                overlap = true;
                if (jjee < iiee)
                {
                    m_breakMap.remove(jjbb);
                    m_breakMap.insert(jjbb, iiee - jjbb);
                }
            }

            jj++;
        }
    }

    frameAnalyzerReportMap(&m_breakMap, m_fps, "BF Break");
    return 0;
}

int
BlankFrameDetector::computeForLogoDeficit(
        [[maybe_unused]] const TemplateMatcher *templateMatcher)
{
    LOG(VB_COMMFLAG, LOG_INFO, "BlankFrameDetector adjusting for "
                               "too little logo coverage (unimplemented)");
    return 0;
}

int
BlankFrameDetector::computeBreaks(FrameAnalyzer::FrameMap *breaks)
{
    if (m_breakMap.empty())
    {
        /* Compute breaks (m_breakMap). */
        computeBreakMap(&m_breakMap, &m_blankMap, m_fps, m_debugLevel);
        frameAnalyzerReportMap(&m_breakMap, m_fps, "BF Break");
    }

    breaks->clear();
    for (auto bb = m_breakMap.begin(); bb != m_breakMap.end(); ++bb)
        breaks->insert(bb.key(), *bb);

    return 0;
}

int
BlankFrameDetector::reportTime(void) const
{
    return m_histogramAnalyzer->reportTime();
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
