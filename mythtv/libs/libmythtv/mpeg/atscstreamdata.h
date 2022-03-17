// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson
#ifndef ATSCSTREAMDATA_H_
#define ATSCSTREAMDATA_H_

#include "libmythtv/mythtvexp.h"

#include "mpegstreamdata.h"
#include "tablestatus.h"

using pid_tsid_vec_t     = QMap<uint, uint_vec_t>;
using tvct_ptr_t         = TerrestrialVirtualChannelTable *;
using tvct_const_ptr_t   = const TerrestrialVirtualChannelTable *;
using cvct_ptr_t         = CableVirtualChannelTable *;
using cvct_const_ptr_t   = const CableVirtualChannelTable *;
using tvct_vec_t         = std::vector<const TerrestrialVirtualChannelTable *>;
using cvct_vec_t         = std::vector<const CableVirtualChannelTable *>;
using tvct_cache_t       = QMap<uint, tvct_ptr_t>;
using cvct_cache_t       = QMap<uint, cvct_ptr_t>;
using atsc_eit_pid_map_t = QMap<uint, uint>;
using atsc_ett_pid_map_t = QMap<uint, uint>;

using atsc_main_listener_vec_t  = std::vector<ATSCMainStreamListener *>;
using scte_main_listener_vec_t  = std::vector<SCTEMainStreamListener *>;
using atsc_aux_listener_vec_t   = std::vector<ATSCAuxStreamListener *>;
using atsc_eit_listener_vec_t   = std::vector<ATSCEITStreamListener *>;
using atsc81_eit_listener_vec_t = std::vector<ATSC81EITStreamListener *>;

class MTV_PUBLIC ATSCStreamData : virtual public MPEGStreamData
{
  public:
    ATSCStreamData(int desiredMajorChannel,
                   int desiredMinorChannel,
                   int cardnum, bool cacheTables = false);
    ~ATSCStreamData() override;

    void Reset(void) override { Reset(-1, -1); } // MPEGStreamData
    void Reset(int desiredProgram) override; // MPEGStreamData
    void Reset(int desiredMajorChannel, int desiredMinorChannel);
    void SetDesiredChannel(int major, int minor);

    // Table processing
    bool HandleTables(uint pid, const PSIPTable &psip) override; // MPEGStreamData
    bool IsRedundant(uint pid, const PSIPTable &psip) const override; // MPEGStreamData

    /// Current UTC to GPS time offset in seconds
    uint GPSOffset(void) const { return m_gpsUtcOffset; }

    inline uint GetATSCMajorMinor(uint eit_sourceid) const;
    inline bool HasATSCMajorMinorMap(void) const;
    bool HasEITPIDChanges(const uint_vec_t &in_use_pid) const override; // MPEGStreamData
    bool GetEITPIDChanges(const uint_vec_t &cur_pids,
                          uint_vec_t &add_pids,
                          uint_vec_t &del_pids) const override; // MPEGStreamData

    // Table versions
    void SetVersionMGT(int version)
        {    m_mgtVersion     = version; }
    void SetVersionTVCT(uint tsid, int version)
        { m_tvctVersion[tsid] = version; }
    void SetVersionCVCT(uint tsid, int version)
        { m_cvctVersion[tsid] = version; }
    void SetVersionRRT(uint region, int version)
        { m_rrtVersion[region&0xff] = version; }

    int VersionMGT() const { return m_mgtVersion; }
    inline int VersionTVCT(uint tsid) const;
    inline int VersionCVCT(uint tsid) const;
    inline int VersionRRT(uint region) const;

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

    void ReturnCachedTVCTTables(tvct_vec_t &tvcts) const;
    void ReturnCachedCVCTTables(cvct_vec_t &cvcts) const;

    bool HasChannel(uint major, uint minor) const;

    // Single channel stuff
    int DesiredMajorChannel(void) const { return m_desiredMajorChannel; }
    int DesiredMinorChannel(void) const { return m_desiredMinorChannel; }

