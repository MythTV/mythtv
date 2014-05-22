// POSIX headers
#include <sys/time.h> // for gettimeofday

// ANSI C headers
#include <cmath>

// MythTV headers
#include "mythcorecontext.h"
#include "mythplayer.h"
#include "mythlogging.h"

// Commercial Flagging headers
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
readData(QString filename, float *mean, unsigned char *median, float *stddev,
        int *frow, int *fcol, int *fwidth, int *fheight,
        HistogramAnalyzer::Histogram *histogram, unsigned char *monochromatic,
        long long nframes)
{
    FILE            *fp;
    long long       frameno;
    int             counter[UCHAR_MAX + 1];

    QByteArray fname = filename.toLocal8Bit();
    if (!(fp = fopen(fname.constData(), "r")))
        return false;

    for (frameno = 0; frameno < nframes; frameno++)
    {
        int monochromaticval, medianval, widthval, heightval, colval, rowval;
        float meanval, stddevval;
        int nitems = fscanf(fp, "%20d %20f %20d %20f %20d %20d %20d %20d",
                &monochromaticval, &meanval, &medianval, &stddevval,
                &widthval, &heightval, &colval, &rowval);
        if (nitems != 8)
        {
            LOG(VB_COMMFLAG, LOG_ERR,
                QString("Not enough data in %1: frame %2")
                    .arg(filename).arg(frameno));
            goto error;
        }
        if (monochromaticval < 0 || monochromaticval > 1 ||
                medianval < 0 || (uint)medianval > UCHAR_MAX ||
                widthval < 0 || heightval < 0 || colval < 0 || rowval < 0)
        {
            LOG(VB_COMMFLAG, LOG_ERR,
                QString("Data out of range in %1: frame %2")
                    .arg(filename).arg(frameno));
            goto error;
        }
        for (unsigned int ii = 0; ii < sizeof(counter)/sizeof(*counter); ii++)
        {
            if ((nitems = fscanf(fp, "%20x", &counter[ii])) != 1)
            {
                LOG(VB_COMMFLAG, LOG_ERR,
                    QString("Not enough data in %1: frame %2")
                        .arg(filename).arg(frameno));
                goto error;
            }
            if (counter[ii] < 0 || (uint)(counter[ii]) > UCHAR_MAX)
            {
                LOG(VB_COMMFLAG, LOG_ERR,
                    QString("Data out of range in %1: frame %2")
                        .arg(filename).arg(frameno));
                goto error;
            }
        }
        mean[frameno] = meanval;
        median[frameno] = medianval;
        stddev[frameno] = stddevval;
        frow[frameno] = rowval;
        fcol[frameno] = colval;
        fwidth[frameno] = widthval;
        fheight[frameno] = heightval;
        for (unsigned int ii = 0; ii < sizeof(counter)/sizeof(*counter); ii++)
            histogram[frameno][ii] = counter[ii];
        monochromatic[frameno] = !widthval || !heightval ? 1 : 0;
        /*
         * monochromaticval not used; it's written to file for debugging
         * convenience
         */
    }
    if (fclose(fp))
        LOG(VB_COMMFLAG, LOG_ERR, QString("Error closing %1: %2")
                .arg(filename).arg(strerror(errno)));
    return true;

error:
    if (fclose(fp))
        LOG(VB_COMMFLAG, LOG_ERR, QString("Error closing %1: %2")
                .arg(filename).arg(strerror(errno)));
    return false;
}

