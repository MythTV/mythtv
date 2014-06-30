/*
 * PGMConverter
 *
 * Object to convert a MythPlayer frame into a greyscale image.
 */

#ifndef __PGMCONVERTER_H__
#define __PGMCONVERTER_H__

extern "C" {
#include "libavcodec/avcodec.h"    /* AVPicture */
}

typedef struct VideoFrame_ VideoFrame;
class MythPlayer;
class MythAVCopy;

/*
 * PGM_CONVERT_GREYSCALE:
 *
 * If #define'd, perform a true greyscale conversion of "frame" in
 * PGMConverter::getImage .
 *
 * If #undef'd, just fake up a greyscale data structure. The "frame" data is
 * YUV data, and the Y channel is close enough for our purposes, and it's
 * faster than a full-blown true greyscale conversion.
 */
#define PGM_CONVERT_GREYSCALE

class PGMConverter
{
public:
    /* Ctor/dtor. */
    PGMConverter(void);
    ~PGMConverter(void);

    int MythPlayerInited(const MythPlayer *player);
    const AVPicture *getImage(const VideoFrame *frame, long long frameno,
            int *pwidth, int *pheight);
    int reportTime(void);

private:
    long long       frameno;            /* frame number */
    int             width, height;      /* frame dimensions */
    AVPicture       pgm;                /* grayscale frame */
#ifdef PGM_CONVERT_GREYSCALE
    struct timeval  convert_time;
    bool            time_reported;
    MythAVCopy     *m_copy;
#endif /* PGM_CONVERT_GREYSCALE */
};

#endif  /* !__PGMCONVERTER_H__ */

/* vim: set expandtab tabstop=4 shiftwidth=4: */
