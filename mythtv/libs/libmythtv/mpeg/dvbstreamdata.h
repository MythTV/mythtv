// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson
#ifndef DVBSTREAMDATA_H_
#define DVBSTREAMDATA_H_

/// \file dvbstreamdata.h

#include "mpegstreamdata.h"
#include "mythtvexp.h"

typedef NetworkInformationTable* nit_ptr_t;
typedef NetworkInformationTable const * nit_const_ptr_t;
typedef vector<nit_const_ptr_t>  nit_const_vec_t;
typedef vector<nit_ptr_t>  nit_vec_t;
typedef QMap<uint, nit_ptr_t>    nit_cache_t; // section->sdts

typedef ServiceDescriptionTable* sdt_ptr_t;
typedef ServiceDescriptionTable const * sdt_const_ptr_t;
typedef vector<sdt_const_ptr_t>  sdt_const_vec_t;
typedef vector<sdt_ptr_t>  sdt_vec_t;

///@{
// SDT table sections are held in a cache that is indexed by
// section number within transport stream ID within
// original network ID.
///
typedef QMap<uint16_t, sdt_ptr_t>
        sdt_sections_cache_t;                  ///< Section level cache
typedef struct SdtSectionsAndStatus
{
    SdtSectionsAndStatus() { status.SetVersion(-1,0); }
    void DeepCopy(const SdtSectionsAndStatus& orig)
    {
        // Deep copy ?
    }
    sdt_sections_cache_t sections;
    TableStatus status;
    QDateTime timestamp;
    QMap<uint16_t, QByteArray> hashes;
} sdt_sections_cache_wrapper_t;
typedef QMap<uint16_t,sdt_sections_cache_wrapper_t>
        sdt_t_cache_t;                       ///< Table level cache
typedef QMap<uint16_t,sdt_t_cache_t>
        sdt_ts_cache_t;                       ///< Transport stream level cache
typedef struct SdtCache : public QMap<uint16_t,sdt_ts_cache_t>
{
	~SdtCache();
}        sdt_tsn_cache_t;                      ///< Original network ID level cache
///@}

///@{
typedef DVBEventInformationTable* eit_ptr_t;
typedef DVBEventInformationTable const * eit_const_ptr_t;
typedef vector<eit_ptr_t>  eit_vec_t;
typedef vector<eit_const_ptr_t>  eit_const_vec_t;
///@}


///@{
/// EIT table sections are held in a cache that is indexed by
/// section number within table ID within service ID within transport stream
/// ID within original network ID.
///
typedef QMap<uint16_t, eit_ptr_t>
		eit_sections_cache_t; 					///< Section level cache
typedef struct EitSectionsAndStatus
{
    EitSectionsAndStatus() { status.SetVersion(-1,0); }
    eit_sections_cache_t sections;
    TableStatus status;
    QDateTime timestamp;
    QMap<uint16_t, QByteArray> hashes;
} eit_sections_cache_wrapper_t;
typedef QMap<uint16_t, eit_sections_cache_wrapper_t>
		eit_t_cache_t;							///< Table level cache
typedef QMap<uint16_t,eit_t_cache_t>
		eit_ts_cache_t;						///< Service level cache
typedef QMap<uint16_t,eit_ts_cache_t>
		eit_tss_cache_t;						///< Transport stream level cache
typedef struct EitCache : public QMap<uint16_t,eit_tss_cache_t>
{
	~EitCache();
} eit_tssn_cache_t;						///< Original network ID level cache
///@}



typedef vector<DVBMainStreamListener*>   dvb_main_listener_vec_t;
typedef vector<DVBOtherStreamListener*>  dvb_other_listener_vec_t;
typedef vector<DVBEITStreamListener*>    dvb_eit_listener_vec_t;

typedef QMap<uint, bool>                 dvb_has_eit_t;

