/** -*- Mode: c++ -*-
 *  CetonRTP
 *  Copyright (c) 2011 Ronald Frazier
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>

#include <QMutexLocker>
#include <QStringList>
#include <QUdpSocket>

#include "cetonrtp.h"
#include "mythlogging.h"

#define LOC QString("CetonRTP(%1-%2): ").arg(_ip).arg(_tuner)

#define RTPDATAGRAMSIZE 1328

CetonRTP::CetonRTP(const QString& ip, uint tuner, ushort listenPort,
                   ushort controlPort):
    _ip(ip),
    _tuner(tuner),
    _listenPort(listenPort),
    _rtpSocket(NULL),
    _acceptFrom(ip),
    _rtsp(ip, tuner, controlPort),
    _lastRead(0),
    _thisRead(0),
    _lastSequenceNumber(-1)
{
}

bool CetonRTP::Init(void)
{
    QStringList options;
    if (!(_rtsp.GetOptions(options)     && options.contains("OPTIONS")  &&
          options.contains("DESCRIBE")  && options.contains("SETUP")    &&
          options.contains("PLAY")      && options.contains("TEARDOWN")))
    {
        LOG(VB_RECORD, LOG_ERR, LOC +
            "RTSP interface did not support the necessary options");
        return false;
    }

    if (!_rtpSocket.bind(_listenPort))
    {
        LOG(VB_RECORD, LOG_ERR, LOC +
            QString("UDP socket failed to bind on port %1").arg(_listenPort));
        return false;
    }

    if (!_dummySocket.bind())
    {
        LOG(VB_RECORD, LOG_ERR, LOC +
            "UDP dummy socket failed to bind on any port");
        return false;
    }

    int socket = _rtpSocket.socketDescriptor();
    if (socket > 0)
    {
        int oldBufferSize;
        socklen_t bs_size = sizeof(oldBufferSize);
        getsockopt(socket, SOL_SOCKET, SO_RCVBUF, &oldBufferSize, &bs_size);

        int requestedBufferSize = 1024*1024*2;
        bs_size = sizeof(requestedBufferSize);
        if (setsockopt(socket, SOL_SOCKET, SO_RCVBUF, &requestedBufferSize,
                       sizeof(requestedBufferSize)) == -1)
        {
            LOG(VB_RECORD, LOG_WARNING, LOC +
                "Unable to adjust socket receive buffer size");
        }

        int newBufferSize;
        bs_size = sizeof(newBufferSize);
        getsockopt(socket,SOL_SOCKET, SO_RCVBUF, &newBufferSize, &bs_size);
        LOG(VB_RECORD, LOG_INFO, LOC +
            QString("Buffer size was %1 - requested %2 - now %3")
            .arg(oldBufferSize).arg(requestedBufferSize).arg(newBufferSize));
    }

    return true;
}

bool CetonRTP::StartStreaming(void)
{
    return (_rtsp.Describe() &&
            _rtsp.Setup(_rtpSocket.localPort(), _dummySocket.localPort()) &&
            _rtsp.Play());
}

bool CetonRTP::StopStreaming(void)
{
    return _rtsp.Teardown();
}

int CetonRTP::Read(char *buffer, int bufferSize)
{
    ProcessIncomingData();

    int count =_mpegBuffer.size();
    if (count > bufferSize) count = bufferSize;

    char* data = _mpegBuffer.data();
    memcpy(buffer, data, count);
    _mpegBuffer.remove(0, count);

    return count;
}

void CetonRTP::ProcessIncomingData(void)
{
    QHostAddress receivedFrom;

    _lastRead = _thisRead;
    _thisRead = 0;

    int bufferSize = _readBuffer.size();

    while (_rtpSocket.hasPendingDatagrams())
    {
        int bytesWaiting = _rtpSocket.pendingDatagramSize();
        _readBuffer.resize(bufferSize+bytesWaiting);
        char *data = _readBuffer.data();

        int bytesRead = _rtpSocket.readDatagram(data+bufferSize,
                                                bytesWaiting, &receivedFrom);
        if (bytesRead != bytesWaiting)
        {
            LOG(VB_RECORD, LOG_ERR, LOC +
                QString("RTP packet error. Expected %1 bytes, received %2")
                .arg(bytesWaiting).arg(bytesRead));
        }
        if (receivedFrom != _acceptFrom)
        {
            LOG(VB_RECORD, LOG_WARNING, LOC +
                QString("Got packet from %1 instead of %2")
                .arg(receivedFrom.toString(), _acceptFrom.toString()));

            _readBuffer.resize(bufferSize);
            continue;
        }

        bufferSize += bytesWaiting;
        _thisRead += bytesWaiting;
     }

    char *data = _readBuffer.data();
    int offset = 0;
    while((offset+RTPDATAGRAMSIZE) <= bufferSize)
    {
        char *packet = &data[offset];
        char *payload;
        int payloadSize;
        int sequenceNumber;

        if (FindRTPPayload(packet, payload, payloadSize, sequenceNumber))
        {
            _mpegBuffer.append(payload, payloadSize);

            if (_lastSequenceNumber != -1)
            {
                int expectedSequenceNumber = (_lastSequenceNumber+1) & 0x0FFFF;
                if (sequenceNumber != expectedSequenceNumber)
                {
                    LOG(VB_RECORD, LOG_WARNING, LOC +
                        QString("Sequence number went from %1 to %2 "
                                "[last read=%3, this read=%4]")
                        .arg(_lastSequenceNumber).arg(sequenceNumber)
                        .arg(_lastRead).arg(_thisRead));
                }
            }
            _lastSequenceNumber = sequenceNumber;
        }

        offset += RTPDATAGRAMSIZE;
    }

    _readBuffer.remove(0, offset);
}

bool CetonRTP::FindRTPPayload(
    char *packet, char *&payload, int &payloadSize, int &sequenceNumber)
{
    RtpFixedHeader *header = reinterpret_cast<RtpFixedHeader*>(packet);

    if (header->version != 2)
    {
        LOG(VB_RECORD, LOG_ERR, LOC +
            QString("Unhandled RTP version %1 packet (expected ver 2)")
            .arg(header->version));
        return false;
    }

    if (header->payloadtype != 33)
    {
        LOG(VB_RECORD, LOG_ERR, LOC +
            "Error: can only process RTP packets with content type 33");
        return false;
    }

    int headerSize = sizeof(RtpFixedHeader) + (4*header->csrccount);

    if (header->extension == 1)
    {
        ushort extHeaderLength = *(ushort*)(packet+headerSize+2);
        headerSize += 4 * (1 + extHeaderLength);
    }

    payload = packet + headerSize;
    payloadSize = 1328 - headerSize;
    sequenceNumber = ntohs(header->sequencenumber);

    if (header->padding)
    {
        int paddingSize = payload[1328];
        payloadSize -= paddingSize;
    }

    return true;
}
