/**
 *  Dbox2Recorder
 *  Copyright (c) 2005 by Levent Gündogdu (mythtv@feature-it.com)
 *  Distributed as part of MythTV under GPL v2 and later.
 */

// POSIX headers
#include <pthread.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>    // For sockaddr_in on OS X

// C++ headers
#include <iostream>
using namespace std;

// Qt headers
#include <qhttp.h>
#include <qdeepcopy.h>

// MythTV headers
#include "dbox2recorder.h"
#include "dbox2channel.h"
#include "RingBuffer.h"
#include "mythcontext.h"
#include "tspacket.h"

#define DEBUG_DBOX2_TS
//#define DEBUG_DBOX2_PMT
//#define DEBUG_DBOX2_PID

#define DBOX2_TIMEOUT                 15
#define PAT_TID                     0x00
#define PMT_TID                     0x02
#define STREAM_TYPE_VIDEO_MPEG1     0x01
#define STREAM_TYPE_VIDEO_MPEG2     0x02
#define STREAM_TYPE_AUDIO_MPEG1     0x03
#define STREAM_TYPE_AUDIO_MPEG2     0x04
#define STREAM_TYPE_PRIVATE_SECTION 0x05
#define STREAM_TYPE_PRIVATE_DATA    0x06
#define STREAM_TYPE_AUDIO_AAC       0x0f
#define STREAM_TYPE_VIDEO_MPEG4     0x10
#define STREAM_TYPE_VIDEO_H264      0x1b

#define STREAM_TYPE_AUDIO_AC3       0x81
#define STREAM_TYPE_AUDIO_DTS       0x8a

#define LOC      QString("DBox2Rec(%1): ").arg(m_cardid)
#define LOC_WARN QString("DBox2Rec(%1) Warning: ").arg(m_cardid)
#define LOC_ERR  QString("DBox2Rec(%1) Error: ").arg(m_cardid)

static int socketConnect(int socket, sockaddr* addr, int len)
{
    return connect(socket, addr, len);
}

static unsigned int mpegts_crc32(const unsigned char *data, int len);

void DBox2Relay::SetRecorder(DBox2Recorder *rec)
{
    QMutexLocker locker(&m_lock);
    m_rec = rec;
}

void DBox2Relay::httpRequestFinished(int id, bool error)
{
    QMutexLocker locker(&m_lock);
    if (m_rec)
        m_rec->httpRequestFinished(id, error);
}

typedef struct ts_packet_
{
    uint8_t pid[2];
    uint8_t flags;
    uint8_t count;
    uint8_t data[184];
    uint8_t adapt_length;
    uint8_t adapt_flags;
    uint8_t pcr[6];
    uint8_t opcr[6];
    uint8_t splice_count;
    uint8_t priv_dat_len;
    uint8_t *priv_dat;
    uint8_t adapt_ext_len;
    uint8_t adapt_eflags;
    uint8_t ltw[2];
    uint8_t piece_rate[3];
    uint8_t dts[5];
    int rest;
    uint8_t stuffing;
} ts_packet;

/** \fn DBox2Recorder::DBox2Recorder(TVRec*,DBox2Channel*)
 *  \brief Constructs a DBox2Recorder
 */
DBox2Recorder::DBox2Recorder(TVRec *rec, DBox2Channel *channel)
    : DTVRecorder(rec), m_cardid(rec->GetCaptureCardNum()),
      // MPEG stuff
      m_patPacket(new uint8_t[TSPacket::SIZE]), pat_cc(0), pkts_until_pat(0),
      m_pidPAT(0x0), m_pmtPID(-1), m_ac3PID(-1),
      m_sectionID(-1),
      // ptr to DBox2Channel
      m_channel(channel),
      // Connection relevant members
      port(-1), httpPort(-1), ip(""), isOpen(false), http(new QHttp()),
      m_relay(new DBox2Relay(this)),
      m_lastPIDRequestID(-1), m_lastInfoRequestID(-1), lastpacket(0),
      // Buffer info
      bufferSize(1024 * 1024),
      // video info
      m_videoWidth(-1), m_videoHeight(-1), m_videoFormat(""),
      // Other state
      _request_abort(false)
{
    VERBOSE(VB_RECORD, LOC + "Instantiating recorder.");

    // Initialize buffers to 1MB
    transportStream.socket = -1;
    transportStream.buffer = new uint8_t[bufferSize];
    transportStream.bufferIndex = 0;

    // Inter-object signaling
    QObject::connect(http,      SIGNAL(requestFinished    (int,bool)),
                     m_relay,   SLOT(  httpRequestFinished(int,bool)));

    m_channel->SetRecorder(this);
    m_channel->RecorderAlive(true);
}

