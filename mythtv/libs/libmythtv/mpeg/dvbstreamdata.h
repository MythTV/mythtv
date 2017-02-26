// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson
#ifndef DVBSTREAMDATA_H_
#define DVBSTREAMDATA_H_

/// \file dvbstreamdata.h

#include "mpegstreamdata.h"
#include "tablestatus.h"
#include "qdatetime.h"

#include "sicachetypes.h"

class DVBMainStreamListener;
class DVBOtherStreamListener;
class DVBEITStreamListener;

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

    // Table processing
    bool HandleTables(uint pid, const PSIPTable&);
    void CheckStaleEIT(const DVBEventInformationTableSection& eit, uint onid, uint tsid, uint sid, uint tid) const;
    void CheckStaleSDT(const ServiceDescriptionTableSection& sdt, uint onid, uint tsid, uint tid) const;
    bool IsRedundant(uint pid, const PSIPTable&);
    void ProcessSDTSection(sdt_section_ptr_t);

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
    /** \fn static uint InternalOriginalNetworkID(uint transport_stream_id)
     *  \brief Generates a unique original network ID in the locally allocated range
     *  at the moment this only applies to UK onids.
     */
    static uint InternalOriginalNetworkID(uint onid, uint tsid)
    {
        switch (onid)
        {
        case 0x233a:
        {
			// Convert United Kingdom DTT id to temporary temporary local range
			uint mux_id = (tsid & 0x3f000) >> 12;
			if (mux_id > 0xb)
				mux_id = 0;
			if ((mux_id == 0x7) && ((tsid & 0xfff) < 0x3ff))
				mux_id = 0xc;
			// Shift id to private temporary use range
			return 0xff01  + mux_id;
        }

        default:
        	return onid;
        }
    }

   // Sections seen
    bool HasAllNITSections(void) const;

    bool HasAllNIToSections(void) const;

    void SetEITSectionSeen(DVBEventInformationTableSection& eit_section);
    bool EITSectionSeen(const DVBEventInformationTableSection& eit_section) const;
    bool HasAllEITSections(const DVBEventInformationTableSection& eit_section) const;


    void SetSDTSectionSeen(ServiceDescriptionTableSection& sdt_section);
    bool SDTSectionSeen(const ServiceDescriptionTableSection& sdt_section) const;
    bool HasAllSDTSections(const ServiceDescriptionTableSection& sdt_section) const;


    bool HasAllBATSections(uint bid) const;

    // Caching
    bool HasCachedAnyNIT(bool current = true) const;
    bool HasCachedAllNIT(bool current = true) const;
    nit_const_ptr_t GetCachedNIT(uint section_num, bool current = true) const;
    nit_const_vec_t GetCachedNIT(bool current = true) const;

    void AddDVBMainListener(DVBMainStreamListener*);
    void AddDVBOtherListener(DVBOtherStreamListener*);
    void AddDVBEITListener(DVBEITStreamListener*);

    void RemoveDVBMainListener(DVBMainStreamListener*);
    void RemoveDVBOtherListener(DVBOtherStreamListener*);
    void RemoveDVBEITListener(DVBEITStreamListener*);
    static void LogSICache();

    bool HasCurrentTSID(uint& onid, uint& tsid)
    {
    	if ((_current_onid < 0) || (_current_tsid < 0))
    		return false;
    	else
    	{
    		onid = _current_onid;
    		tsid = _current_tsid;
    		return true;
    	}
    }

  private:
    // Caching
    void CacheNIT(nit_ptr_t);

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

    uint                      _current_onid;
    uint                      _current_tsid;

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
