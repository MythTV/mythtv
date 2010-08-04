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
void createDebugDirectory(QString dirname, QString comment);
QString frameToTimestamp(long long frameno, float fps);
QString frameToTimestampms(long long frameno, float fps);
QString strftimeval(const struct timeval *tv);

};  /* namespace */

typedef vector<FrameAnalyzer*>    FrameAnalyzerItem;
typedef vector<FrameAnalyzerItem> FrameAnalyzerList;

class CommDetector2 : public CommDetectorBase
{
  public:
    CommDetector2(
        SkipType commDetectMethod,
        bool showProgress, bool fullSpeed, MythPlayer* player,
        int chanid, const QDateTime& startts, const QDateTime& endts,
        const QDateTime& recstartts, const QDateTime& recendts, bool useDB);
    virtual bool go(void);
    virtual void GetCommercialBreakList(frm_dir_map_t &comms);
    virtual void recordingFinished(long long totalFileSize);
    virtual void requestCommBreakMapUpdate(void);
    virtual void PrintFullMap(
        ostream &out, const frm_dir_map_t *comm_breaks, bool verbose) const;

  private:
    virtual ~CommDetector2() {}

    void reportState(int elapsed_sec, long long frameno, long long nframes,
            unsigned int passno, unsigned int npasses);
    int computeBreaks(long long nframes);

  private:
    enum SkipTypes          commDetectMethod;
    bool                    showProgress;
    bool                    fullSpeed;
    MythPlayer             *player;
    QDateTime               startts, endts, recstartts, recendts;

    bool                    isRecording;        /* current state */
    bool                    sendBreakMapUpdates;
    bool                    breakMapUpdateRequested;
    bool                    finished;

    long long               currentFrameNumber;
    FrameAnalyzerList       frameAnalyzers;     /* one list per scan of file */
    FrameAnalyzerList::iterator currentPass;
    FrameAnalyzerItem       finishedAnalyzers;

    FrameAnalyzer::FrameMap breaks;

    TemplateFinder          *logoFinder;
    TemplateMatcher         *logoMatcher;
    BlankFrameDetector      *blankFrameDetector;
    SceneChangeDetector     *sceneChangeDetector;

    QString                 debugdir;
};

#endif  /* !_COMMDETECTOR2_H_ */

/* vim: set expandtab tabstop=4 shiftwidth=4: */