    void AddATSCMainListener(ATSCMainStreamListener *val);
    void AddSCTEMainListener(SCTEMainStreamListener *val);
    void AddATSCAuxListener(ATSCAuxStreamListener *val);
    void AddATSCEITListener(ATSCEITStreamListener *val);
    void AddATSC81EITListener(ATSC81EITStreamListener *val);

    void RemoveATSCMainListener(ATSCMainStreamListener *val);
    void RemoveSCTEMainListener(SCTEMainStreamListener *val);
    void RemoveATSCAuxListener(ATSCAuxStreamListener *val);
    void RemoveATSCEITListener(ATSCEITStreamListener *val);
    void RemoveATSC81EITListener(ATSC81EITStreamListener *val);

  private:
    void ProcessMGT(const MasterGuideTable *mgt);
    void ProcessVCT(uint tsid, const VirtualChannelTable *vct);
    void ProcessTVCT(uint tsid, const TerrestrialVirtualChannelTable *vct);
    void ProcessCVCT(uint tsid, const CableVirtualChannelTable *vct);

    // Caching
    void CacheMGT(MasterGuideTable *mgt);
    void CacheTVCT(uint pid, TerrestrialVirtualChannelTable *tvct);
    void CacheCVCT(uint pid, CableVirtualChannelTable *cvct);
  protected:
    bool DeleteCachedTable(const PSIPTable *psip) const override; // MPEGStreamData

  private:
    uint                      m_gpsUtcOffset { GPS_LEAP_SECONDS };
    mutable bool              m_atscEitReset { false };
    atsc_eit_pid_map_t        m_atscEitPids;
    atsc_ett_pid_map_t        m_atscEttPids;

    QMap<uint,uint>           m_sourceIdToAtscMajMin;


    // Signals
    atsc_main_listener_vec_t  m_atscMainListeners;
    scte_main_listener_vec_t  m_scteMainlisteners;
    atsc_aux_listener_vec_t   m_atscAuxListeners;
    atsc_eit_listener_vec_t   m_atscEitListeners;
    atsc81_eit_listener_vec_t m_atsc81EitListeners;

    // Table versions
    int             m_mgtVersion { -1 };
    QMap<uint, int> m_tvctVersion;
    QMap<uint, int> m_cvctVersion;
    QMap<uint, int> m_rrtVersion;
    TableStatusMap  m_eitStatus;

    // Caching
    mutable MasterGuideTable *m_cachedMgt { nullptr };
    mutable tvct_cache_t      m_cachedTvcts; // pid->tvct
    mutable cvct_cache_t      m_cachedCvcts; // pid->cvct

    // Single program variables
    int m_desiredMajorChannel;
    int m_desiredMinorChannel;
};


inline uint ATSCStreamData::GetATSCMajorMinor(uint eit_sourceid) const
{
    QMutexLocker locker(&m_listenerLock);
    return m_sourceIdToAtscMajMin[eit_sourceid];
}

inline bool ATSCStreamData::HasATSCMajorMinorMap(void) const
{
    QMutexLocker locker(&m_listenerLock);
    return !m_sourceIdToAtscMajMin.empty();
}

inline int ATSCStreamData::VersionTVCT(uint tsid) const
{
    const QMap<uint, int>::const_iterator it = m_tvctVersion.find(tsid);
    if (it == m_tvctVersion.end())
        return -1;
    return *it;
}

inline int ATSCStreamData::VersionCVCT(uint tsid) const
{
    const QMap<uint, int>::const_iterator it = m_cvctVersion.find(tsid);
    if (it == m_cvctVersion.end())
        return -1;
    return *it;
}

inline int ATSCStreamData::VersionRRT(uint region) const
{
    const QMap<uint, int>::const_iterator it = m_rrtVersion.find(region&0xff);
    if (it == m_rrtVersion.end())
        return -1;
    return *it;
}

#endif
