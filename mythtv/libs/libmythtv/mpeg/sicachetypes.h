// -*- Mode: c++ -*-
#ifndef SICACHETYPES_H_
#define SICACHETYPES_H_

#include "tablestatus.h"

/// \file sicachetypes.h
class NetworkInformationTable;
class ServiceDescriptionTableSection;
class DVBEventInformationTableSection;
class DVBMainStreamListener;
class DVBOtherStreamListener;
class DVBEITStreamListener;

typedef NetworkInformationTable* nit_ptr_t;
typedef NetworkInformationTable const * nit_const_ptr_t;
typedef vector<nit_const_ptr_t>  nit_const_vec_t;
typedef vector<nit_ptr_t>  nit_vec_t;
typedef QMap<uint, nit_ptr_t>    nit_cache_t; // section->sdts

typedef ServiceDescriptionTableSection* sdt_section_ptr_t;
typedef ServiceDescriptionTableSection const * sdt_section_const_ptr_t;
typedef vector<sdt_section_const_ptr_t>  sdt_section_const_vec_t;
typedef vector<sdt_section_ptr_t>  sdt_section_vec_t;

///@{
// SDT table sections are held in a cache that is indexed by
// section number within transport stream ID within
// original network ID.
///
typedef QMap<uint16_t, sdt_section_ptr_t>
        sdt_sections_cache_t;					///< Section level cache
typedef QMap<uint16_t, sdt_section_ptr_t> const
		sdt_sections_cache_const_t; 			///< Section level cache const
typedef struct SdtSectionsAndStatus
{
    SdtSectionsAndStatus() { status.SetVersion(-1,0); }
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
typedef DVBEventInformationTableSection* eit_section_ptr_t;
typedef DVBEventInformationTableSection const * eit_section_const_ptr_t;
typedef vector<eit_section_ptr_t>  eit_section_vec_t;
typedef vector<eit_section_const_ptr_t>  eit_section_const_vec_t;
///@}


///@{
/// EIT table sections are held in a cache that is indexed by
/// section number within table ID within service ID within transport stream
/// ID within original network ID.
///
typedef QMap<uint16_t, eit_section_ptr_t>
		eit_sections_cache_t; 					///< Section level cache
typedef QMap<uint16_t, eit_section_ptr_t> const
		eit_sections_cache_const_t; 					///< Section level cache
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

#endif // SICACHETYPES_H_

