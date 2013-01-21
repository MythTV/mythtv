// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson
#ifndef ATSCSTREAMDATA_H_
#define ATSCSTREAMDATA_H_

#include "mpegstreamdata.h"
#include "mythtvexp.h"

typedef QMap<uint, uint_vec_t>          pid_tsid_vec_t;
typedef TerrestrialVirtualChannelTable* tvct_ptr_t;
typedef TerrestrialVirtualChannelTable const* tvct_const_ptr_t;
typedef CableVirtualChannelTable*       cvct_ptr_t;
typedef CableVirtualChannelTable const* cvct_const_ptr_t;
typedef vector<const TerrestrialVirtualChannelTable*> tvct_vec_t;
typedef vector<const CableVirtualChannelTable*>       cvct_vec_t;
typedef QMap<uint, tvct_ptr_t>          tvct_cache_t;
typedef QMap<uint, cvct_ptr_t>          cvct_cache_t;
typedef QMap<uint, uint>                atsc_eit_pid_map_t;
typedef QMap<uint, uint>                atsc_ett_pid_map_t;

typedef vector<ATSCMainStreamListener*> atsc_main_listener_vec_t;
typedef vector<SCTEMainStreamListener*> scte_main_listener_vec_t;
typedef vector<ATSCAuxStreamListener*>  atsc_aux_listener_vec_t;
typedef vector<ATSCEITStreamListener*>  atsc_eit_listener_vec_t;
typedef vector<ATSC81EITStreamListener*> atsc81_eit_listener_vec_t;

class MTV_PUBLIC ATSCStreamData : virtual public MPEGStreamData
{
  public:
    ATSCStreamData(int desiredMajorChannel,
                   int desiredMinorChannel,
                   bool cacheTables = false);
    virtual ~ATSCStreamData();

    virtual void Reset(void) { ResetATSC(-1, -1); }
    virtual void ResetMPEG(int desiredProgram);
    virtual void ResetATSC(int desiredMajorChannel, int desiredMinorChannel);
    void SetDesiredChannel(int major, int minor);

    // Table processing
    virtual bool HandleTables(uint pid, const PSIPTable &psip);
    virtual bool IsRedundant(uint, const PSIPTable&) const;

    /// Current UTC to GPS time offset in seconds
    uint GPSOffset(void) const { return _GPS_UTC_offset; }

    inline uint GetATSCMajorMinor(uint eit_sourceid) const;
    inline bool HasATSCMajorMinorMap(void) const;
    bool HasEITPIDChanges(const uint_vec_t &in_use_pid) const;
    bool GetEITPIDChanges(const uint_vec_t &in_use_pid,
                          uint_vec_t &pids_to_add,
                          uint_vec_t &pids_to_del) const;

    // Table versions
    void SetVersionMGT(int version)
        {    _mgt_version     = version; }
    void SetVersionTVCT(uint tsid, int version)
        { _tvct_version[tsid] = version; }
    void SetVersionCVCT(uint tsid, int version)
        { _cvct_version[tsid] = version; }
    void SetVersionRRT(uint region, int version)
        { _rrt_version[region&0xff] = version; }
    void SetVersionEIT(uint pid, uint atsc_source_id, int version)
    {
        if (VersionEIT(pid, atsc_source_id) == version)
            return;
        uint key = (pid<<16) | atsc_source_id;
        _eit_version[key] = version;
        _eit_section_seen[key].clear();
        _eit_section_seen[key].resize(32, 0);
    }
    void SetEITSectionSeen(uint pid, uint atsc_source_id, uint section);

    int VersionMGT() const { return _mgt_version; }
    inline int VersionTVCT(uint tsid) const;
    inline int VersionCVCT(uint tsid) const;
    inline int VersionRRT(uint region) const;
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

    bool HasCachedAnyTVCTs(bool current = true) const;
    bool HasCachedAnyCVCTs(bool current = true) const;
    bool HasCachedAnyVCTs(bool current = true) const
        { return HasCachedAnyTVCTs(current) || HasCachedAnyCVCTs(current); }

