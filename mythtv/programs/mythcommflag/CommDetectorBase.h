#ifndef _CommDetectorBase_H_
#define _CommDetectorBase_H_

#include <iostream>
using namespace std;

#include <QObject>
#include <QMap>

#include "programtypes.h"

#define MAX_BLANK_FRAMES 60

typedef enum commMapValues {
    MARK_START   = 0,
    MARK_END     = 1,
    MARK_PRESENT = 2,
} CommMapValue;

typedef QMap<uint64_t, CommMapValue> show_map_t;

/** \class CommDetectorBase
 *  \brief Abstract base class for all CommDetectors.
 *   Please use the CommDetectFactory to make actual instances.
 */

class CommDetectorBase : public QObject
{
    Q_OBJECT

public:
    CommDetectorBase();

    virtual bool go() = 0;
    void stop();
    void pause();
    void resume();

    virtual void GetCommercialBreakList(frm_dir_map_t &comms) = 0;
    virtual void recordingFinished(long long totalFileSize)
        { (void)totalFileSize; };
    virtual void requestCommBreakMapUpdate(void) {};

    virtual void PrintFullMap(
        ostream &out, const frm_dir_map_t *comm_breaks, bool verbose) const = 0;

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
