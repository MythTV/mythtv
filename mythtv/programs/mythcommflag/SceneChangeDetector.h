/*
 * SceneChangeDetector
 *
 * Detect scene changes based on histogram analysis.
 */

#ifndef SCENECHANGEDETECTOR_H
#define SCENECHANGEDETECTOR_H

#include <QString>

#include "FrameAnalyzer.h"

using AVFrame = struct AVFrame;
class HistogramAnalyzer;

class SceneChangeDetector : public FrameAnalyzer
{
public:
    SceneChangeDetector(std::shared_ptr<HistogramAnalyzer> ha, const QString& debugdir);
    virtual void deleteLater(void) {};

    /* FrameAnalyzer interface. */
    const char *name(void) const override // FrameAnalyzer
        { return "SceneChangeDetector"; }
    enum analyzeFrameResult MythPlayerInited(MythPlayer *player,
            long long nframes) override; // FrameAnalyzer
    enum analyzeFrameResult analyzeFrame(const MythVideoFrame *frame,
            long long frameno, long long *pNextFrame) override; // FrameAnalyzer
    int finished(long long nframes, bool final) override; // FrameAnalyzer
    int reportTime(void) const override; // FrameAnalyzer
    FrameMap GetMap(unsigned int /*index*/) const override // FrameAnalyzer
        { return m_changeMap; }

    /* SceneChangeDetector interface. */
    const FrameAnalyzer::FrameMap *getChanges(void) const { return &m_changeMap; }

    struct scenechange_data {
        unsigned char   color;
        unsigned char   frequency;
    };
    using SceneChangeData = std::array<scenechange_data,UCHAR_MAX + 1>;

  protected:
    ~SceneChangeDetector(void) override = default;

  private:
    std::shared_ptr<HistogramAnalyzer> m_histogramAnalyzer {nullptr};
    float                   m_fps                {0.0F};

    /* per-frame info */
    std::vector<SceneChangeData> m_scData;
    std::vector<uint16_t>        m_scDiff;

    FrameAnalyzer::FrameMap m_changeMap;

    /* Debugging */
    int                     m_debugLevel         {0};
    QString                 m_debugData;            /* filename */
    bool                    m_debugSceneChange   {false};
    bool                    m_sceneChangeDone    {false};
};

#endif  /* !SCENECHANGEDETECTOR_H */

/* vim: set expandtab tabstop=4 shiftwidth=4: */
