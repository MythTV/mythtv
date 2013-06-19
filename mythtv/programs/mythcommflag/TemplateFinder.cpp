// POSIX headers
#include <sys/time.h>      /* gettimeofday */

// ANSI C headers
#include <cmath>
#include <cstdlib>

// Qt headers
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

// MythTV headers
#include "mythplayer.h"
#include "mythcorecontext.h"    /* gContext */
#include "frame.h"          /* VideoFrame */
#include "mythdate.h"
#include "mythsystemlegacy.h"
#include "exitcodes.h"

// Commercial Flagging headers
#include "CommDetector2.h"
#include "pgm.h"
#include "PGMConverter.h"
#include "BorderDetector.h"
#include "EdgeDetector.h"
#include "TemplateFinder.h"

using namespace commDetector2;

namespace {

int writeJPG(QString prefix, const AVPicture *img, int imgheight)
{
    const int imgwidth = img->linesize[0];
    QFileInfo jpgfi(prefix + ".jpg");
    if (!jpgfi.exists())
    {
        QFile pgmfile(prefix + ".pgm");
        if (!pgmfile.exists())
        {
            QByteArray pfname = pgmfile.fileName().toLocal8Bit();
            if (pgm_write(img->data[0], imgwidth, imgheight,
                          pfname.constData()))
            {
                return -1;
            }
        }

        QString cmd = QString("convert -quality 50 -resize 192x144 %1 %2")
                      .arg(pgmfile.fileName()).arg(jpgfi.filePath());
        if (myth_system(cmd) != GENERIC_EXIT_OK)
            return -1;

        if (!pgmfile.remove())
        {
            LOG(VB_COMMFLAG, LOG_ERR, 
                QString("TemplateFinder.writeJPG error removing %1 (%2)")
                    .arg(pgmfile.fileName()).arg(strerror(errno)));
            return -1;
        }
    }
    return 0;
}

int
pgm_scorepixels(unsigned int *scores, int width, int row, int col,
        const AVPicture *src, int srcheight)
{
    /* Every time a pixel is an edge, give it a point. */
    const int   srcwidth = src->linesize[0];
    int         rr, cc;

    for (rr = 0; rr < srcheight; rr++)
    {
        for (cc = 0; cc < srcwidth; cc++)
        {
            if (src->data[0][rr * srcwidth + cc])
                scores[(row + rr) * width + col + cc]++;
        }
    }

    return 0;
}

int
sort_ascending(const void *aa, const void *bb)
{
    return *(unsigned int*)aa - *(unsigned int*)bb;
}

float
bounding_score(const AVPicture *img, int row, int col, int width, int height)
{
    /* Return a value between [0..1] */
    const int       imgwidth = img->linesize[0];
    unsigned int    score;
    int             rr, cc, rr2, cc2;

    score = 0;
    rr2 = row + height;
    cc2 = col + width;
    for (rr = row; rr < rr2; rr++)
    {
        for (cc = col; cc < cc2; cc++)
        {
            if (img->data[0][rr * imgwidth + cc])
                score++;
        }
    }
    return (float)score / (width * height);
}

bool
rowisempty(const AVPicture *img, int row, int col, int width)
{
    const int   imgwidth = img->linesize[0];
    for (int cc = col; cc < col + width; cc++)
        if (img->data[0][row * imgwidth + cc])
            return false;
    return true;
}

bool
colisempty(const AVPicture *img, int col, int row, int height)
{
    const int   imgwidth = img->linesize[0];
    for (int rr = row; rr < row + height; rr++)
        if (img->data[0][rr * imgwidth + col])
            return false;
    return true;
}

int
bounding_box(const AVPicture *img, int imgheight,
        int minrow, int mincol, int maxrow1, int maxcol1,
        int *prow, int *pcol, int *pwidth, int *pheight)
{
    const int           imgwidth = img->linesize[0];
    /*
     * TUNABLE:
     *
     * Maximum logo size, expressed as a percentage of the content area
     * (adjusting for letterboxing and pillarboxing).
     */
    static const int    MAXWIDTHPCT = 20;
    static const int    MAXHEIGHTPCT = 20;

    /*
     * TUNABLE:
     *
     * Safety margin to avoid cutting too much of the logo.
     * Higher values cut more, but avoid noise as part of the template..
     * Lower values cut less, but can include noise as part of the template.
     */
    const int           VERTSLOP = max(4, imgheight * 1 / 15);
    const int           HORIZSLOP = max(4, imgwidth * 1 / 20);

    int                 maxwidth, maxheight;
    int                 width, height, row, col;
    int                 newrow, newcol, newright, newbottom;

    maxwidth = (maxcol1 - mincol) * MAXWIDTHPCT / 100;
    maxheight = (maxrow1 - minrow) * MAXHEIGHTPCT / 100;

    row = minrow;
    col = mincol;
    width = maxcol1 - mincol;
    height = maxrow1 - minrow;

    for (;;)
    {
        float           score, newscore;
        int             ii;
        bool            improved = false;

        LOG(VB_COMMFLAG, LOG_INFO, QString("bounding_box %1x%2@(%3,%4)")
                .arg(width).arg(height).arg(col).arg(row));

        /* Chop top. */
        score = bounding_score(img, row, col, width, height);
        newrow = row;
        for (ii = 1; ii < height; ii++)
        {
            if ((newscore = bounding_score(img, row + ii, col,
                            width, height - ii)) < score)
                break;
            score = newscore;
            newrow = row + ii;
            improved = true;
        }

        /* Chop left. */
        score = bounding_score(img, row, col, width, height);
        newcol = col;
        for (ii = 1; ii < width; ii++)
        {
            if ((newscore = bounding_score(img, row, col + ii,
                            width - ii, height)) < score)
                break;
            score = newscore;
            newcol = col + ii;
            improved = true;
        }

        /* Chop bottom. */
        score = bounding_score(img, row, col, width, height);
        newbottom = row + height;
        for (ii = 1; ii < height; ii++)
        {
            if ((newscore = bounding_score(img, row, col,
                            width, height - ii)) < score)
                break;
            score = newscore;
            newbottom = row + height - ii;
            improved = true;
        }

        /* Chop right. */
        score = bounding_score(img, row, col, width, height);
        newright = col + width;
        for (ii = 1; ii < width; ii++)
        {
            if ((newscore = bounding_score(img, row, col,
                            width - ii, height)) < score)
                break;
            score = newscore;
            newright = col + width - ii;
            improved = true;
        }

        if (!improved)
            break;

        row = newrow;
        col = newcol;
        width = newright - newcol;
        height = newbottom - newrow;

        /*
         * Noise edge pixels in the frequency template can sometimes stretch
         * the template area to be larger than it should be.
         *
         * However, noise needs to be distinguished from a uniform distribution
         * of noise pixels (e.g., no real statically-located template). So if
         * the template area is too "large", then some quadrant must have a
         * clear majority of the edge pixels; otherwise we declare failure (no
         * template found).
         *
         * Intuitively, we should simply repeat until a single bounding box is
         * converged upon. However, this requires a more sophisticated
         * bounding_score function that I don't feel like figuring out.
         * Indefinitely repeating with the present bounding_score function will
         * tend to chop off too much. Instead, simply do some sanity checks on
         * the candidate template's size, and prune the template area and
         * repeat if it is too "large".
         */

        if (width > maxwidth)
        {
            /* Too wide; test left and right portions. */
            int             chop = width / 3;
            int             chopwidth = width - chop;
            float           left, right, minscore, maxscore;

            left = bounding_score(img, row, col, chopwidth, height);
            right = bounding_score(img, row, col + chop, chopwidth, height);
            LOG(VB_COMMFLAG, LOG_INFO, 
                QString("bounding_box too wide (%1 > %2); left=%3, right=%4")
                    .arg(width).arg(maxwidth)
                    .arg(left, 0, 'f', 3).arg(right, 0, 'f', 3));
            minscore = min(left, right);
            maxscore = max(left, right);
            if (maxscore < 3 * minscore / 2)
            {
                /*
                 * Edge pixel distribution too uniform; give up.
                 *
                 * XXX: also fails for horizontally-centered templates ...
                 */
                LOG(VB_COMMFLAG, LOG_ERR, "bounding_box giving up (edge "
                                          "pixels distributed too uniformly)");
                return -1;
            }

            if (left < right)
                col += chop;
            width -= chop;
            continue;
        }

        if (height > maxheight)
        {
            /* Too tall; test upper and lower portions. */
            int             chop = height / 3;
            int             chopheight = height - chop;
            float           upper, lower, minscore, maxscore;

            upper = bounding_score(img, row, col, width, chopheight);
            lower = bounding_score(img, row + chop, col, width, chopheight);
            LOG(VB_COMMFLAG, LOG_INFO,
                QString("bounding_box too tall (%1 > %2); upper=%3, lower=%4")
                    .arg(height).arg(maxheight)
                    .arg(upper, 0, 'f', 3).arg(lower, 0, 'f', 3));
            minscore = min(upper, lower);
            maxscore = max(upper, lower);
            if (maxscore < 3 * minscore / 2)
            {
                /*
                 * Edge pixel distribution too uniform; give up.
                 *
                 * XXX: also fails for vertically-centered templates ...
                 */
                LOG(VB_COMMFLAG, LOG_ERR, "bounding_box giving up (edge "
                                          "pixel distribution too uniform)");
                return -1;
            }

            if (upper < lower)
                row += chop;
            height -= chop;
            continue;
        }

        break;
    }

    /*
     * The above "chop" algorithm often cuts off the outside edges of the
     * logos because the outside edges don't contribute enough to the score. So
     * compensate by now expanding the bounding box (up to a *SLOP pixels in
     * each direction) to include all edge pixels.
     */

    LOG(VB_COMMFLAG, LOG_INFO,
        QString("bounding_box %1x%2@(%3,%4); horizslop=%5,vertslop=%6")
            .arg(width).arg(height).arg(col).arg(row)
            .arg(HORIZSLOP).arg(VERTSLOP));

    /* Expand upwards. */
    newrow = row - 1;
    for (;;)
    {
        if (newrow <= minrow)
        {
            newrow = minrow;
            break;
        }
        if (row - newrow >= VERTSLOP)
        {
            newrow = row - VERTSLOP;
            break;
        }
        if (rowisempty(img, newrow, col, width))
        {
            newrow++;
            break;
        }
        newrow--;
    }
    newrow = max(minrow, newrow - 1);   /* Empty row on top. */

    /* Expand leftwards. */
    newcol = col - 1;
    for (;;)
    {
        if (newcol <= mincol)
        {
            newcol = mincol;
            break;
        }
        if (col - newcol >= HORIZSLOP)
        {
            newcol = col - HORIZSLOP;
            break;
        }
        if (colisempty(img, newcol, row, height))
        {
            newcol++;
            break;
        }
        newcol--;
    }
    newcol = max(mincol, newcol - 1);   /* Empty column to left. */

    /* Expand rightwards. */
    newright = col + width;
    for (;;)
    {
        if (newright >= maxcol1)
        {
            newright = maxcol1;
            break;
        }
        if (newright - (col + width) >= HORIZSLOP)
        {
            newright = col + width + HORIZSLOP;
            break;
        }
        if (colisempty(img, newright, row, height))
            break;
        newright++;
    }
    newright = min(maxcol1, newright + 1);  /* Empty column to right. */

    /* Expand downwards. */
    newbottom = row + height;
    for (;;)
    {
        if (newbottom >= maxrow1)
        {
            newbottom = maxrow1;
            break;
        }
        if (newbottom - (row + height) >= VERTSLOP)
        {
            newbottom = row + height + VERTSLOP;
            break;
        }
        if (rowisempty(img, newbottom, col, width))
            break;
        newbottom++;
    }
    newbottom = min(maxrow1, newbottom + 1);    /* Empty row on bottom. */

    row = newrow;
    col = newcol;
    width = newright - newcol;
    height = newbottom - newrow;

    LOG(VB_COMMFLAG, LOG_INFO, QString("bounding_box %1x%2@(%3,%4)")
            .arg(width).arg(height).arg(col).arg(row));

    *prow = row;
    *pcol = col;
    *pwidth = width;
    *pheight = height;
    return 0;
}

int
template_alloc(const unsigned int *scores, int width, int height,
        int minrow, int mincol, int maxrow1, int maxcol1, AVPicture *tmpl,
        int *ptmplrow, int *ptmplcol, int *ptmplwidth, int *ptmplheight,
        bool debug_edgecounts, QString debugdir)
{
    /*
     * TUNABLE:
     *
     * Higher values select for "stronger" pixels to be in the template, but
     * weak pixels might be missed.
     *
     * Lower values allow more pixels to be included as part of the template,
     * but strong non-template pixels might be included.
     */
    static const float      MINSCOREPCTILE = 0.998;

    const int               nn = width * height;
    int                     ii, first, last;
    unsigned int            *sortedscores, threshscore;
    AVPicture               thresh;

    if (avpicture_alloc(&thresh, PIX_FMT_GRAY8, width, height))
    {
        LOG(VB_COMMFLAG, LOG_ERR,
            QString("template_alloc avpicture_alloc thresh (%1x%2) failed")
                .arg(width).arg(height));
        return -1;
    }

    sortedscores = new unsigned int[nn];
    memcpy(sortedscores, scores, nn * sizeof(*sortedscores));
    qsort(sortedscores, nn, sizeof(*sortedscores), sort_ascending);

    if (sortedscores[0] == sortedscores[nn - 1])
    {
        /* All pixels in the template area look the same; no template. */
        LOG(VB_COMMFLAG, LOG_ERR,
            QString("template_alloc: %1x%2 pixels all identical!")
                .arg(width).arg(height));
        goto free_thresh;
    }

    /* Threshold the edge frequences. */

    ii = (int)roundf(nn * MINSCOREPCTILE);
    threshscore = sortedscores[ii];
    for (first = ii; first > 0 && sortedscores[first] == threshscore; first--)
        ;
    if (sortedscores[first] != threshscore)
        first++;
    for (last = ii; last < nn - 1 && sortedscores[last] == threshscore; last++)
        ;
    if (sortedscores[last] != threshscore)
        last--;

    LOG(VB_COMMFLAG, LOG_INFO, QString("template_alloc wanted %1, got %2-%3")
            .arg(MINSCOREPCTILE, 0, 'f', 6)
            .arg((float)first / nn, 0, 'f', 6)
            .arg((float)last / nn, 0, 'f', 6));

    for (ii = 0; ii < nn; ii++)
        thresh.data[0][ii] = scores[ii] >= threshscore ? UCHAR_MAX : 0;

    if (debug_edgecounts)
    {
        /* Scores, rescaled to [0..UCHAR_MAX]. */
        AVPicture scored;
        if (avpicture_alloc(&scored, PIX_FMT_GRAY8, width, height))
        {
            LOG(VB_COMMFLAG, LOG_ERR,
                QString("template_alloc avpicture_alloc scored (%1x%2) failed")
                    .arg(width).arg(height));
            goto free_thresh;
        }
        unsigned int maxscore = sortedscores[nn - 1];
        for (ii = 0; ii < nn; ii++)
            scored.data[0][ii] = scores[ii] * UCHAR_MAX / maxscore;
        int error = writeJPG(debugdir + "/TemplateFinder-scores", &scored,
                height);
        avpicture_free(&scored);
        if (error)
            goto free_thresh;

        /* Thresholded scores. */
        if (writeJPG(debugdir + "/TemplateFinder-edgecounts", &thresh, height))
            goto free_thresh;
    }

    /* Crop to a minimal bounding box. */

    if (bounding_box(&thresh, height, minrow, mincol, maxrow1, maxcol1,
                ptmplrow, ptmplcol, ptmplwidth, ptmplheight))
        goto free_thresh;

    if ((uint)(*ptmplwidth * *ptmplheight) > USHRT_MAX)
    {
        /* Max value of data type of TemplateMatcher::edgematch */
        LOG(VB_COMMFLAG, LOG_ERR,
            QString("template_alloc bounding_box too big (%1x%2)")
                .arg(*ptmplwidth).arg(*ptmplheight));
        goto free_thresh;
    }

    if (avpicture_alloc(tmpl, PIX_FMT_GRAY8, *ptmplwidth, *ptmplheight))
    {
        LOG(VB_COMMFLAG, LOG_ERR,
            QString("template_alloc avpicture_alloc tmpl (%1x%2) failed")
                .arg(*ptmplwidth).arg(*ptmplheight));
        goto free_thresh;
    }

    if (pgm_crop(tmpl, &thresh, height, *ptmplrow, *ptmplcol,
                *ptmplwidth, *ptmplheight))
        goto free_thresh;

    delete []sortedscores;
    avpicture_free(&thresh);
    return 0;

free_thresh:
    delete []sortedscores;
    avpicture_free(&thresh);
    return -1;
}

int
analyzeFrameDebug(long long frameno, const AVPicture *pgm, int pgmheight,
        const AVPicture *cropped, const AVPicture *edges, int cropheight,
        int croprow, int cropcol, bool debug_frames, QString debugdir)
{
    static const int    delta = 24;
    static int          lastrow, lastcol, lastwidth, lastheight;
    const int           cropwidth = cropped->linesize[0];
    int                 rowsame, colsame, widthsame, heightsame;

    rowsame = abs(lastrow - croprow) <= delta ? 1 : 0;
    colsame = abs(lastcol - cropcol) <= delta ? 1 : 0;
    widthsame = abs(lastwidth - cropwidth) <= delta ? 1 : 0;
    heightsame = abs(lastheight - cropheight) <= delta ? 1 : 0;

    if (frameno > 0 && rowsame + colsame + widthsame + heightsame >= 3)
        return 0;

    LOG(VB_COMMFLAG, LOG_INFO,
        QString("TemplateFinder Frame %1: %2x%3@(%4,%5)")
            .arg(frameno, 5)
            .arg(cropwidth).arg(cropheight)
            .arg(cropcol).arg(croprow));

    lastrow = croprow;
    lastcol = cropcol;
    lastwidth = cropwidth;
    lastheight = cropheight;

    if (debug_frames)
    {
        QString base = QString("%1/TemplateFinder-%2")
            .arg(debugdir).arg(frameno, 5, 10, QChar('0'));

        /* PGM greyscale image of frame. */
        if (writeJPG(base, pgm, pgmheight))
            goto error;

        /* Cropped template area of frame. */
        if (writeJPG(base + "-cropped", cropped, cropheight))
            goto error;

        /* Edges of cropped template area of frame. */
        if (writeJPG(base + "-edges", edges, cropheight))
            goto error;
    }

    return 0;

error:
    return -1;
}

bool
readTemplate(QString datafile, int *prow, int *pcol, int *pwidth, int *pheight,
        QString tmplfile, AVPicture *tmpl, bool *pvalid)
{
    QFile dfile(datafile);
    QFileInfo dfileinfo(dfile);

    if (!dfile.open(QIODevice::ReadOnly))
        return false;

    if (!dfileinfo.size())
    {
        /* Dummy file: no template. */
        *pvalid = false;
        return true;
    }

    QTextStream stream(&dfile);
    stream >> *prow >> *pcol >> *pwidth >> *pheight;
    dfile.close();

    if (avpicture_alloc(tmpl, PIX_FMT_GRAY8, *pwidth, *pheight))
    {
        LOG(VB_COMMFLAG, LOG_ERR,
            QString("readTemplate avpicture_alloc %1 (%2x%3) failed")
                .arg(tmplfile).arg(*pwidth).arg(*pheight));
        return false;
    }

    QByteArray tmfile = tmplfile.toLatin1();
    if (pgm_read(tmpl->data[0], *pwidth, *pheight, tmfile.constData()))
    {
        avpicture_free(tmpl);
        return false;
    }

    *pvalid = true;
    return true;
}

void
writeDummyTemplate(QString datafile)
{
    /* Leave a 0-byte file. */
    QFile dfile(datafile);

    if (!dfile.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
        dfile.exists())
        (void)dfile.remove();
}

bool
writeTemplate(QString tmplfile, const AVPicture *tmpl, QString datafile,
        int row, int col, int width, int height)
{
    QFile tfile(tmplfile);

    QByteArray tmfile = tmplfile.toLatin1();
    if (pgm_write(tmpl->data[0], width, height, tmfile.constData()))
        return false;

    QFile dfile(datafile);
    if (!dfile.open(QIODevice::WriteOnly))
        return false;

    QTextStream stream(&dfile);
    stream << row << " " << col << "\n" << width << " " << height << "\n";
    dfile.close();
    return true;
}

};  /* namespace */

