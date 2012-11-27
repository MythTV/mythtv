// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson
#ifndef SCANSTREAMDATA_H_
#define SCANSTREAMDATA_H_

#include "atscstreamdata.h"
#include "dvbstreamdata.h"
#include "mythtvexp.h"

class MTV_PUBLIC ScanStreamData :
    public ATSCStreamData,
    public DVBStreamData
{
  public:
    ScanStreamData(bool no_default_pid = false);
    virtual ~ScanStreamData();

    bool IsRedundant(uint pid, const PSIPTable&) const;
    bool HandleTables(uint pid, const PSIPTable &psip);

    virtual void Reset(void);
    virtual void ResetMPEG(int program);
    virtual void ResetATSC(int major_channel, int minor_channel);
    virtual void ResetDVB(uint netid, uint tsid, int sid);

    bool HasEITPIDChanges(const uint_vec_t& /*in_use_pids*/) const
        { return false; }
    bool GetEITPIDChanges(const uint_vec_t& /*in_use_pids*/,
                          uint_vec_t& /*add_pids*/,
                          uint_vec_t& /*del_pids*/) const { return false; }

    QString GetSIStandard(QString guess = "mpeg") const;

    void SetFreesatAdditionalSI(bool freesat_si);

  private:
    virtual bool DeleteCachedTable(PSIPTable *psip) const;
    /// listen for addiotional Freesat service information
    int dvb_uk_freesat_si;
    bool m_no_default_pid;
};

inline void ScanStreamData::SetFreesatAdditionalSI(bool freesat_si)
{
    QMutexLocker locker(&_listener_lock);
    dvb_uk_freesat_si = freesat_si;
}

#endif // SCANSTREAMDATA_H_
