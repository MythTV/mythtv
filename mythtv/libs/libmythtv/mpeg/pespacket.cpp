// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson
#include "pespacket.h"
#include "mpegtables.h"
#include "mythcontext.h"

static inline unsigned int calcOffset(unsigned int cnt) {
    return (cnt*TSPacket::PAYLOAD_SIZE)+TSPacket::HEADER_SIZE;
}

// return true if complete or broken
bool PESPacket::AddTSPacket(const TSPacket* packet) {
    unsigned int tlen = Length() + (_pesdata-_fullbuffer) + 4/*CRC*/;

    if (!tsheader()->PayloadStart()) {
        VERBOSE(VB_RECORD, "Error: We started a PES packet, without a payloadStart!");
        return false;
    } else if (!IsClone()) {
        VERBOSE(VB_RECORD, "Error: Must clone initially to use addPackets()");
        return false;
    }


    const int cc = packet->ContinuityCounter();
    const int ccExp = (_ccLast+1)&0xf;
    if (ccExp == cc) {
        if (calcOffset(_cnt)>_allocSize) {
            VERBOSE(VB_RECORD, QString("Too much data, need to handle...   %1:%2").
                    arg(calcOffset(_cnt)).arg(_allocSize));
            return true;
        }
        memcpy(_fullbuffer+calcOffset(_cnt),
               packet->data()+calcOffset(0), TSPacket::PAYLOAD_SIZE);
        _ccLast=cc;
        _cnt++;
    } else if (int(_ccLast) == cc) {
        // do nothing with repeats
    } else {
        VERBOSE(VB_RECORD, "Out of sync!!! Need to wait for next payloadStart");
        return true;
    }

    if (calcOffset(_cnt) >= tlen) {
        if (CalcCRC()==CRC()) {
            _badPacket=false;
            return true;
        }

        return true;
    } else
        return false;
}
