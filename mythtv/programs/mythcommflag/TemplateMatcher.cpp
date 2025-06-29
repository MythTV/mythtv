// C++ headers
#include <algorithm>
#include <cmath>
#include <cstdlib>

// Qt headers
#include <QFile>
#include <QFileInfo>
#include <utility>

// MythTV headers
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythlogging.h"
#include "libmythtv/mythplayer.h"

// Commercial Flagging headers
#include "BlankFrameDetector.h"
#include "CommDetector2.h"
#include "EdgeDetector.h"
#include "FrameAnalyzer.h"
#include "PGMConverter.h"
#include "TemplateFinder.h"
#include "TemplateMatcher.h"
#include "pgm.h"

extern "C" {
#include "libavutil/imgutils.h"
}

using namespace commDetector2;
using namespace frameAnalyzer;

namespace {

int pgm_set(const AVFrame *pict, int height)
{
    const int   width = pict->linesize[0];
    const int   size = height * width;

    int score = 0;
    for (int ii = 0; ii < size; ii++)
        if (pict->data[0][ii])
            score++;
    return score;
}

int pgm_match_inner_loop (const AVFrame *test, int rr, int cc, int radius, int height, int width)
{
    int r2min = std::max(0, rr - radius);
    int r2max = std::min(height, rr + radius);

    int c2min = std::max(0, cc - radius);
    int c2max = std::min(width, cc + radius);

    for (int r2 = r2min; r2 <= r2max; r2++)
    {
        for (int c2 = c2min; c2 <= c2max; c2++)
        {
            if (test->data[0][(r2 * width) + c2])
                return 1;
        }
    }
    return 0;
}

int pgm_match(const AVFrame *tmpl, const AVFrame *test, int height,
              int radius, unsigned short *pscore)
{
    /* Return the number of matching "edge" and non-edge pixels. */
    const int       width = tmpl->linesize[0];

    if (width != test->linesize[0])
    {
        LOG(VB_COMMFLAG, LOG_ERR,
            QString("pgm_match widths don't match: %1 != %2")
                .arg(width).arg(test->linesize[0]));
        return -1;
    }

    int score = 0;
    for (int rr = 0; rr < height; rr++)
    {
        for (int cc = 0; cc < width; cc++)
        {
            if (!tmpl->data[0][(rr * width) + cc])
                continue;
            score += pgm_match_inner_loop(test, rr, cc, radius, height, width);
        }
    }

    *pscore = score;
    return 0;
}

bool readMatches(const QString& filename, unsigned short *matches, long long nframes)
{
    QByteArray fname = filename.toLocal8Bit();
    FILE *fp = fopen(fname.constData(), "r");
    if (fp == nullptr)
        return false;
    // Automatically close file at function exit
    auto close_fp = [&](FILE *fp2) {
        if (fclose(fp2) == 0)
            return;
        LOG(VB_COMMFLAG, LOG_ERR, QString("Error closing %1: %2")
            .arg(filename, strerror(errno)));
    };
    std::unique_ptr<FILE,decltype(close_fp)> cleanup { fp, close_fp };

    for (long long frameno = 0; frameno < nframes; frameno++)
    {
        int nitems = fscanf(fp, "%20hu", &matches[frameno]);
        if (nitems != 1)
        {
            LOG(VB_COMMFLAG, LOG_ERR,
                QString("Not enough data in %1: frame %2")
                    .arg(filename).arg(frameno));
            return false;
        }
    }
    return true;
}

bool writeMatches(const QString& filename, unsigned short *matches, long long nframes)
{
    QByteArray fname = filename.toLocal8Bit();
    FILE *fp = fopen(fname.constData(), "w");
    if (fp == nullptr)
        return false;

    for (long long frameno = 0; frameno < nframes; frameno++)
        (void)fprintf(fp, "%hu\n", matches[frameno]);

    if (fclose(fp))
        LOG(VB_COMMFLAG, LOG_ERR, QString("Error closing %1: %2")
                .arg(filename, strerror(errno)));
    return true;
}

int finishedDebug(long long nframes, const unsigned short *matches,
                  const unsigned char *match)
{
    ushort score = matches[0];
    ushort low = score;
    ushort high = score;
    long long startframe = 0;

    for (long long frameno = 1; frameno < nframes; frameno++)
    {
        score = matches[frameno];
        if (match[frameno - 1] == match[frameno])
        {
            low = std::min(score, low);
            high = std::max(score, high);
            continue;
        }

        LOG(VB_COMMFLAG, LOG_INFO, QString("Frame %1-%2: %3 L-H: %4-%5 (%6)")
                .arg(startframe, 6).arg(frameno - 1, 6)
                .arg(match[frameno - 1] ? "logo        " : "     no-logo")
                .arg(low, 4).arg(high, 4).arg(frameno - startframe, 5));

        low = score;
        high = score;
        startframe = frameno;
    }

    return 0;
}

int sort_ascending(const void *aa, const void *bb)
{
    return *(unsigned short*)aa - *(unsigned short*)bb;
}

long long matchspn(long long nframes, const unsigned char *match, long long frameno,
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

unsigned int range_area(const unsigned short *freq, unsigned short start,
                        unsigned short end)
{
    /* Return the integrated area under the curve of the plotted histogram. */
    const unsigned short    width = end - start;

    uint sum = 0;
    uint nsamples = 0;
    for (ushort matchcnt = start; matchcnt < end; matchcnt++)
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

unsigned short pick_mintmpledges(const unsigned short *matches,
                                 long long nframes)
{
    // Validate arguments.  If nframes is equal to zero there's no
    // work to do.  This check also prevents "sorted[nframes - 1]"
    // below from referencing memory before the start of the array.
    if (nframes <= 0)
        return 0;

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
    static constexpr float  kLeftWidth = 0.04;
    static constexpr float  kMiddleWidth = 0.04;
    static constexpr float  kRightWidth = 0.04;

    static constexpr float  kMatchStart = 0.20;
    static constexpr float  kMatchEnd = 0.80;

    auto *sorted = new unsigned short[nframes];
    memcpy(sorted, matches, nframes * sizeof(*matches));
    qsort(sorted, nframes, sizeof(*sorted), sort_ascending);
    ushort minmatch = sorted[0];
    ushort maxmatch = sorted[nframes - 1];
    ushort matchrange = maxmatch - minmatch;
    /* degenerate minmatch==maxmatch case is gracefully handled */

    auto leftwidth = (unsigned short)(kLeftWidth * matchrange);
    auto middlewidth = (unsigned short)(kMiddleWidth * matchrange);
    auto rightwidth = (unsigned short)(kRightWidth * matchrange);

    int nfreq = maxmatch + 1;
    auto *freq = new unsigned short[nfreq];
    memset(freq, 0, nfreq * sizeof(*freq));
    for (long long frameno = 0; frameno < nframes; frameno++)
        freq[matches[frameno]]++;   /* freq[<matchcnt>] = <framecnt> */

    ushort matchstart = minmatch + (unsigned short)(kMatchStart * matchrange);
    ushort matchend = minmatch + (unsigned short)(kMatchEnd * matchrange);

    int local_minimum = matchstart;
    uint maxdelta = 0;
    for (int matchcnt = matchstart + leftwidth + (middlewidth / 2);
            matchcnt < matchend - rightwidth - (middlewidth / 2);
            matchcnt++)
    {
        ushort p0 = matchcnt - leftwidth - (middlewidth / 2);
        ushort p1 = p0 + leftwidth;
        ushort p2 = p1 + middlewidth;
        ushort p3 = p2 + rightwidth;

        uint leftscore = range_area(freq, p0, p1);
        uint middlescore = range_area(freq, p1, p2);
        uint rightscore = range_area(freq, p2, p3);
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

    LOG(VB_COMMFLAG, LOG_INFO,
        QString("pick_mintmpledges minmatch=%1 maxmatch=%2"
                " matchstart=%3 matchend=%4 widths=%5,%6,%7 local_minimum=%8")
            .arg(minmatch).arg(maxmatch).arg(matchstart).arg(matchend)
            .arg(leftwidth).arg(middlewidth).arg(rightwidth)
            .arg(local_minimum));

    delete []freq;
    delete []sorted;
    return local_minimum;
}

};  /* namespace */

TemplateMatcher::TemplateMatcher(std::shared_ptr<PGMConverter> pgmc,
                                 std::shared_ptr<EdgeDetector> ed,
                                 TemplateFinder *tf, const QString& debugdir) :
    m_pgmConverter(std::move(pgmc)),
    m_edgeDetector(std::move(ed)),   m_templateFinder(tf),
    m_debugDir(debugdir),
#ifdef PGM_CONVERT_GREYSCALE
    m_debugData(debugdir + "/TemplateMatcher-pgm.txt")
#else  /* !PGM_CONVERT_GREYSCALE */
    m_debugData(debugdir + "/TemplateMatcher-yuv.txt")
#endif /* !PGM_CONVERT_GREYSCALE */
{
    /*
     * debugLevel:
     *      0: no debugging
     *      1: cache frame edge counts into debugdir [1 file]
     *      2: extra verbosity [O(nframes)]
     */
    m_debugLevel = gCoreContext->GetNumSetting("TemplateMatcherDebugLevel", 0);

    if (m_debugLevel >= 1)
    {
        createDebugDirectory(m_debugDir,
            QString("TemplateMatcher debugLevel %1").arg(m_debugLevel));
        m_debugMatches = true;
        if (m_debugLevel >= 2)
            m_debugRemoveRunts = true;
    }
}

TemplateMatcher::~TemplateMatcher(void)
{
    delete []m_matches;
    delete []m_match;
    av_freep(reinterpret_cast<void*>(&m_cropped.data[0]));
}

enum FrameAnalyzer::analyzeFrameResult
TemplateMatcher::MythPlayerInited(MythPlayer *_player,
        long long nframes)
{
    m_player = _player;
    m_fps = m_player->GetFrameRate();

    m_tmpl = m_templateFinder->getTemplate(&m_tmplRow, &m_tmplCol,
                    &m_tmplWidth, &m_tmplHeight);
    if (m_tmpl == nullptr)
    {
        LOG(VB_COMMFLAG, LOG_ERR,
            QString("TemplateMatcher::MythPlayerInited: no template"));
        return ANALYZE_FATAL;
    }

    if (av_image_alloc(m_cropped.data, m_cropped.linesize,
        m_tmplWidth, m_tmplHeight, AV_PIX_FMT_GRAY8, IMAGE_ALIGN) < 0)
    {
        LOG(VB_COMMFLAG, LOG_ERR,
            QString("TemplateMatcher::MythPlayerInited "
                    "av_image_alloc cropped (%1x%2) failed")
                .arg(m_tmplWidth).arg(m_tmplHeight));
        return ANALYZE_FATAL;
    }

    if (m_pgmConverter->MythPlayerInited(m_player))
    {
        av_freep(reinterpret_cast<void*>(&m_cropped.data[0]));
        return ANALYZE_FATAL;
    }

    m_matches = new unsigned short[nframes];
    memset(m_matches, 0, nframes * sizeof(*m_matches));

    m_match = new unsigned char[nframes];

    if (m_debugMatches)
    {
        if (readMatches(m_debugData, m_matches, nframes))
        {
            LOG(VB_COMMFLAG, LOG_INFO,
                QString("TemplateMatcher::MythPlayerInited read %1")
                    .arg(m_debugData));
            m_matchesDone = true;
        }
    }

    if (m_matchesDone)
        return ANALYZE_FINISHED;

    return ANALYZE_OK;
}

enum FrameAnalyzer::analyzeFrameResult
TemplateMatcher::analyzeFrame(const MythVideoFrame *frame, long long frameno,
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

    const AVFrame  *edges = nullptr;
    int             pgmwidth = 0;
    int             pgmheight = 0;
    std::chrono::microseconds start {0us};
    std::chrono::microseconds end   {0us};

    *pNextFrame = kNextFrame;

    try
    {
        const AVFrame *pgm = m_pgmConverter->getImage(frame, frameno, &pgmwidth, &pgmheight);
        if (pgm == nullptr)
            throw 1;

        start = nowAsDuration<std::chrono::microseconds>();

        if (pgm_crop(&m_cropped, pgm, pgmheight, m_tmplRow, m_tmplCol,
                    m_tmplWidth, m_tmplHeight))
            throw 2;

        edges = m_edgeDetector->detectEdges(&m_cropped, m_tmplHeight, FRAMESGMPCTILE);
        if (edges == nullptr)
            throw 3;

        if (pgm_match(m_tmpl, edges, m_tmplHeight, JITTER_RADIUS, &m_matches[frameno]))
            throw 4;

        end = nowAsDuration<std::chrono::microseconds>();
        m_analyzeTime += (end - start);

        return ANALYZE_OK;
    }
    catch (int e)
    {
        LOG(VB_COMMFLAG, LOG_ERR,
            QString("TemplateMatcher::analyzeFrame error at frame %1, step %2")
            .arg(frameno).arg(e));
        return ANALYZE_ERROR;
    }
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
    const int       MINBREAKLEN = (int)roundf(45 * m_fps);  /* frames */
    const int       MINSEGLEN = (int)roundf(105 * m_fps);    /* frames */

    int       minbreaklen = 1;
    int       minseglen = 1;
    long long brkb = 0;

    if (!m_matchesDone && m_debugMatches)
    {
        if (final && writeMatches(m_debugData, m_matches, nframes))
        {
            LOG(VB_COMMFLAG, LOG_INFO,
                QString("TemplateMatcher::finished wrote %1") .arg(m_debugData));
            m_matchesDone = true;
        }
    }

    int tmpledges = pgm_set(m_tmpl, m_tmplHeight);
    int mintmpledges = pick_mintmpledges(m_matches, nframes);

    LOG(VB_COMMFLAG, LOG_INFO,
        QString("TemplateMatcher::finished %1x%2@(%3,%4),"
                " %5 edge pixels, want %6")
            .arg(m_tmplWidth).arg(m_tmplHeight).arg(m_tmplCol).arg(m_tmplRow)
            .arg(tmpledges).arg(mintmpledges));

    for (long long ii = 0; ii < nframes; ii++)
        m_match[ii] = m_matches[ii] >= mintmpledges ? 1 : 0;

    if (m_debugLevel >= 2)
    {
        if (final && finishedDebug(nframes, m_matches, m_match))
            return -1;
    }

    /*
     * Construct breaks; find the first logo break.
     */
    m_breakMap.clear();
    brkb = m_match[0] ? matchspn(nframes, m_match, 0, m_match[0]) : 0;
    while (brkb < nframes)
    {
        /* Skip over logo-less frames; find the next logo frame (brke). */
        long long brke = matchspn(nframes, m_match, brkb, m_match[brkb]);
        long long brklen = brke - brkb;
        m_breakMap.insert(brkb, brklen);

        /* Find the next logo break. */
        brkb = matchspn(nframes, m_match, brke, m_match[brke]);
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
            f1 = removeShortBreaks(&m_breakMap, m_fps, minbreaklen,
                    m_debugRemoveRunts);
            minbreaklen++;
        }
        if (minseglen <= MINSEGLEN)
        {
            f2 = removeShortSegments(&m_breakMap, nframes, m_fps, minseglen,
                    m_debugRemoveRunts);
            minseglen++;
        }
        if (minbreaklen > MINBREAKLEN && minseglen > MINSEGLEN)
            break;
        if (m_debugRemoveRunts && (f1 || f2))
            frameAnalyzerReportMap(&m_breakMap, m_fps, "** TM Break");
    }

    /*
     * Report breaks.
     */
    frameAnalyzerReportMap(&m_breakMap, m_fps, "TM Break");

    return 0;
}

int
TemplateMatcher::reportTime(void) const
{
    if (m_pgmConverter->reportTime())
        return -1;

    LOG(VB_COMMFLAG, LOG_INFO, QString("TM Time: analyze=%1s")
            .arg(strftimeval(m_analyzeTime)));
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
    static const int64_t MINBREAKS { nframes * 20 / 100 };
    static const int64_t MAXBREAKS { nframes * 45 / 100 };

    const int64_t brklen = frameAnalyzerMapSum(&m_breakMap);
    const bool good = brklen >= MINBREAKS && brklen <= MAXBREAKS;

    if (m_debugLevel >= 1)
    {
        if (!m_tmpl)
        {
            LOG(VB_COMMFLAG, LOG_ERR,
                QString("TemplateMatcher: no template (wanted %2-%3%)")
                    .arg(100 * MINBREAKS / nframes)
                    .arg(100 * MAXBREAKS / nframes));
        }
        else if (!final)
        {
            LOG(VB_COMMFLAG, LOG_ERR,
                QString("TemplateMatcher has %1% breaks (real-time flagging)")
                    .arg(100 * brklen / nframes));
        }
        else if (good)
        {
            LOG(VB_COMMFLAG, LOG_INFO, QString("TemplateMatcher has %1% breaks")
                    .arg(100 * brklen / nframes));
        }
        else
        {
            LOG(VB_COMMFLAG, LOG_INFO, 
                QString("TemplateMatcher has %1% breaks (wanted %2-%3%)")
                    .arg(100 * brklen / nframes)
                    .arg(100 * MINBREAKS / nframes)
                    .arg(100 * MAXBREAKS / nframes));
        }
    }

    if (!final)
        return 0;   /* real-time flagging */

    if (brklen < MINBREAKS)
        return 1;
    if (brklen <= MAXBREAKS)
        return 0;
    return -1;
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
    const int BLANK_NEARBY = (int)roundf(0.5F * m_fps);
    const int TEMPLATE_DISAPPEARS_EARLY = (int)roundf(25 * m_fps);
    const int TEMPLATE_DISAPPEARS_LATE = (int)roundf(0 * m_fps);
    const int TEMPLATE_REAPPEARS_LATE = (int)roundf(35 * m_fps);
    const int TEMPLATE_REAPPEARS_EARLY = (int)roundf(1.5F * m_fps);

    LOG(VB_COMMFLAG, LOG_INFO, QString("TemplateMatcher adjusting for blanks"));

    FrameAnalyzer::FrameMap::Iterator ii = m_breakMap.begin();
    long long prevbrke = 0;
    while (ii != m_breakMap.end())
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
                std::max(prevbrke,
                    brkb - std::max(BLANK_NEARBY, TEMPLATE_DISAPPEARS_LATE)),
                std::min(brke,
                    brkb + std::max(BLANK_NEARBY, TEMPLATE_DISAPPEARS_EARLY)));
        }
        long long newbrkb = brkb;
        if (jj != blankMap->constEnd())
        {
            newbrkb = jj.key();
            long long adj = *jj / 2;
            adj = std::min<long long>(adj, MAX_BLANK_FRAMES);
            newbrkb += adj;
        }