TemplateFinder::TemplateFinder(PGMConverter *pgmc, BorderDetector *bd,
        EdgeDetector *ed, MythPlayer *player, int proglen,
        QString debugdir)
    : FrameAnalyzer()
    , pgmConverter(pgmc)
    , borderDetector(bd)
    , edgeDetector(ed)
    , nextFrame(0)
    , width(-1)
    , height(-1)
    , scores(NULL)
    , mincontentrow(INT_MAX)
    , mincontentcol(INT_MAX)
    , maxcontentrow1(INT_MIN)
    , maxcontentcol1(INT_MIN)
    , tmplrow(-1)
    , tmplcol(-1)
    , tmplwidth(-1)
    , tmplheight(-1)
    , cwidth(-1)
    , cheight(-1)
    , debugLevel(0)
    , debugdir(debugdir)
    , debugdata(debugdir + "/TemplateFinder.txt")
    , debugtmpl(debugdir + "/TemplateFinder.pgm")
    , debug_template(false)
    , debug_edgecounts(false)
    , debug_frames(false)
    , tmpl_valid(false)
    , tmpl_done(false)
{
    /*
     * TUNABLE:
     *
     * The number of frames desired for sampling to build the template.
     *
     * Higher values should yield a more accurate template, but requires more
     * time.
     */
    unsigned int        samplesNeeded = 300;

    /*
     * TUNABLE:
     *
     * The leading amount of time (in seconds) to sample frames for building up
     * the possible template, and the interval between frames for analysis.
     * This affects how soon flagging can start after a recording has begun
     * (a.k.a. "real-time flagging").
     *
     * Sample half of the program length or 20 minutes, whichever is less.
     */
    sampleTime = min(proglen / 2, 20 * 60);

    const float fps = player->GetFrameRate();

    frameInterval = (int)roundf(sampleTime * fps / samplesNeeded);
    endFrame = 0 + frameInterval * samplesNeeded - 1;

    LOG(VB_COMMFLAG, LOG_INFO,
        QString("TemplateFinder: sampleTime=%1s, samplesNeeded=%2, endFrame=%3")
            .arg(sampleTime).arg(samplesNeeded).arg(endFrame));

    memset(&cropped, 0, sizeof(cropped));
    memset(&tmpl, 0, sizeof(tmpl));
    memset(&analyze_time, 0, sizeof(analyze_time));

    /*
     * debugLevel:
     *      0: no extra debugging
     *      1: cache computations into debugdir [O(1) files]
     *      2: extra verbosity [O(nframes)]
     *      3: dump frames into debugdir [O(nframes) files]
     */
    debugLevel = gCoreContext->GetNumSetting("TemplateFinderDebugLevel", 0);

    if (debugLevel >= 1)
    {
        createDebugDirectory(debugdir,
            QString("TemplateFinder debugLevel %1").arg(debugLevel));

        debug_template = true;
        debug_edgecounts = true;

        if (debugLevel >= 3)
            debug_frames = true;
    }
}