void DBox2Recorder::TeardownAll(void)
{
    VERBOSE(VB_RECORD, LOC + "Shutting down recorder.");

    m_channel->RecorderAlive(false);

    // Abort any pending http operation
    if (http)
    {
        http->abort();
        http->closeConnection();
        http->disconnect();
    }

    Close();

    if (transportStream.buffer)
    {
        delete [] transportStream.buffer;
        transportStream.buffer = NULL;
    }

    if (http)
    {
        http->deleteLater();
        http = NULL;
    }

    if (m_relay)
    {
        m_relay->SetRecorder(NULL);
        m_relay->deleteLater();
        m_relay = NULL;
    }

    m_channel->SetRecorder(NULL);
}

void DBox2Recorder::Close(void)
{
    if (!isOpen)
        return;

    VERBOSE(VB_RECORD, LOC + "Closing all connections...");

    // Close all sockets
    if (transportStream.socket > 0)
    {
        close(transportStream.socket);
        transportStream.socket = -1;
    }
    isOpen = false;
}

bool DBox2Recorder::Open(void)
{
    if (isOpen)
        Close();

    m_channel->RecorderAlive(true);
    return RequestStream();
}

bool DBox2Recorder::RequestStream()
{
    VERBOSE(VB_RECORD, LOC +
            QString("Initializing Host: %1, Streaming-Port: %2, Http-Port: %3")
            .arg(ip).arg(port).arg(httpPort));

    // Request PIDs via http.
    // Signal will be emmitted when PIDs are ready.
    // Streaming will be activated afterwards

    VERBOSE(VB_RECORD, LOC + QString("Retrieving PIDs from %1:%2...")
            .arg(ip).arg(httpPort));

    QHttpRequestHeader header( "GET", "/control/zapto?getallpids" );
    header.setValue("Host", ip);
    http->setHost(ip, httpPort);
    m_lastPIDRequestID = http->request(header);
    return true;
}

bool DBox2Recorder::RequestInfo()
{
    VERBOSE(VB_RECORD, LOC + "Requesting stream info on " +
            QString("Host: %1, Streaming-Port: %2, Http-Port: %3")
            .arg(ip).arg(port).arg(httpPort));

    // Request Stream info via http.
    // Signal will be emmitted when Info is ready.
    // PIDS will be retrieved afterwards.

    VERBOSE(VB_RECORD, LOC + QString("Retrieving Info from %1:%2...")
            .arg(ip).arg(httpPort));

    QHttpRequestHeader header( "GET", "/control/info?streaminfo" );
    header.setValue("Host", ip);
    http->setHost(ip, httpPort);
    m_lastInfoRequestID = http->request(header);
    return true;
}