/** \class DVBStreamData
 *  \brief Methods for managing a DVB stream
*/
class MTV_PUBLIC DVBStreamData : virtual public MPEGStreamData
{
  public:
    DVBStreamData(uint desired_netid, uint desired_tsid,
                  int desired_program, int cardnum, bool cacheTableSections = false);
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
    void CheckStaleEIT(const DVBEventInformationTable& eit, uint onid, uint tsid, uint sid, uint tid) const;
    void CheckStaleSDT(const ServiceDescriptionTable& sdt, uint onid, uint tsid, uint tid) const;
    bool IsRedundant(uint pid, const PSIPTable&) const;
    // RFJ SDT needs ONID as well
    void ProcessSDT(sdt_const_ptr_t);

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
    void SetVersionSDT(uint onid, uint tsid, uint tid, int version, uint last_section)
    {
        _cached_sdts[onid][tsid][tid].status.SetVersion(version, last_section);
    }

    /** \fn static uint GenerateUniqueUKOriginalNetworkID(uint transport_stream_id)
     *  \brief Generates a unique original network ID in the locally allocated range
     *  using the UK multiplex identifier extracted from the transport stream ID.
     */
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

    void SetEITSectionSeen(uint original_network_id, uint transport_stream_id,
                           uint serviceid, uint tableid,
                           uint version, uint section,
                           uint segment_last_section, uint last_section);
    bool EITSectionSeen(uint original_network_id, uint transport_stream_id,
                        uint serviceid, uint tableid,
                        uint version, uint section) const;
    bool HasAllEITSections(uint original_network_id, uint transport_stream_id,
                           uint serviceid, uint tableid) const;


    void SetSDTSectionSeen(uint original_network_id, uint transport_stream_id,
                           uint tableid, uint version,
                           uint section, uint last_section);
    bool SDTSectionSeen(uint original_network_id, uint transport_stream_id,
                        uint tableid, uint version, uint section) const;
    bool HasAllSDTSections(uint original_network_id, uint transport_stream_id,
                           uint tableid) const;


    bool HasAllBATSections(uint bid) const;

    // Caching
    bool HasCachedAnyNIT(bool current = true) const;
    bool HasCachedAllNIT(bool current = true) const;
    nit_const_ptr_t GetCachedNIT(uint section_num, bool current = true) const;
    nit_const_vec_t GetCachedNIT(bool current = true) const;

    sdt_const_ptr_t GetCachedSDTSection(uint onid, uint tsid, uint tid, uint section_num) const;
    sdt_const_vec_t GetCachedSDTs() const;
    bool HasCachedAnySDTs() const;

    void AddDVBMainListener(DVBMainStreamListener*);
    void AddDVBOtherListener(DVBOtherStreamListener*);
    void AddDVBEITListener(DVBEITStreamListener*);

    void RemoveDVBMainListener(DVBMainStreamListener*);
    void RemoveDVBOtherListener(DVBOtherStreamListener*);
    void RemoveDVBEITListener(DVBEITStreamListener*);

  private:
    // Caching
    void CacheNIT(nit_ptr_t);
    void CacheSDT(sdt_ptr_t);
    void CacheEIT(eit_ptr_t);
    void ValidateEITCache();
    void ValidateSDTCache();

  protected:
    /** \fn virtual bool DeleteCachedTableSection(PSIPTable *psip) const
     *  \brief Deletes the PESPacket containing this teable section. Because
     *  the destructor is virtual any derived class oblects will be deleted as well.
     */
   virtual bool DeleteCachedTableSection(PSIPTable *psip) const;

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
    TableStatusMap            _cit_status;
    TableStatusMap            _bat_status;

    // Caching
    mutable nit_cache_t       _cached_nit;  // NIT sections cached within transport stream ID

    // Static shared caches
    static sdt_tsn_cache_t    _cached_sdts; // SDT sections cached within transport stream ID
                                            // within original network ID
    static QMutex             _cached_sdts_lock;
    static eit_tssn_cache_t   _cached_eits; // EIT sections cached within table ID within
                                            // service ID within transport stream ID within
                                            // original network ID
    static QMutex             _cached_eits_lock;

    static qint64 EIT_STALE_TIME;
    static qint64 SDT_STALE_TIME;
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
