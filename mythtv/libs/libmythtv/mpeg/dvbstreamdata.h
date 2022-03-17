// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson
#ifndef DVBSTREAMDATA_H_
#define DVBSTREAMDATA_H_

#include "libmythtv/mythtvexp.h"

#include "mpegstreamdata.h"
#include "tablestatus.h"

using nit_ptr_t       = NetworkInformationTable*;
using nit_const_ptr_t = NetworkInformationTable const*;
using nit_vec_t       = std::vector<const NetworkInformationTable*>;
using nit_cache_t     = QMap<uint, nit_ptr_t>; // section->sdts

using sdt_ptr_t       = ServiceDescriptionTable*;
using sdt_const_ptr_t = ServiceDescriptionTable const*;
using sdt_vec_t       = std::vector<const ServiceDescriptionTable*>;
using sdt_cache_t     = QMap<uint, sdt_ptr_t>; // tsid+section->sdts
using sdt_map_t       = QMap<uint, sdt_vec_t>;   // tsid->sdts

using bat_ptr_t       = BouquetAssociationTable*;
using bat_const_ptr_t = BouquetAssociationTable const*;
using bat_vec_t       = std::vector<const BouquetAssociationTable*>;
using bat_cache_t     = QMap<uint, bat_ptr_t>;  // batid+section->bats

using dvb_main_listener_vec_t  = std::vector<DVBMainStreamListener*>;
using dvb_other_listener_vec_t = std::vector<DVBOtherStreamListener*>;
using dvb_eit_listener_vec_t   = std::vector<DVBEITStreamListener*>;

using dvb_has_eit_t = QMap<uint, bool>;

class MTV_PUBLIC DVBStreamData : virtual public MPEGStreamData
{
  public:
    DVBStreamData(uint desired_netid, uint desired_tsid,
                  int desired_program, int cardnum, bool cacheTables = false);
    ~DVBStreamData() override;

    using MPEGStreamData::Reset;
    void Reset(void) override // MPEGStreamData
        { Reset(0, 0, -1); }
    virtual void Reset(uint desired_netid, uint desired_tsid, int desired_serviceid);

    // DVB table monitoring
    void SetDesiredService(uint netid, uint tsid, int serviceid);
    uint DesiredNetworkID(void)   const { return m_desiredNetId; }
    uint DesiredTransportID(void) const { return m_desiredTsId;  }

    // Table processing
    bool HandleTables(uint pid, const PSIPTable &psip) override; // MPEGStreamData
    bool IsRedundant(uint pid, const PSIPTable &psip) const override; // MPEGStreamData
    void ProcessSDT(uint tsid, const ServiceDescriptionTable *sdt);

    // NIT for broken providers
    inline void SetRealNetworkID(int real_network_id);

    // EIT info/processing
    inline void SetDishNetEIT(bool use_dishnet_eit);
    inline bool HasAnyEIT(void) const;
    inline bool HasEIT(uint serviceid) const;
    bool HasEITPIDChanges(const uint_vec_t &in_use_pids) const override; // MPEGStreamData
    bool GetEITPIDChanges(const uint_vec_t &cur_pids,
                          uint_vec_t &add_pids,
                          uint_vec_t &del_pids) const override; // MPEGStreamData

    // Table versions
    void SetVersionSDT(uint tsid, int version, uint last_section)
    {
        m_sdtStatus.SetVersion(tsid, version, last_section);
    }

    void SetVersionSDTo(uint tsid, int version, uint last_section)
    {
        m_sdtoStatus.SetVersion(tsid, version, last_section);
    }

    // Sections seen
    bool HasAllNITSections(void) const;

    bool HasAllNIToSections(void) const;

    bool HasAllSDTSections(uint tsid) const;

    bool HasAllSDToSections(uint tsid) const;

    bool HasAllBATSections(uint bid) const;

