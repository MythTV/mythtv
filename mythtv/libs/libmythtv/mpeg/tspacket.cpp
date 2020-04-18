// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson
#include <cstdint> // for intptr_t
#include "tspacket.h"

const TSHeaderArray TSHeader::kPayloadOnlyHeader
{
    SYNC_BYTE,
    0x40, // payload start
    0x0,
    0x10,  // adaptation field control == payload only
};

const std::array<unsigned char,188> NULL_PACKET_BYTES
{
    SYNC_BYTE, 0x1F, 0xFF, 0x00,
    0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, // 36
    0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, // 68
    0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, // 100
    0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, // 132
    0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, // 164
    0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, // 188
};

const TSPacket *TSPacket::kNullPacket =
    reinterpret_cast<const TSPacket*>(NULL_PACKET_BYTES.data());

QString TSPacket::toString() const
{
    QString str;
    str.append(QString("TSPacket @0x%1  ").arg((intptr_t)(&data()[0]),0,16));
    str.append(QString("raw: 0x%1 0x%2 0x%3 0x%4\n")
               .arg(int(data()[0]),0,16).arg(int(data()[1]),0,16)
               .arg(int(data()[2]),0,16).arg(int(data()[3]),0,16));
    str.append(QString("                 inSync: %1\n").arg(static_cast<int>(HasSync())));
    str.append(QString("         transportError: %1\n").arg(static_cast<int>(TransportError())));
    str.append(QString("           payloadStart: %1\n").arg(static_cast<int>(PayloadStart())));
    str.append(QString("               priority: %1\n").arg(static_cast<int>(Priority())));
    str.append(QString("                    pid: %1\n").arg(PID()));
    str.append(QString("       scrambled (if>1): %1\n")
               .arg(ScramblingControl()));
    str.append(QString(" adaptationFieldControl: %1\n")
               .arg(AdaptationFieldControl()));
    str.append(QString("      continuityCounter: %1\n")
               .arg(ContinuityCounter()));
    return str;
}
