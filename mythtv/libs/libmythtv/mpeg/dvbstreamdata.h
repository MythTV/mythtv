// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson
#ifndef DVBSTREAMDATA_H_
#define DVBSTREAMDATA_H_

#include "mpegstreamdata.h"
#include "mythtvexp.h"

typedef NetworkInformationTable* nit_ptr_t;
typedef NetworkInformationTable const* nit_const_ptr_t;
typedef vector<const NetworkInformationTable*>  nit_vec_t;
typedef QMap<uint, nit_ptr_t>    nit_cache_t; // section->sdts

typedef ServiceDescriptionTable* sdt_ptr_t;
typedef ServiceDescriptionTable const* sdt_const_ptr_t;
typedef vector<const ServiceDescriptionTable*>  sdt_vec_t;
typedef QMap<uint, sdt_ptr_t>    sdt_cache_t; // tsid+section->sdts
typedef QMap<uint, sdt_vec_t>    sdt_map_t;   // tsid->sdts

typedef EventInformationTable* eit_ptr_t;
typedef EventInformationTable const* eit_const_ptr_t;
typedef vector<const EventInformationTable*>  eit_vec_t;
typedef QMap<uint64_t, eit_ptr_t>    eit_cache_t;

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
    void Reset(void) { Reset(0, 0, -1); }
    void Reset(uint desired_netid, uint desired_tsid, int desired_sid);

    // DVB table monitoring
    void SetDesiredService(uint netid, uint tsid, int serviceid);
    uint DesiredNetworkID(void)   const { return _desired_netid; }
    uint DesiredTransportID(void) const { return _desired_tsid;  }

    // Table processing
    bool HandleTables(uint pid, const PSIPTable&);
    bool IsRedundant(uint pid, const PSIPTable&) const;
    void ProcessSDT(uint tsid, const ServiceDescriptionTable*);

    // NIT for broken providers
    inline void SetRealNetworkID(int);

    // EIT info/processing
    inline void SetDishNetEIT(bool);
    inline bool HasAnyEIT(void) const;
    inline bool HasEIT(uint serviceid) const;
    bool HasEITPIDChanges(const uint_vec_t &in_use_pids) const;
    bool GetEITPIDChanges(const uint_vec_t &in_use_pids,
                          uint_vec_t &add_pids,
                          uint_vec_t &del_pids) const;

    // Table versions
    void SetVersionSDT(uint tsid, int version, uint last_section)
    {
        _sdt_status.SetVersion(tsid, version, last_section);
    }

    static uint GenerateUniqueUKOriginalNetworkID(uint transport_stream_id)
    {
        // Convert United Kingdom DTT id to temporary temporary local range
        uint mux_id = (transport_stream_id & 0x3f000) >> 12;
        if (mux_id > 0xb)
            mux_id = 0;
        if ((mux_id == 0x7) && ((transport_stream_id & 0xfff) < 0x3ff))
            mux_id = 0xc;
        // Shift id to private temporary use range
        return 0xff01  + mux_id;
    }

   // Sections seen
    bool HasAllNITSections(void) const;

    bool HasAllNIToSections(void) const;

    bool HasAllSDTSections(uint tsid) const;

    bool HasAllSDToSections(uint tsid) const;

    void SetEITSectionSeen(uint original_network_id, uint transport_stream_id,
                           uint serviceid, uint tableid,
                           uint version, uint section,
                           uint segment_last_section, uint last_section);
    bool EITSectionSeen(uint original_network_id, uint transport_stream_id,
                        uint serviceid, uint tableid,
                        uint version, uint section) const;
    bool HasAllEITSections(uint original_network_id, uint transport_stream_id,
                           uint serviceid, uint tableid) const;

    bool HasAllBATSections(uint bid) const;

    // Caching
    bool HasCachedAnyNIT(bool current = true) const;
    bool HasCachedAllNIT(bool current = true) const;
    bool HasCachedAnySDT(uint tsid, bool current = true) const;
    bool HasCachedAllSDT(uint tsid, bool current = true) const;
    bool HasCachedSDT(bool current = true) const;
    bool HasCachedAnySDTs(bool current = true) const;
    bool HasCachedAllSDTs(bool current = true) const;

    nit_const_ptr_t GetCachedNIT(uint section_num, bool current = true) const;
    nit_vec_t GetCachedNIT(bool current = true) const;
    sdt_const_ptr_t GetCachedSDT(uint tsid, uint section_num,
                           bool current = true) const;
    sdt_vec_t GetCachedSDTs(bool current = true) const;

    void ReturnCachedSDTTables(sdt_vec_t&) const;

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
  protected:
    virtual bool DeleteCachedTable(PSIPTable *psip) const;

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
    TableStatus               _nito_status;
    TableStatusMap            _sdt_status;
    TableStatusMap            _cit_status;
    TableStatusMap            _sdto_status;
    TableStatusMap            _bat_status;
    TableStatusMap            _eit_status;

    //QMap<uint64_t, int>       _eit_version;
    //QMap<uint64_t, sections_t> _eit_section_seen;
    // Premiere private ContentInformationTable

    // Caching
    mutable nit_cache_t       _cached_nit;  // section -> sdt
    mutable sdt_cache_t       _cached_sdts; // tsid+section -> sdt
    mutable eit_cache_t       _cached_eits;
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