int DBox2Recorder::OpenStream(void)
{
    QString message = "";
    for (uint i = 0; i < m_pids.size(); i++)
        message += QString("0x%1 ").arg(m_pids[i], 0, 16);

    VERBOSE(VB_RECORD, LOC + "Opening pids " + message);

    struct hostent * hp = gethostbyname(ip);
    struct sockaddr_in adr;
    memset ((char *)&adr, 0, sizeof(struct sockaddr_in));

    adr.sin_family = AF_INET;
    adr.sin_addr.s_addr = ((struct in_addr *)(hp->h_addr))->s_addr;
    port = 31339;

    adr.sin_port = htons(port);

    if (adr.sin_addr.s_addr == 0)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "OpenStream() Unable to look up hostname.");

        return -1;
    }

    // Open socket
    int newSocket = socket(AF_INET, SOCK_STREAM, 0);

    if (-1 == socketConnect(newSocket, (sockaddr*) &adr,
                            sizeof(struct sockaddr_in)))
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to connect socket" + ENO);
        close(newSocket);
        return -1;
    }

    // Request TS
    QString request = "GET /";

    // Add PIDs
    for (uint i = 0; i < m_pids.size(); i++)
    {
        request += QString("%1").arg(m_pids[i], 0, 16);
        if (i + 1 < m_pids.size())
            request += ",";
    }

    request += " HTTP/1.0";

    VERBOSE(VB_RECORD, LOC + "Sending Request '"<<request<<"'");

    request += "\r\n\r\n";

    write(newSocket, request.ascii(), strlen(request.ascii()));

    return newSocket;
}

void DBox2Recorder::StartRecording(void)
{
    struct timeval tv;

    VERBOSE(VB_RECORD, LOC + "StartRecording");

    if (!Open())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Could not open recorder. Aborting.");
        _error = true;
        return;
    }

    _request_recording = true;
    _recording = true;
    _request_abort = false;

    lastpacket = time(NULL);
    long lastShown = time(NULL);

    while (_request_recording)
    {
        if (_request_abort)
              break;

        bool was_paused = request_pause || paused;
        if (PauseAndWait())
            continue;
        if (was_paused)
            lastpacket = time(NULL);

        if ((m_lastPIDRequestID >= 0) || (m_lastInfoRequestID >= 0))
        {
            if ((time(NULL) - lastShown) >= 1)
            {
                VERBOSE(VB_IMPORTANT, LOC +
                        "Waiting for request to finish...");

                lastShown  = time(NULL);
                lastpacket = time(NULL);
            }
            usleep(1000);
        }
        else if ((time(NULL) - lastpacket) > DBOX2_TIMEOUT)
        {
            VERBOSE(VB_IMPORTANT, LOC +
                    QString("No Input in %1 seconds.").arg(DBOX2_TIMEOUT));

            _error = true;
            return;
        }

        tv.tv_sec  = DBOX2_TIMEOUT;
        tv.tv_usec = 0;

        // Process transport stream data if open
        if (isOpen)
        {
            processStream(&transportStream);
        }
        else
        {
            // Don't process this loop too fast
            usleep(1000);
        }
    }

    FinishRecording();
    _recording = false;
}

void DBox2Recorder::SetOptionsFromProfile(RecordingProfile *profile,
                                          const QString &videodev,
                                          const QString &audiodev,
                                          const QString &vbidev)
{
    (void)videodev;
    (void)audiodev;
    (void)vbidev;
    (void)profile;
}

void DBox2Recorder::SetOption(const QString &name, const QString &value)
{
    if (name == "ip")
        ip = QDeepCopy<QString>(value);
    if (name == "host")
        ip = QDeepCopy<QString>(value);
}

void DBox2Recorder::SetOption(const QString &name, int value)
{
    if (name == "port")
        port = value;
    if (name == "httpport")
        httpPort = value;
}

