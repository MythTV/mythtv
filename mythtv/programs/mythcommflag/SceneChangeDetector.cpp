// ANSI C headers
#include <cmath>
#include <cstdlib>
#include <utility>

// MythTV headers
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythlogging.h"
#include "libmythtv/mythplayer.h"

// Commercial Flagging headers
#include "CommDetector2.h"
#include "FrameAnalyzer.h"
#include "HistogramAnalyzer.h"
#include "SceneChangeDetector.h"
#include "quickselect.h"

using namespace commDetector2;
using namespace frameAnalyzer;

namespace {

int
scenechange_data_sort_desc_frequency(const void *aa, const void *bb)
{
    /* Descending by frequency, then ascending by color. */
    const auto *sc1 = (const struct SceneChangeDetector::scenechange_data*)aa;
    const auto *sc2 = (const struct SceneChangeDetector::scenechange_data*)bb;
    int freqdiff = sc2->frequency - sc1->frequency;
    return freqdiff ? freqdiff : sc1->color - sc2->color;
}

void
scenechange_data_init(SceneChangeDetector::SceneChangeData *scdata,
        const HistogramAnalyzer::Histogram *hh)
{
    unsigned int    ncolors = hh->size();

    for (unsigned int ii = 0; ii < ncolors; ii++)
    {
        (*scdata)[ii].color = ii;
        (*scdata)[ii].frequency = (*hh)[ii];
    }
    qsort(scdata->data(), scdata->size(), sizeof(SceneChangeDetector::scenechange_data),
            scenechange_data_sort_desc_frequency);
}

uint16_t
scenechange_data_diff(const SceneChangeDetector::SceneChangeData *sc1,
        const SceneChangeDetector::SceneChangeData *sc2)
{
    /*
     * Compute a notion of "difference" that takes into account the difference
     * in relative frequencies of the dominant colors.
     */
    uint16_t diff = 0;
    for (size_t ii = 0; ii < sizeof(*sc1)/sizeof((*sc1)[0]); ii++)
        diff += abs((*sc1)[ii].frequency - (*sc2)[ii].frequency) +
            abs((*sc1)[ii].color - (*sc2)[ii].color);
    return diff;
}

bool
writeData(const QString& filename, const std::vector<uint16_t>& scdiff)
{
    QByteArray fname = filename.toLocal8Bit();
    FILE *fp = fopen(fname.constData(), "w");
    if (fp == nullptr)
        return false;
    for (auto value : scdiff)
        (void)fprintf(fp, "%5u\n", value);
    if (fclose(fp))
        LOG(VB_COMMFLAG, LOG_ERR, QString("Error closing %1: %2")
                .arg(filename, strerror(errno)));
    return true;
}

void
computeChangeMap(FrameAnalyzer::FrameMap *changeMap,
        const std::vector<uint16_t>& scdiff, uint16_t mindiff)
{
    /*
     * Look for sudden changes in histogram.
     */
    changeMap->clear();
    for (size_t frameno = 0; frameno < scdiff.size(); frameno++)
    {
        if (scdiff[frameno] > mindiff)
            changeMap->insert(frameno, 0);
    }
}

};  /* namespace */

SceneChangeDetector::SceneChangeDetector(std::shared_ptr<HistogramAnalyzer> ha,
        const QString& debugdir)
    : m_histogramAnalyzer(std::move(ha))
    , m_debugData(debugdir + "/SceneChangeDetector.txt")
{
    LOG(VB_COMMFLAG, LOG_INFO, "SceneChangeDetector");

    /*
     * debugLevel:
     *      0: no debugging
     *      2: extra verbosity [O(nframes)]
     */
    m_debugLevel = gCoreContext->GetNumSetting("SceneChangeDetectorDebugLevel", 0);

    if (m_debugLevel >= 1)
    {
        createDebugDirectory(debugdir,
            QString("SceneChangeDetector debugLevel %1").arg(m_debugLevel));
        m_debugSceneChange = true;
    }
}


enum FrameAnalyzer::analyzeFrameResult
SceneChangeDetector::MythPlayerInited(MythPlayer *player,
        long long nframes)
{
    FrameAnalyzer::analyzeFrameResult ares =
        m_histogramAnalyzer->MythPlayerInited(player, nframes);

    m_fps = player->GetFrameRate();

    m_scData.resize(nframes);
    m_scDiff.resize(nframes);

    QSize video_disp_dim = player->GetVideoSize();

    LOG(VB_COMMFLAG, LOG_INFO, 
        QString("SceneChangeDetector::MythPlayerInited %1x%2")
            .arg(video_disp_dim.width())
            .arg(video_disp_dim.height()));

    return ares;
}

enum FrameAnalyzer::analyzeFrameResult
SceneChangeDetector::analyzeFrame(const MythVideoFrame *frame, long long frameno,
        long long *pNextFrame)
{
    *pNextFrame = kNextFrame;

    if (m_histogramAnalyzer->analyzeFrame(frame, frameno) ==
            FrameAnalyzer::ANALYZE_OK)
        return ANALYZE_OK;

    LOG(VB_COMMFLAG, LOG_ERR,
        QString("SceneChangeDetector::analyzeFrame error at frame %1")
            .arg(frameno));
    return ANALYZE_ERROR;
}

int
SceneChangeDetector::finished(long long nframes, bool final)
{
    if (m_histogramAnalyzer->finished(nframes, final))
        return -1;

    LOG(VB_COMMFLAG, LOG_INFO, QString("SceneChangeDetector::finished(%1)")
            .arg(nframes));

    const HistogramAnalyzer::Histogram *histogram =
        m_histogramAnalyzer->getHistograms();
    for (long long frameno = 0; frameno < nframes; frameno++)
        scenechange_data_init(&m_scData[frameno], &histogram[frameno]);
    m_scDiff[0] = 0;
    for (long long frameno = 1; frameno < nframes; frameno++)
        m_scDiff[frameno] = scenechange_data_diff(&m_scData[frameno - 1],
                &m_scData[frameno]);

    if (!m_sceneChangeDone && m_debugSceneChange)
    {
        if (final && writeData(m_debugData, m_scDiff))
        {
            LOG(VB_COMMFLAG, LOG_INFO, 
                QString("SceneChangeDetector::finished wrote %1")
                    .arg(m_debugData));
            m_sceneChangeDone = true;
        }
    }

    /* Identify all scene-change frames (changeMap). */
    std::vector<uint16_t> scdiffsort = m_scDiff;
    auto mindiff = quick_select<uint16_t>(scdiffsort.data(), nframes,
                                          (int)(0.979472 * nframes));
    LOG(VB_COMMFLAG, LOG_INFO,
        QString("SceneChangeDetector::finished applying threshold value %1")
            .arg(mindiff));
    computeChangeMap(&m_changeMap, m_scDiff, mindiff);
    if (m_debugLevel >= 2)
        frameAnalyzerReportMapms(&m_changeMap, m_fps, "SC frame");

    return 0;
}

int
SceneChangeDetector::reportTime(void) const
{
    return m_histogramAnalyzer->reportTime();
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