    // Caching
    bool HasCachedAnyNIT(bool current = true) const;
    bool HasCachedAllNIT(bool current = true) const;
    bool HasCachedAnySDT(uint tsid, bool current = true) const;
    bool HasCachedAllSDT(uint tsid, bool current = true) const;
    bool HasCachedSDT(bool current = true) const;
    bool HasCachedAnySDTs(bool current = true) const;
    bool HasCachedAllSDTs(bool current = true) const;
    bool HasCachedAnyBAT(uint batid, bool current = true) const;
    bool HasCachedAllBAT(uint batid, bool current = true) const;
    bool HasCachedAnyBATs(bool current = true) const;
    bool HasCachedAllBATs(bool current = true) const;

    nit_const_ptr_t GetCachedNIT(uint section_num, bool current = true) const;
    nit_vec_t GetCachedNIT(bool current = true) const;
    sdt_const_ptr_t GetCachedSDT(uint tsid, uint section_num,
                           bool current = true) const;
    sdt_vec_t GetCachedSDTSections(uint tsid, bool current = true) const;
    sdt_vec_t GetCachedSDTs(bool current = true) const;

    void ReturnCachedSDTTables(sdt_vec_t &sdts) const;

    bat_const_ptr_t GetCachedBAT(uint batid, uint section_num, bool current = true) const;
    bat_vec_t GetCachedBATs(bool current = true) const;

    void AddDVBMainListener(DVBMainStreamListener *val);
    void AddDVBOtherListener(DVBOtherStreamListener *val);
    void AddDVBEITListener(DVBEITStreamListener *val);

    void RemoveDVBMainListener(DVBMainStreamListener *val);
    void RemoveDVBOtherListener(DVBOtherStreamListener *val);
    void RemoveDVBEITListener(DVBEITStreamListener *val);

  private:
    // Caching
    void CacheNIT(NetworkInformationTable *nit);
    void CacheSDT(ServiceDescriptionTable *sdt);
    void CacheBAT(BouquetAssociationTable * bat);

  protected:
    bool DeleteCachedTable(const PSIPTable *psip) const override; // MPEGStreamData

  private:
    /// DVB table monitoring
    uint                      m_desiredNetId;
    uint                      m_desiredTsId;

    // Real network ID for broken providers
    int                       m_dvbRealNetworkId { -1 };

    /// Decode DishNet's long-term DVB EIT
    bool                      m_dvbEitDishnetLong{ false};
    /// Tell us if the DVB service has EIT
    dvb_has_eit_t             m_dvbHasEit;

    // Signals
    dvb_main_listener_vec_t   m_dvbMainListeners;
    dvb_other_listener_vec_t  m_dvbOtherListeners;
    dvb_eit_listener_vec_t    m_dvbEitListeners;

    // Table versions
    TableStatus               m_nitStatus;
    TableStatusMap            m_sdtStatus;
    TableStatusMap            m_eitStatus;
    TableStatusMap            m_citStatus;

    TableStatus               m_nitoStatus;
    TableStatusMap            m_sdtoStatus;
    TableStatusMap            m_batStatus;

    // Caching
    mutable nit_cache_t       m_cachedNit;  // section -> sdt
    mutable sdt_cache_t       m_cachedSdts; // tsid+section -> sdt
    mutable bat_cache_t       m_cachedBats; // batid+section -> sdt
};

inline void DVBStreamData::SetDishNetEIT(bool use_dishnet_eit)
{
    QMutexLocker locker(&m_listenerLock);
    m_dvbEitDishnetLong = use_dishnet_eit;
}

inline void DVBStreamData::SetRealNetworkID(int real_network_id)
{
    QMutexLocker locker(&m_listenerLock);
    m_dvbRealNetworkId = real_network_id;
}

inline bool DVBStreamData::HasAnyEIT(void) const
{
    QMutexLocker locker(&m_listenerLock);
    return !m_dvbHasEit.empty();
}

inline bool DVBStreamData::HasEIT(uint serviceid) const
{
    QMutexLocker locker(&m_listenerLock);

    dvb_has_eit_t::const_iterator it = m_dvbHasEit.find(serviceid);
    if (it != m_dvbHasEit.end())
        return *it;

    return false;
}

#endif // DVBSTREAMDATA_H_
