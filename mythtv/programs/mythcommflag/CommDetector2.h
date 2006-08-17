#ifndef _COMMDETECTOR2_H_
#define _COMMDETECTOR2_H_

#include <qdatetime.h>
#include <qptrlist.h>
#include <qvaluelist.h>

#include "CommDetector.h"
#include "CommDetectorBase.h"
#include "FrameAnalyzer.h"

class NuppelVideoPlayer;
class TemplateMatcher;
class BlankFrameDetector;

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
    bool go(void);
    void getCommercialBreakList(QMap<long long, int> &comms);
    void recordingFinished(long long totalFileSize);

private:
    int buildBuffer(int minbuffer); // seconds
    void reportState(int elapsed_sec, long long frameno, long long nframes,
            unsigned int passno, unsigned int npasses);
    int computeBreaks(void);

    enum SkipTypes          commDetectMethod;
    bool                    showProgress;
    bool                    fullSpeed;
    NuppelVideoPlayer       *nvp;
    QDateTime               startts, endts, recstartts, recendts;

    bool                    isRecording;        /* current state */

    typedef QValueList<QPtrList<FrameAnalyzer> >    frameAnalyzerList;
    frameAnalyzerList       frameAnalyzers;     /* one list per scan of file */

    FrameAnalyzer::FrameMap breaks;

    TemplateMatcher         *logoMatcher;
    BlankFrameDetector      *blankFrameDetector;

    QString                 debugdir;
};

#endif  /* !_COMMDETECTOR2_H_ */

/* vim: set expandtab tabstop=4 shiftwidth=4: */
