// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson
#include "scanstreamdata.h"
#include "atsctables.h"
#include "dvbtables.h"

ScanStreamData::ScanStreamData()
    : MPEGStreamData(-1, true),
      ATSCStreamData(-1,-1, true),
      DVBStreamData(0, 0, -1, true)
{
}

ScanStreamData::~ScanStreamData() { ; }

/** \fn ScanStreamData::IsRedundant(uint,const PSIPTable&) const
 *  \brief Returns true if table already seen.
 */
bool ScanStreamData::IsRedundant(uint pid, const PSIPTable &psip) const
{
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
    MPEGStreamData::Reset(-1);
    ATSCStreamData::Reset(-1,-1);
    DVBStreamData::Reset(0,0,-1);

    AddListeningPID(MPEG_PAT_PID);
    AddListeningPID(ATSC_PSIP_PID);
    AddListeningPID(DVB_NIT_PID);
    AddListeningPID(DVB_SDT_PID);
}
