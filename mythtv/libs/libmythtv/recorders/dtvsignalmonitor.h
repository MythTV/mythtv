// -*- Mode: c++ -*-

#ifndef DTVSIGNALMONITOR_H
#define DTVSIGNALMONITOR_H

#include <vector>

#include "mpeg/streamlisteners.h"
#include "signalmonitor.h"
#include "signalmonitorvalue.h"

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
    ~DTVSignalMonitor() override;

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
    virtual void SetRotorTarget(float /*target*/) {}
    virtual void GetRotorStatus(bool &was_moving, bool &is_moving)
        { was_moving = is_moving = false; }
    virtual void SetRotorValue(int /*val*/) {}

    void AddFlags(uint64_t _flags) override; // SignalMonitor
    void RemoveFlags(uint64_t _flags) override; // SignalMonitor

    /// Sets the MPEG stream data for DTVSignalMonitor to use,
    /// and connects the table signals to the monitor.
    virtual void SetStreamData(MPEGStreamData* data);

    /// Returns the MPEG stream data if it exists
    MPEGStreamData *GetStreamData()      { return m_streamData; }
    /// Returns the ATSC stream data if it exists
    ATSCStreamData *GetATSCStreamData();
    /// Returns the DVB stream data if it exists
    DVBStreamData *GetDVBStreamData();
    /// Returns the scan stream data if it exists
    ScanStreamData *GetScanStreamData();

    /// Returns the MPEG stream data if it exists
    const MPEGStreamData *GetStreamData() const { return m_streamData; }
    /// Returns the ATSC stream data if it exists
    const ATSCStreamData *GetATSCStreamData() const;
    /// Returns the DVB stream data if it exists
    const DVBStreamData *GetDVBStreamData() const;
    /// Returns the scan stream data if it exists
    const ScanStreamData *GetScanStreamData() const;

    bool IsAllGood(void) const override; // SignalMonitor

    // MPEG
    void HandlePAT(const ProgramAssociationTable *pat) override; // MPEGStreamListener
    void HandleCAT(const ConditionalAccessTable */*cat*/) override {}  // MPEGStreamListener
    void HandlePMT(uint program_num, const ProgramMapTable *pmt) override; // MPEGStreamListener
    void HandleEncryptionStatus(uint pnum, bool enc_status) override; // MPEGStreamListener

    // ATSC Main
    void HandleSTT(const SystemTimeTable *stt) override; // ATSCMainStreamListener
    void HandleVCT(uint /*tsid*/, const VirtualChannelTable */*vct*/) override {} // ATSCMainStreamListener
    void HandleMGT(const MasterGuideTable *mgt) override; // ATSCMainStreamListener

    // ATSC Aux
    void HandleTVCT(uint pid, const TerrestrialVirtualChannelTable *tvct) override; // ATSCAuxStreamListener
    void HandleCVCT(uint pid, const CableVirtualChannelTable *cvct) override; // ATSCAuxStreamListener
    void HandleRRT(const RatingRegionTable */*rrt*/) override {} // ATSCAuxStreamListener
    void HandleDCCT(const DirectedChannelChangeTable */*dcct*/) override {} // ATSCAuxStreamListener
    void HandleDCCSCT(
        const DirectedChannelChangeSelectionCodeTable */*dccsct*/) override {} // ATSCAuxStreamListener

    // DVB Main
    void HandleTDT(const TimeDateTable *tdt) override; // DVBMainStreamListener
    void HandleNIT(const NetworkInformationTable *nit) override; // DVBMainStreamListener
    void HandleSDT(uint tsid, const ServiceDescriptionTable *sdt) override; // DVBMainStreamListener

    void IgnoreEncrypted(bool ignore) { m_ignoreEncrypted = ignore; }

  protected:
    DTVChannel *GetDTVChannel(void);
    void UpdateMonitorValues(void);
    void UpdateListeningForEIT(void);

  protected:
    MPEGStreamData    *m_streamData         {nullptr};
    std::vector<uint>  m_eitPids;
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
    QList<uint64_t>    m_seenTableCrc;

    bool               m_ignoreEncrypted     {false};
};

#endif // DTVSIGNALMONITOR_H
