// ANSI C headers
#include <cstdlib>
#include <cmath>

// MythTV headers
#include "mythcorecontext.h"    /* gContext */
#include "mythplayer.h"
#include "mythlogging.h"

// Commercial Flagging headers
#include "CommDetector2.h"
#include "FrameAnalyzer.h"
#include "quickselect.h"
#include "HistogramAnalyzer.h"
#include "SceneChangeDetector.h"

using namespace commDetector2;
using namespace frameAnalyzer;

namespace {

int
scenechange_data_sort_desc_frequency(const void *aa, const void *bb)
{
    /* Descending by frequency, then ascending by color. */
    const struct SceneChangeDetector::scenechange_data *sc1 =
        (const struct SceneChangeDetector::scenechange_data*)aa;
    const struct SceneChangeDetector::scenechange_data *sc2 =
        (const struct SceneChangeDetector::scenechange_data*)bb;
    int freqdiff = sc2->frequency - sc1->frequency;
    return freqdiff ? freqdiff : sc1->color - sc2->color;
}

void
scenechange_data_init(SceneChangeDetector::SceneChangeData *scdata,
        const HistogramAnalyzer::Histogram *hh)
{
    unsigned int    ncolors = sizeof(*hh)/sizeof((*hh)[0]);

    for (unsigned int ii = 0; ii < ncolors; ii++)
    {
        (*scdata)[ii].color = ii;
        (*scdata)[ii].frequency = (*hh)[ii];
    }
    qsort(*scdata, sizeof(*scdata)/sizeof((*scdata)[0]), sizeof((*scdata)[0]),
            scenechange_data_sort_desc_frequency);
}

unsigned short
scenechange_data_diff(const SceneChangeDetector::SceneChangeData *sc1,
        const SceneChangeDetector::SceneChangeData *sc2)
{
    /*
     * Compute a notion of "difference" that takes into account the difference
     * in relative frequencies of the dominant colors.
     */
    unsigned short diff = 0;
    for (size_t ii = 0; ii < sizeof(*sc1)/sizeof((*sc1)[0]); ii++)
        diff += abs((*sc1)[ii].frequency - (*sc2)[ii].frequency) +
            abs((*sc1)[ii].color - (*sc2)[ii].color);
    return diff;
}

bool
writeData(const QString& filename, const unsigned short *scdiff, long long nframes)
{
    QByteArray fname = filename.toLocal8Bit();
    FILE *fp = fopen(fname.constData(), "w");
    if (fp == nullptr)
        return false;
    for (long long frameno = 0; frameno < nframes; frameno++)
        (void)fprintf(fp, "%5u\n", scdiff[frameno]);
    if (fclose(fp))
        LOG(VB_COMMFLAG, LOG_ERR, QString("Error closing %1: %2")
                .arg(filename).arg(strerror(errno)));
    return true;
}

void
computeChangeMap(FrameAnalyzer::FrameMap *changeMap, long long nframes,
        const unsigned short *scdiff, unsigned short mindiff)
{
    /*
     * Look for sudden changes in histogram.
     */
    changeMap->clear();
    for (long long frameno = 0; frameno < nframes; frameno++)
    {
        if (scdiff[frameno] > mindiff)
            changeMap->insert(frameno, 0);
    }
}

};  /* namespace */

SceneChangeDetector::SceneChangeDetector(HistogramAnalyzer *ha,
        const QString& debugdir)
    : m_histogramAnalyzer(ha)
    , m_debugdata(debugdir + "/SceneChangeDetector.txt")
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
        m_debug_scenechange = true;
    }
}

void SceneChangeDetector::deleteLater(void)
{
    delete []m_scdata;
    delete []m_scdiff;
}

enum FrameAnalyzer::analyzeFrameResult
SceneChangeDetector::MythPlayerInited(MythPlayer *player,
        long long nframes)
{
    FrameAnalyzer::analyzeFrameResult ares =
        m_histogramAnalyzer->MythPlayerInited(player, nframes);

    m_fps = player->GetFrameRate();

    m_scdata = new SceneChangeData[nframes];
    memset(m_scdata, 0, nframes * sizeof(*m_scdata));

    m_scdiff = new unsigned short[nframes];
    memset(m_scdiff, 0, nframes * sizeof(*m_scdiff));

    QSize video_disp_dim = player->GetVideoSize();

    LOG(VB_COMMFLAG, LOG_INFO, 
        QString("SceneChangeDetector::MythPlayerInited %1x%2")
            .arg(video_disp_dim.width())
            .arg(video_disp_dim.height()));

    return ares;
}

enum FrameAnalyzer::analyzeFrameResult
SceneChangeDetector::analyzeFrame(const VideoFrame *frame, long long frameno,
        long long *pNextFrame)
{
    *pNextFrame = NEXTFRAME;

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
        scenechange_data_init(&m_scdata[frameno], &histogram[frameno]);
    m_scdiff[0] = 0;
    for (long long frameno = 1; frameno < nframes; frameno++)
        m_scdiff[frameno] = scenechange_data_diff(&m_scdata[frameno - 1],
                &m_scdata[frameno]);

    if (!m_scenechange_done && m_debug_scenechange)
    {
        if (final && writeData(m_debugdata, m_scdiff, nframes))
        {
            LOG(VB_COMMFLAG, LOG_INFO, 
                QString("SceneChangeDetector::finished wrote %1")
                    .arg(m_debugdata));
            m_scenechange_done = true;
        }
    }

    /* Identify all scene-change frames (changeMap). */
    unsigned short *scdiffsort = new unsigned short[nframes];
    memcpy(scdiffsort, m_scdiff, nframes * sizeof(*m_scdiff));
    unsigned short mindiff = quick_select_ushort(scdiffsort, nframes,
                                                 (int)(0.979472 * nframes));
    LOG(VB_COMMFLAG, LOG_INFO,
        QString("SceneChangeDetector::finished applying threshold value %1")
            .arg(mindiff));
    computeChangeMap(&m_changeMap, nframes, m_scdiff, mindiff);
    delete []scdiffsort;
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