void DBox2Recorder::httpRequestFinished(int id, bool error)
{
    if (error)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "HTTP Request failed!");
        return;
    }

    QString buffer = http->readAll();

    if (id == m_lastPIDRequestID)
    {
        VERBOSE(VB_RECORD, LOC + "Retrieving PIDs succeeded. Parsing...");

        m_pids.clear();
        m_pmtPID = -1;
        m_ac3PID = -1;
        for (int i = 0; i < 10; i ++)
        {
            QString pidString = buffer.section("\n", i, i);
            if (!pidString.isEmpty())
            {
                int pid = pidString.section(" ", 0, 0).toInt();
                if (pid == 0)
                {
                    VERBOSE(VB_GENERAL, LOC_WARN + "Got 0 PID!!!.");
                    continue;
                }

                m_pids.push_back(pid);

                QString pidType = pidString.section(" ", 1).upper();
                if (pidType == "PMT")
                {
                    m_pmtPID = m_pids.back();

#ifdef DEBUG_DBOX2_PMT
                    VERBOSE(VB_RECORD, LOC + "Found PMT with " +
                            QString("PID 0x%1.").arg(m_pmtPID,0,16));
#endif // DEBUG_DBOX2_PMT
                }
                else if ((pidType.contains("DOLBY DIGITAL") > 0) ||
                         (pidType.contains("AC3") > 0))
                {
                    m_ac3PID = m_pids.back();

#ifdef DEBUG_DBOX2_PMT
                    VERBOSE(VB_RECORD, LOC + "Found AC3 stream with " +
                            QString("PID 0x%1.").arg(m_ac3PID,0,16));
#endif // DEBUG_DBOX2_PMT
                }

#ifdef DEBUG_DBOX2_PID
                VERBOSE(VB_RECORD, LOC +
                        QString("Found PID 0x%1.").arg(m_pids[i]));
#endif // DEBUG_DBOX2_PMT
            }
        }

        if (m_pids.size() == 0)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    "No usable PIDS found. Cannot continue.");

            // Try channel change to last channel
            m_channel->SwitchToLastChannel();
            return;
        }

        // Open transport stream
        transportStream.bufferIndex = 0;
        transportStream.socket = OpenStream();
        if (transportStream.socket == -1)
            return;

        // Schedule PAT and PMT creation
        pkts_until_pat = 0;
        m_sectionID = 1;
        CreatePAT(m_patPacket);
        // Set recorder state to open
        isOpen = true;
        m_lastPIDRequestID = -1;
        return;
    }

    if (id == m_lastInfoRequestID)
    {
        VERBOSE(VB_RECORD, LOC + "Retrieving info succeeded. Parsing..." +
                QString("\n\t\t\tGot: %1.").arg(buffer));

        m_videoWidth  = buffer.section("\n", 0, 0).toInt();
        m_videoHeight = buffer.section("\n", 1, 1).toInt();
        m_videoFormat = buffer.section("\n", 3, 3);

        VERBOSE(VB_RECORD, LOC + QString("Video is %1x%2 (Format %3).")
                .arg(m_videoWidth).arg(m_videoHeight).arg(m_videoFormat));

        m_lastInfoRequestID = -1;
        RequestStream();
    }
}

int DBox2Recorder::processStream(stream_meta *stream)
{
    // Read and parse
    int bytesRead = read(stream->socket,
                         stream->buffer + stream->bufferIndex,
                         bufferSize - stream->bufferIndex);

    if (bytesRead <= 0)
    {
        VERBOSE(VB_IMPORTANT, LOC + "Error reading data.");
        return 0;
    }

    stream->bufferIndex += bytesRead;

    int readIndex = 0;
    while (readIndex < stream->bufferIndex)
    {
        // Try to find next TS
        int tsPos = findTSHeader(stream->buffer + readIndex,
                                 stream->bufferIndex);
        if (tsPos == -1)
        {
            VERBOSE(VB_IMPORTANT, LOC + "No TS header.");
            break;
        }

        if (tsPos > 0)
        {
            VERBOSE(VB_IMPORTANT, LOC +
                    QString("TS header at %1, not in sync.").arg(tsPos));
        }

        // Check if enough data is available to process complete TS packet.
        if ((stream->bufferIndex - readIndex - tsPos) < 188)
        {
            // VERBOSE(VB_IMPORTANT, LOC +
            //         QString("TS header at %1").arg(tsPos) +
            //         "but packet not yet complete.");
            break;
        }

        updatePMTSectionID(stream->buffer + readIndex + tsPos, m_pmtPID);

        // Inject PAT every 2000 TS packets.
        if (pkts_until_pat == 0)
        {
            BufferedWrite(*(reinterpret_cast<const TSPacket*>(m_patPacket)));
            pkts_until_pat = 2000;
        }
        else
        {
            pkts_until_pat--;
        }

        const void     *data     = stream->buffer + readIndex + tsPos;
        const TSPacket *tspacket = reinterpret_cast<const TSPacket*>(data);

        _buffer_packets = !FindMPEG2Keyframes(tspacket);

        BufferedWrite(*tspacket);

        lastpacket = time(NULL);
        readIndex += tsPos + TSPacket::SIZE;
    }

    memcpy(stream->buffer, stream->buffer + readIndex, bufferSize - readIndex);
    stream->bufferIndex = stream->bufferIndex - readIndex;

    if (stream->bufferIndex < 0)
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN +
                QString("Buffer index is %1. Resetting.")
                .arg(stream->bufferIndex));

        stream->bufferIndex = 0;
    }

    return 0;
}

