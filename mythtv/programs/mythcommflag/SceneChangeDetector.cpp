#include <stdio.h>
#include <math.h>

#include "mythcontext.h"    /* gContext */
#include "NuppelVideoPlayer.h"

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
    for (unsigned int ii = 0; ii < sizeof(*sc1)/sizeof((*sc1)[0]); ii++)
        diff += abs((*sc1)[ii].frequency - (*sc2)[ii].frequency) +
            abs((*sc1)[ii].color - (*sc2)[ii].color);
    return diff;
}

bool
writeData(QString filename, const unsigned short *scdiff, long long nframes)
{
    FILE            *fp;
    long long       frameno;

    if (!(fp = fopen(filename, "w")))
        return false;
    for (frameno = 0; frameno < nframes; frameno++)
        (void)fprintf(fp, "%5u\n", scdiff[frameno]);
    if (fclose(fp))
        VERBOSE(VB_COMMFLAG, QString("Error closing %1: %2")
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
    long long           frameno;

    changeMap->clear();
    for (frameno = 0; frameno < nframes; frameno++)
    {
        if (scdiff[frameno] > mindiff)
            changeMap->insert(frameno, 0);
    }
}

};  /* namespace */

SceneChangeDetector::SceneChangeDetector(HistogramAnalyzer *ha,
        QString debugdir)
    : FrameAnalyzer()
    , histogramAnalyzer(ha)
    , scdata(NULL)
    , scdiff(NULL)
    , debugLevel(0)
    , debugdata(debugdir + "/SceneChangeDetector.txt")
    , debug_scenechange(false)
    , scenechange_done(false)
{
    VERBOSE(VB_COMMFLAG, "SceneChangeDetector");

    /*
     * debugLevel:
     *      0: no debugging
     *      2: extra verbosity [O(nframes)]
     */
    debugLevel = gContext->GetNumSetting("SceneChangeDetectorDebugLevel", 0);

    if (debugLevel >= 1)
        createDebugDirectory(debugdir,
            QString("SceneChangeDetector debugLevel %1").arg(debugLevel));

    if (debugLevel >= 1)
        debug_scenechange = true;
}

SceneChangeDetector::~SceneChangeDetector(void)
{
    if (scdata)
        delete []scdata;
    if (scdiff)
        delete []scdiff;
}

enum FrameAnalyzer::analyzeFrameResult
SceneChangeDetector::nuppelVideoPlayerInited(NuppelVideoPlayer *nvp,
        long long nframes)
{
    FrameAnalyzer::analyzeFrameResult ares =
        histogramAnalyzer->nuppelVideoPlayerInited(nvp, nframes);

    fps = nvp->GetFrameRate();

    scdata = new SceneChangeData[nframes];
    memset(scdata, 0, nframes * sizeof(*scdata));

    scdiff = new unsigned short[nframes];
    memset(scdiff, 0, nframes * sizeof(*scdiff));

    VERBOSE(VB_COMMFLAG, QString(
                "SceneChangeDetector::nuppelVideoPlayerInited %1x%2")
            .arg(nvp->GetVideoWidth())
            .arg(nvp->GetVideoHeight()));

    return ares;
}

enum FrameAnalyzer::analyzeFrameResult
SceneChangeDetector::analyzeFrame(const VideoFrame *frame, long long frameno,
        long long *pNextFrame)
{
    *pNextFrame = NEXTFRAME;

    if (histogramAnalyzer->analyzeFrame(frame, frameno) ==
            FrameAnalyzer::ANALYZE_OK)
        return ANALYZE_OK;

    VERBOSE(VB_COMMFLAG,
            QString("SceneChangeDetector::analyzeFrame error at frame %1")
            .arg(frameno));
    return ANALYZE_ERROR;
}

int
SceneChangeDetector::finished(long long nframes, bool final)
{
    if (histogramAnalyzer->finished(nframes, final))
        return -1;

    VERBOSE(VB_COMMFLAG, QString("SceneChangeDetector::finished(%1)")
            .arg(nframes));

    const HistogramAnalyzer::Histogram *histogram =
        histogramAnalyzer->getHistograms();
    for (unsigned int frameno = 0; frameno < nframes; frameno++)
        (void)scenechange_data_init(&scdata[frameno], &histogram[frameno]);
    scdiff[0] = 0;
    for (unsigned int frameno = 1; frameno < nframes; frameno++)
        scdiff[frameno] = scenechange_data_diff(&scdata[frameno - 1],
                &scdata[frameno]);

    if (!scenechange_done && debug_scenechange)
    {
        if (final && writeData(debugdata, scdiff, nframes))
        {
            VERBOSE(VB_COMMFLAG, QString(
                        "SceneChangeDetector::finished wrote %1")
                    .arg(debugdata));
            scenechange_done = true;
        }
    }

    /* Identify all scene-change frames (changeMap). */
    unsigned short *scdiffsort = new unsigned short[nframes];
    memcpy(scdiffsort, scdiff, nframes * sizeof(*scdiff));
    unsigned short mindiff = quick_select_ushort(scdiffsort, nframes,
            (int)(0.979472 * nframes));
    VERBOSE(VB_COMMFLAG, QString(
                "SceneChangeDetector::finished applying threshold value %1")
            .arg(mindiff));
    computeChangeMap(&changeMap, nframes, scdiff, mindiff);
    delete []scdiffsort;
    if (debugLevel >= 2)
        frameAnalyzerReportMapms(&changeMap, fps, "SC frame");

    return 0;
}

int
SceneChangeDetector::reportTime(void) const
{
    return histogramAnalyzer->reportTime();
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
