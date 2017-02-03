// -*- Mode: c++ -*-
#ifndef SICACHETYPES_H_
#define SICACHETYPES_H_

#include "tablestatus.h"

/// \file sicachetypes.h
/// \brief Externally visible types for the DVB System Information caches
///
///The ETSI DVB standard defines a subtable as collection of sections with
///the same value of table_id and:
///- for a NIT: the same table_id_extension (network_id) and version_number;
///- for a BAT: the same table_id_extension (bouquet_id) and version_number;
///-for a SDT: the same table_id_extension (transport_stream_id), the same original_network_id and
///version_number
///- for a EIT: the same table_id_extension (service_id), the same transport_stream_id, the same
///original_network_id and version_number.
///

class NetworkInformationTable;
class ServiceDescriptionTableSection;
class DVBEventInformationTableSection;

// Network Information Table (NIT)

// Network information table
/// Pointer to non const NIT section
typedef NetworkInformationTable* nit_ptr_t;
/// Pointer to const NIT section
typedef NetworkInformationTable const * nit_const_ptr_t;
/// Vector of pointers to non const NIT sections
typedef vector<nit_ptr_t>  nit_vec_t;
/// Vector of pointers to const NIT sections
typedef vector<nit_const_ptr_t>  nit_const_vec_t;
/// Map of non const pointers to NIT sections indexed by section number
typedef QMap<uint, nit_ptr_t>    nit_cache_t;

// Service Description Table (SDT)

/// Pointer to non const SDT section
typedef ServiceDescriptionTableSection* sdt_section_ptr_t;
/// Pointer to const SDT section
typedef ServiceDescriptionTableSection const * sdt_section_const_ptr_t;
/// Vector of pointers to non const SDT sections
typedef vector<sdt_section_ptr_t>  sdt_section_vec_t;
/// Vector of pointers to const SDT sections
typedef vector<sdt_section_const_ptr_t>  sdt_section_const_vec_t;
/// Non const SDT section level cache
typedef QMap<uint16_t, sdt_section_ptr_t> sdt_sections_cache_t;
/// const SDT Section level cache
typedef QMap<uint16_t, sdt_section_ptr_t> const sdt_sections_cache_const_t;
/// SDT sections cache and status information
typedef struct SdtSectionsAndStatus
{
    /// Constructor initialises status info
    SdtSectionsAndStatus(){ status.SetVersion(-1,0); }
    /// Cache of the sections of this table
    sdt_sections_cache_t sections;
    /// Status of the this table
    TableStatus status;
    /// Time status was last modified or re-verified
    QDateTime timestamp;
    /// Map of MD5 hashes of table contents
    QMap<uint16_t, QByteArray> hashes;
} sdt_sections_cache_wrapper_t;
/// SDT table level cache
typedef QMap<uint16_t,sdt_sections_cache_wrapper_t>
        sdt_t_cache_t;
/// SDT transport stream level cache
typedef QMap<uint16_t,sdt_t_cache_t>
        sdt_ts_cache_t;

/// Service description tables cache
///
/// SDT table sections are held in a cache that is indexed by
/// section number within table ID within transport stream ID within
/// original network ID. Per table status information is held at the table
/// ID level.
typedef struct SdtCache : public QMap<uint16_t,sdt_ts_cache_t>
{
	~SdtCache();
}        sdt_tsn_cache_t;

// Event Information Table (EIT)

/// Pointer to non const EIT section
typedef DVBEventInformationTableSection* eit_section_ptr_t;
/// Pointer to const EIT section
typedef DVBEventInformationTableSection const * eit_section_const_ptr_t;
/// Vector of pointers to non const EIT sections
typedef vector<eit_section_ptr_t>  eit_section_vec_t;
/// Vector of pointers to const EIT sections
typedef vector<eit_section_const_ptr_t>  eit_section_const_vec_t;
/// Non const EIT section level cache
typedef QMap<uint16_t, eit_section_ptr_t> eit_sections_cache_t;
/// const EIT section level cache
typedef QMap<uint16_t, eit_section_ptr_t> const eit_sections_cache_const_t;
/// EIT sections cache and status information
typedef struct EitSectionsAndStatus
{
    /// Constructor initialises status info
    EitSectionsAndStatus(){ status.SetVersion(-1,0); }
    /// Cache of the sections of this table
    eit_sections_cache_t sections;
    /// Status of the this table
    TableStatus status;
    /// Time status was last modified or re-verified
    QDateTime timestamp;
    /// Map of MD5 hashes of table contents
    QMap<uint16_t, QByteArray> hashes;
} eit_sections_cache_wrapper_t;
/// Table level cache
typedef QMap<uint16_t, eit_sections_cache_wrapper_t> eit_t_cache_t;
/// Service level cache
typedef QMap<uint16_t,eit_t_cache_t> eit_ts_cache_t;
/// Transport stream level cache
typedef QMap<uint16_t,eit_ts_cache_t> eit_tss_cache_t;

/// Event Information Tables cache
///
/// EIT table sections are held in a cache that is indexed by
/// section number within table ID within service ID within transport stream
/// ID within original network ID.
typedef struct EitCache : public QMap<uint16_t,eit_tss_cache_t>
{
	~EitCache();
} eit_tssn_cache_t;

#endif // SICACHETYPES_H_

