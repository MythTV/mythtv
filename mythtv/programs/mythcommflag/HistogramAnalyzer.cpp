#include <stdio.h>
#include <math.h>

#include "mythcontext.h"    /* gContext */
#include "NuppelVideoPlayer.h"

#include "CommDetector2.h"
#include "FrameAnalyzer.h"
#include "PGMConverter.h"
#include "BorderDetector.h"
#include "quickselect.h"
#include "TemplateFinder.h"
#include "HistogramAnalyzer.h"

using namespace commDetector2;
using namespace frameAnalyzer;

namespace {

bool
readData(QString filename, unsigned char *blank, float *mean,
        unsigned char *median, float *stddev,
        int *frow, int *fcol, int *fwidth, int *fheight, long long nframes)
{
    FILE            *fp;
    long long       frameno;

    if (!(fp = fopen(filename, "r")))
        return false;

    for (frameno = 0; frameno < nframes; frameno++)
    {
        int blankval, medianval, widthval, heightval, colval, rowval;
        float meanval, stddevval;
        int nitems = fscanf(fp, "%d %f %d %f %d %d %d %d",
                &blankval, &meanval, &medianval, &stddevval,
                &widthval, &heightval, &colval, &rowval);
        if (nitems != 8)
        {
            VERBOSE(VB_COMMFLAG, QString(
                        "Not enough data in %1: frame %2")
                    .arg(filename).arg(frameno));
            goto error;
        }
        if (blankval < 0 || blankval > 1 ||
                medianval < 0 || medianval > UCHAR_MAX ||
                widthval < 0 || heightval < 0 || colval < 0 || rowval < 0)
        {
            VERBOSE(VB_COMMFLAG, QString(
                        "Data out of range in %1: frame %2")
                    .arg(filename).arg(frameno));
            goto error;
        }
        blank[frameno] = blankval ? 1 : 0;
        mean[frameno] = meanval;
        median[frameno] = medianval;
        stddev[frameno] = stddevval;
        frow[frameno] = rowval;
        fcol[frameno] = colval;
        fwidth[frameno] = widthval;
        fheight[frameno] = heightval;
    }
    if (fclose(fp))
        VERBOSE(VB_COMMFLAG, QString("Error closing %1: %2")
                .arg(filename).arg(strerror(errno)));
    return true;

error:
    if (fclose(fp))
        VERBOSE(VB_COMMFLAG, QString("Error closing %1: %2")
                .arg(filename).arg(strerror(errno)));
    return false;
}

bool
writeData(QString filename, unsigned char *blank, float *mean,
        unsigned char *median, float *stddev,
        int *frow, int *fcol, int *fwidth, int *fheight, long long nframes)
{
    FILE            *fp;
    long long       frameno;

    if (!(fp = fopen(filename, "w")))
        return false;

    for (frameno = 0; frameno < nframes; frameno++)
        (void)fprintf(fp, "%3u %10.6f %3u %10.6f %5u %5u %5u %5u\n",
                      blank[frameno],
                      mean[frameno], median[frameno], stddev[frameno],
                      fwidth[frameno], fheight[frameno],
                      fcol[frameno], frow[frameno]);
    if (fclose(fp))
        VERBOSE(VB_COMMFLAG, QString("Error closing %1: %2")
                .arg(filename).arg(strerror(errno)));
    return true;
}

};  /* namespace */

HistogramAnalyzer::HistogramAnalyzer(PGMConverter *pgmc, BorderDetector *bd,
        QString debugdir)
    : pgmConverter(pgmc)
    , borderDetector(bd)
    , logoFinder(NULL)
    , logo(NULL)
    , blank(NULL)
    , mean(NULL)
    , median(NULL)
    , stddev(NULL)
    , frow(NULL)
    , fcol(NULL)
    , fwidth(NULL)
    , fheight(NULL)
    , buf(NULL)
    , debugLevel(0)
#ifdef PGM_CONVERT_GREYSCALE
    , debugdata(debugdir + "/HistogramAnalyzer-pgm.txt")
#else  /* !PGM_CONVERT_GREYSCALE */
    , debugdata(debugdir + "/HistogramAnalyzer-yuv.txt")
