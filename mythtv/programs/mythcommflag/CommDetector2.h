#ifndef _COMMDETECTOR2_H_
#define _COMMDETECTOR2_H_

// C++ headers
#include <vector>
using namespace std;

// Qt headers
#include <QDateTime>

// MythTV headers
#include "programinfo.h"

// Commercial Flagging headers
#include "CommDetectorBase.h"
#include "FrameAnalyzer.h"

class MythPlayer;
class TemplateFinder;
class TemplateMatcher;
class BlankFrameDetector;
class SceneChangeDetector;

namespace commDetector2 {

QString debugDirectory(int chanid, const QDateTime& recstartts);
void createDebugDirectory(const QString& dirname, const QString& comment);
QString frameToTimestamp(long long frameno, float fps);
QString frameToTimestampms(long long frameno, float fps);
QString strftimeval(const struct timeval *tv);

};  /* namespace */

using FrameAnalyzerItem = vector<FrameAnalyzer*>;
using FrameAnalyzerList = vector<FrameAnalyzerItem>;

class CommDetector2 : public CommDetectorBase
{
  public:
    CommDetector2(
        SkipType commDetectMethod,
        bool showProgress, bool fullSpeed, MythPlayer* player,
        int chanid, QDateTime startts, QDateTime endts,
        QDateTime recstartts, QDateTime recendts, bool useDB);
    bool go(void) override; // CommDetectorBase
    void GetCommercialBreakList(frm_dir_map_t &marks) override; // CommDetectorBase
    void recordingFinished(long long totalFileSize) override; // CommDetectorBase
    void requestCommBreakMapUpdate(void) override; // CommDetectorBase
    void PrintFullMap(ostream &out, const frm_dir_map_t *comm_breaks,
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
    MythPlayer                  *m_player                  {nullptr};
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

#endif  /* !_COMMDETECTOR2_H_ */

/* vim: set expandtab tabstop=4 shiftwidth=4: */
