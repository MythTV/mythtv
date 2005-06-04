#ifndef _CommDetectorBase_H_
#define _CommDetectorBase_H_

#include "qobject.h"
class QString;

/* Abstract base class for all CommDetectors. Please use the CommDetectFactory
 * to make actual instances.
 */

enum commMapValues {
    MARK_START = 0,
    MARK_END,
    MARK_PRESENT
};

class CommDetectorBase : public QObject
{
    Q_OBJECT

public:
    CommDetectorBase();

    virtual bool go() = 0;
    void stop();
    void pause();
    void resume();

    virtual void getCommercialBreakList(QMap<long long, int> &comms) =0;
    virtual void recordingFinished(long long totalFileSize) {};
    virtual void requestCommBreakMapUpdate(void) {};

signals:
    void statusUpdate(const QString& a) ;
    void gotNewCommercialBreakList();
    void breathe();

protected:    
    bool m_bPaused;
    bool m_bStop;    
    
};

#endif


/* vim: set expandtab tabstop=4 shiftwidth=4: */
