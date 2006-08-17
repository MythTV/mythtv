/*
 * BorderDetector
 *
 * Attempt to infer content boundaries of an image by looking for rows and
 * columns of pixels whose values fall within some narrow range.
 *
 * Handle letterboxing, pillarboxing, and combinations of the two. Handle cases
 * where letterboxing embedded inside of pillarboxing (or vice versa) uses a
 * different filler color.
 */

#ifndef __BORDERDETECTOR_H__
#define __BORDERDETECTOR_H__

typedef struct AVPicture AVPicture;
class NuppelVideoPlayer;
class TemplateFinder;

class BorderDetector
{
public:
    /* Ctor/dtor. */
    BorderDetector(void);
    ~BorderDetector(void);

    int nuppelVideoPlayerInited(const NuppelVideoPlayer *nvp);
    void setLogoState(TemplateFinder *finder);

    static const long long UNCACHED = -1;
    int getDimensions(const AVPicture *pgm, int pgmheight, long long frameno,
            int *prow, int *pcol, int *pwidth, int *pheight);

    int reportTime(void);

private:
    TemplateFinder          *logoFinder;
    const struct AVPicture  *logo;
    int                     logowidth, logoheight;
    int                     logorr1, logocc1, logorr2, logocc2;

    long long               frameno;            /* frame number */
    int                     row, col;           /* content location */
    int                     width, height;      /* content dimensions */
    bool                    ismonochromatic;

    /* Debugging. */
    int                     debugLevel;
    struct timeval          analyze_time;
    bool                    time_reported;
};

#endif  /* !__BORDERDETECTOR_H__ */

/* vim: set expandtab tabstop=4 shiftwidth=4: */