void DBox2Recorder::CreatePAT(unsigned char *ts_packet)
{
    VERBOSE(VB_IMPORTANT, LOC +
            QString("Creating PAT for PMT pid 0x%1.").arg(m_pmtPID,0,16));

    memset(ts_packet, 0xFF, 188);

    ts_packet[0] = 0x47;                            // sync byte
    ts_packet[1] = 0x40 | ((m_pidPAT >> 8) & 0x1F);  // payload start & PID
    ts_packet[2] = m_pidPAT & 0xFF;                  // PID
    ts_packet[3] = 0x10 | pat_cc;                   // scrambling, adaptation & continuity counter
    ts_packet[4] = 0x00;                            // pointer field

    ++pat_cc &= 0x0F;   // inc. continuity counter
    unsigned char *pat = ts_packet + 5;
    int p = 0;

    pat[p++] = PAT_TID; // table ID
    pat[p++] = 0xB0;    // section syntax indicator
    p++;                // section length (set later)
    pat[p++] = 0;       // TSID
    pat[p++] = 1;       // TSID
    pat[p++] = 0xC3;    // Version + Current/Next
    pat[p++] = 0;       // Current Section
    pat[p++] = 0;       // Last Section
    pat[p++] = (m_sectionID >> 8) & 0xFF;
    pat[p++] = m_sectionID & 0xFF;
    pat[p++] = (m_pmtPID >> 8) & 0x1F;
    pat[p++] = m_pmtPID & 0xFF;

    pat[2] = p + 4 - 3; // section length

    unsigned int crc = mpegts_crc32(pat, p);
    pat[p++] = (crc >> 24) & 0xFF;
    pat[p++] = (crc >> 16) & 0xFF;
    pat[p++] = (crc >> 8) & 0xFF;
    pat[p++] = crc & 0xFF;
}

void DBox2Recorder::ChannelChanged(void)
{
    VERBOSE(VB_IMPORTANT, LOC + "Channel changed. Requesting streams...");
    Open();
}

void DBox2Recorder::ChannelChanging(void)
{
    VERBOSE(VB_IMPORTANT, LOC + "Channel changing. Closing current stream...");
    Close();
}

int DBox2Recorder::findTSHeader(unsigned char *buffer, int len)
{
    int pos = 0;
    while (pos < len)
    {
        if (buffer[pos] == 0x47)
            return pos;
        pos++;
    }
    return -1;
}

int DBox2Recorder::getPMTSectionID(unsigned char *buffer, int pmtPID)
{
    int tempPID = ((buffer[1] & 0x1F) << 8) | (buffer[2] & 0xFF);
    if (tempPID != pmtPID)
        return -1;

#ifdef DEBUG_DBOX2_PMT
    VERBOSE(VB_IMPORTANT, LOC + "Found PMT packet with " +
            QString("PID 0x%2.").arg(tempPID));
#endif // DEBUG_DBOX2_PMT

    return (buffer[8] & 0xFF << 8) | (buffer[9] & 0xFF);
}

