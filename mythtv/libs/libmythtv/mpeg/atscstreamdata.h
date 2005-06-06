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
    ATSCStreamData(int desiredMajorChannel,
                   int desiredMinorChannel,
                   bool cacheTables = false);

    void Reset(int desiredMajorChannel = -1, int desiredMinorChannel = -1);
    void SetDesiredChannel(int major, int minor)
    {
        _desired_major_channel = major;
        _desired_minor_channel = minor;
    }

    void SetVersionMGT(int version)             {    _mgt_version     = version; }
    void SetVersionTVCT(uint tsid, int version) { _tvct_version[tsid] = version; }
    void SetVersionCVCT(uint tsid, int version) { _cvct_version[tsid] = version; }
    void SetVersionEIT(uint pid,   int version) {   _eit_version[pid] = version; }
    void SetVersionETT(uint pid,   int version) {   _ett_version[pid] = version; }

    int VersionMGT() const                      { return _mgt_version; }
    inline int VersionTVCT(uint tsid) const;
    inline int VersionCVCT(uint tsid) const;
    inline int VersionEIT(uint pid) const;
    inline int VersionETT(uint pid) const;

    void HandleTables(const TSPacket* tspacket);
    bool IsRedundant(const PSIPTable&) const;

    uint GPSOffset() const { return _GPS_UTC_offset; }

    // Single channel stuff
    int DesiredMajorChannel() const { return _desired_major_channel; }
    int DesiredMinorChannel() const { return _desired_minor_channel; }
  signals:
    void UpdateMGT(const MasterGuideTable*);
    void UpdateSTT(const SystemTimeTable*);
    void UpdateRRT(const RatingRegionTable*);
    void UpdateDCCT(const DirectedChannelChangeTable*);
    void UpdateDCCSCT(const DirectedChannelChangeSelectionCodeTable*);

    void UpdateVCT(uint tsid,  const VirtualChannelTable*);
    void UpdateTVCT(uint tsid, const TerrestrialVirtualChannelTable*);
    void UpdateCVCT(uint tsid, const CableVirtualChannelTable*);
    void UpdateEIT(uint pid,   const EventInformationTable*);
    void UpdateETT(uint pid,   const ExtendedTextTable*);

  private slots:
    void PrintMGT(const MasterGuideTable*) const;
    void PrintSTT(const SystemTimeTable*) const;
    void PrintRRT(const RatingRegionTable*) const;
    void PrintDCCT(const DirectedChannelChangeTable*) const;
    void PrintDCCSCT(const DirectedChannelChangeSelectionCodeTable*) const;

    void PrintTVCT(uint tsid, const TerrestrialVirtualChannelTable*) const;
    void PrintCVCT(uint tsid, const CableVirtualChannelTable*) const;
    void PrintEIT(uint pid,   const EventInformationTable*) const;
    void PrintETT(uint pid,   const ExtendedTextTable*) const;

  private:
    int             _mgt_version;
    uint            _GPS_UTC_offset;
    QMap<uint, int> _tvct_version;
    QMap<uint, int> _cvct_version;
    QMap<uint, int> _eit_version;
    QMap<uint, int> _ett_version;

    // Single program variables
    int _desired_major_channel;
    int _desired_minor_channel;
};

inline int ATSCStreamData::VersionTVCT(uint tsid) const
{
    const QMap<uint, int>::const_iterator it = _tvct_version.find(tsid);
    return (it == _tvct_version.end()) ? -1 : *it;
}

inline int ATSCStreamData::VersionCVCT(uint tsid) const
{
    const QMap<uint, int>::const_iterator it = _cvct_version.find(tsid);
    return (it == _cvct_version.end()) ? -1 : *it;
}

inline int ATSCStreamData::VersionEIT(uint pid) const
{
    const QMap<uint, int>::const_iterator it = _eit_version.find(pid);
    return (it == _eit_version.end()) ? -1 : *it;
}

inline int ATSCStreamData::VersionETT(uint pid) const
{
    const QMap<uint, int>::const_iterator it = _ett_version.find(pid);
    return (it == _ett_version.end()) ? -1 : *it;
}

#endif
