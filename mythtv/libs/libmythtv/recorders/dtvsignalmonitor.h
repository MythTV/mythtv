// -*- Mode: c++ -*-

#ifndef DTVSIGNALMONITOR_H
#define DTVSIGNALMONITOR_H

#include <vector>
using namespace std;

#include "signalmonitor.h"
#include "signalmonitorvalue.h"
#include "streamlisteners.h"

class DTVChannel;

class DTVSignalMonitor : public SignalMonitor,
                         public MPEGStreamListener,
                         public ATSCMainStreamListener,
                         public ATSCAuxStreamListener,
                         public DVBMainStreamListener
{
  public:
    DTVSignalMonitor(int db_cardnum,
                     DTVChannel *_channel,
                     bool _release_stream,
                     uint64_t wait_for_mask);
    virtual ~DTVSignalMonitor();

  public:
    QStringList GetStatusList(void) const override; // SignalMonitor

    void SetChannel(int major, int minor);
    int  GetMajorChannel() const { return m_majorChannel; }
    int  GetMinorChannel() const { return m_minorChannel; }

    void SetProgramNumber(int progNum);
    int  GetProgramNumber() const { return m_programNumber; }

    void SetDVBService(uint network_id, uint transport_id, int service_id);
    uint GetTransportID(void) const { return m_transportID;   }
    uint GetNetworkID(void)   const { return m_networkID;     }
    int  GetServiceID(void)   const { return m_programNumber; }

    uint GetDetectedNetworkID(void)   const  { return m_detectedNetworkID; }
    uint GetDetectedTransportID(void) const  { return m_detectedTransportID; }

    /// Sets rotor target pos from 0.0 to 1.0
    virtual void SetRotorTarget(float) {}
    virtual void GetRotorStatus(bool &was_moving, bool &is_moving)
        { was_moving = is_moving = false; }
    virtual void SetRotorValue(int) {}

    void AddFlags(uint64_t _flags) override; // SignalMonitor
    void RemoveFlags(uint64_t _flags) override; // SignalMonitor

    /// Sets the MPEG stream data for DTVSignalMonitor to use,
    /// and connects the table signals to the monitor.
    virtual void SetStreamData(MPEGStreamData* data);

    /// Returns the MPEG stream data if it exists
    MPEGStreamData *GetStreamData()      { return m_stream_data; }
    /// Returns the ATSC stream data if it exists
    ATSCStreamData *GetATSCStreamData();
    /// Returns the DVB stream data if it exists
    DVBStreamData *GetDVBStreamData();
    /// Returns the scan stream data if it exists
    ScanStreamData *GetScanStreamData();

    /// Returns the MPEG stream data if it exists
    const MPEGStreamData *GetStreamData() const { return m_stream_data; }
    /// Returns the ATSC stream data if it exists
    const ATSCStreamData *GetATSCStreamData() const;
    /// Returns the DVB stream data if it exists
    const DVBStreamData *GetDVBStreamData() const;
    /// Returns the scan stream data if it exists
    const ScanStreamData *GetScanStreamData() const;

    bool IsAllGood(void) const override; // SignalMonitor

    // MPEG
    void HandlePAT(const ProgramAssociationTable*) override; // MPEGStreamListener
    void HandleCAT(const ConditionalAccessTable*) override {}  // MPEGStreamListener
    void HandlePMT(uint, const ProgramMapTable*) override; // MPEGStreamListener
    void HandleEncryptionStatus(uint, bool enc_status) override; // MPEGStreamListener

    // ATSC Main
    void HandleSTT(const SystemTimeTable*) override; // ATSCMainStreamListener
    void HandleVCT(uint /*tsid*/, const VirtualChannelTable*) override {} // ATSCMainStreamListener
    void HandleMGT(const MasterGuideTable*) override; // ATSCMainStreamListener

    // ATSC Aux
    void HandleTVCT(uint, const TerrestrialVirtualChannelTable*) override; // ATSCAuxStreamListener
    void HandleCVCT(uint, const CableVirtualChannelTable*) override; // ATSCAuxStreamListener
    void HandleRRT(const RatingRegionTable*) override {} // ATSCAuxStreamListener
    void HandleDCCT(const DirectedChannelChangeTable*) override {} // ATSCAuxStreamListener
    void HandleDCCSCT(
        const DirectedChannelChangeSelectionCodeTable*) override {} // ATSCAuxStreamListener

    // DVB Main
    void HandleTDT(const TimeDateTable*) override; // DVBMainStreamListener
    void HandleNIT(const NetworkInformationTable*) override; // DVBMainStreamListener
    void HandleSDT(uint, const ServiceDescriptionTable*) override; // DVBMainStreamListener

    void IgnoreEncrypted(bool ignore) { m_ignore_encrypted = ignore; }

  protected:
    DTVChannel *GetDTVChannel(void);
    void UpdateMonitorValues(void);
    void UpdateListeningForEIT(void);

  protected:
    MPEGStreamData    *m_stream_data         {nullptr};
    vector<uint>       m_eit_pids;
    SignalMonitorValue m_seenPAT;
    SignalMonitorValue m_seenPMT;
    SignalMonitorValue m_seenMGT;
    SignalMonitorValue m_seenVCT;
    SignalMonitorValue m_seenNIT;
    SignalMonitorValue m_seenSDT;
    SignalMonitorValue m_seenCrypt;
    SignalMonitorValue m_matchingPAT;
    SignalMonitorValue m_matchingPMT;
    SignalMonitorValue m_matchingMGT;
    SignalMonitorValue m_matchingVCT;
    SignalMonitorValue m_matchingNIT;
    SignalMonitorValue m_matchingSDT;
    SignalMonitorValue m_matchingCrypt;

    // ATSC tuning info
    int                m_majorChannel        {-1};
    int                m_minorChannel        {-1};
    // DVB tuning info
    uint               m_networkID           {0};
    uint               m_transportID         {0};
    // DVB scanning info
    uint               m_detectedNetworkID   {0};
    uint               m_detectedTransportID {0};
    // MPEG/DVB/ATSC tuning info
    int                m_programNumber       {-1};
    // table_id & CRC of tables already seen
    QList<uint64_t>    m_seen_table_crc;

    bool               m_ignore_encrypted    {false};
};

#endif // DTVSIGNALMONITOR_H