    const MasterGuideTable *GetCachedMGT(bool current = true) const;
    tvct_const_ptr_t GetCachedTVCT(uint pid, bool current = true) const;
    cvct_const_ptr_t GetCachedCVCT(uint pid, bool current = true) const;

    tvct_vec_t GetCachedTVCTs(bool current = true) const;
    cvct_vec_t GetCachedCVCTs(bool current = true) const;

    void ReturnCachedTVCTTables(tvct_vec_t&) const;
    void ReturnCachedCVCTTables(cvct_vec_t&) const;

    bool HasChannel(uint major, uint minor) const;

    // Single channel stuff
    int DesiredMajorChannel(void) const { return _desired_major_channel; }
    int DesiredMinorChannel(void) const { return _desired_minor_channel; }

    void AddATSCMainListener(ATSCMainStreamListener*);
    void AddSCTEMainListener(SCTEMainStreamListener*);
    void AddATSCAuxListener(ATSCAuxStreamListener*);
    void AddATSCEITListener(ATSCEITStreamListener*);
    void AddATSC81EITListener(ATSC81EITStreamListener*);

    void RemoveATSCMainListener(ATSCMainStreamListener*);
    void RemoveSCTEMainListener(SCTEMainStreamListener*);
    void RemoveATSCAuxListener(ATSCAuxStreamListener*);
    void RemoveATSCEITListener(ATSCEITStreamListener*);
    void RemoveATSC81EITListener(ATSC81EITStreamListener*);

  private:
    void ProcessMGT(const MasterGuideTable*);
    void ProcessVCT(uint tsid, const VirtualChannelTable*);
    void ProcessTVCT(uint tsid, const TerrestrialVirtualChannelTable*);
    void ProcessCVCT(uint tsid, const CableVirtualChannelTable*);

    // Caching
    void CacheMGT(MasterGuideTable*);
    void CacheTVCT(uint pid, TerrestrialVirtualChannelTable*);
    void CacheCVCT(uint pid, CableVirtualChannelTable*);
  protected:
    virtual bool DeleteCachedTable(PSIPTable *psip) const;

  private:
    uint                      _GPS_UTC_offset;
    mutable bool              _atsc_eit_reset;
    atsc_eit_pid_map_t        _atsc_eit_pids;
    atsc_ett_pid_map_t        _atsc_ett_pids;

    QMap<uint,uint>           _sourceid_to_atsc_maj_min;


    // Signals
    atsc_main_listener_vec_t  _atsc_main_listeners;
    scte_main_listener_vec_t  _scte_main_listeners;
    atsc_aux_listener_vec_t   _atsc_aux_listeners;
    atsc_eit_listener_vec_t   _atsc_eit_listeners;
    atsc81_eit_listener_vec_t _atsc81_eit_listeners;

    // Table versions
    int             _mgt_version;
    QMap<uint, int> _tvct_version;
    QMap<uint, int> _cvct_version;
    QMap<uint, int> _rrt_version;
    QMap<uint, int> _eit_version;
    sections_map_t  _eit_section_seen;

    // Caching
    mutable MasterGuideTable *_cached_mgt;
    mutable tvct_cache_t      _cached_tvcts; // pid->tvct
    mutable cvct_cache_t      _cached_cvcts; // pid->cvct

    // Single program variables
    int _desired_major_channel;
    int _desired_minor_channel;
};


inline uint ATSCStreamData::GetATSCMajorMinor(uint eit_sourceid) const
{
    QMutexLocker locker(&_listener_lock);
    return _sourceid_to_atsc_maj_min[eit_sourceid];
}

inline bool ATSCStreamData::HasATSCMajorMinorMap(void) const
{
    QMutexLocker locker(&_listener_lock);
    return !_sourceid_to_atsc_maj_min.empty();
}

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

inline int ATSCStreamData::VersionRRT(uint region) const
{
    const QMap<uint, int>::const_iterator it = _rrt_version.find(region&0xff);
    if (it == _rrt_version.end())
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
