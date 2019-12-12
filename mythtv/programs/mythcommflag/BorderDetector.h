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

using AVFrame = struct AVFrame;
class MythPlayer;
class TemplateFinder;

class BorderDetector
{
public:
    /* Ctor/dtor. */
    BorderDetector(void);

    int MythPlayerInited(const MythPlayer *player);
    void setLogoState(TemplateFinder *finder);

    static const long long kUncached = -1;
    int getDimensions(const AVFrame *pgm, int pgmheight, long long frameno,
            int *prow, int *pcol, int *pwidth, int *pheight);

    int reportTime(void);

private:
    TemplateFinder         *m_logoFinder      {nullptr};
    const struct AVFrame   *m_logo            {nullptr};
    int                     m_logoRow         {-1};
    int                     m_logoCol         {-1};
    int                     m_logoWidth       {-1};
    int                     m_logoHeight      {-1};

    long long               m_frameNo         {-1}; /* frame number */
    int                     m_row             {-1}; /* content location */
    int                     m_col             {-1}; /* content location */
    int                     m_width           {-1}; /* content dimensions */
    int                     m_height          {-1}; /* content dimensions */
    bool                    m_isMonochromatic {false};

    /* Debugging. */
    int                     m_debugLevel      {0};
    struct timeval          m_analyzeTime     {0,0};
    bool                    m_timeReported    {false};
};

#endif  /* !__BORDERDETECTOR_H__ */

/* vim: set expandtab tabstop=4 shiftwidth=4: */