TemplateFinder::~TemplateFinder(void)
{
    if (scores)
        delete []scores;
    avpicture_free(&tmpl);
    avpicture_free(&cropped);
}

enum FrameAnalyzer::analyzeFrameResult
TemplateFinder::MythPlayerInited(MythPlayer *player, long long nframes)
{
    /*
     * Only detect edges in portions of the frame where we expect to find
     * a template. This serves two purposes:
     *
     *  - Speed: reduce search space.
     *  - Correctness (insofar as the assumption of template location is
     *    correct): don't "pollute" the set of candidate template edges with
     *    the "content" edges in the non-template portions of the frame.
     */
    QString tmpldims, playerdims;

    (void)nframes; /* gcc */
    QSize buf_dim = player->GetVideoBufferSize();
    width  = buf_dim.width();
    height = buf_dim.height();
    playerdims = QString("%1x%2").arg(width).arg(height);

    if (debug_template)
    {
        if ((tmpl_done = readTemplate(debugdata, &tmplrow, &tmplcol,
                        &tmplwidth, &tmplheight, debugtmpl, &tmpl,
                        &tmpl_valid)))
        {
            tmpldims = tmpl_valid ? QString("%1x%2@(%3,%4)")
                .arg(tmplwidth).arg(tmplheight).arg(tmplcol).arg(tmplrow) :
                    "no template";

            LOG(VB_COMMFLAG, LOG_INFO,
                QString("TemplateFinder::MythPlayerInited read %1: %2")
                    .arg(debugtmpl)
                    .arg(tmpldims));
        }
    }

    if (pgmConverter->MythPlayerInited(player))
        goto free_tmpl;

    if (borderDetector->MythPlayerInited(player))
        goto free_tmpl;

    if (tmpl_done)
    {
        if (tmpl_valid)
        {
            LOG(VB_COMMFLAG, LOG_INFO,
                QString("TemplateFinder::MythPlayerInited %1 of %2 (%3)")
                    .arg(tmpldims).arg(playerdims).arg(debugtmpl));
        }
        return ANALYZE_FINISHED;
    }

    LOG(VB_COMMFLAG, LOG_INFO,
        QString("TemplateFinder::MythPlayerInited framesize %1")
            .arg(playerdims));
    scores = new unsigned int[width * height];

    return ANALYZE_OK;

free_tmpl:
    avpicture_free(&tmpl);
    return ANALYZE_FATAL;
}

