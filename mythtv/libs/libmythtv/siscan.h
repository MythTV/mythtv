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
#include "streamlisteners.h"
#include "dvbconfparser.h"

class MSqlQuery;

class ChannelBase;
class DTVChannel;
class Channel;
class DVBChannel;
class HDHRChannel;

class SignalMonitor;
class DTVSignalMonitor;
class DVBSignalMonitor;

typedef enum
{
    IDLE,           ///< Don't do anything
    TRANSPORT_LIST, ///< Actively scan for channels
} SCANMODE;

typedef vector<const ProgramMapTable*>  pmt_vec_t;
typedef QMap<uint, pmt_vec_t>           pmt_map_t;

class SIScan : public QObject,
               public MPEGStreamListener,
               public ATSCMainStreamListener,
               public DVBMainStreamListener
{
    Q_OBJECT
  public:
    SIScan(const QString &_cardtype, ChannelBase* _channel, int _sourceID,
           uint signal_timeout, uint channel_timeout,
           const QString &_inputname);
    ~SIScan();

    void StartScanner(void);
    void StopScanner(void);

    bool ScanTransports(
        int src, const QString std, const QString mod, const QString country);
    bool ScanTransportsStartingOn(
        int sourceid, const QMap<QString,QString> &valueMap);
    bool ScanTransport(int mplexid);
    bool ScanForChannels(
        uint sourceid, const QString &std, const QString &cardtype,
        const DTVChannelList&);

    bool ScanServicesSourceID(int SourceID);

    void SetAnalog(bool is_analog);
    void SetSourceID(int _SourceID)   { sourceID                = _SourceID; }
    void SetFTAOnly(bool _fFTAOnly)   { ignoreEncryptedServices = _fFTAOnly; }
    void SetTVOnly(bool _tvOnly)
        { ignoreAudioOnlyServices = ignoreDataServices = _tvOnly; }
    void SetForceUpdate(bool _force)  { forceUpdate             = _force;    }
    void SetRenameChannels(bool _r)   { renameChannels          = _r; }
    void SetChannelFormat(const QString _fmt) { channelFormat   = _fmt; }
    void SetSignalTimeout(uint val)    { signalTimeout = val; }
    void SetChannelTimeout(uint val)   { channelTimeout = val; }

    uint GetSignalTimeout(void)  const { return signalTimeout; }
    uint GetChannelTimeout(void) const { return channelTimeout; }

    SignalMonitor    *GetSignalMonitor(void) { return signalMonitor; }
    DTVSignalMonitor *GetDTVSignalMonitor(void);
    DVBSignalMonitor *GetDVBSignalMonitor(void);

    // MPEG
    void HandlePAT(const ProgramAssociationTable*);
    void HandleCAT(const ConditionalAccessTable*) { }
    void HandlePMT(uint, const ProgramMapTable*) { }
    void HandleEncryptionStatus(uint /*pnum*/, bool /*encrypted*/) { }

    // ATSC Main
    void HandleSTT(const SystemTimeTable*) {}
    void HandleMGT(const MasterGuideTable*);
    void HandleVCT(uint tsid, const VirtualChannelTable*);

    // DVB Main
    void HandleNIT(const NetworkInformationTable*);
    void HandleSDT(uint tsid, const ServiceDescriptionTable*);
    void HandleTDT(const TimeDateTable*) {}

  private slots:
    void HandleAllGood(void);

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
    // some useful gets
    DTVChannel       *GetDTVChannel(void);
    Channel          *GetChannel(void);
    DVBChannel       *GetDVBChannel(void);

    /// \brief Called by SpawnScanner to run scanning thread
    void RunScanner(void);
    /// \brief Thunk to call RunScanner from pthread
    static void *SpawnScanner(void *param);

    bool HasTimedOut(void);
    void HandleActiveScan(void);
    bool Tune(const transport_scan_items_it_t transport);
    uint InsertMultiplex(const transport_scan_items_it_t transport);
    void ScanTransport(const transport_scan_items_it_t transport);

    /// \brief Updates Transport Scan progress bar
    inline void UpdateScanPercentCompleted(void);

    bool CheckImportedList(const DTVChannelInfoList&,
                           uint mpeg_program_num,
                           QString &service_name,
                           QString &callsign,
                           QString &common_status_info);

    void IgnoreDataOnlyMsg( const QString &name, int aux_num);
    void IgnoreEmptyChanMsg(const QString &name, int aux_num);
    void IgnoreAudioOnlyMsg(const QString &name, int aux_num);
    void IgnoreEncryptedMsg(const QString &name, int aux_num);

    void HandleMPEGDBInsertion(const ScanStreamData *sd, bool wait);
    void UpdatePATinDB(int mplexid, const QString &friendlyName, int freqid,
                       const ProgramAssociationTable*, const pmt_map_t&,
                       const DTVChannelInfoList&, const QString &si_standard,
                       bool force_update);

    void UpdatePMTinDB(int sourceid,
                       int mplexid, const QString &friendlyName, int freqid,
                       int pmt_indx, const ProgramMapTable*,
                       const DTVChannelInfoList&,
                       bool force_update);

    void HandleATSCDBInsertion(const ScanStreamData *sd, bool wait);
    void UpdateVCTinDB(int mplexid, const QString &friendlyName, int freqid,
                       const VirtualChannelTable*,
                       const DTVChannelInfoList&,
                       bool force_update);

    void HandleDVBDBInsertion(const ScanStreamData *sd, bool wait);
    void UpdateSDTinDB(int mplexid,
                       const ServiceDescriptionTable*,
                       const DTVChannelInfoList&,
                       bool force_update);

    bool HandlePostInsertion(void);

    uint64_t FindBestMplexFreq(const uint64_t tuning_freq,
                               const transport_scan_items_it_t transport,
                               const uint sourceid, const uint transportid,
                               const uint networkid);


    static QString loc(const SIScan*);

  private:
    // Set in constructor
    ChannelBase      *channel;
    SignalMonitor    *signalMonitor;
    int               sourceID;
    SCANMODE          scanMode;
    uint              signalTimeout;
    uint              channelTimeout;
    QString           inputname;

    // Settable
    bool              ignoreAudioOnlyServices;
    bool              ignoreDataServices;
    bool              ignoreEncryptedServices;
    bool              forceUpdate;
    bool              renameChannels;
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
