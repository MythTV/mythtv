#ifndef _CommDetectorBase_H_
#define _CommDetectorBase_H_

#include <iostream>
using namespace std;

#include <QObject>
#include <QMap>

enum commMapValues {
    MARK_START = 0,
    MARK_END,
    MARK_PRESENT
};

/** \class CommDetectorBase
 *  \brief Abstract base class for all CommDetectors.
 *   Please use the CommDetectFactory to make actual instances.
 */

typedef QMap<long long, int> comm_break_t;

class CommDetectorBase : public QObject
{
    Q_OBJECT

public:
    CommDetectorBase();

    virtual bool go() = 0;
    void stop();
    void pause();
    void resume();

    virtual void getCommercialBreakList(comm_break_t &comms) = 0;
    virtual void recordingFinished(long long totalFileSize)
        { (void)totalFileSize; };
    virtual void requestCommBreakMapUpdate(void) {};

    virtual void PrintFullMap(
        ostream &out, const comm_break_t *comm_breaks, bool verbose) const = 0;

signals:
    void statusUpdate(const QString& a) ;
    void gotNewCommercialBreakList();
    void breathe();

protected:    
    ~CommDetectorBase() {}
    bool m_bPaused;
    bool m_bStop;    
    
};

#endif


/* vim: set expandtab tabstop=4 shiftwidth=4: */
