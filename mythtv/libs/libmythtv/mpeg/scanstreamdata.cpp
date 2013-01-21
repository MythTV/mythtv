// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson
#include "scanstreamdata.h"
#include "atsctables.h"
#include "dvbtables.h"

ScanStreamData::ScanStreamData(bool no_default_pid) :
    MPEGStreamData(-1, -1, true),
    ATSCStreamData(-1, -1, -1, true),
    DVBStreamData(0, 0, -1, -1, true),
    dvb_uk_freesat_si(false),
    m_no_default_pid(no_default_pid)
{
    if (m_no_default_pid)
        _pids_listening.clear();
}

ScanStreamData::~ScanStreamData() { ; }

/** \fn ScanStreamData::IsRedundant(uint,const PSIPTable&) const
 *  \brief Returns true if table already seen.
 */
bool ScanStreamData::IsRedundant(uint pid, const PSIPTable &psip) const
{
    // Treat BAT and SDTo as redundant unless they are on the FREESAT_SI_PID
    if (dvb_uk_freesat_si &&
        (psip.TableID() == TableID::BAT || psip.TableID() == TableID::SDTo))
        return pid != FREESAT_SI_PID;

    return (ATSCStreamData::IsRedundant(pid,psip) ||
            DVBStreamData::IsRedundant(pid,psip));
}

/** \fn ScanStreamData::HandleTables(uint, const PSIPTable&)
 *  \brief Processes PSIP tables
 */
bool ScanStreamData::HandleTables(uint pid, const PSIPTable &psip)
{
    bool h0 = ATSCStreamData::HandleTables(pid, psip);
    bool h1 = DVBStreamData::HandleTables(pid, psip);
    return h0 || h1;
}

void ScanStreamData::Reset(void)
{
    ATSCStreamData::Reset();
    DVBStreamData::Reset();

    if (m_no_default_pid)
    {
        _pids_listening.clear();
        return;
    }

    AddListeningPID(MPEG_PAT_PID);
    AddListeningPID(ATSC_PSIP_PID);
    AddListeningPID(DVB_NIT_PID);
    AddListeningPID(DVB_SDT_PID);
    if (dvb_uk_freesat_si)
        AddListeningPID(FREESAT_SI_PID);
}

void ScanStreamData::ResetMPEG(int desired_program)
{
    ATSCStreamData::ResetMPEG(desired_program);
    DVBStreamData::ResetMPEG(desired_program);
}

void ScanStreamData::ResetATSC(
    int desired_major_channel, int desired_minor_channel)
{
    DVBStreamData::ResetDVB(0, 0, -1);
    ATSCStreamData::ResetATSC(desired_major_channel, desired_minor_channel);
}

void ScanStreamData::ResetDVB(
    uint desired_netid, uint desired_tsid, int desired_sid)
{
    ATSCStreamData::ResetATSC(0, 0);
    DVBStreamData::ResetDVB(desired_netid, desired_tsid, desired_sid);
}

QString ScanStreamData::GetSIStandard(QString guess) const
{
    if (HasCachedMGT())
        return "atsc";

    if (HasCachedAnyNIT())
        return "dvb";

    QMutexLocker locker(&_cache_lock);

    pmt_cache_t::const_iterator it = _cached_pmts.begin();
    for (; it != _cached_pmts.end(); ++it)
    {
        ProgramMapTable *pmt = *it;

        for (uint i = 0; (guess != "dvb") && (i < pmt->StreamCount()); i++)
        {
            if (StreamID::OpenCableVideo == pmt->StreamType(i))
                return "opencable";
        }

        desc_list_t descs = MPEGDescriptor::ParseOnlyInclude(
            pmt->ProgramInfo(), pmt->ProgramInfoLength(),
            DescriptorID::registration);

        for (uint i = 0; i < descs.size(); i++)
        {
            RegistrationDescriptor reg(descs[i]);
            if (reg.FormatIdentifierString() == "SCTE")
                return "opencable";
        }
    }

    return "mpeg";
}


bool ScanStreamData::DeleteCachedTable(PSIPTable *psip) const
{
    if (!psip)
        return false;

    if (ATSCStreamData::DeleteCachedTable(psip))
        return true;
    else
        return DVBStreamData::DeleteCachedTable(psip);
}
