// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson
#include "scanstreamdata.h"
#include "atsctables.h"
#include "dvbtables.h"

ScanStreamData::ScanStreamData()
    : ATSCStreamData(-1,-1, true), dvb(true)
{
    setName("ScanStreamData");

    // MPEG
    connect(&dvb, SIGNAL(UpdatePAT(const ProgramAssociationTable*)),
            SLOT(RelayPAT(const ProgramAssociationTable*)));
    connect(&dvb, SIGNAL(UpdatePMT(uint, const ProgramMapTable*)),
            SLOT(RelayPMT(uint, const ProgramMapTable*)));

    // DVB
    connect(&dvb, SIGNAL(UpdateNIT(const NetworkInformationTable*)),
             SLOT(RelayNIT(const NetworkInformationTable*)));
    connect(&dvb, SIGNAL(UpdateSDT(uint, const ServiceDescriptionTable*)),
             SLOT(RelaySDT(uint, const ServiceDescriptionTable*)));
}

ScanStreamData::~ScanStreamData() { ; }

/** \fn ScanStreamData::IsRedundant(const PSIPTable&) const
 *  \brief Returns true if table already seen.
 */
bool ScanStreamData::IsRedundant(const PSIPTable &psip) const
{
    return ATSCStreamData::IsRedundant(psip) || dvb.IsRedundant(psip);
}

/** \fn ScanStreamData::HandleTables(const PSIPTable&)
 *  \brief Processes PSIP tables
 */
bool ScanStreamData::HandleTables(uint pid, const PSIPTable &psip)
{
    bool h0 = ATSCStreamData::HandleTables(pid, psip);
    bool h1 = dvb.HandleTables(pid, psip);
    return h0 || h1;
}

void ScanStreamData::Reset()
{
    ATSCStreamData::Reset(-1,-1);
    ATSCStreamData::AddListeningPID(MPEG_PAT_PID);
    ATSCStreamData::AddListeningPID(ATSC_PSIP_PID);

    dvb.Reset();
    dvb.AddListeningPID(DVB_NIT_PID);
}

void ScanStreamData::ReturnCachedTable(const PSIPTable *psip) const
{
    ATSCStreamData::ReturnCachedTable(psip);
    dvb.ReturnCachedTable(psip);
}

void ScanStreamData::ReturnCachedTables(pmt_vec_t &x) const
{
    ATSCStreamData::ReturnCachedTables(x);
    dvb.ReturnCachedTables(x);
}

void ScanStreamData::ReturnCachedTables(pmt_map_t &x) const
{
    ATSCStreamData::ReturnCachedTables(x);
    dvb.ReturnCachedTables(x);
}
