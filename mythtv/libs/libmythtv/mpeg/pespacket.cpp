// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson
#include "pespacket.h"
#include "mpegtables.h"

static inline unsigned int calcOffset(unsigned int cnt) {
    return (cnt*TSPacket::PAYLOAD_SIZE)+TSPacket::HEADER_SIZE;
}

// return true if complete or broken
bool PESPacket::AddTSPacket(const TSPacket* packet) {
    unsigned int tlen = Length() + (_pesdata-_fullbuffer) + 4/*CRC*/;

    if (!tsheader()->PayloadStart()) {
        cerr<<"Error: We started a PES packet,"
            <<" without a payloadStart!"<<endl;
        return false;
    } else if (!IsClone()) {
        cerr<<"Error: Must clone initially to use addPackets()"<<endl;
        return false;
    }


    const int cc = packet->ContinuityCounter();
    const int ccExp = (_ccLast+1)&0xf;
    if (ccExp == cc) {
        if (calcOffset(_cnt)>_allocSize) {
            cerr<<"Too much data, need to handle...   "
                <<calcOffset(_cnt)<<":"<<_allocSize<<endl;
            return true;
        }
        memcpy(_fullbuffer+calcOffset(_cnt),
               packet->data()+calcOffset(0), TSPacket::PAYLOAD_SIZE);
        _ccLast=cc;
        _cnt++;
    } else if (int(_ccLast) == cc) {
        // do nothing with repeats
    } else {
        cerr<<"Out of sync!!!  ";
        cerr<<"Need to wait for next payloadStart"<<endl;
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
