// -*- Mode: c++ -*-

// MythTV headers
#include "dvbtypes.h"

QString toString(fe_status status)
{
    QString str("");
    if (FE_HAS_SIGNAL  & status) str += "Signal,";
    if (FE_HAS_CARRIER & status) str += "Carrier,";
    if (FE_HAS_VITERBI & status) str += "FEC Stable,";
    if (FE_HAS_SYNC    & status) str += "Sync,";
    if (FE_HAS_LOCK    & status) str += "Lock,";
    if (FE_TIMEDOUT    & status) str += "Timed Out,";
    if (FE_REINIT      & status) str += "Reinit,";
    return str;
}
