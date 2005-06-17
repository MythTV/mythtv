// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson
#include "scanstreamdata.h"
#include "atsctables.h"
#include "dvbtables.h"

ScanStreamData::~ScanStreamData() { ; }

/** \fn ScanStreamData::IsRedundant(const PSIPTable&) const
 *  \brief Returns true if table already seen.
 *  \todo This is just a stub.
 */
bool ScanStreamData::IsRedundant(const PSIPTable &psip) const
{
    return ATSCStreamData::IsRedundant(psip) ||
        DVBStreamData::IsRedundant(psip);
}

/** \fn ScanStreamData::HandleTables(const PSIPTable&)
 *  \brief Assembles PSIP packets and processes them.
 *  \todo This is just a stub.
 */
bool ScanStreamData::HandleTables(uint pid, const PSIPTable &psip)
{
    if (ATSCStreamData::HandleTables(pid, psip))
        return true;
    else
        return DVBStreamData::HandleTables(pid, psip);
}

void ScanStreamData::Reset()
{
    ATSCStreamData::Reset(-1,-1);
    DVBStreamData::Reset();
}