#endif /* !PGM_CONVERT_GREYSCALE */
    , debug_histval(false)
    , histval_done(false)
{
    memset(&analyze_time, 0, sizeof(analyze_time));

    /*
     * debugLevel:
     *      0: no debugging
     *      1: cache frame information into debugdata [1 file]
     */
    debugLevel = gContext->GetNumSetting("HistogramAnalyzerDebugLevel", 0);

    if (debugLevel >= 1)
        createDebugDirectory(debugdir,
            QString("HistogramAnalyzer debugLevel %1").arg(debugLevel));

    if (debugLevel >= 1)
        debug_histval = true;
}

HistogramAnalyzer::~HistogramAnalyzer(void)
{
    if (blank)
        delete []blank;
    if (mean)
        delete []mean;
    if (median)
        delete []median;
    if (stddev)
        delete []stddev;
    if (frow)
        delete []frow;
    if (fcol)
        delete []fcol;
    if (fwidth)
        delete []fwidth;
    if (fheight)
        delete []fheight;
    if (buf)
        delete []buf;
}

enum FrameAnalyzer::analyzeFrameResult
HistogramAnalyzer::nuppelVideoPlayerInited(NuppelVideoPlayer *nvp)
{
    if (blank)
        return FrameAnalyzer::ANALYZE_OK;

    nframes = nvp->GetTotalFrameCount();

    unsigned int width = nvp->GetVideoWidth();
    unsigned int height = nvp->GetVideoHeight();

    if ((logo = logoFinder->getTemplate(&logorr1, &logocc1,
                    &logowidth, &logoheight)))
    {
        logorr2 = logorr1 + logoheight - 1;
        logocc2 = logocc1 + logowidth - 1;
    }
    QString details = logo ? QString("logo %1x%2@(%3,%4)")
        .arg(logowidth).arg(logoheight).arg(logocc1).arg(logorr1) :
            QString("no logo");

    VERBOSE(VB_COMMFLAG, QString(
                "HistogramAnalyzer::nuppelVideoPlayerInited %1x%2: %3")
            .arg(width).arg(height).arg(details));

    if (pgmConverter->nuppelVideoPlayerInited(nvp))
	    return FrameAnalyzer::ANALYZE_FATAL;

    if (borderDetector->nuppelVideoPlayerInited(nvp))
	    return FrameAnalyzer::ANALYZE_FATAL;

    blank = new unsigned char[nframes];
    mean = new float[nframes];
    median = new unsigned char[nframes];
    stddev = new float[nframes];
    frow = new int[nframes];
    fcol = new int[nframes];
    fwidth = new int[nframes];
    fheight = new int[nframes];

    memset(blank, 0, nframes * sizeof(*blank));
    memset(mean, 0, nframes * sizeof(*mean));
    memset(median, 0, nframes * sizeof(*median));
    memset(stddev, 0, nframes * sizeof(*stddev));
    memset(frow, 0, nframes * sizeof(*frow));
    memset(fcol, 0, nframes * sizeof(*fcol));
    memset(fwidth, 0, nframes * sizeof(*fwidth));
    memset(fheight, 0, nframes * sizeof(*fheight));

    unsigned int npixels = width * height;
    buf = new unsigned char[npixels];

    if (debug_histval)
    {
        if (readData(debugdata, blank, mean, median, stddev,
                    frow, fcol, fwidth, fheight, nframes))
        {
            VERBOSE(VB_COMMFLAG, QString(
                        "HistogramAnalyzer::nuppelVideoPlayerInited read %1")
                    .arg(debugdata));
            histval_done = true;
            return FrameAnalyzer::ANALYZE_FINISHED;
        }
    }

    return FrameAnalyzer::ANALYZE_OK;
}

void
HistogramAnalyzer::setLogoState(TemplateFinder *finder)
{
    logoFinder = finder;
}

