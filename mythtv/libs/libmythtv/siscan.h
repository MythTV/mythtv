// -*- Mode: c++ -*-

#ifndef SISCAN_H
#define SISCAN_H

// Qt includes
#include <qobject.h>
#include <qstring.h>
#include <qmap.h>
#include <qmutex.h>
#include <qvaluelist.h>
#include <qdatetime.h>

// MythTV includes
#include "frequencytables.h"

#ifdef USING_DVB
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
class DVBTuning;
typedef int fe_type_t;
typedef void* NITObject;
typedef void* NIT;
#define SISCAN(args...) \
    VERBOSE(VB_SIPARSER, QString("SIScan: ") << args);
#endif // USING_DVB

class MSqlQuery;
class ChannelBase;
class Channel;
class DVBChannel;
class SignalMonitor;
class DTVSignalMonitor;
class DVBSignalMonitor;
class ServiceDescriptionTable;
class NetworkInformationTable;
class VirtualChannelTable;
class MasterGuideTable;
class ScanStreamData;
class EITHelper;

typedef enum
{
    IDLE,           ///< Don't do anything
    TRANSPORT_LIST, ///< Actively scan for channels
    EIT_CRAWL       ///< Scan for event information
} SCANMODE;

class SIScan : public QObject
{
    Q_OBJECT
  public:
    SIScan(DVBChannel* advbchannel, int _sourceID);
    ~SIScan();

    void StartScanner(void);
    void StopScanner(void);

    bool ScanTransports(int SourceID,
                        const QString std,
                        const QString modulation,
                        const QString country);
    bool ScanTransports(void);
    bool ScanServices(int TransportID = -1);
    bool ScanServicesSourceID(int SourceID);

    void SetSourceID(int _SourceID)   { sourceID                = _SourceID; }
    void SetFTAOnly(bool _fFTAOnly)   { FTAOnly                 = _fFTAOnly; }
    void SetRadioServices(bool _fRadio) { RadioServices         = _fRadio;   }
    void SetForceUpdate(bool _force)  { forceUpdate             = _force;    }
    void SetScanTimeout(int _timeout) { ScanTimeout             = _timeout;  }
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
    void ServiceScanUpdateStatusText(const QString& status);
    void ServiceScanUpdateText(const QString& status);
    void TransportScanUpdateText(const QString& status);
    void ServiceScanComplete(void);
    void TransportScanComplete(void);

  private:
    /// \brief Called by SpawnScanner to run scanning thread
    void RunScanner(void);
    /// \brief Thunk to call RunScanner from pthread
    static void *SpawnScanner(void *param);

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

    /// Scanner thread, runs SIScan::StartScanner()
    pthread_t        scanner_thread;
    bool             scanner_thread_running;
};

#endif // SISCAN_H
