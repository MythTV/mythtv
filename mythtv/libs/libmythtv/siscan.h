
#include <qobject.h>
#include <qstring.h>
#include "dvbchannel.h"
#include "sitypes.h"
#include "dvbtypes.h"
#include "frequencytables.h"

typedef QValueList<Event> QList_Events;
typedef QValueList<QList_Events*> QListList_Events;

typedef enum { IDLE, TRANSPORT_LIST, EIT_CRAWL } SCANMODE;

class SIScan : public QObject
{
Q_OBJECT

public:

    SIScan(DVBChannel* advbchannel, int _sourceID);
    ~SIScan();

    void StartScanner();
    void StopScanner();

    bool ATSCScanTransport(int SourceID, int FrequencyBand);
    bool DVBTScanTransport(int SourceID, unsigned country);
    bool ScanTransports();
    bool ScanServices(int TransportID = -1);
    bool ScanServicesSourceID(int SourceID);
    void SetSourceID(int _SourceID);

    void SetFTAOnly(bool _fFTAOnly) { FTAOnly = _fFTAOnly;}
    void SetRadioServices(bool _fRadio) { RadioServices = _fRadio;}
    void SetForceUpdate(bool _force) { forceUpdate = _force;}
    void SetScanTimeout(int _timeout) { ScanTimeout = _timeout;}

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
    DVBChannel *chan;
    int sourceID;
    QValueList<TransportScanItem> scanTransports;

    QTime timer;

    bool threadExit;
    bool FTAOnly;
    bool RadioServices;
    bool forceUpdate;
    int ScanTimeout;
    SCANMODE scanMode;
    pthread_mutex_t events_lock;
};

#define SISCAN(args...) \
    VERBOSE(VB_SIPARSER, QString("SIScan#%1: ").arg(chan->GetCardNum()) << args);
