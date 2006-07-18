// -*- Mode: c++ -*-

#ifndef DTVSIGNALMONITOR_H
#define DTVSIGNALMONITOR_H

#include <vector>
using namespace std;

#include "signalmonitor.h"
#include "signalmonitorvalue.h"
#include "streamlisteners.h"

class DTVSignalMonitor : public SignalMonitor,
                         public MPEGStreamListener,
                         public ATSCMainStreamListener,
                         public ATSCAuxStreamListener,
                         public DVBMainStreamListener
{
    Q_OBJECT
  public:
    DTVSignalMonitor(int db_cardnum,
                     ChannelBase *_channel,
                     uint wait_for_mask,
                     const char *name = "DTVSignalMonitor");
    ~DTVSignalMonitor();

  public slots:
    void deleteLater(void);

  public:
    virtual QStringList GetStatusList(bool kick = true);

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

    void SetFTAOnly(bool fta)    { ignoreEncrypted = fta;  }
    bool GetFTAOnly() const      { return ignoreEncrypted; }

    /// Sets rotor target pos from 0.0 to 1.0
    virtual void SetRotorTarget(float) {}

    void AddFlags(uint _flags);
    void RemoveFlags(uint _flags);

    /// Sets the MPEG stream data for DTVSignalMonitor to use,
    /// and connects the table signals to the monitor.
    void SetStreamData(MPEGStreamData* data);

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

    bool IsAllGood(void) const;

    bool WaitForLock(int timeout=-1);

    // MPEG
    void HandlePAT(const ProgramAssociationTable*);
    void HandleCAT(const ConditionalAccessTable*) {}
    void HandlePMT(uint, const ProgramMapTable*);

    // ATSC Main
    void HandleSTT(const SystemTimeTable*) {}
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
    void HandleNIT(const NetworkInformationTable*);
    void HandleSDT(uint, const ServiceDescriptionTable*);

  protected:
    void UpdateMonitorValues(void);
    void UpdateListeningForEIT(void);
    MPEGStreamData    *stream_data;
    vector<uint>       eit_pids;
    SignalMonitorValue seenPAT;
    SignalMonitorValue seenPMT;
    SignalMonitorValue seenMGT;
    SignalMonitorValue seenVCT;
    SignalMonitorValue seenNIT;
    SignalMonitorValue seenSDT;
    SignalMonitorValue matchingPAT;
    SignalMonitorValue matchingPMT;
    SignalMonitorValue matchingMGT;
    SignalMonitorValue matchingVCT;
    SignalMonitorValue matchingNIT;
    SignalMonitorValue matchingSDT;

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

    bool               ignoreEncrypted;
    QString            error;
};

#endif // DTVSIGNALMONITOR_H
