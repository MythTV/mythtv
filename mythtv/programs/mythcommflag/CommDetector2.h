#ifndef COMMDETECTOR2_H
#define COMMDETECTOR2_H

// C++ headers
#include <vector>

// Qt headers
#include <QDateTime>

// MythTV headers
#include "libmythbase/programinfo.h"

// Commercial Flagging headers
#include "CommDetectorBase.h"
#include "FrameAnalyzer.h"

class MythCommFlagPlayer;
class TemplateFinder;
class TemplateMatcher;
class BlankFrameDetector;
class SceneChangeDetector;

namespace commDetector2 {

QString debugDirectory(int chanid, const QDateTime& recstartts);
void createDebugDirectory(const QString& dirname, const QString& comment);
QString frameToTimestamp(long long frameno, float fps);
QString frameToTimestampms(long long frameno, float fps);
QString strftimeval(std::chrono::microseconds usecs);

};  /* namespace */

using FrameAnalyzerItem = std::vector<FrameAnalyzer*>;
using FrameAnalyzerList = std::vector<FrameAnalyzerItem>;

class CommDetector2 : public CommDetectorBase
{
  public:
    CommDetector2(
        SkipType commDetectMethod,
        bool showProgress, bool fullSpeed, MythCommFlagPlayer* player,
        int chanid, QDateTime startts, QDateTime endts,
        QDateTime recstartts, QDateTime recendts, bool useDB);
    bool go(void) override; // CommDetectorBase
    void GetCommercialBreakList(frm_dir_map_t &marks) override; // CommDetectorBase
    void recordingFinished(long long totalFileSize) override; // CommDetectorBase
    void requestCommBreakMapUpdate(void) override; // CommDetectorBase
    void PrintFullMap(std::ostream &out, const frm_dir_map_t *comm_breaks,
                      bool verbose) const override; // CommDetectorBase

  private:
    ~CommDetector2() override = default;

    void reportState(int elapsedms, long long frameno, long long nframes,
            unsigned int passno, unsigned int npasses);
    int computeBreaks(long long nframes);

  private:
    SkipType                     m_commDetectMethod;
    bool                         m_showProgress            {false};
    bool                         m_fullSpeed               {false};
    MythCommFlagPlayer          *m_player                  {nullptr};
    QDateTime                    m_startts;
    QDateTime                    m_endts;
    QDateTime                    m_recstartts;
    QDateTime                    m_recendts;

                                 /* current state */
    bool                         m_isRecording             {false};
    bool                         m_sendBreakMapUpdates     {false};
    bool                         m_breakMapUpdateRequested {false};
    bool                         m_finished                {false};

    long long                    m_currentFrameNumber      {0};
    FrameAnalyzerList            m_frameAnalyzers; /* one list per scan of file */
    FrameAnalyzerList::iterator  m_currentPass;
    FrameAnalyzerItem            m_finishedAnalyzers;

    FrameAnalyzer::FrameMap      m_breaks;

    TemplateFinder              *m_logoFinder              {nullptr};
    TemplateMatcher             *m_logoMatcher             {nullptr};
    BlankFrameDetector          *m_blankFrameDetector      {nullptr};
    SceneChangeDetector         *m_sceneChangeDetector     {nullptr};

    QString                      m_debugdir;
};

#endif  /* !COMMDETECTOR2_H */

/* vim: set expandtab tabstop=4 shiftwidth=4: */
