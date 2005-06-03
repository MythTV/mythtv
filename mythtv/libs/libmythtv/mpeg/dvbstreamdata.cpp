// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson
#include "dvbstreamdata.h"
#include "dvbtables.h"

/** \fn DVBStreamData::IsRedundant(const PSIPTable&) const
 *  \brief Returns true if table already seen.
 *  \todo This is just a stub.
 */
bool DVBStreamData::IsRedundant(const PSIPTable &psip) const
{
    return MPEGStreamData::IsRedundant(psip);
}

/** \fn DVBStreamData::HandleTables(const TSPacket*)
 *  \brief Assembles PSIP packets and processes them.
 *  \todo This is just a stub.
 */
void DVBStreamData::HandleTables(const TSPacket* tspacket)
{
    DVBStreamData::HandleTables(tspacket);
}

void DVBStreamData::PrintNIT(const NetworkInformationTable* nit) const
{
    VERBOSE(VB_RECORD, nit->toString());
}

void DVBStreamData::PrintSDT(const ServiceDescriptionTable* sdt) const
{
    VERBOSE(VB_RECORD, sdt->toString());
}
