#ifndef DTVSIGNALMONITOR_H
#define DTVSIGNALMONITOR_H

#include "signalmonitor.h"
#include "signalmonitorvalue.h"

class ATSCStreamData;
class DVBStreamData;
class TSPacket;
class ProgramAssociationTable;
class ProgramMapTable;
class MasterGuideTable;
class TerrestrialVirtualChannelTable;
class CableVirtualChannelTable;

enum {
    kDTVSigMon_PATSeen    = 0x00000001, ///< maps program numbers to PMT pids
    kDTVSigMon_PMTSeen    = 0x00000002, ///< maps to audio, video and other streams pids
    kDTVSigMon_MGTSeen    = 0x00000004, ///< like NIT, but there is only one.
    kDTVSigMon_VCTSeen    = 0x00000008, ///< like SDT
    kDTVSigMon_TVCTSeen   = 0x00000010, ///< terrestrial version of VCT
    kDTVSigMon_CVCTSeen   = 0x00000020, ///< cable version of VCT
    kDTVSigMon_NITSeen    = 0x00000040, ///< like MGT, but there can be multiple tables.
    kDTVSigMon_SDTSeen    = 0x00000080, ///< like VCT

    kDTVSigMon_PATMatch   = 0x00000100,
    kDTVSigMon_PMTMatch   = 0x00000200,
    kDTVSigMon_MGTMatch   = 0x00000400,
    kDTVSigMon_VCTMatch   = 0x00000800,
    kDTVSigMon_TVCTMatch  = 0x00001000,
    kDTVSigMon_CVCTMatch  = 0x00002000,
    kDTVSigMon_NITMatch   = 0x00004000,
    kDTVSigMon_SDTMatch   = 0x00008000,

    kDTVSigMon_WaitForPAT = 0x00010000,
    kDTVSigMon_WaitForPMT = 0x00020000,
    kDTVSigMon_WaitForMGT = 0x00040000,
    kDTVSigMon_WaitForVCT = 0x00080000,
    kDTVSigMon_WaitForNIT = 0x00100000,
    kDTVSigMon_WaitForSDT = 0x00200000,
    kDTVSigMon_WaitForSig = 0x00400000,

    kDVBSigMon_WaitForSNR = 0x01000000,
    kDVBSigMon_WaitForBER = 0x02000000,
    kDVBSigMon_WaitForUB  = 0x04000000,
};

class DTVSignalMonitor: public SignalMonitor
{
    Q_OBJECT
  public:
    DTVSignalMonitor(int capturecardnum, int fd, uint wait_for_mask);

    virtual QStringList GetStatusList(bool kick = true);

    void SetChannel(int major, int minor);
    int GetMajorChannel() const { return majorChannel; }
    int GetMinorChannel() const { return minorChannel; }

    void SetProgramNumber(int progNum);
    int GetProgramNumber() const { return programNumber; }

    void AddFlags(uint _flags);
    void RemoveFlags(uint _flags);
    bool HasFlags(uint _flags) const { return (flags & _flags) == _flags; }

    /// \brief Return true if we've seen a PAT.
    bool HasPAT() const { return (bool)(kDTVSigMon_PATSeen & flags); }
    /// \brief Return true if we've seen a PMT.
    bool HasPMT() const { return (bool)(kDTVSigMon_PMTSeen & flags); }
    /// \brief Return true if we've seen an MGT.
    bool HasMGT() const { return (bool)(kDTVSigMon_MGTSeen & flags); }
    /// \brief Return true if we've seen a VCT.
    bool HasVCT() const { return (bool)(kDTVSigMon_VCTSeen & flags); }

    /// \brief Return true if we've seen a PAT with the program we are looking for.
    bool HasMatchingPAT() const { return (bool)(kDTVSigMon_PATMatch & flags); }
    /// \brief Return true if we've seen a PMT for the program we're looking for.
    bool HasMatchingPMT() const { return (bool)(kDTVSigMon_PMTMatch & flags); }
    /// \brief Return true if we've seen a MGT with the subchannel we're looking for.
    bool HasMatchingMGT() const { return (bool)(kDTVSigMon_MGTMatch & flags); }
    /// \brief Return true if we've seen a VCT for the subchannel we're looking for.
    bool HasMatchingVCT() const { return (bool)(kDTVSigMon_VCTMatch & flags); }

    bool ProcessTSPacket(const TSPacket& tspacket);

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

  private slots:
    void SetPAT(const ProgramAssociationTable* pat);
    void SetPMT(const ProgramMapTable* pmt);
    void SetMGT(const MasterGuideTable*);
    void SetVCT(const TerrestrialVirtualChannelTable*);
    void SetVCT(const CableVirtualChannelTable*);

  private:
    void UpdateMonitorValues();
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
    uint               flags;
    int                majorChannel;
    int                minorChannel;
    int                programNumber;
};

#endif // DTVSIGNALMONITOR_H