enum FrameAnalyzer::analyzeFrameResult
HistogramAnalyzer::analyzeFrame(const VideoFrame *frame, long long frameno)
{
    /*
     * Various statistical computations over pixel values: mean, median,
     * (running) standard deviation over sample population.
     */

    /*
     * TUNABLE:
     *
     * Coverage of each frame. Higher values will allow analysis to proceed
     * faster, but might be less accurate. Lower values will examine more
     * pixels, but will run slower.
     */
    static const int    RINC = 4;
    static const int    CINC = 4;

    const AVPicture     *pgm;
    int                 pgmwidth, pgmheight;
    bool                isblank;
    int                 croprow, cropcol, cropwidth, cropheight;
    unsigned int        borderpixels, livepixels, npixels;
    unsigned char       *pp, bordercolor;
    unsigned long long  sumval, sumsquares;
    int                 rr, cc, rr2, cc2;
    struct timeval      start, end, elapsed;

    if (!(pgm = pgmConverter->getImage(frame, frameno, &pgmwidth, &pgmheight)))
        goto error;

    isblank = borderDetector->getDimensions(pgm, pgmheight, frameno,
            &croprow, &cropcol, &cropwidth, &cropheight) != 0;

    (void)gettimeofday(&start, NULL);

    frow[frameno] = croprow;
    fcol[frameno] = cropcol;
    fwidth[frameno] = cropwidth;
    fheight[frameno] = cropheight;

    if (isblank)
    {
        /*
         * Optimization for monochromatic frames; just sample the center area.
         * This correctly handles both "blank" (black) frames as well as
         * monochromatic frames (e.g., bright white-light "flashback" frames).
         */
        croprow = pgmheight * 3 / 8;
        cropheight = pgmheight / 4;
        cropcol = pgmwidth * 3 / 8;
        cropwidth = pgmwidth / 4;
    }

    borderpixels = ((pgmheight - cropheight) / RINC) * (pgmwidth / CINC) +
        (cropheight / RINC) * ((pgmwidth - cropwidth) / CINC);
    livepixels = (cropheight / RINC) * (cropwidth / CINC);
    if (logo)
        livepixels -= (logoheight / RINC) * (logowidth / CINC);
    npixels = borderpixels + livepixels;

    pp = buf + borderpixels;
    sumval = 0;
    sumsquares = 0;

    rr2 = croprow + cropheight;
    cc2 = cropcol + cropwidth;
    for (rr = croprow; rr < rr2; rr += RINC)
    {
        for (cc = cropcol; cc < cc2; cc += CINC)
        {
            if (logo && rr >= logorr1 && rr <= logorr2 &&
                    cc >= logocc1 && cc <= logocc2)
                continue;   /* Exclude logo area from analysis. */

            unsigned char val = pgm->data[0][rr * pgmwidth + cc];
            *pp++ = val;
            sumval += val;
            sumsquares += val * val;
        }
    }

    bordercolor = 0;
    if (isblank)
    {
        /*
         * Fake up the margin pixels to be of the same color as the sampled
         * area.
         */
        bordercolor = (sumval + livepixels - 1) / livepixels;
        sumval += borderpixels * bordercolor;
        sumsquares += borderpixels * bordercolor * bordercolor;
    }

    memset(buf, bordercolor, borderpixels * sizeof(*buf));
    blank[frameno] = isblank ? 1 : 0;
    mean[frameno] = (float)sumval / npixels;
    median[frameno] = quick_select_median(buf, npixels);
    stddev[frameno] = sqrt((sumsquares - (float)sumval * sumval / npixels) /
            (npixels - 1));

    (void)gettimeofday(&end, NULL);
    timersub(&end, &start, &elapsed);
    timeradd(&analyze_time, &elapsed, &analyze_time);

    return FrameAnalyzer::ANALYZE_OK;

error:
    VERBOSE(VB_COMMFLAG,
            QString("HistogramAnalyzer::analyzeFrame error at frame %1")
            .arg(frameno));

    return FrameAnalyzer::ANALYZE_ERROR;
}

int
HistogramAnalyzer::finished(void)
{
    if (!histval_done && debug_histval)
    {
        if (writeData(debugdata, blank, mean, median, stddev,
                    frow, fcol, fwidth, fheight, nframes))
        {
            VERBOSE(VB_COMMFLAG, QString(
                        "HistogramAnalyzer::finished wrote %1")
                    .arg(debugdata));
            histval_done = true;
        }
    }

    return 0;
}

int
HistogramAnalyzer::reportTime(void) const
{
    if (pgmConverter->reportTime())
        return -1;

    if (borderDetector->reportTime())
        return -1;

    VERBOSE(VB_COMMFLAG, QString("HA Time: analyze=%1s")
            .arg(strftimeval(&analyze_time)));
    return 0;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
