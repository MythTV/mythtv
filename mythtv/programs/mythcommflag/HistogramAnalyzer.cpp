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
readData(const QString& filename, float *mean, unsigned char *median, float *stddev,
        int *frow, int *fcol, int *fwidth, int *fheight,
        HistogramAnalyzer::Histogram *histogram, unsigned char *monochromatic,
        long long nframes)
{
    quint32         counter[UCHAR_MAX + 1];

    QByteArray fname = filename.toLocal8Bit();
    FILE *fp = fopen(fname.constData(), "r");
    if (fp == nullptr)
        return false;

    for (long long frameno = 0; frameno < nframes; frameno++)
    {
        int monochromaticval = 0;
        int medianval = 0;
        int widthval = 0;
        int heightval = 0;
        int colval = 0;
        int rowval = 0;
        float meanval = NAN;
        float stddevval = NAN;
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
        for (uint & ctr : counter)
        {
            if (fscanf(fp, "%20x", &ctr) != 1)
            {
                LOG(VB_COMMFLAG, LOG_ERR,
                    QString("Not enough data in %1: frame %2")
                        .arg(filename).arg(frameno));
                goto error;
            }
            if (ctr > UCHAR_MAX)
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
        for (size_t ii = 0; ii < sizeof(counter)/sizeof(*counter); ii++)
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
writeData(const QString& filename, float *mean, unsigned char *median, float *stddev,
        int *frow, int *fcol, int *fwidth, int *fheight,
        HistogramAnalyzer::Histogram *histogram, unsigned char *monochromatic,
        long long nframes)
{
    QByteArray fname = filename.toLocal8Bit();
    FILE *fp = fopen(fname, "w");
    if (fp == nullptr)
        return false;
    for (long long frameno = 0; frameno < nframes; frameno++)
    {
        (void)fprintf(fp, "%3u %10.6f %3u %10.6f %5d %5d %5d %5d",
                      monochromatic[frameno],
                      static_cast<double>(mean[frameno]), median[frameno],
                      static_cast<double>(stddev[frameno]),
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
        const QString& debugdir)
    : m_pgmConverter(pgmc)
    , m_borderDetector(bd)
#ifdef PGM_CONVERT_GREYSCALE
    , m_debugdata(debugdir + "/HistogramAnalyzer-pgm.txt")
#else  /* !PGM_CONVERT_GREYSCALE */
    , m_debugdata(debugdir + "/HistogramAnalyzer-yuv.txt")
#endif /* !PGM_CONVERT_GREYSCALE */
{
    /*
     * debugLevel:
     *      0: no debugging
     *      1: cache frame information into debugdata [1 file]
     */
    m_debugLevel = gCoreContext->GetNumSetting("HistogramAnalyzerDebugLevel", 0);

    if (m_debugLevel >= 1)
    {
        createDebugDirectory(debugdir,
            QString("HistogramAnalyzer debugLevel %1").arg(m_debugLevel));
        m_debugHistVal = true;
    }
}

HistogramAnalyzer::~HistogramAnalyzer()
{
    delete []m_monochromatic;
    delete []m_mean;
    delete []m_median;
    delete []m_stddev;
    delete []m_fRow;
    delete []m_fCol;
    delete []m_fWidth;
    delete []m_fHeight;
    delete []m_histogram;
    delete []m_buf;
}

enum FrameAnalyzer::analyzeFrameResult
HistogramAnalyzer::MythPlayerInited(MythPlayer *player, long long nframes)
{
    if (m_histValDone)
        return FrameAnalyzer::ANALYZE_FINISHED;

    if (m_monochromatic)
        return FrameAnalyzer::ANALYZE_OK;

    QSize buf_dim = player->GetVideoBufferSize();
    unsigned int width  = buf_dim.width();
    unsigned int height = buf_dim.height();

    if (m_logoFinder && (m_logo = m_logoFinder->getTemplate(&m_logoRr1, &m_logoCc1,
                    &m_logoWidth, &m_logoHeight)))
    {
        m_logoRr2 = m_logoRr1 + m_logoHeight - 1;
        m_logoCc2 = m_logoCc1 + m_logoWidth - 1;
    }
    QString details = m_logo ? QString("logo %1x%2@(%3,%4)")
        .arg(m_logoWidth).arg(m_logoHeight).arg(m_logoCc1).arg(m_logoRr1) :
            QString("no logo");

    LOG(VB_COMMFLAG, LOG_INFO,
        QString("HistogramAnalyzer::MythPlayerInited %1x%2: %3")
            .arg(width).arg(height).arg(details));

    if (m_pgmConverter->MythPlayerInited(player))
        return FrameAnalyzer::ANALYZE_FATAL;

    if (m_borderDetector->MythPlayerInited(player))
        return FrameAnalyzer::ANALYZE_FATAL;

    m_mean = new float[nframes];
    m_median = new unsigned char[nframes];
    m_stddev = new float[nframes];
    m_fRow = new int[nframes];
    m_fCol = new int[nframes];
    m_fWidth = new int[nframes];
    m_fHeight = new int[nframes];
    m_histogram = new Histogram[nframes];
    m_monochromatic = new unsigned char[nframes];

    memset(m_mean, 0, nframes * sizeof(*m_mean));
    memset(m_median, 0, nframes * sizeof(*m_median));
    memset(m_stddev, 0, nframes * sizeof(*m_stddev));
    memset(m_fRow, 0, nframes * sizeof(*m_fRow));
    memset(m_fCol, 0, nframes * sizeof(*m_fCol));
    memset(m_fWidth, 0, nframes * sizeof(*m_fWidth));
    memset(m_fHeight, 0, nframes * sizeof(*m_fHeight));
    memset(m_histogram, 0, nframes * sizeof(*m_histogram));
    memset(m_monochromatic, 0, nframes * sizeof(*m_monochromatic));

    unsigned int npixels = width * height;
    m_buf = new unsigned char[npixels];

    if (m_debugHistVal)
    {
        if (readData(m_debugdata, m_mean, m_median, m_stddev, m_fRow, m_fCol,
                    m_fWidth, m_fHeight, m_histogram, m_monochromatic, nframes))
        {
            LOG(VB_COMMFLAG, LOG_INFO,
                QString("HistogramAnalyzer::MythPlayerInited read %1")
                    .arg(m_debugdata));
            m_histValDone = true;
            return FrameAnalyzer::ANALYZE_FINISHED;
        }
    }

    return FrameAnalyzer::ANALYZE_OK;
}

void
HistogramAnalyzer::setLogoState(TemplateFinder *finder)
{
    m_logoFinder = finder;
}

enum FrameAnalyzer::analyzeFrameResult
HistogramAnalyzer::analyzeFrame(const VideoFrame *frame, long long frameno)
{
    /*
     * Various statistical computations over pixel values: mean, median,
     * (running) standard deviation over sample population.
     */
    static constexpr int kDefaultColor = 0;

    /*
     * TUNABLE:
     *
     * Sampling coarseness of each frame. Higher values will allow analysis to
     * proceed faster (lower resolution), but might be less accurate. Lower
     * values will examine more pixels (higher resolution), but will run
     * slower.
     */
    static constexpr int kRInc = 4;
    static constexpr int kCInc = 4;
#define ROUNDUP(a,b)    (((a) + (b) - 1) / (b) * (b))

    int                 pgmwidth = 0;
    int                 pgmheight = 0;
    bool                ismonochromatic = false;
    int                 croprow = 0;
    int                 cropcol = 0;
    int                 cropwidth = 0;
    int                 cropheight = 0;
    unsigned int        borderpixels = 0;
    unsigned int        livepixels = 0;
    unsigned int        npixels = 0;
    unsigned int        halfnpixels = 0;
    unsigned char       *pp = nullptr;
    unsigned char       bordercolor = 0;
    unsigned long long  sumval = 0;
    unsigned long long  sumsquares = 0;
    int                 rr1 = 0;
    int                 cc1 = 0;
    int                 rr2 = 0;
    int                 cc2 = 0;
    int                 rr3 = 0;
    int                 cc3 = 0;
    struct timeval      start {};
    struct timeval      end {};
    struct timeval      elapsed {};

    if (m_lastFrameNo != kUncached && m_lastFrameNo == frameno)
        return FrameAnalyzer::ANALYZE_OK;

    const AVFrame *pgm = m_pgmConverter->getImage(frame, frameno, &pgmwidth, &pgmheight);
    if (pgm == nullptr)
        goto error;

    ismonochromatic = m_borderDetector->getDimensions(pgm, pgmheight, frameno,
            &croprow, &cropcol, &cropwidth, &cropheight) != 0;

    gettimeofday(&start, nullptr);

    m_fRow[frameno] = croprow;
    m_fCol[frameno] = cropcol;
    m_fWidth[frameno] = cropwidth;
    m_fHeight[frameno] = cropheight;

    if (ismonochromatic)
    {
        /* Optimization for monochromatic frames; just sample center area. */
        croprow = pgmheight * 3 / 8;
        cropheight = pgmheight / 4;
        cropcol = pgmwidth * 3 / 8;
        cropwidth = pgmwidth / 4;
    }

    rr1 = ROUNDUP(croprow, kRInc);
    cc1 = ROUNDUP(cropcol, kCInc);
    rr2 = ROUNDUP(croprow + cropheight, kRInc);
    cc2 = ROUNDUP(cropcol + cropwidth, kCInc);
    rr3 = ROUNDUP(pgmheight, kRInc);
    cc3 = ROUNDUP(pgmwidth, kCInc);

    borderpixels = (rr1 / kRInc) * (cc3 / kCInc) +        /* top */
        ((rr2 - rr1) / kRInc) * (cc1 / kCInc) +           /* left */
        ((rr2 - rr1) / kRInc) * ((cc3 - cc2) / kCInc) +   /* right */
        ((rr3 - rr2) / kRInc) * (cc3 / kCInc);            /* bottom */

    pp = &m_buf[borderpixels];
    memset(m_histVal, 0, sizeof(m_histVal));
    m_histVal[kDefaultColor] += borderpixels;
    for (int rr = rr1; rr < rr2; rr += kRInc)
    {
        int rroffset = rr * pgmwidth;

        for (int cc = cc1; cc < cc2; cc += kCInc)
        {
            if (m_logo && rr >= m_logoRr1 && rr <= m_logoRr2 &&
                    cc >= m_logoCc1 && cc <= m_logoCc2)
                continue; /* Exclude logo area from analysis. */

            unsigned char val = pgm->data[0][rroffset + cc];
            *pp++ = val;
            sumval += val;
            sumsquares += val * val;
            livepixels++;
            m_histVal[val]++;
        }
    }
    npixels = borderpixels + livepixels;

    /* Scale scores down to [0..255]. */
    halfnpixels = npixels / 2;
    for (unsigned int color = 0; color < UCHAR_MAX + 1; color++)
        m_histogram[frameno][color] =
            (m_histVal[color] * UCHAR_MAX + halfnpixels) / npixels;

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

    memset(m_buf, bordercolor, borderpixels * sizeof(*m_buf));
    m_monochromatic[frameno] = ismonochromatic ? 1 : 0;
    m_mean[frameno] = (float)sumval / npixels;
    m_median[frameno] = quick_select_median(m_buf, npixels);
    m_stddev[frameno] = npixels > 1 ?
        sqrt((sumsquares - (float)sumval * sumval / npixels) / (npixels - 1)) :
            0;

    (void)gettimeofday(&end, nullptr);
    timersub(&end, &start, &elapsed);
    timeradd(&m_analyzeTime, &elapsed, &m_analyzeTime);

    m_lastFrameNo = frameno;

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
    if (!m_histValDone && m_debugHistVal)
    {
        if (final && writeData(m_debugdata, m_mean, m_median, m_stddev, m_fRow, m_fCol,
                    m_fWidth, m_fHeight, m_histogram, m_monochromatic, nframes))
        {
            LOG(VB_COMMFLAG, LOG_INFO,
                QString("HistogramAnalyzer::finished wrote %1")
                    .arg(m_debugdata));
            m_histValDone = true;
        }
    }

    return 0;
}

int
HistogramAnalyzer::reportTime(void) const
{
    if (m_pgmConverter->reportTime())
        return -1;

    if (m_borderDetector->reportTime())
        return -1;

    LOG(VB_COMMFLAG, LOG_INFO, QString("HA Time: analyze=%1s")
            .arg(strftimeval(&m_analyzeTime)));
    return 0;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
