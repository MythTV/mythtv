
#ifndef DVBCAM_H
#define DVBCAM_H

#include <qobject.h>
#include <qsqldatabase.h>
#include <pthread.h>

#include <dvbci.h>
#include "dvbtypes.h"

class DVBCam: public QObject
{
    Q_OBJECT
public:
    DVBCam(int cardnum);
    ~DVBCam();

    bool Open();
    bool Close();

    static void *CiHandlerThreadHelper(void*self);
    void CiHandlerLoop();

    void SetDatabase(QSqlDatabase* _db, pthread_mutex_t* _db_lock)
        { db = _db; db_lock = _db_lock; };

public slots:
    void ChannelChanged(dvb_channel_t& chan);
    void ChannelChanged(dvb_channel_t& chan, uint8_t* pmt, int len);

private:
    void SetPids(cCiCaPmt& capmt, dvb_pids_t& pid);
    bool FindCaDescriptors(cCiCaPmt& capmt, const unsigned short *caids, int slot);

    QSqlDatabase*   db;
    pthread_mutex_t* db_lock;

    int             cardnum;
    cCiHandler      *ciHandler;

    bool            exitCiThread;
    bool            ciThreadRunning;

    pthread_mutex_t pmt_lock;
    uint8_t*        cachedpmtbuf;
    uint8_t*        pmtbuf;
    int             pmtlen;
    pthread_t       ciHandlerThread;
    bool            first_send;

    dvb_channel_t   chan_opts;
};

#endif // DVBCAM_H
