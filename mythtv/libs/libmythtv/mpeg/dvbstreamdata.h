// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson
#ifndef DVBSTREAMDATA_H_
#define DVBSTREAMDATA_H_

#include "mpegstreamdata.h"
#include "mythtvexp.h"
#include "tablestatus.h"

typedef NetworkInformationTable* nit_ptr_t;
typedef NetworkInformationTable const* nit_const_ptr_t;
typedef vector<const NetworkInformationTable*>  nit_vec_t;
typedef QMap<uint, nit_ptr_t>    nit_cache_t; // section->sdts

typedef ServiceDescriptionTable* sdt_ptr_t;
typedef ServiceDescriptionTable const* sdt_const_ptr_t;
typedef vector<const ServiceDescriptionTable*>  sdt_vec_t;
typedef QMap<uint, sdt_ptr_t>    sdt_cache_t; // tsid+section->sdts
typedef QMap<uint, sdt_vec_t>    sdt_map_t;   // tsid->sdts

typedef BouquetAssociationTable*  bat_ptr_t;
typedef BouquetAssociationTable const*  bat_const_ptr_t;
typedef vector<const BouquetAssociationTable*>  bat_vec_t;
typedef QMap<uint, bat_ptr_t>    bat_cache_t;  // batid+section->bats

typedef vector<DVBMainStreamListener*>   dvb_main_listener_vec_t;
typedef vector<DVBOtherStreamListener*>  dvb_other_listener_vec_t;
typedef vector<DVBEITStreamListener*>    dvb_eit_listener_vec_t;

typedef QMap<uint, bool>                 dvb_has_eit_t;

class MTV_PUBLIC DVBStreamData : virtual public MPEGStreamData
{
  public:
    DVBStreamData(uint desired_netid, uint desired_tsid,
                  int desired_program, int cardnum, bool cacheTables = false);
    virtual ~DVBStreamData();

    using MPEGStreamData::Reset;
    void Reset(void) override // MPEGStreamData
        { Reset(0, 0, -1); }
    void Reset(uint desired_netid, uint desired_tsid, int desired_serviceid);

    // DVB table monitoring
    void SetDesiredService(uint netid, uint tsid, int serviceid);
    uint DesiredNetworkID(void)   const { return _desired_netid; }
    uint DesiredTransportID(void) const { return _desired_tsid;  }

    // Table processing
    bool HandleTables(uint pid, const PSIPTable&) override; // MPEGStreamData
    bool IsRedundant(uint pid, const PSIPTable&) const override; // MPEGStreamData
    void ProcessSDT(uint tsid, const ServiceDescriptionTable*);

    // NIT for broken providers
    inline void SetRealNetworkID(int);

    // EIT info/processing
    inline void SetDishNetEIT(bool);
    inline bool HasAnyEIT(void) const;
    inline bool HasEIT(uint serviceid) const;
    bool HasEITPIDChanges(const uint_vec_t &in_use_pids) const override; // MPEGStreamData
    bool GetEITPIDChanges(const uint_vec_t &cur_pids,
                          uint_vec_t &add_pids,
                          uint_vec_t &del_pids) const override; // MPEGStreamData

    // Table versions
    void SetVersionSDT(uint tsid, int version, uint last_section)
    {
        _sdt_status.SetVersion(tsid, version, last_section);
    }

    void SetVersionSDTo(uint tsid, int version, uint last_section)
    {
        _sdto_status.SetVersion(tsid, version, last_section);
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

    void ReturnCachedSDTTables(sdt_vec_t&) const;

    bat_const_ptr_t GetCachedBAT(uint batid, uint section_num, bool current = true) const;
    bat_vec_t GetCachedBATs(bool current = true) const;

    void AddDVBMainListener(DVBMainStreamListener*);
    void AddDVBOtherListener(DVBOtherStreamListener*);
    void AddDVBEITListener(DVBEITStreamListener*);

    void RemoveDVBMainListener(DVBMainStreamListener*);
    void RemoveDVBOtherListener(DVBOtherStreamListener*);
    void RemoveDVBEITListener(DVBEITStreamListener*);

  private:
    // Caching
    void CacheNIT(NetworkInformationTable*);
    void CacheSDT(ServiceDescriptionTable*);
    void CacheBAT(BouquetAssociationTable*);

  protected:
    bool DeleteCachedTable(const PSIPTable *psip) const override; // MPEGStreamData

  private:
    /// DVB table monitoring
    uint                      _desired_netid;
    uint                      _desired_tsid;

    // Real network ID for broken providers
    int                       _dvb_real_network_id;

    /// Decode DishNet's long-term DVB EIT
    bool                      _dvb_eit_dishnet_long;
    /// Tell us if the DVB service has EIT
    dvb_has_eit_t             _dvb_has_eit;

    // Signals
    dvb_main_listener_vec_t   _dvb_main_listeners;
    dvb_other_listener_vec_t  _dvb_other_listeners;
    dvb_eit_listener_vec_t    _dvb_eit_listeners;

    // Table versions
    TableStatus               _nit_status;
    TableStatusMap            _sdt_status;
    TableStatusMap            _eit_status;
    TableStatusMap            _cit_status;

    TableStatus               _nito_status;
    TableStatusMap            _sdto_status;
    TableStatusMap            _bat_status;

    // Caching
    mutable nit_cache_t       _cached_nit;  // section -> sdt
    mutable sdt_cache_t       _cached_sdts; // tsid+section -> sdt
    mutable bat_cache_t       _cached_bats; // batid+section -> sdt
};

inline void DVBStreamData::SetDishNetEIT(bool use_dishnet_eit)
{
    QMutexLocker locker(&_listener_lock);
    _dvb_eit_dishnet_long = use_dishnet_eit;
}

inline void DVBStreamData::SetRealNetworkID(int real_network_id)
{
    QMutexLocker locker(&_listener_lock);
    _dvb_real_network_id = real_network_id;
}

inline bool DVBStreamData::HasAnyEIT(void) const
{
    QMutexLocker locker(&_listener_lock);
    return _dvb_has_eit.size();
}

inline bool DVBStreamData::HasEIT(uint serviceid) const
{
    QMutexLocker locker(&_listener_lock);

    dvb_has_eit_t::const_iterator it = _dvb_has_eit.find(serviceid);
    if (it != _dvb_has_eit.end())
        return *it;

    return false;
}

#endif // DVBSTREAMDATA_H_
