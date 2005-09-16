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
#define USE_SIPARSER
#include "sitypes.h"
#include "dvbtypes.h"
#endif // USING_DVB

#ifdef USE_SIPARSER
typedef QValueList<Event> QList_Events;
typedef QValueList<QList_Events*> QListList_Events;
#endif // USING_SIPARSER

#define SISCAN(args...) \
    if (channel) VERBOSE(VB_SIPARSER, QString("SIScan(%1): ") \
                         .arg(channel->GetDevice()) << args); \
    else VERBOSE(VB_SIPARSER, QString("SIScan(u): ") << args);

class MSqlQuery;
class ChannelBase;
class Channel;
class DVBChannel;
class SignalMonitor;
class DTVSignalMonitor;
class DVBSignalMonitor;
class ProgramAssociationTable;
class ServiceDescriptionTable;
class NetworkInformationTable;
class VirtualChannelTable;
class MasterGuideTable;
class ScanStreamData;
class EITHelper;
class DVBSIParser;

typedef enum
{
    IDLE,           ///< Don't do anything
    TRANSPORT_LIST, ///< Actively scan for channels
} SCANMODE;

class SIScan : public QObject
{
    Q_OBJECT
  public:
    SIScan(QString _cardtype, ChannelBase* _channel, int _sourceID,
           uint signal_timeout, uint channel_timeout);
    ~SIScan();

    void StartScanner(void);
    void StopScanner(void);

    bool ScanTransports(int SourceID,
                        const QString std,
                        const QString modulation,
                        const QString country);
    bool ScanTransports(const QString sistandard);
    bool ScanTransport(int mplexid);

    bool ScanServices(void);
    bool ScanServicesSourceID(int SourceID);

    void SetSourceID(int _SourceID)   { sourceID                = _SourceID; }
    void SetFTAOnly(bool _fFTAOnly)   { ignoreEncryptedServices = _fFTAOnly; }
    void SetTVOnly(bool _tvOnly)      { ignoreAudioOnlyServices = _tvOnly;   }
    void SetForceUpdate(bool _force)  { forceUpdate             = _force;    }
    void SetChannelFormat(const QString _fmt) { channelFormat   = _fmt; }

    SignalMonitor    *GetSignalMonitor(void) { return signalMonitor; }
    DTVSignalMonitor *GetDTVSignalMonitor(void);
    DVBSignalMonitor *GetDVBSignalMonitor(void);

  public slots:
    // Handler for complete tables
    void TransportTableComplete();
    void ServiceTableComplete();

  signals:
    // Values from 1-100 of scan completion
    void PctServiceScanComplete(int pct);
    void PctTransportScanComplete(int pct);
    void ServiceScanUpdateStatusText(const QString& status);
    void ServiceScanUpdateText(const QString& status);
    void TransportScanUpdateText(const QString& status);
    void ServiceScanComplete(void);
    void TransportScanComplete(void);

  private slots:
    void HandleVCT(uint tsid, const VirtualChannelTable*);
    void HandleMGT(const MasterGuideTable*);
    void HandleSDT(uint tsid, const ServiceDescriptionTable*);
    void HandleNIT(const NetworkInformationTable*);

  private:
    // some useful gets
    Channel          *GetChannel(void);
    DVBChannel       *GetDVBChannel(void);

    /// \brief Called by SpawnScanner to run scanning thread
    void RunScanner(void);
    /// \brief Thunk to call RunScanner from pthread
    static void *SpawnScanner(void *param);

    bool HasTimedOut(void);
    void HandleActiveScan(void);
    bool Tune(const transport_scan_items_it_t transport);
    int InsertMultiplex(const transport_scan_items_it_t transport);
    void ScanTransport(const transport_scan_items_it_t transport);

    /// \brief Updates Transport Scan progress bar
    inline void UpdateScanPercentCompleted(void);

    void HandleMPEGDBInsertion(const ScanStreamData *sd, bool wait);
    void UpdatePATinDB(int mplexid,
                       const ProgramAssociationTable*,
                       bool force_update);

    void HandleATSCDBInsertion(const ScanStreamData *sd, bool wait);
    void UpdateVCTinDB(int mplexid,
                       const VirtualChannelTable*,
                       bool force_update);

    void HandleDVBDBInsertion(const ScanStreamData *sd, bool wait);
    void UpdateSDTinDB(int mplexid,
                       const ServiceDescriptionTable*,
                       bool force_update);

    bool HandlePostInsertion(void);

    void OptimizeNITFrequencies(NetworkInformationTable *nit);

    // old stuff
    void verifyTransport(TransportScanItem& t);
#ifdef USE_SIPARSER
    void HandleSIParserEvents(void);
    void CheckNIT(NITObject& NIT);
    void UpdateTransportsInDB(NITObject NIT);
    void UpdateServicesInDB(int tid_db, QMap_SDTObject SDT);
    static void *SpawnSectionReader(void*);
#endif // USE_SIPARSER

  private:
    // Set in constructor
    ChannelBase      *channel;
    SignalMonitor    *signalMonitor;
    int               sourceID;
    SCANMODE          scanMode;
    uint              signalTimeout;
    uint              channelTimeout;

    // Settable
    bool              ignoreAudioOnlyServices;
    bool              ignoreEncryptedServices;
    bool              forceUpdate;
    QString           channelFormat;

    // State
    bool              threadExit;
    bool              waitingForTables;
    QTime             timer;

    // Transports List
    int                         transportsScanned;
    transport_scan_items_t      scanTransports;
    transport_scan_items_it_t   current;
    transport_scan_items_it_t   nextIt;
    QMap<uint, uint>            dvbChanNums;

#ifdef USE_SIPARSER
    bool serviceListReady;
    bool transportListReady;

    QMap_SDTObject SDT;
    NITObject NIT;
    pthread_t siparser_thread;
  public:
    DVBSIParser* siparser;
  private:
#endif // USE_SIPARSER

    /// Scanner thread, runs SIScan::StartScanner()
    pthread_t        scanner_thread;
    bool             scanner_thread_running;
};

inline void SIScan::UpdateScanPercentCompleted(void)
{
    int tmp = (transportsScanned * 100) / scanTransports.size();
    emit PctServiceScanComplete(tmp);
}

#endif // SISCAN_H