        /*
         * Where the template reappears, look for nearby blank frames. Do not
         * search into adjacent breaks.
         */
        FrameAnalyzer::FrameMap::const_iterator kk = frameMapSearchBackwards(
            blankMap,
            std::max(newbrkb,
                brke - std::max(BLANK_NEARBY, TEMPLATE_REAPPEARS_LATE)),
            std::min(iinext == m_breakMap.end() ? nframes : iinext.key(),
                brke + std::max(BLANK_NEARBY, TEMPLATE_REAPPEARS_EARLY)));
        long long newbrke = brke;
        if (kk != blankMap->constEnd())
        {
            newbrke = kk.key();
            long long adj = *kk;
            newbrke += adj;
            adj /= 2;
            adj = std::min<long long>(adj, MAX_BLANK_FRAMES);
            newbrke -= adj;
        }

        /*
         * Adjust breakMap for blank frames.
         */
        long long newbrklen = newbrke - newbrkb;
        if (newbrkb != brkb)
        {
            m_breakMap.erase(ii);
            if (newbrkb < nframes && newbrklen)
                m_breakMap.insert(newbrkb, newbrklen);
        }
        else if (newbrke != brke)
        {
            if (newbrklen)
            {
                m_breakMap.remove(newbrkb);
                m_breakMap.insert(newbrkb, newbrklen);
            }
            else
            {
                m_breakMap.erase(ii);
            }
        }

        prevbrke = newbrke;
        ii = iinext;
    }

    /*
     * Report breaks.
     */
    frameAnalyzerReportMap(&m_breakMap, m_fps, "TM Break");
    return 0;
}

int
TemplateMatcher::computeBreaks(FrameAnalyzer::FrameMap *breaks)
{
    breaks->clear();
    for (auto bb = m_breakMap.begin();
            bb != m_breakMap.end();
            ++bb)
    {
        breaks->insert(bb.key(), *bb);
    }
    return 0;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
