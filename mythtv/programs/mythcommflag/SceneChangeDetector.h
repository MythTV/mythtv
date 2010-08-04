/*
 * SceneChangeDetector
 *
 * Detect scene changes based on histogram analysis.
 */

#ifndef __SCENECHANGEDETECTOR_H__
#define __SCENECHANGEDETECTOR_H__

#include <QString>

#include "FrameAnalyzer.h"

typedef struct AVPicture AVPicture;
class HistogramAnalyzer;

class SceneChangeDetector : public FrameAnalyzer
{
public:
    SceneChangeDetector(HistogramAnalyzer *ha, QString debugdir);
    virtual void deleteLater(void);

    /* FrameAnalyzer interface. */
    const char *name(void) const { return "SceneChangeDetector"; }
    enum analyzeFrameResult MythPlayerInited(MythPlayer *player,
            long long nframes);
    enum analyzeFrameResult analyzeFrame(const VideoFrame *frame,
            long long frameno, long long *pNextFrame);
    int finished(long long nframes, bool final);
    int reportTime(void) const;
    FrameMap GetMap(unsigned int) const { return changeMap; }

    /* SceneChangeDetector interface. */
    const FrameAnalyzer::FrameMap *getChanges(void) const { return &changeMap; }

    typedef struct scenechange_data {
        unsigned char   color;
        unsigned char   frequency;
    } SceneChangeData[UCHAR_MAX + 1];

  protected:
    virtual ~SceneChangeDetector(void) {}

  private:
    HistogramAnalyzer       *histogramAnalyzer;
    float                   fps;

    /* per-frame info */
    SceneChangeData         *scdata;
    unsigned short          *scdiff;

    FrameAnalyzer::FrameMap changeMap;

    /* Debugging */
    int                     debugLevel;
    QString                 debugdata;              /* filename */
    bool                    debug_scenechange;
    bool                    scenechange_done;
};

#endif  /* !__SCENECHANGEDETECTOR_H__ */

/* vim: set expandtab tabstop=4 shiftwidth=4: */

