#ifndef DTVSIGNALMONITOR_H
#define DTVSIGNALMONITOR_H

#include "signalmonitor.h"
#include "signalmonitorvalue.h"

class MPEGStreamData;
class ATSCStreamData;
class DVBStreamData;
class ScanStreamData;

class TSPacket;
class ProgramAssociationTable;
class ProgramMapTable;
class MasterGuideTable;
class TerrestrialVirtualChannelTable;
class CableVirtualChannelTable;
class NetworkInformationTable;
class ServiceDescriptionTable;

class DTVSignalMonitor: public SignalMonitor
{
    Q_OBJECT
  public:
    DTVSignalMonitor(int db_cardnum,
                     ChannelBase *_channel,
                     uint wait_for_mask,
                     const char *name = "DTVSignalMonitor");

    virtual QStringList GetStatusList(bool kick = true);

    void SetChannel(int major, int minor);
    int  GetMajorChannel() const { return majorChannel; }
    int  GetMinorChannel() const { return minorChannel; }

    void SetProgramNumber(int progNum);
    int  GetProgramNumber() const { return programNumber; }

    void SetFTAOnly(bool fta)    { ignoreEncrypted = fta;  }
    bool GetFTAOnly() const      { return ignoreEncrypted; }

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

  private slots:
    void SetPAT(const ProgramAssociationTable*);
    void SetPMT(uint, const ProgramMapTable*);
    void SetMGT(const MasterGuideTable*);
    void SetVCT(uint, const TerrestrialVirtualChannelTable*);
    void SetVCT(uint, const CableVirtualChannelTable*);
    void SetNIT(const NetworkInformationTable*);
    void SetSDT(uint, const ServiceDescriptionTable*);

  private:
    void UpdateMonitorValues();
    MPEGStreamData    *stream_data;
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
    int                majorChannel;
    int                minorChannel;
    int                programNumber;
    bool               ignoreEncrypted;
    QString            error;
};

#endif // DTVSIGNALMONITOR_H
