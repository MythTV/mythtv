/**
 *  PSIPGenerator
 *  Copyright (c) 2011 by Chase Douglas
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef _PSIP_GENERATOR_H_
#define _PSIP_GENERATOR_H_

// C++ headers
#include <vector>

// Qt headers
#include <QByteArray>

// MythTV headers
extern "C" {
#include "libavformat/avio.h"
}
#include "mythtimer.h"
#include "tspacket.h"

struct AVInputFormat;

class PSIPGenerator
{
  public:
    PSIPGenerator();

    bool Initialize();
    void Reset();
    bool IsPSIAvailable() const;
    void AddData(const unsigned char* data, uint data_size);
    bool CheckPSIAvailable();
    void SetupRetry();
    const vector<TSPacket>& PATPackets();
    const vector<TSPacket>& PMTPackets();

    int ReadStream(uint8_t* buf, int size);
    int64_t SeekStream(int64_t offset, int whence);

  private:
    struct AVInputFormat*         m_av_input_format;
    ByteIOContext*                m_bio_ctx;
    unsigned char                 m_avio_buffer[4 * 1024];

    QByteArray                    m_buffer;
    int64_t                       m_buffer_pos;
    int                           m_check_thresh;
    bool                          m_detection_failed;

    vector<TSPacket>              m_pat_packets;
    MythTimer                     m_pat_timer;

    vector<TSPacket>              m_pmt_packets;
    MythTimer                     m_pmt_timer;
};

#endif // _PSIP_GENERATOR_H_
