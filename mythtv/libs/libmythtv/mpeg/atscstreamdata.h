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

typedef vector<uint>                    uint_vec_t;
typedef QMap<uint, uint_vec_t>          pid_tsid_vec_t;
typedef TerrestrialVirtualChannelTable* tvct_ptr_t;
typedef CableVirtualChannelTable*       cvct_ptr_t;
typedef vector<const TerrestrialVirtualChannelTable*> tvct_vec_t;
typedef vector<const CableVirtualChannelTable*>       cvct_vec_t;
typedef QMap<uint, tvct_ptr_t>          tvct_cache_t;
typedef QMap<uint, cvct_ptr_t>          cvct_cache_t;

class ATSCStreamData : public MPEGStreamData
{
    Q_OBJECT
  public:
    ATSCStreamData(int desiredMajorChannel,
                   int desiredMinorChannel,
                   bool cacheTables = false);

    void Reset(int desiredMajorChannel = -1, int desiredMinorChannel = -1);
    void SetDesiredChannel(int major, int minor);

    // Table processing
    virtual bool HandleTables(uint pid, const PSIPTable &psip);
    bool IsRedundant(const PSIPTable&) const;

    /// Current UTC to GPS time offset in seconds
    uint GPSOffset() const { return _GPS_UTC_offset; }

    // Table versions
    void SetVersionMGT(int version)
        {    _mgt_version     = version; }
    void SetVersionTVCT(uint tsid, int version)
        { _tvct_version[tsid] = version; }
    void SetVersionCVCT(uint tsid, int version)
        { _cvct_version[tsid] = version; }
    void SetVersionEIT(uint pid,   int version)
        {   _eit_version[pid] = version; }
    void SetVersionETT(uint pid,   int version)
        {   _ett_version[pid] = version; }

    int VersionMGT() const { return _mgt_version; }
    inline int VersionTVCT(uint tsid) const;
    inline int VersionCVCT(uint tsid) const;
    inline int VersionEIT(uint pid) const;
    inline int VersionETT(uint pid) const;

    // Caching
    bool HasCachedMGT(bool current = true) const;
    bool HasCachedTVCT(uint pid, bool current = true) const;
    bool HasCachedCVCT(uint pid, bool current = true) const;
    bool HasCachedAllTVCTs(bool current = true) const;
    bool HasCachedAllCVCTs(bool current = true) const;
    bool HasCachedAllVCTs(bool current = true) const 
        { return HasCachedAllTVCTs(current) && HasCachedAllCVCTs(current); }

    const MasterGuideTable *GetCachedMGT(bool current = true) const;
    const tvct_ptr_t        GetCachedTVCT(uint pid, bool current = true) const;
    const cvct_ptr_t        GetCachedCVCT(uint pid, bool current = true) const;

    tvct_vec_t GetAllCachedTVCTs(bool current = true) const;
    cvct_vec_t GetAllCachedCVCTs(bool current = true) const;

    void ReturnCachedTVCTTables(tvct_vec_t&) const;
    void ReturnCachedCVCTTables(cvct_vec_t&) const;

    // Single channel stuff
    int DesiredMajorChannel(void) const { return _desired_major_channel; }
    int DesiredMinorChannel(void) const { return _desired_minor_channel; }
  signals:
    void UpdateMGT(   const MasterGuideTable*);
    void UpdateSTT(   const SystemTimeTable*);
    void UpdateRRT(   const RatingRegionTable*);
    void UpdateDCCT(  const DirectedChannelChangeTable*);
    void UpdateDCCSCT(const DirectedChannelChangeSelectionCodeTable*);

    void UpdateVCT( uint pid, const VirtualChannelTable*);
    void UpdateTVCT(uint pid, const TerrestrialVirtualChannelTable*);
    void UpdateCVCT(uint pid, const CableVirtualChannelTable*);
    void UpdateEIT( uint pid, const EventInformationTable*);
    void UpdateETT( uint pid, const ExtendedTextTable*);

  private slots:
    void PrintMGT(   const MasterGuideTable*) const;
    void PrintSTT(   const SystemTimeTable*) const;
    void PrintRRT(   const RatingRegionTable*) const;
    void PrintDCCT(  const DirectedChannelChangeTable*) const;
    void PrintDCCSCT(const DirectedChannelChangeSelectionCodeTable*) const;

    void PrintTVCT(uint pid, const TerrestrialVirtualChannelTable*) const;
    void PrintCVCT(uint pid, const CableVirtualChannelTable*) const;
    void PrintEIT( uint pid, const EventInformationTable*) const;
    void PrintETT( uint pid, const ExtendedTextTable*) const;

  private:
    // Caching
    void CacheMGT(MasterGuideTable*);
    void CacheTVCT(uint pid, TerrestrialVirtualChannelTable*);
    void CacheCVCT(uint pid, CableVirtualChannelTable*);

  private:
    uint            _GPS_UTC_offset;
    int             _mgt_version;
    QMap<uint, int> _tvct_version;
    QMap<uint, int> _cvct_version;
    QMap<uint, int> _eit_version;
    QMap<uint, int> _ett_version;

    // Caching
    MasterGuideTable *_cached_mgt;
    tvct_cache_t      _cached_tvcts; // pid->tvct
    cvct_cache_t      _cached_cvcts; // pid->cvct
    pid_tsid_vec_t    _cached_pid_tsids;

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
