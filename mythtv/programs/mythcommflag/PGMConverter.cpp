// POSIX headers
#include <sys/time.h> // for gettimeofday

// Qt headers
#include <QSize>

// MythTV headers
#include "mythlogging.h"
#include "mythplayer.h"
#include "mythframe.h"          /* VideoFrame */

// Commercial Flagging headers
#include "CommDetector2.h"
#include "pgm.h"
#include "PGMConverter.h"

using namespace commDetector2;

PGMConverter::PGMConverter(void)
    : frameno(-1)
    , width(-1)
    , height(-1)
#ifdef PGM_CONVERT_GREYSCALE
    , time_reported(false)
#endif /* PGM_CONVERT_GREYSCALE */
{
    memset(&pgm, 0, sizeof(pgm));
    memset(&convert_time, 0, sizeof(convert_time));
}

PGMConverter::~PGMConverter(void)
{
    width = -1;
#ifdef PGM_CONVERT_GREYSCALE
    avpicture_free(&pgm);
    memset(&pgm, 0, sizeof(pgm));
#endif /* PGM_CONVERT_GREYSCALE */
}

int
PGMConverter::MythPlayerInited(const MythPlayer *player)
{
#ifdef PGM_CONVERT_GREYSCALE
    time_reported = false;
    memset(&convert_time, 0, sizeof(convert_time));
#endif /* PGM_CONVERT_GREYSCALE */

    if (width != -1)
        return 0;

    QSize buf_dim = player->GetVideoBufferSize();
    width  = buf_dim.width();
    height = buf_dim.height();

#ifdef PGM_CONVERT_GREYSCALE
    if (avpicture_alloc(&pgm, PIX_FMT_GRAY8, width, height))
    {
        LOG(VB_COMMFLAG, LOG_ERR, QString("PGMConverter::MythPlayerInited "
                                          "avpicture_alloc pgm (%1x%2) failed")
                .arg(width).arg(height));
        return -1;
    }
    LOG(VB_COMMFLAG, LOG_INFO, QString("PGMConverter::MythPlayerInited "
                                       "using true greyscale conversion"));
#else  /* !PGM_CONVERT_GREYSCALE */
    LOG(VB_COMMFLAG, LOG_INFO, QString("PGMConverter::MythPlayerInited "
                                       "(YUV shortcut)"));
#endif /* !PGM_CONVERT_GREYSCALE */

    return 0;
}

const AVPicture *
PGMConverter::getImage(const VideoFrame *frame, long long _frameno,
        int *pwidth, int *pheight)
{
#ifdef PGM_CONVERT_GREYSCALE
    struct timeval      start, end, elapsed;
#endif /* PGM_CONVERT_GREYSCALE */

    if (frameno == _frameno)
        goto out;

    if (!frame->buf)
    {
        LOG(VB_COMMFLAG, LOG_ERR, "PGMConverter::getImage no buf");
        goto error;
    }

#ifdef PGM_CONVERT_GREYSCALE
    (void)gettimeofday(&start, NULL);
    if (pgm_fill(&pgm, frame))
        goto error;
    (void)gettimeofday(&end, NULL);
    timersub(&end, &start, &elapsed);
    timeradd(&convert_time, &elapsed, &convert_time);
#else  /* !PGM_CONVERT_GREYSCALE */
    if (avpicture_fill(&pgm, frame->buf, PIX_FMT_GRAY8, width, height) == -1)
    {
        LOG(VB_COMMFLAG, LOG_ERR,
            QString("PGMConverter::getImage error at frame %1 (%2x%3)")
                .arg(_frameno).arg(width).arg(height));
        goto error;
    }
#endif /* !PGM_CONVERT_GREYSCALE */

    frameno = _frameno;

out:
    *pwidth = width;
    *pheight = height;
    return &pgm;

error:
    return NULL;
}

int
PGMConverter::reportTime(void)
{
#ifdef PGM_CONVERT_GREYSCALE
    if (!time_reported)
    {
        LOG(VB_COMMFLAG, LOG_INFO, QString("PGM Time: convert=%1s")
                .arg(strftimeval(&convert_time)));
        time_reported = true;
    }
#endif /* PGM_CONVERT_GREYSCALE */
    return 0;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
