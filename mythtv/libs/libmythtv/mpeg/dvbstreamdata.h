// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson
#ifndef DVBSTREAMDATA_H_
#define DVBSTREAMDATA_H_

#include "mpegstreamdata.h"

class PSIPTable;
class NetworkInformationTable;
class ServiceDescriptionTable;

typedef ServiceDescriptionTable*               sdt_ptr_t;
typedef vector<const ServiceDescriptionTable*> sdt_vec_t;
typedef QMap<uint, sdt_ptr_t>                  sdt_cache_t;

class DVBStreamData : virtual public MPEGStreamData
{
    Q_OBJECT
  public:
    DVBStreamData(bool cacheTables = false);

    void Reset();

    // Table processing
    bool HandleTables(uint pid, const PSIPTable&);
    bool IsRedundant(const PSIPTable&) const;

    // Table versions
    void SetVersionNIT(int version)            { _nit_version = version;       }
    int  VersionNIT() const                    { return _nit_version;          }
    void SetVersionSDT(uint tsid, int version) { _sdt_versions[tsid] = version; }
    inline int VersionSDT(uint tsid) const;

    // Caching
    bool HasCachedNIT(bool current = true) const;
    bool HasCachedSDT(uint tsid, bool current = true) const;
    bool HasCachedAllSDTs(bool current = true) const;

    const NetworkInformationTable *GetCachedNIT(bool current = true) const;
    const sdt_ptr_t GetCachedSDT(uint tsid, bool current = true) const;
    sdt_vec_t GetAllCachedSDTs(bool current = true) const;

    void ReturnCachedTables(sdt_vec_t&) const;

  signals:
    void UpdateNIT(const NetworkInformationTable*);
    void UpdateSDT(uint tsid, const ServiceDescriptionTable*);

  private slots:
    void PrintNIT(const NetworkInformationTable*) const;
    void PrintSDT(uint tsid, const ServiceDescriptionTable*) const;

  private:
    // Caching
    void CacheNIT(NetworkInformationTable *);
    void CacheSDT(uint tsid, ServiceDescriptionTable*);

    // Table versions
    int                      _nit_version;
    QMap<uint, int>          _sdt_versions;
    // Caching
    NetworkInformationTable *_cached_nit;
    sdt_cache_t              _cached_sdts; // pid->sdt
};

inline int DVBStreamData::VersionSDT(uint tsid) const
{
    const QMap<uint, int>::const_iterator it = _sdt_versions.find(tsid);
    return (it == _sdt_versions.end()) ? -1 : *it;
}

#endif // DVBSTREAMDATA_H_
