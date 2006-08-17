#ifndef _COMMDETECTOR2_H_
#define _COMMDETECTOR2_H_

#include <qdatetime.h>
#include <qptrlist.h>
#include <qvaluelist.h>

#include "CommDetector.h"
#include "CommDetectorBase.h"
#include "FrameAnalyzer.h"

class NuppelVideoPlayer;
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

class CommDetector2 : public CommDetectorBase
{
public:
    CommDetector2(enum SkipTypes commDetectMethod,
            bool showProgress, bool fullSpeed, NuppelVideoPlayer* nvp,
            int chanid, const QDateTime& startts, const QDateTime& endts,
            const QDateTime& recstartts, const QDateTime& recendts);
    virtual bool go(void);
    virtual void getCommercialBreakList(QMap<long long, int> &comms);
    virtual void recordingFinished(long long totalFileSize);
    virtual void requestCommBreakMapUpdate(void);

private:
    void reportState(int elapsed_sec, long long frameno, long long nframes,
            unsigned int passno, unsigned int npasses);
    int computeBreaks(long long nframes);

    enum SkipTypes          commDetectMethod;
    bool                    showProgress;
    bool                    fullSpeed;
    NuppelVideoPlayer       *nvp;
    QDateTime               startts, endts, recstartts, recendts;

    bool                    isRecording;        /* current state */
    bool                    sendBreakMapUpdates;
    bool                    breakMapUpdateRequested;
    bool                    finished;

    long long               currentFrameNumber;
    typedef QValueList<QPtrList<FrameAnalyzer> >    frameAnalyzerList;
    frameAnalyzerList       frameAnalyzers;     /* one list per scan of file */
    frameAnalyzerList::iterator currentPass;
    QPtrList<FrameAnalyzer> finishedAnalyzers;

    FrameAnalyzer::FrameMap breaks;

    TemplateFinder          *logoFinder;
    TemplateMatcher         *logoMatcher;
    BlankFrameDetector      *blankFrameDetector;
    SceneChangeDetector     *sceneChangeDetector;

    QString                 debugdir;
};

#endif  /* !_COMMDETECTOR2_H_ */

/* vim: set expandtab tabstop=4 shiftwidth=4: */
