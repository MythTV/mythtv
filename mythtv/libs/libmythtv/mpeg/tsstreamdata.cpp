// -*- Mode: c++ -*-
// Copyright (c) 2015, Digital Nirvana

#include "tsstreamdata.h"

#define LOC QString("TSStream[%1](0x%2): ").arg(_cardid).arg((intptr_t)this, QT_POINTER_SIZE, 16, QChar('0'))

/** \class TSStreamData
 *  \brief Specialized version of MPEGStreamData which is used to 'blindly'
 *         record the entire MPTS transport from an input
 */


TSStreamData::TSStreamData(int cardnum) : MPEGStreamData(-1, cardnum, false)
{
}

/** \fn TSStreamData::ProcessTSPacket(const TSPacket& tspacket)
 *  \brief Write out all packets without any filtering.
 */
bool TSStreamData::ProcessTSPacket(const TSPacket& tspacket)
{
    bool ok = !tspacket.TransportError();

    if (IsEncryptionTestPID(tspacket.PID()))
        LOG(VB_GENERAL, LOG_DEBUG, LOC + "ProcessTSPacket: Encrypted.");

    if (!ok)
        LOG(VB_GENERAL, LOG_DEBUG, LOC + "ProcessTSPacket: Transport Error.");

    if (tspacket.Scrambled())
        LOG(VB_GENERAL, LOG_DEBUG, LOC + "ProcessTSPacket: Scrambled.");

    for (size_t j = 0; j < _ts_writing_listeners.size(); j++)
        _ts_writing_listeners[j]->ProcessTSPacket(tspacket);

    return true;
}