void DBox2Recorder::updatePMTSectionID(unsigned char *buffer, int pmtPID)
{
    int sectionID = getPMTSectionID(buffer, pmtPID);
    if (sectionID == -1)
        return;

#ifdef DEBUG_DBOX2_PMT
    VERBOSE(VB_IMPORTANT, LOC +
            QString("Found PMT packet with PID 0x%1 ").arg(pmtPID,0,16) +
            QString("and section id %3. Updating...").arg(sectionID));
#endif // DEBUG_DBOX2_PMT

    unsigned char *pmt = buffer + 5;
    // Set service ID to match PAT
    pmt[3] = 0;       // program number (ServiceID)
    pmt[4] = 1;       // program number (ServiceID)
    if (m_ac3PID != -1)
    {
#ifdef DEBUG_DBOX2_PMT
        VERBOSE(VB_IMPORTANT, LOC + "Looking for AC3 PID in PMT packet...");
#endif // DEBUG_DBOX2_PMT

        // AC3 PID is given, change the stream type
        unsigned char p = 10;
        // Advance by program info length + 2
        p += (((pmt[p] & 0x0F) << 8) | (pmt[p+1] & 0xFF)) + 2;
        for (uint i = 0; i < m_pids.size(); i++)
        {
            // Step over stream type
            p++;
            // Get pid
            int pid = (pmt[p] & 0x0F) << 8;
            pid = pid | (pmt[p+1] & 0xFF);

#ifdef DEBUG_DBOX2_PMT
            VERBOSE(VB_IMPORTANT, LOC + "Looking for AC3 PID in PMT packet " +
                    QString("(Pid #%1 == %2)").arg(i).arg(pid));
#endif // DEBUG_DBOX2_PMT

            if (pid == m_ac3PID)
            {
#ifdef DEBUG_DBOX2_PMT
                VERBOSE(VB_IMPORTANT, LOC + "Found AC3 PID in PMT packet. "
                        "Updating Stream Type...");
#endif // DEBUG_DBOX2_PMT

                pmt[p-1] = STREAM_TYPE_AUDIO_AC3;
                break;
            }
            // Advance by pid
            p += 2;
            // Advance by es info length + 2
            p += ((pmt[p] & 0x0F) << 8) | (pmt[p+1] & 0xFF) + 2;
        }
    }
    int len = pmt[2];
    int crcOffset = len + 3 - 4;

    // Recalculate CRC
    unsigned long crc = mpegts_crc32(pmt, crcOffset);
    pmt[crcOffset] = (crc >> 24) & 0xFF;
    pmt[crcOffset+1] = (crc >> 16) & 0xFF;
    pmt[crcOffset+2] = (crc >> 8) & 0xFF;
    pmt[crcOffset+3] = crc & 0xFF;
}