int
TemplateFinder::resetBuffers(int newwidth, int newheight)
{
    if (cwidth == newwidth && cheight == newheight)
        return 0;

    avpicture_free(&cropped);

    if (avpicture_alloc(&cropped, PIX_FMT_GRAY8, newwidth, newheight))
    {
        LOG(VB_COMMFLAG, LOG_ERR,
            QString("TemplateFinder::resetBuffers "
                    "avpicture_alloc cropped (%1x%2) failed")
                .arg(newwidth).arg(newheight));
        return -1;
    }

    cwidth = newwidth;
    cheight = newheight;
    return 0;
}

enum FrameAnalyzer::analyzeFrameResult
TemplateFinder::analyzeFrame(const VideoFrame *frame, long long frameno,
        long long *pNextFrame)
{
    /*
     * TUNABLE:
     *
     * When looking for edges in frames, select some percentile of
     * squared-gradient magnitudes (intensities) as candidate edges. (This
     * number conventionally should not go any lower than the 95th percentile;
     * see edge_mark.)
     *
     * Higher values result in fewer edges; faint logos might not be picked up.
     * Lower values result in more edges; non-logo edges might be picked up.
     *
     * The TemplateFinder accumulates all its state in the "scores" array to
     * be processed later by TemplateFinder::finished.
     */
    const int           FRAMESGMPCTILE = 90;

    /*
     * TUNABLE:
     *
     * Exclude some portion of the center of the frame from edge analysis.
     * Elminate false edge-detection logo positives from talking-host types of
     * shows where the high-contrast host and clothes (e.g., tie against white
     * shirt against dark jacket) dominates the edges.
     *
     * This has a nice side-effect of reducing the area to be examined (speed
     * optimization).
     */
    static const float  EXCLUDEWIDTH = 0.5;
    static const float  EXCLUDEHEIGHT = 0.5;

    const AVPicture     *pgm, *edges;
    int                 pgmwidth, pgmheight;
    int                 croprow, cropcol, cropwidth, cropheight;
    int                 excluderow, excludecol, excludewidth, excludeheight;
    struct timeval      start, end, elapsed;

    if (frameno < nextFrame)
    {
        *pNextFrame = nextFrame;
        return ANALYZE_OK;
    }

    nextFrame = frameno + frameInterval;
    *pNextFrame = min(endFrame, nextFrame);

    if (!(pgm = pgmConverter->getImage(frame, frameno, &pgmwidth, &pgmheight)))
        goto error;

    if (!borderDetector->getDimensions(pgm, pgmheight, frameno,
                &croprow, &cropcol, &cropwidth, &cropheight))
    {
        /* Not a blank frame. */

        (void)gettimeofday(&start, NULL);

        if (croprow < mincontentrow)
            mincontentrow = croprow;
        if (cropcol < mincontentcol)
            mincontentcol = cropcol;
        if (cropcol + cropwidth > maxcontentcol1)
            maxcontentcol1 = cropcol + cropwidth;
        if (croprow + cropheight > maxcontentrow1)
            maxcontentrow1 = croprow + cropheight;

        if (resetBuffers(cropwidth, cropheight))
            goto error;

        if (pgm_crop(&cropped, pgm, pgmheight, croprow, cropcol,
                    cropwidth, cropheight))
            goto error;

        /*
         * Translate the excluded area of the screen into "cropped"
         * coordinates.
         */
        excludewidth = (int)(pgmwidth * EXCLUDEWIDTH);
        excludeheight = (int)(pgmheight * EXCLUDEHEIGHT);
        excluderow = (pgmheight - excludeheight) / 2 - croprow;
        excludecol = (pgmwidth - excludewidth) / 2 - cropcol;
        (void)edgeDetector->setExcludeArea(excluderow, excludecol,
                excludewidth, excludeheight);

        if (!(edges = edgeDetector->detectEdges(&cropped, cropheight,
                        FRAMESGMPCTILE)))
            goto error;

        if (pgm_scorepixels(scores, pgmwidth, croprow, cropcol,
                    edges, cropheight))
            goto error;

        if (debugLevel >= 2)
        {
            if (analyzeFrameDebug(frameno, pgm, pgmheight, &cropped, edges,
                        cropheight, croprow, cropcol, debug_frames, debugdir))
                goto error;
        }

        (void)gettimeofday(&end, NULL);
        timersub(&end, &start, &elapsed);
        timeradd(&analyze_time, &elapsed, &analyze_time);
    }

    if (nextFrame > endFrame)
        return ANALYZE_FINISHED;

    return ANALYZE_OK;

error:
    LOG(VB_COMMFLAG, LOG_ERR,
        QString("TemplateFinder::analyzeFrame error at frame %1")
            .arg(frameno));

    if (nextFrame > endFrame)
        return ANALYZE_FINISHED;

    return ANALYZE_ERROR;
}

