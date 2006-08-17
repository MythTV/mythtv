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

class BorderDetector
{
public:
    /* Ctor/dtor. */
    BorderDetector(void);
    ~BorderDetector(void);

    static const long long UNCACHED = -1;
    int getDimensions(const AVPicture *pgm, int pgmheight, long long frameno,
            int *prow, int *pcol, int *pwidth, int *pheight);

private:
    long long       frameno;            /* frame number */
    int             row, col;           /* content location */
    int             width, height;      /* content dimensions */

    int             debugLevel;
};

#endif  /* !__BORDERDETECTOR_H__ */

/* vim: set expandtab tabstop=4 shiftwidth=4: */
