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
                     uint64_t wait_for_mask);
    virtual ~DTVSignalMonitor();

  public:
    virtual QStringList GetStatusList(void) const;

    void SetChannel(int major, int minor);
    int  GetMajorChannel() const { return majorChannel; }
    int  GetMinorChannel() const { return minorChannel; }

    void SetProgramNumber(int progNum);
    int  GetProgramNumber() const { return programNumber; }

    void SetDVBService(uint network_id, uint transport_id, int service_id);
    uint GetTransportID(void) const { return transportID;   }
    uint GetNetworkID(void)   const { return networkID;     }
    int  GetServiceID(void)   const { return programNumber; }

    uint GetDetectedNetworkID(void)   const  { return detectedNetworkID; }
    uint GetDetectedTransportID(void) const  { return detectedTransportID; }

    /// Sets rotor target pos from 0.0 to 1.0
    virtual void SetRotorTarget(float) {}
    virtual void GetRotorStatus(bool &was_moving, bool &is_moving)
        { was_moving = is_moving = false; }
    virtual void SetRotorValue(int) {}

    virtual void AddFlags(uint64_t _flags);
    virtual void RemoveFlags(uint64_t _flags);

    /// Sets the MPEG stream data for DTVSignalMonitor to use,
    /// and connects the table signals to the monitor.
    virtual void SetStreamData(MPEGStreamData* data);

    /// Returns the MPEG stream data if it exists
    MPEGStreamData *GetStreamData()      { return stream_data; }
    /// Returns the ATSC stream data if it exists
    ATSCStreamData *GetATSCStreamData();
    /// Returns the DVB stream data if it exists
    DVBStreamData *GetDVBStreamData();
    /// Returns the scan stream data if it exists
    ScanStreamData *GetScanStreamData();

    /// Returns the MPEG stream data if it exists
    const MPEGStreamData *GetStreamData() const { return stream_data; }
    /// Returns the ATSC stream data if it exists
    const ATSCStreamData *GetATSCStreamData() const;
    /// Returns the DVB stream data if it exists
    const DVBStreamData *GetDVBStreamData() const;
    /// Returns the scan stream data if it exists
    const ScanStreamData *GetScanStreamData() const;

    virtual bool IsAllGood(void) const;

    // MPEG
    void HandlePAT(const ProgramAssociationTable*);
    void HandleCAT(const ConditionalAccessTable*) {}
    void HandlePMT(uint, const ProgramMapTable*);
    void HandleEncryptionStatus(uint, bool enc_status);

    // ATSC Main
    void HandleSTT(const SystemTimeTable*);
    void HandleVCT(uint /*tsid*/, const VirtualChannelTable*) {}
    void HandleMGT(const MasterGuideTable*);

    // ATSC Aux
    void HandleTVCT(uint, const TerrestrialVirtualChannelTable*);
    void HandleCVCT(uint, const CableVirtualChannelTable*);
    void HandleRRT(const RatingRegionTable*) {}
    void HandleDCCT(const DirectedChannelChangeTable*) {}
    void HandleDCCSCT(
        const DirectedChannelChangeSelectionCodeTable*) {}

    // DVB Main
    void HandleTDT(const TimeDateTable*);
    void HandleNIT(const NetworkInformationTable*);
    void HandleSDT(uint, const ServiceDescriptionTable*);

    void IgnoreEncrypted(bool ignore) { ignore_encrypted = ignore; }

  protected:
    DTVChannel *GetDTVChannel(void);
    void UpdateMonitorValues(void);
    void UpdateListeningForEIT(void);

  protected:
    MPEGStreamData    *stream_data;
    vector<uint>       eit_pids;
    SignalMonitorValue seenPAT;
    SignalMonitorValue seenPMT;
    SignalMonitorValue seenMGT;
    SignalMonitorValue seenVCT;
    SignalMonitorValue seenNIT;
    SignalMonitorValue seenSDT;
    SignalMonitorValue seenCrypt;
    SignalMonitorValue matchingPAT;
    SignalMonitorValue matchingPMT;
    SignalMonitorValue matchingMGT;
    SignalMonitorValue matchingVCT;
    SignalMonitorValue matchingNIT;
    SignalMonitorValue matchingSDT;
    SignalMonitorValue matchingCrypt;

    // ATSC tuning info
    int                majorChannel;
    int                minorChannel;
    // DVB tuning info
    uint               networkID;
    uint               transportID;
    // DVB scanning info
    uint               detectedNetworkID;
    uint               detectedTransportID;
    // MPEG/DVB/ATSC tuning info
    int                programNumber;
    // table_id & CRC of tables already seen
    QList<uint64_t>    seen_table_crc;

    bool ignore_encrypted;
};

#endif // DTVSIGNALMONITOR_H
