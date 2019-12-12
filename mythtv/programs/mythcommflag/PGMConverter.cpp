// POSIX headers
#include <sys/time.h> // for gettimeofday

// Qt headers
#include <QSize>

// MythTV headers
#include "mythlogging.h"
#include "mythplayer.h"
#include "mythframe.h"          /* VideoFrame */
#include "mythavutil.h"

// Commercial Flagging headers
#include "CommDetector2.h"
#include "pgm.h"
#include "PGMConverter.h"

extern "C" {
#include "libavutil/imgutils.h"
}

using namespace commDetector2;

PGMConverter::~PGMConverter(void)
{
    m_width = -1;
#ifdef PGM_CONVERT_GREYSCALE
    av_freep(&m_pgm.data[0]);
    memset(&m_pgm, 0, sizeof(m_pgm));
    delete m_copy;
#endif /* PGM_CONVERT_GREYSCALE */
}

int
PGMConverter::MythPlayerInited(const MythPlayer *player)
{
#ifdef PGM_CONVERT_GREYSCALE
    m_timeReported = false;
    memset(&m_convertTime, 0, sizeof(m_convertTime));
#endif /* PGM_CONVERT_GREYSCALE */

    if (m_width != -1)
        return 0;

    QSize buf_dim = player->GetVideoBufferSize();
    m_width  = buf_dim.width();
    m_height = buf_dim.height();

#ifdef PGM_CONVERT_GREYSCALE
    if (av_image_alloc(m_pgm.data, m_pgm.linesize,
        m_width, m_height, AV_PIX_FMT_GRAY8, IMAGE_ALIGN))
    {
        LOG(VB_COMMFLAG, LOG_ERR, QString("PGMConverter::MythPlayerInited "
                                          "av_image_alloc m_pgm (%1x%2) failed")
                .arg(m_width).arg(m_height));
        return -1;
    }

    delete m_copy;
    m_copy = new MythAVCopy;
    LOG(VB_COMMFLAG, LOG_INFO, QString("PGMConverter::MythPlayerInited "
                                       "using true greyscale conversion"));
#else  /* !PGM_CONVERT_GREYSCALE */
    LOG(VB_COMMFLAG, LOG_INFO, QString("PGMConverter::MythPlayerInited "
                                       "(YUV shortcut)"));
#endif /* !PGM_CONVERT_GREYSCALE */

    return 0;
}

const AVFrame *
PGMConverter::getImage(const VideoFrame *frame, long long _frameno,
        int *pwidth, int *pheight)
{
#ifdef PGM_CONVERT_GREYSCALE
    struct timeval      start {};
    struct timeval      end {};
    struct timeval      elapsed {};
#endif /* PGM_CONVERT_GREYSCALE */

    if (m_frameNo == _frameno)
        goto out;

    if (!frame->buf)
    {
        LOG(VB_COMMFLAG, LOG_ERR, "PGMConverter::getImage no buf");
        goto error;
    }

#ifdef PGM_CONVERT_GREYSCALE
    (void)gettimeofday(&start, nullptr);
    if (m_copy->Copy(&m_pgm, frame, m_pgm.data[0], AV_PIX_FMT_GRAY8) < 0)
        goto error;
    (void)gettimeofday(&end, nullptr);
    timersub(&end, &start, &elapsed);
    timeradd(&m_convertTime, &elapsed, &m_convertTime);
#else  /* !PGM_CONVERT_GREYSCALE */
    if (av_image_fill_arrays(m_pgm.data, m_pgm.linesize,
        frame->buf, AV_PIX_FMT_GRAY8, m_width, m_height,IMAGE_ALIGN) < 0)
    {
        LOG(VB_COMMFLAG, LOG_ERR,
            QString("PGMConverter::getImage error at frame %1 (%2x%3)")
                .arg(_frameno).arg(m_width).arg(m_height));
        goto error;
    }
#endif /* !PGM_CONVERT_GREYSCALE */

    m_frameNo = _frameno;

out:
    *pwidth = m_width;
    *pheight = m_height;
    return &m_pgm;

error:
    return nullptr;
}

int
PGMConverter::reportTime(void)
{
#ifdef PGM_CONVERT_GREYSCALE
    if (!m_timeReported)
    {
        LOG(VB_COMMFLAG, LOG_INFO, QString("PGM Time: convert=%1s")
                .arg(strftimeval(&m_convertTime)));
        m_timeReported = true;
    }
#endif /* PGM_CONVERT_GREYSCALE */
    return 0;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
