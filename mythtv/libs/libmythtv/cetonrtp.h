/** -*- Mode: c++ -*-
 *  CetonRTP
 *  Copyright (c) 2011 Ronald Frazier
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef CETONRTP_H
#define CETONRTP_H

#include <QObject>
#include <QUdpSocket>

#include "cetonrtsp.h"

struct RtpFixedHeader
{
    uint csrccount      : 4;
    uint extension      : 1;
    uint padding        : 1;
    uint version        : 2;

    uint payloadtype    : 7;
    uint marker         : 1;

    uint sequencenumber : 16;
    uint timestamp      : 32;
    uint ssrc           : 32;
};

class CetonRTP
{
  public:
    explicit CetonRTP(const QString &ip, uint tuner, ushort listenPort = 0,
                      ushort controlPort = 8554);

    bool Init(void);

    bool StartStreaming(void);
    bool StopStreaming(void);

    int Read(char *buffer, int bufferSize);

  private:
    void ProcessIncomingData(void);
    bool FindRTPPayload(
        char* packet, char* &payload, int &payloadSize, int &sequenceNumber);

  private:
    QString         _ip;
    uint            _tuner;
    uint            _listenPort;

    QUdpSocket      _rtpSocket;
    QUdpSocket      _dummySocket;
    QHostAddress    _acceptFrom;
    CetonRTSP       _rtsp;

    QByteArray      _readBuffer;
    QByteArray      _mpegBuffer;

    int             _lastRead;
    int             _thisRead;
    int             _lastSequenceNumber;
};

#endif // CETONRTP_H
