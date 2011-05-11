#ifndef DVBCAM_H
#define DVBCAM_H

#include <deque>
using namespace std;

#include <QWaitCondition>
#include <QString>
#include <QThread>
#include <QMutex>

#include "mpegtables.h"

#include "dvbtypes.h"

class ChannelBase;
class cCiHandler;
class DVBCam;

typedef QMap<const ChannelBase*, ProgramMapTable*> pmt_list_t;

class DVBCamThread : public QThread
{
    Q_OBJECT
  public:
    DVBCamThread(DVBCam *p) : m_parent(p) {}
    virtual ~DVBCamThread() { wait(); m_parent = NULL; }
    virtual void run(void);
  private:
    DVBCam *m_parent;
};

class DVBCam
{
    friend class DVBCamThread;

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
    void CiHandlerLoop(void);
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
    DVBCamThread   *ciHandlerThread;

    QMutex          pmt_lock;
    pmt_list_t      PMTList;
    pmt_list_t      PMTAddList;
    bool            have_pmt;
    bool            pmt_sent;
    bool            pmt_updated;
    bool            pmt_added;
};

#endif // DVBCAM_H

