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
typedef vector<unsigned char>           uchar_vec_t;
typedef uchar_vec_t                     sections_t;

class ATSCStreamData : public MPEGStreamData
{
    Q_OBJECT
  public:
    ATSCStreamData(int desiredMajorChannel,
                   int desiredMinorChannel,
                   bool cacheTables = false);
   ~ATSCStreamData();

    void Reset(int desiredMajorChannel = -1, int desiredMinorChannel = -1);
    void SetDesiredChannel(int major, int minor);

    // Table processing
    virtual bool HandleTables(uint pid, const PSIPTable &psip);
    virtual bool IsRedundant(uint, const PSIPTable&) const;

    /// Current UTC to GPS time offset in seconds
    uint GPSOffset() const { return _GPS_UTC_offset; }

    // Table versions
    void SetVersionMGT(int version)
        {    _mgt_version     = version; }
    void SetVersionTVCT(uint tsid, int version)
        { _tvct_version[tsid] = version; }
    void SetVersionCVCT(uint tsid, int version)
        { _cvct_version[tsid] = version; }
    void SetVersionEIT(uint pid, uint atsc_source_id, int version)
    {
        _eit_version[(pid<<16) | atsc_source_id] = version;
        _eit_section_seen.clear();
    }
    void SetEITSectionSeen(uint pid, uint atsc_source_id, uint section);

    int VersionMGT() const { return _mgt_version; }
    inline int VersionTVCT(uint tsid) const;
    inline int VersionCVCT(uint tsid) const;
    inline int VersionEIT(uint pid, uint atsc_sourceid) const;
    bool EITSectionSeen(uint pid, uint atsc_source_id, uint section) const;

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
    virtual void DeleteCachedTable(PSIPTable *psip) const;

  private:
    uint            _GPS_UTC_offset;
    int             _mgt_version;
    QMap<uint, int> _tvct_version;
    QMap<uint, int> _cvct_version;
    QMap<uint, int> _eit_version;
    QMap<uint, sections_t> _eit_section_seen;

    // Caching
    mutable MasterGuideTable *_cached_mgt;
    mutable tvct_cache_t      _cached_tvcts; // pid->tvct
    mutable cvct_cache_t      _cached_cvcts; // pid->cvct

    // Single program variables
    int _desired_major_channel;
    int _desired_minor_channel;
};

inline int ATSCStreamData::VersionTVCT(uint tsid) const
{
    const QMap<uint, int>::const_iterator it = _tvct_version.find(tsid);
    if (it == _tvct_version.end())
        return -1;
    return *it;
}

inline int ATSCStreamData::VersionCVCT(uint tsid) const
{
    const QMap<uint, int>::const_iterator it = _cvct_version.find(tsid);
    if (it == _cvct_version.end())
        return -1;
    return *it;
}

inline int ATSCStreamData::VersionEIT(uint pid, uint atsc_source_id) const
{
    uint key = (pid<<16) | atsc_source_id;
    const QMap<uint, int>::const_iterator it = _eit_version.find(key);
    if (it == _eit_version.end())
        return -1;
    return *it;
}

#endif
