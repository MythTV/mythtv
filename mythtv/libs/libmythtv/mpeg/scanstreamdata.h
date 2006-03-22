// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson
#ifndef SCANSTREAMDATA_H_
#define SCANSTREAMDATA_H_

#include "atscstreamdata.h"
#include "dvbstreamdata.h"

class ScanStreamData : public ATSCStreamData
{
    Q_OBJECT
  public:
    ScanStreamData();
    virtual ~ScanStreamData();

    bool IsRedundant(uint pid, const PSIPTable&) const;
    bool HandleTables(uint pid, const PSIPTable &psip);

    void Reset();

    void AddListeningPID(uint pid)
    {
        ATSCStreamData::AddListeningPID(pid);
        dvb.AddListeningPID(pid);
    }

    void RemoveListeningPID(uint pid)
    {
        ATSCStreamData::RemoveListeningPID(pid);
        dvb.RemoveListeningPID(pid);
    }

    bool IsListeningPID(uint pid) const
    {
        return ATSCStreamData::IsListeningPID(pid) || dvb.IsListeningPID(pid);
    }

    QMap<uint, bool> ListeningPIDs(void) const;

    void ReturnCachedTable(const PSIPTable *psip) const;
    void ReturnCachedTables(pmt_vec_t&) const;
    void ReturnCachedTables(pmt_map_t&) const;

    // DVB
    const NetworkInformationTable *GetCachedNIT(bool current = true) const
        { return dvb.GetCachedNIT(current); }
    const sdt_ptr_t GetCachedSDT(uint tsid, bool current = true) const
        { return dvb.GetCachedSDT(tsid, current); }
    sdt_vec_t GetAllCachedSDTs(bool current = true) const
        { return dvb.GetAllCachedSDTs(current); }
    void ReturnCachedSDTTables(sdt_vec_t &x) const
        { dvb.ReturnCachedSDTTables(x); }

    operator DVBStreamData& () { return dvb; }
    operator const DVBStreamData& () const { return dvb; }

  signals:
    // DVB
    void UpdateNIT(const NetworkInformationTable*);
    void UpdateSDT(uint tsid, const ServiceDescriptionTable*);

  private slots:
    // MPEG
    void RelayPAT(const ProgramAssociationTable *x)  { emit UpdatePAT(x); }
    void RelayPMT(uint pid, const ProgramMapTable *x){ emit UpdatePMT(pid, x); }

    // DVB
    void RelayNIT(const NetworkInformationTable *x)  { emit UpdateNIT(x); }
    void RelaySDT(uint tsid, const ServiceDescriptionTable *x)
        { emit UpdateSDT(tsid, x); }

  public:
    DVBStreamData dvb;
};

#endif // SCANSTREAMDATA_H_
