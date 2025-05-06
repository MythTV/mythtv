// Qt headers
#include <QSize>

// MythTV headers
#include "libmythbase/mythlogging.h"
#include "libmythtv/mythavutil.h"
#include "libmythtv/mythframe.h"          /* VideoFrame */
#include "libmythtv/mythplayer.h"

// Commercial Flagging headers
#include "CommDetector2.h"
#include "PGMConverter.h"
#include "pgm.h"

extern "C" {
#include "libavutil/imgutils.h"
}

using namespace commDetector2;

PGMConverter::~PGMConverter(void)
{
    m_width = -1;
#ifdef PGM_CONVERT_GREYSCALE
    av_freep(reinterpret_cast<void*>(&m_pgm.data[0]));
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
        m_width, m_height, AV_PIX_FMT_GRAY8, IMAGE_ALIGN) < 0)
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
PGMConverter::getImage(const MythVideoFrame *frame, long long _frameno,
        int *pwidth, int *pheight)
{
    if (m_frameNo != _frameno)
    {
        if (!frame->m_buffer)
        {
            LOG(VB_COMMFLAG, LOG_ERR, "PGMConverter::getImage no buf");
            return nullptr;
        }

#ifdef PGM_CONVERT_GREYSCALE
        auto start = nowAsDuration<std::chrono::microseconds>();
        if (m_copy->Copy(&m_pgm, frame, m_pgm.data[0], AV_PIX_FMT_GRAY8) < 0)
            return nullptr;
        auto end = nowAsDuration<std::chrono::microseconds>();
        m_convertTime += (end - start);
#else  /* !PGM_CONVERT_GREYSCALE */
        if (av_image_fill_arrays(m_pgm.data, m_pgm.linesize,
            frame->m_buffer, AV_PIX_FMT_GRAY8, m_width, m_height,IMAGE_ALIGN) < 0)
        {
            LOG(VB_COMMFLAG, LOG_ERR,
                QString("PGMConverter::getImage error at frame %1 (%2x%3)")
                    .arg(_frameno).arg(m_width).arg(m_height));
            return nullptr;
        }
#endif /* !PGM_CONVERT_GREYSCALE */

        m_frameNo = _frameno;
    }

    *pwidth = m_width;
    *pheight = m_height;
    return &m_pgm;
}

int
PGMConverter::reportTime(void)
{
#ifdef PGM_CONVERT_GREYSCALE
    if (!m_timeReported)
    {
        LOG(VB_COMMFLAG, LOG_INFO, QString("PGM Time: convert=%1s")
                .arg(strftimeval(m_convertTime)));
        m_timeReported = true;
    }
#endif /* PGM_CONVERT_GREYSCALE */
    return 0;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
