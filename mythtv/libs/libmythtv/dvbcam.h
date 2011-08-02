#ifndef DVBCAM_H
#define DVBCAM_H

#include <deque>
using namespace std;

#include <QWaitCondition>
#include <QRunnable>
#include <QString>
#include <QMutex>

#include "mpegtables.h"

#include "dvbtypes.h"

class ChannelBase;
class cCiHandler;
class MThread;
class DVBCam;

typedef QMap<const ChannelBase*, ProgramMapTable*> pmt_list_t;

class DVBCam : public QRunnable
{
  public:
    DVBCam(const QString &device);
    ~DVBCam();

    bool Start(void);
    bool Stop(void);
    bool IsRunning(void) const
    {
        QMutexLocker locker(&ciHandlerLock);
        return ciHandlerRunning;
    }

    void SetPMT(const ChannelBase *chan, const ProgramMapTable *pmt);
    void SetTimeOffset(double offset_in_seconds);

  private:
    void run(void); // QRunnable
    void HandleUserIO(void);
    void HandlePMT(void);

    void SendPMT(const ProgramMapTable &pmt, uint cplm);

    QString         device;
    int             numslots;

    mutable QMutex  ciHandlerLock;
    QWaitCondition  ciHandlerWait;
    bool            ciHandlerDoRun;
    bool            ciHandlerRunning;
    cCiHandler     *ciHandler;
    MThread        *ciHandlerThread;

    QMutex          pmt_lock;
    pmt_list_t      PMTList;
    pmt_list_t      PMTAddList;
    bool            have_pmt;
    bool            pmt_sent;
    bool            pmt_updated;
    bool            pmt_added;
};

#endif // DVBCAM_H

