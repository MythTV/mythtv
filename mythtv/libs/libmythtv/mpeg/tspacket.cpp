// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson
#include <iostream>
#include "tspacket.h"

const unsigned char TSHeader::PAYLOAD_ONLY_HEADER[4] =
{
    SYNC_BYTE,
    0x40, // payload start
    0x0,
    0x10,  // adaptation field control == payload only
};


QString TSPacket::toString() const {
    QString str;
    str.append("TSPacket @0x%1  ").arg(long(&data()[0]),0,16);
    str.append("raw: 0x%1 0x%2 0x%3 0x%4\n").arg(int(data()[0]),0,16).
        arg(int(data()[1]),0,16).arg(int(data()[2]),0,16).arg(int(data()[3]),0,16);
    str.append("                 inSync: %1\n").arg( HasSync());
    str.append("         transportError: %1\n").arg( TransportError());
    str.append("           payloadStart: %1\n").arg( PayloadStart() );
    str.append("               priority: %1\n").arg( Priority() );
    str.append("                    pid: %1\n").arg( PID() );
    str.append("              scrampled: %1\n").arg( ScramplingControl() );
    str.append(" adaptationFieldControl: %1\n").arg( AdaptationFieldControl() );
    str.append("      continuityCounter: %1\n").arg( ContinuityCounter() );
    return str;
}