static const uint32_t crc_table[256] = 
{
    0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9, 0x130476dc, 0x17c56b6b,
    0x1a864db2, 0x1e475005, 0x2608edb8, 0x22c9f00f, 0x2f8ad6d6, 0x2b4bcb61,
    0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd, 0x4c11db70, 0x48d0c6c7,
    0x4593e01e, 0x4152fda9, 0x5f15adac, 0x5bd4b01b, 0x569796c2, 0x52568b75,
    0x6a1936c8, 0x6ed82b7f, 0x639b0da6, 0x675a1011, 0x791d4014, 0x7ddc5da3,
    0x709f7b7a, 0x745e66cd, 0x9823b6e0, 0x9ce2ab57, 0x91a18d8e, 0x95609039,
    0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5, 0xbe2b5b58, 0xbaea46ef,
    0xb7a96036, 0xb3687d81, 0xad2f2d84, 0xa9ee3033, 0xa4ad16ea, 0xa06c0b5d,
    0xd4326d90, 0xd0f37027, 0xddb056fe, 0xd9714b49, 0xc7361b4c, 0xc3f706fb,
    0xceb42022, 0xca753d95, 0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1,
    0xe13ef6f4, 0xe5ffeb43, 0xe8bccd9a, 0xec7dd02d, 0x34867077, 0x30476dc0,
    0x3d044b19, 0x39c556ae, 0x278206ab, 0x23431b1c, 0x2e003dc5, 0x2ac12072,
    0x128e9dcf, 0x164f8078, 0x1b0ca6a1, 0x1fcdbb16, 0x018aeb13, 0x054bf6a4,
    0x0808d07d, 0x0cc9cdca, 0x7897ab07, 0x7c56b6b0, 0x71159069, 0x75d48dde,
    0x6b93dddb, 0x6f52c06c, 0x6211e6b5, 0x66d0fb02, 0x5e9f46bf, 0x5a5e5b08,
    0x571d7dd1, 0x53dc6066, 0x4d9b3063, 0x495a2dd4, 0x44190b0d, 0x40d816ba,
    0xaca5c697, 0xa864db20, 0xa527fdf9, 0xa1e6e04e, 0xbfa1b04b, 0xbb60adfc,
    0xb6238b25, 0xb2e29692, 0x8aad2b2f, 0x8e6c3698, 0x832f1041, 0x87ee0df6,
    0x99a95df3, 0x9d684044, 0x902b669d, 0x94ea7b2a, 0xe0b41de7, 0xe4750050,
    0xe9362689, 0xedf73b3e, 0xf3b06b3b, 0xf771768c, 0xfa325055, 0xfef34de2,
    0xc6bcf05f, 0xc27dede8, 0xcf3ecb31, 0xcbffd686, 0xd5b88683, 0xd1799b34,
    0xdc3abded, 0xd8fba05a, 0x690ce0ee, 0x6dcdfd59, 0x608edb80, 0x644fc637,
    0x7a089632, 0x7ec98b85, 0x738aad5c, 0x774bb0eb, 0x4f040d56, 0x4bc510e1,
    0x46863638, 0x42472b8f, 0x5c007b8a, 0x58c1663d, 0x558240e4, 0x51435d53,
    0x251d3b9e, 0x21dc2629, 0x2c9f00f0, 0x285e1d47, 0x36194d42, 0x32d850f5,
    0x3f9b762c, 0x3b5a6b9b, 0x0315d626, 0x07d4cb91, 0x0a97ed48, 0x0e56f0ff,
    0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623, 0xf12f560e, 0xf5ee4bb9,
    0xf8ad6d60, 0xfc6c70d7, 0xe22b20d2, 0xe6ea3d65, 0xeba91bbc, 0xef68060b,
    0xd727bbb6, 0xd3e6a601, 0xdea580d8, 0xda649d6f, 0xc423cd6a, 0xc0e2d0dd,
    0xcda1f604, 0xc960ebb3, 0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7,
    0xae3afba2, 0xaafbe615, 0xa7b8c0cc, 0xa379dd7b, 0x9b3660c6, 0x9ff77d71,
    0x92b45ba8, 0x9675461f, 0x8832161a, 0x8cf30bad, 0x81b02d74, 0x857130c3,
    0x5d8a9099, 0x594b8d2e, 0x5408abf7, 0x50c9b640, 0x4e8ee645, 0x4a4ffbf2,
    0x470cdd2b, 0x43cdc09c, 0x7b827d21, 0x7f436096, 0x7200464f, 0x76c15bf8,
    0x68860bfd, 0x6c47164a, 0x61043093, 0x65c52d24, 0x119b4be9, 0x155a565e,
    0x18197087, 0x1cd86d30, 0x029f3d35, 0x065e2082, 0x0b1d065b, 0x0fdc1bec,
    0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088, 0x2497d08d, 0x2056cd3a,
    0x2d15ebe3, 0x29d4f654, 0xc5a92679, 0xc1683bce, 0xcc2b1d17, 0xc8ea00a0,
    0xd6ad50a5, 0xd26c4d12, 0xdf2f6bcb, 0xdbee767c, 0xe3a1cbc1, 0xe760d676,
    0xea23f0af, 0xeee2ed18, 0xf0a5bd1d, 0xf464a0aa, 0xf9278673, 0xfde69bc4,
    0x89b8fd09, 0x8d79e0be, 0x803ac667, 0x84fbdbd0, 0x9abc8bd5, 0x9e7d9662,
    0x933eb0bb, 0x97ffad0c, 0xafb010b1, 0xab710d06, 0xa6322bdf, 0xa2f33668,
    0xbcb4666d, 0xb8757bda, 0xb5365d03, 0xb1f740b4
};

static unsigned int mpegts_crc32(const unsigned char *data, int len)
{
    register int i;
    unsigned int crc = 0xffffffff;

    for (i = 0; i < len; i++)
        crc = (crc << 8) ^ crc_table[((crc >> 24) ^ *data++) & 0xff];

    return crc;
}
