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

class DVBStreamData : public MPEGStreamData
{
    Q_OBJECT
  public:
    DVBStreamData(bool cacheTables = false);

    void Reset();

    // Table processing
    bool HandleTables(uint pid, const PSIPTable&);
    bool IsRedundant(uint pid, const PSIPTable&) const;

    // Table versions
    void SetVersionNIT(int version)
    {
        if (_nit_version == version)
            return;
        _nit_version = version;
        _nit_section_seen.clear();
        _nit_section_seen.resize(32, 0);
    }
    int  VersionNIT() const { return _nit_version; }

    void SetVersionNITo(int version)
    {
        if (_nito_version == version)
            return;
        _nito_version = version;
        _nito_section_seen.clear();
        _nito_section_seen.resize(32, 0);
    }
    int  VersionNITo() const { return _nito_version; }

    void SetVersionSDT(uint tsid, int version)
    {
        if (_sdt_versions[tsid] == version)
            return;
        _sdt_versions[tsid] = version;
        _sdt_section_seen[tsid].clear();
        _sdt_section_seen[tsid].resize(32, 0);
    }
    int VersionSDT(uint tsid) const
    {
        const QMap<uint, int>::const_iterator it = _sdt_versions.find(tsid);
        if (it == _sdt_versions.end())
            return -1;
        return *it;
    }

    void SetVersionSDTo(uint tsid, int version)
    {
        if (_sdto_versions[tsid] == version)
            return;
        _sdt_versions[tsid] = version;
        _sdt_section_seen[tsid].clear();
        _sdt_section_seen[tsid].resize(32, 0);
    }
    int VersionSDTo(uint tsid) const
    {
        const QMap<uint, int>::const_iterator it = _sdto_versions.find(tsid);
        if (it == _sdto_versions.end())
            return -1;
        return *it;
    }

    // Sections seen
    void SetNITSectionSeen(uint section);
    bool NITSectionSeen(uint section) const;
    void SetNIToSectionSeen(uint section);
    bool NIToSectionSeen(uint section) const;

    void SetSDTSectionSeen(uint tsid, uint section);
    bool SDTSectionSeen(uint tsid, uint section) const;
    void SetSDToSectionSeen(uint tsid, uint section);
    bool SDToSectionSeen(uint tsid, uint section) const;

    // Caching
    bool HasCachedNIT(bool current = true) const;
    bool HasCachedSDT(uint tsid, bool current = true) const;
    bool HasCachedAllSDTs(bool current = true) const;

    const NetworkInformationTable *GetCachedNIT(bool current = true) const;
    const sdt_ptr_t GetCachedSDT(uint tsid, bool current = true) const;
    sdt_vec_t GetAllCachedSDTs(bool current = true) const;

    void ReturnCachedSDTTables(sdt_vec_t&) const;

  signals:
    void UpdateNIT(const NetworkInformationTable*);
    void UpdateSDT(uint tsid, const ServiceDescriptionTable*);

    void UpdateNITo(const NetworkInformationTable*);
    void UpdateSDTo(uint tsid, const ServiceDescriptionTable*);

  private slots:
    void PrintNIT(const NetworkInformationTable*) const;
    void PrintSDT(uint tsid, const ServiceDescriptionTable*) const;

  private:
    // Caching
    void CacheNIT(NetworkInformationTable *);
    void CacheSDT(uint tsid, ServiceDescriptionTable*);

    // Table versions
    int                       _nit_version;
    QMap<uint, int>           _sdt_versions;
    sections_t                _nit_section_seen;
    sections_map_t            _sdt_section_seen;

    int                       _nito_version;
    QMap<uint, int>           _sdto_versions;
    sections_t                _nito_section_seen;
    sections_map_t            _sdto_section_seen;

    // Caching
    NetworkInformationTable  *_cached_nit;
    sdt_cache_t               _cached_sdts; // pid->sdt
};

#endif // DVBSTREAMDATA_H_
