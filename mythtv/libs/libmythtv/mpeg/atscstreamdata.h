// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson
#ifndef ATSCSTREAMDATA_H_
#define ATSCSTREAMDATA_H_

#include "mpegstreamdata.h"

class PESPacket;
class ProgramAssociationTable;
class ProgramMapTable;
class TSPacket_nonconst;
class HDTVRecorder;
class PSIPTable;
class RingBuffer;
class MasterGuideTable;
class VirtualChannelTable;
class TerrestrialVirtualChannelTable;
class CableVirtualChannelTable;
class EventInformationTable;
class ExtendedTextTable;
class SystemTimeTable;
class CensorshipTable;
class RatingRegionTable;
class DirectedChannelChangeTable;
class DirectedChannelChangeSelectionCodeTable;

class ATSCStreamData : public MPEGStreamData
{
    Q_OBJECT
  public:
    ATSCStreamData(int desiredMajorChannel, int desiredMinorChannel) :
        MPEGStreamData(-1),
        _desired_major_channel(desiredMajorChannel),
        _desired_minor_channel(desiredMinorChannel),
        _mgt_version(-1),
        _GPS_UTC_offset(13 /* leap seconds as of 2004 */)
    { ; }

    void SetChannel(int major, int minor)
    {
        _desired_major_channel = major;
        _desired_minor_channel = minor;
    }

    virtual inline void Reset(int desiredChannel = -1,
                              int desiredSubchannel = -1)
    {
        if (desiredChannel>0)
            _desired_major_channel = desiredChannel;
        if (desiredSubchannel>0)
            _desired_minor_channel = desiredSubchannel;

        MPEGStreamData::Reset(-1);
        _mgt_version = -1;
        _tvct_version.clear();
        _eit_version.clear();
        _ett_version.clear();
    }

    void SetVersionMGT(int ver) { _mgt_version = ver; }
    int VersionMGT() const { return _mgt_version; }

    void SetVersionTVCT(unsigned int pid, int version) { _tvct_version[pid]=version; }
    int VersionTVCT(unsigned int pid) const
    {
        const QMap<unsigned int, int>::const_iterator it = _tvct_version.find(pid);
        return (it==_tvct_version.end()) ? -1 : *it;
    }

    void SetVersionCVCT(unsigned int pid, int version) { _cvct_version[pid]=version; }
    int VersionCVCT(unsigned int pid) const
    {
        const QMap<unsigned int, int>::const_iterator it = _cvct_version.find(pid);
        return (it==_cvct_version.end()) ? -1 : *it;
    }

    void SetVersionEIT(unsigned int pid, int version) { _eit_version[pid]=version; }
    int VersionEIT(unsigned int pid) const
    {
        const QMap<unsigned int, int>::const_iterator it = _eit_version.find(pid);
        return (it==_eit_version.end()) ? -1 : *it;
    }

    void SetVersionETT(unsigned int pid, int version) { _ett_version[pid]=version; }
    int VersionETT(unsigned int pid) const
    {
        const QMap<unsigned int, int>::const_iterator it = _ett_version.find(pid);
        return (it==_ett_version.end()) ? -1 : *it;
    }

    void HandleTables(const TSPacket* tspacket);
    bool IsRedundant(const PSIPTable&) const;

    int DesiredMajorChannel() const { return _desired_major_channel; }
    int DesiredMinorChannel() const { return _desired_minor_channel; }

    uint GPSOffset() const { return _GPS_UTC_offset; }
  signals:
    void UpdateMGT(const MasterGuideTable*);
    void UpdateSTT(const SystemTimeTable*);
    void UpdateRRT(const RatingRegionTable*);
    void UpdateDCCT(const DirectedChannelChangeTable*);
    void UpdateDCCSCT(const DirectedChannelChangeSelectionCodeTable*);

    void UpdateVCT(uint pid, const VirtualChannelTable*);
    void UpdateTVCT(uint pid, const TerrestrialVirtualChannelTable*);
    void UpdateCVCT(uint pid, const CableVirtualChannelTable*);
    void UpdateEIT(uint pid, const EventInformationTable*);
    void UpdateETT(uint pid, const ExtendedTextTable*);

  private slots:
    void PrintMGT(const MasterGuideTable*);
    void PrintSTT(const SystemTimeTable*);
    void PrintRRT(const RatingRegionTable*);
    void PrintDCCT(const DirectedChannelChangeTable*);
    void PrintDCCSCT(const DirectedChannelChangeSelectionCodeTable*);

    void PrintTVCT(uint pid, const TerrestrialVirtualChannelTable*);
    void PrintCVCT(uint pid, const CableVirtualChannelTable*);
    void PrintEIT(uint pid, const EventInformationTable*);
    void PrintETT(uint pid, const ExtendedTextTable*);

  private:
    int _desired_major_channel;
    int _desired_minor_channel;

    int _mgt_version;
    uint _GPS_UTC_offset;
    QMap<unsigned int, int> _tvct_version;
    QMap<unsigned int, int> _cvct_version;
    QMap<unsigned int, int> _eit_version;
    QMap<unsigned int, int> _ett_version;
};

#endif
