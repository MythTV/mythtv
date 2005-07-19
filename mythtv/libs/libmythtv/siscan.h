// -*- Mode: c++ -*-
#include <qobject.h>
#include <qstring.h>
#include <qmap.h>
#include <qvaluelist.h>
#include <qdatetime.h>

#include "frequencytables.h"

#ifdef USING_DVB
#include "dvbchannel.h"
#include "sitypes.h"
#include "dvbtypes.h"
typedef QValueList<Event> QList_Events;
typedef QValueList<QList_Events*> QListList_Events;
#define SISCAN(args...) \
    VERBOSE(VB_SIPARSER, QString("SIScan(%1): ").arg(chan->GetDevice()) << args);
#else
typedef unsigned short uint16_t;
typedef QMap<uint16_t, void*> QMap_Events;
typedef QMap<uint16_t, void*> QMap_SDTObject;
typedef QValueList<void*> QList_Events;
typedef QValueList<QList_Events> QListList_Events;
class DVBChannel;
class DVBTuning;
typedef int fe_type_t;
typedef void* NITObject;
typedef void* NIT;
#define SISCAN(args...) \
    VERBOSE(VB_SIPARSER, QString("SIScan: ") << args);
#endif // USING_DVB

typedef enum { IDLE, TRANSPORT_LIST, EIT_CRAWL } SCANMODE;

class SIScan : public QObject
{
Q_OBJECT

public:

    SIScan(DVBChannel* advbchannel, int _sourceID);
    ~SIScan();

    void StartScanner();
    void StopScanner();

    bool ScanTransports(int SourceID,
                        const QString std,
                        const QString modulation,
                        const QString country);
    bool ScanTransports();
    bool ScanServices(int TransportID = -1);
    bool ScanServicesSourceID(int SourceID);
    void SetSourceID(int _SourceID);

    void SetFTAOnly(bool _fFTAOnly) { FTAOnly = _fFTAOnly;}
    void SetRadioServices(bool _fRadio) { RadioServices = _fRadio;}
    void SetForceUpdate(bool _force) { forceUpdate = _force;}
    void SetScanTimeout(int _timeout) { ScanTimeout = _timeout;}
    void SetChannelFormat(const QString _fmt) { channelFormat   = _fmt; }

public slots:

    // Handler for complete tables
    void TransportTableComplete();
    void ServiceTableComplete();
    void EventsReady(QMap_Events* EventList);

signals:
    // Values from 1-100 of scan completion
    void PctServiceScanComplete(int pct);
    void PctTransportScanComplete(int pct);
    void ServiceScanUpdateText(const QString& status);
    void TransportScanUpdateText(const QString& status);
    void ServiceScanComplete();
    void TransportScanComplete();

private:
    bool ATSCScanTransport(int SourceID, int FrequencyBand);
    bool DVBTScanTransport(int SourceID, unsigned country);

    static void *ServiceScanThreadHelper(void*);
    static void *TransportScanThreadHelper(void*);

    void verifyTransport(TransportScanItem& t);

    int CreateMultiplex(const fe_type_t cardType,const TransportScanItem& a,
                    const DVBTuning& tuning);

    void UpdateTransportsInDB(NITObject NIT);
    void CheckNIT(NITObject& NIT);

    void UpdateServicesInDB(QMap_SDTObject SDT);
    int  GenerateNewChanID(int SourceID);
    int  GetDVBTID(uint16_t NetworkID,uint16_t TransportID,int CurrentMplexID);
    void AddEvents();

  private:
    // Set in constructor
    DVBChannel       *chan;
    int               sourceID;
    SCANMODE          scanMode;

    // Settable
    bool              RadioServices;
    bool              FTAOnly;
    bool              forceUpdate;
    int               ScanTimeout;
    QString           channelFormat;

    // State
    bool              threadExit;
    QTime             timer;

    bool scannerRunning;                 
    bool serviceListReady;
    bool sourceIDTransportTuned;
    bool transportListReady;
    bool eventsReady;
    int  transportsToScan;
    int  transportsCount;

    QListList_Events Events;
    QMap_SDTObject SDT;

    NITObject NIT;
    QValueList<TransportScanItem> scanTransports;

    pthread_mutex_t events_lock;
};
