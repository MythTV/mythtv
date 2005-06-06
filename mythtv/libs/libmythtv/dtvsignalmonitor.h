#ifndef DTVSIGNALMONITOR_H
#define DTVSIGNALMONITOR_H

#include "signalmonitor.h"
#include "signalmonitorvalue.h"

class MPEGStreamData;
class ATSCStreamData;
class DVBStreamData;
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
    DTVSignalMonitor(int capturecardnum, int fd, uint wait_for_mask);

    virtual QStringList GetStatusList(bool kick = true);

    void SetChannel(int major, int minor);
    int  GetMajorChannel() const { return majorChannel; }
    int  GetMinorChannel() const { return minorChannel; }

    void SetProgramNumber(int progNum);
    int  GetProgramNumber() const { return programNumber; }

    void AddFlags(uint _flags);
    void RemoveFlags(uint _flags);

    // ATSC
    void SetStreamData(ATSCStreamData* data);
    /// Returns the ATSC stream data if it exists
    ATSCStreamData *GetATSCStreamData() { return atsc_stream_data; }
    /// Returns the ATSC stream data if it exists
    const ATSCStreamData *GetATSCStreamData() const { return atsc_stream_data; }

    // DVB
    void SetStreamData(DVBStreamData* data);
    /// Returns the DVB stream data if it exists
    DVBStreamData *GetDVBStreamData() { return dvb_stream_data; }
    /// Returns the DVB stream data if it exists
    const DVBStreamData *GetDVBStreamData() const { return dvb_stream_data; }

    // Generic
    MPEGStreamData *GetStreamData() { return stream_data; }
    const MPEGStreamData *GetStreamData() const { return stream_data; }

  private slots:
    void SetPAT(const ProgramAssociationTable*);
    void SetPMT(uint, const ProgramMapTable*);
    void SetMGT(const MasterGuideTable*);
    void SetVCT(uint, const TerrestrialVirtualChannelTable*);
    void SetVCT(uint, const CableVirtualChannelTable*);
    void SetNIT(uint, const NetworkInformationTable*);
    void SetSDT(uint, const ServiceDescriptionTable*);

  private:
    void UpdateMonitorValues();
    MPEGStreamData    *stream_data;
    ATSCStreamData    *atsc_stream_data;
    DVBStreamData     *dvb_stream_data;
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
};

#endif // DTVSIGNALMONITOR_H
