/*
 * SceneChangeDetector
 *
 * Detect scene changes based on histogram analysis.
 */

#ifndef __SCENECHANGEDETECTOR_H__
#define __SCENECHANGEDETECTOR_H__

#include <QString>

#include "FrameAnalyzer.h"

using AVFrame = struct AVFrame;
class HistogramAnalyzer;

class SceneChangeDetector : public FrameAnalyzer
{
public:
    SceneChangeDetector(HistogramAnalyzer *ha, const QString& debugdir);
    virtual void deleteLater(void);

    /* FrameAnalyzer interface. */
    const char *name(void) const override // FrameAnalyzer
        { return "SceneChangeDetector"; }
    enum analyzeFrameResult MythPlayerInited(MythPlayer *player,
            long long nframes) override; // FrameAnalyzer
    enum analyzeFrameResult analyzeFrame(const VideoFrame *frame,
            long long frameno, long long *pNextFrame) override; // FrameAnalyzer
    int finished(long long nframes, bool final) override; // FrameAnalyzer
    int reportTime(void) const override; // FrameAnalyzer
    FrameMap GetMap(unsigned int) const override // FrameAnalyzer
        { return m_changeMap; }

    /* SceneChangeDetector interface. */
    const FrameAnalyzer::FrameMap *getChanges(void) const { return &m_changeMap; }

    struct scenechange_data {
        unsigned char   color;
        unsigned char   frequency;
    };
    using SceneChangeData = scenechange_data[UCHAR_MAX + 1];

  protected:
    virtual ~SceneChangeDetector(void) = default;

  private:
    HistogramAnalyzer       *m_histogramAnalyzer {nullptr};
    float                   m_fps                {0.0F};

    /* per-frame info */
    SceneChangeData         *m_scdata            {nullptr};
    unsigned short          *m_scdiff            {nullptr};

    FrameAnalyzer::FrameMap m_changeMap;

    /* Debugging */
    int                     m_debugLevel         {0};
    QString                 m_debugdata;            /* filename */
    bool                    m_debug_scenechange  {false};
    bool                    m_scenechange_done   {false};
};

#endif  /* !__SCENECHANGEDETECTOR_H__ */

/* vim: set expandtab tabstop=4 shiftwidth=4: */