int
TemplateFinder::finished(long long nframes, bool final)
{
    (void)nframes;  /* gcc */
    if (!tmpl_done)
    {
        if (template_alloc(scores, width, height,
                    mincontentrow, mincontentcol,
                    maxcontentrow1, maxcontentcol1,
                    &tmpl, &tmplrow, &tmplcol, &tmplwidth, &tmplheight,
                    debug_edgecounts, debugdir))
        {
            if (final)
                writeDummyTemplate(debugdata);
        }
        else
        {
            if (final && debug_template)
            {
                if (!(tmpl_valid = writeTemplate(debugtmpl, &tmpl, debugdata,
                                tmplrow, tmplcol, tmplwidth, tmplheight)))
                    goto free_tmpl;

                LOG(VB_COMMFLAG, LOG_INFO,
                    QString("TemplateFinder::finished wrote %1"
                            " and %2 [%3x%4@(%5,%6)]")
                        .arg(debugtmpl).arg(debugdata)
                        .arg(tmplwidth).arg(tmplheight)
                        .arg(tmplcol).arg(tmplrow));
            }
        }

        if (final)
            tmpl_done = true;
    }

    borderDetector->setLogoState(this);

    return 0;

free_tmpl:
    avpicture_free(&tmpl);
    return -1;
}

int
TemplateFinder::reportTime(void) const
{
    if (pgmConverter->reportTime())
        return -1;

    if (borderDetector->reportTime())
        return -1;

    LOG(VB_COMMFLAG, LOG_INFO, QString("TF Time: analyze=%1s")
            .arg(strftimeval(&analyze_time)));
    return 0;
}

const struct AVPicture *
TemplateFinder::getTemplate(int *prow, int *pcol, int *pwidth, int *pheight)
    const
{
    if (tmpl_valid)
    {
        *prow = tmplrow;
        *pcol = tmplcol;
        *pwidth = tmplwidth;
        *pheight = tmplheight;
        return &tmpl;
    }
    return NULL;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