bool
writeData(QString filename, float *mean, unsigned char *median, float *stddev,
        int *frow, int *fcol, int *fwidth, int *fheight,
        HistogramAnalyzer::Histogram *histogram, unsigned char *monochromatic,
        long long nframes)
{
    FILE            *fp;
    long long       frameno;

    QByteArray fname = filename.toLocal8Bit();
    if (!(fp = fopen(fname, "w")))
        return false;
    for (frameno = 0; frameno < nframes; frameno++)
    {
        (void)fprintf(fp, "%3u %10.6f %3u %10.6f %5d %5d %5d %5d",
                      monochromatic[frameno],
                      mean[frameno], median[frameno], stddev[frameno],
                      fwidth[frameno], fheight[frameno],
                      fcol[frameno], frow[frameno]);
        for (unsigned int ii = 0; ii < UCHAR_MAX + 1; ii++)
            (void)fprintf(fp, " %02x", histogram[frameno][ii]);
        (void)fprintf(fp, "\n");
    }
    if (fclose(fp))
        LOG(VB_COMMFLAG, LOG_ERR, QString("Error closing %1: %2")
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
    , logowidth(-1)
    , logoheight(-1)
    , logorr1(-1)
    , logocc1(-1)
    , logorr2(-1)
    , logocc2(-1)
    , mean(NULL)
    , median(NULL)
    , stddev(NULL)
    , frow(NULL)
    , fcol(NULL)
    , fwidth(NULL)
    , fheight(NULL)
    , histogram(NULL)
    , monochromatic(NULL)
    , buf(NULL)
    , lastframeno(-1)
    , debugLevel(0)
#ifdef PGM_CONVERT_GREYSCALE
    , debugdata(debugdir + "/HistogramAnalyzer-pgm.txt")
#else  /* !PGM_CONVERT_GREYSCALE */
    , debugdata(debugdir + "/HistogramAnalyzer-yuv.txt")
#endif /* !PGM_CONVERT_GREYSCALE */
    , debug_histval(false)
    , histval_done(false)
{
    memset(histval, 0, sizeof(int) * (UCHAR_MAX + 1));
    memset(&analyze_time, 0, sizeof(analyze_time));

    /*
     * debugLevel:
     *      0: no debugging
     *      1: cache frame information into debugdata [1 file]
     */
    debugLevel = gCoreContext->GetNumSetting("HistogramAnalyzerDebugLevel", 0);

    if (debugLevel >= 1)
    {
        createDebugDirectory(debugdir,
            QString("HistogramAnalyzer debugLevel %1").arg(debugLevel));
        debug_histval = true;
    }
}

HistogramAnalyzer::~HistogramAnalyzer()
{
    if (monochromatic)
        delete []monochromatic;
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
    if (histogram)
        delete []histogram;
    if (buf)
        delete []buf;
}

enum FrameAnalyzer::analyzeFrameResult
HistogramAnalyzer::MythPlayerInited(MythPlayer *player, long long nframes)
{
    if (histval_done)
        return FrameAnalyzer::ANALYZE_FINISHED;

    if (monochromatic)
        return FrameAnalyzer::ANALYZE_OK;

    QSize buf_dim = player->GetVideoBufferSize();
    unsigned int width  = buf_dim.width();
    unsigned int height = buf_dim.height();

    if (logoFinder && (logo = logoFinder->getTemplate(&logorr1, &logocc1,
                    &logowidth, &logoheight)))
    {
        logorr2 = logorr1 + logoheight - 1;
        logocc2 = logocc1 + logowidth - 1;
    }
    QString details = logo ? QString("logo %1x%2@(%3,%4)")
        .arg(logowidth).arg(logoheight).arg(logocc1).arg(logorr1) :
            QString("no logo");

    LOG(VB_COMMFLAG, LOG_INFO,
        QString("HistogramAnalyzer::MythPlayerInited %1x%2: %3")
            .arg(width).arg(height).arg(details));

    if (pgmConverter->MythPlayerInited(player))
        return FrameAnalyzer::ANALYZE_FATAL;

    if (borderDetector->MythPlayerInited(player))
        return FrameAnalyzer::ANALYZE_FATAL;

    mean = new float[nframes];
    median = new unsigned char[nframes];
    stddev = new float[nframes];
    frow = new int[nframes];
    fcol = new int[nframes];
    fwidth = new int[nframes];
    fheight = new int[nframes];
    histogram = new Histogram[nframes];
    monochromatic = new unsigned char[nframes];

    memset(mean, 0, nframes * sizeof(*mean));
    memset(median, 0, nframes * sizeof(*median));
    memset(stddev, 0, nframes * sizeof(*stddev));
    memset(frow, 0, nframes * sizeof(*frow));
    memset(fcol, 0, nframes * sizeof(*fcol));
    memset(fwidth, 0, nframes * sizeof(*fwidth));
    memset(fheight, 0, nframes * sizeof(*fheight));
    memset(histogram, 0, nframes * sizeof(*histogram));
    memset(monochromatic, 0, nframes * sizeof(*monochromatic));

    unsigned int npixels = width * height;
    buf = new unsigned char[npixels];

    if (debug_histval)
    {
        if (readData(debugdata, mean, median, stddev, frow, fcol,
                    fwidth, fheight, histogram, monochromatic, nframes))
        {
            LOG(VB_COMMFLAG, LOG_INFO,
                QString("HistogramAnalyzer::MythPlayerInited read %1")
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
    static const int    DEFAULT_COLOR = 0;

    /*
     * TUNABLE:
     *
     * Sampling coarseness of each frame. Higher values will allow analysis to
     * proceed faster (lower resolution), but might be less accurate. Lower
     * values will examine more pixels (higher resolution), but will run
     * slower.
     */
    static const int    RINC = 4;
    static const int    CINC = 4;
#define ROUNDUP(a,b)    (((a) + (b) - 1) / (b) * (b))

    const AVPicture     *pgm;
    int                 pgmwidth, pgmheight;
    bool                ismonochromatic;
    int                 croprow, cropcol, cropwidth, cropheight;
    unsigned int        borderpixels, livepixels, npixels, halfnpixels;
    unsigned char       *pp, bordercolor;
    unsigned long long  sumval, sumsquares;
    int                 rr, cc, rr1, cc1, rr2, cc2, rr3, cc3;
    struct timeval      start, end, elapsed;

    if (lastframeno != UNCACHED && lastframeno == frameno)
        return FrameAnalyzer::ANALYZE_OK;

    if (!(pgm = pgmConverter->getImage(frame, frameno, &pgmwidth, &pgmheight)))
        goto error;

    ismonochromatic = borderDetector->getDimensions(pgm, pgmheight, frameno,
            &croprow, &cropcol, &cropwidth, &cropheight) != 0;

    gettimeofday(&start, NULL);

    frow[frameno] = croprow;
    fcol[frameno] = cropcol;
    fwidth[frameno] = cropwidth;
    fheight[frameno] = cropheight;

    if (ismonochromatic)
    {
        /* Optimization for monochromatic frames; just sample center area. */
        croprow = pgmheight * 3 / 8;
        cropheight = pgmheight / 4;
        cropcol = pgmwidth * 3 / 8;
        cropwidth = pgmwidth / 4;
    }

    rr1 = ROUNDUP(croprow, RINC);
    cc1 = ROUNDUP(cropcol, CINC);
    rr2 = ROUNDUP(croprow + cropheight, RINC);
    cc2 = ROUNDUP(cropcol + cropwidth, CINC);
    rr3 = ROUNDUP(pgmheight, RINC);
    cc3 = ROUNDUP(pgmwidth, CINC);

    borderpixels = (rr1 / RINC) * (cc3 / CINC) +        /* top */
        ((rr2 - rr1) / RINC) * (cc1 / CINC) +           /* left */
        ((rr2 - rr1) / RINC) * ((cc3 - cc2) / CINC) +   /* right */
        ((rr3 - rr2) / RINC) * (cc3 / CINC);            /* bottom */

    sumval = 0;
    sumsquares = 0;
    livepixels = 0;
    pp = &buf[borderpixels];
    memset(histval, 0, sizeof(histval));
    histval[DEFAULT_COLOR] += borderpixels;
    for (rr = rr1; rr < rr2; rr += RINC)
    {
        int rroffset = rr * pgmwidth;

        for (cc = cc1; cc < cc2; cc += CINC)
        {
            if (logo && rr >= logorr1 && rr <= logorr2 &&
                    cc >= logocc1 && cc <= logocc2)
                continue; /* Exclude logo area from analysis. */

            unsigned char val = pgm->data[0][rroffset + cc];
            *pp++ = val;
            sumval += val;
            sumsquares += val * val;
            livepixels++;
            histval[val]++;
        }
    }
    npixels = borderpixels + livepixels;

    /* Scale scores down to [0..255]. */
    halfnpixels = npixels / 2;
    for (unsigned int color = 0; color < UCHAR_MAX + 1; color++)
        histogram[frameno][color] =
            (histval[color] * UCHAR_MAX + halfnpixels) / npixels;

    bordercolor = 0;
    if (ismonochromatic && livepixels)
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
    monochromatic[frameno] = ismonochromatic ? 1 : 0;
    mean[frameno] = (float)sumval / npixels;
    median[frameno] = quick_select_median(buf, npixels);
    stddev[frameno] = npixels > 1 ?
        sqrt((sumsquares - (float)sumval * sumval / npixels) / (npixels - 1)) :
            0;

    (void)gettimeofday(&end, NULL);
    timersub(&end, &start, &elapsed);
    timeradd(&analyze_time, &elapsed, &analyze_time);

    lastframeno = frameno;

    return FrameAnalyzer::ANALYZE_OK;

error:
    LOG(VB_COMMFLAG, LOG_ERR,
        QString("HistogramAnalyzer::analyzeFrame error at frame %1")
            .arg(frameno));

    return FrameAnalyzer::ANALYZE_ERROR;
}

int
HistogramAnalyzer::finished(long long nframes, bool final)
{
    if (!histval_done && debug_histval)
    {
        if (final && writeData(debugdata, mean, median, stddev, frow, fcol,
                    fwidth, fheight, histogram, monochromatic, nframes))
        {
            LOG(VB_COMMFLAG, LOG_INFO,
                QString("HistogramAnalyzer::finished wrote %1")
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

    LOG(VB_COMMFLAG, LOG_INFO, QString("HA Time: analyze=%1s")
            .arg(strftimeval(&analyze_time)));
    return 0;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
