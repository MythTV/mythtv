#ifndef DVBCAM_H
#define DVBCAM_H

#include <qobject.h>
#include <pthread.h>

#include <dvbci.h>
#include "dvbtypes.h"

class DVBCam: public QObject
{
    Q_OBJECT
public:
    DVBCam(int cardnum);
    ~DVBCam();

    bool Start();
    bool Stop();
    bool IsRunning();
    void SetPMT(const PMTObject *pmt);
    void AddPMT(const PMTObject *pmt);

private:
    static void *CiHandlerThreadHelper(void*self);
    void CiHandlerLoop();

    void SendPMT(PMTObject &pmt, uint8_t cplm);

    int             cardnum;
    int             numslots;
    cCiHandler      *ciHandler;

    bool            exitCiThread;
    bool            ciThreadRunning;

    pthread_t       ciHandlerThread;

    QValueList<PMTObject> PMTList;
    QValueList<PMTObject> PMTAddList;
    pthread_mutex_t pmt_lock;
    bool            have_pmt;
    bool            pmt_sent;
    bool            pmt_updated;
    bool            pmt_added;
};

#endif // DVBCAM_H
