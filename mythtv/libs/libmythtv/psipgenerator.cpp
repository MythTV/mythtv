/**
 *  PSIPGenerator
 *  Copyright (c) 2011 by Chase Douglas
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#include <psipgenerator.h>

// MythTV headers
extern "C" {
#include "libavformat/avformat.h"
}
#include "mythlogging.h"
#include "mpegtables.h"
#include "pespacket.h"

static const vector<TSPacket> kNoPackets;
static const int kMaxCheckSize = 20 * 1024 * 1024;

static int read_stream(void* opaque, uint8_t* buf, int size)
{
    PSIPGenerator *generator = reinterpret_cast<PSIPGenerator*>(opaque);
    if (!generator)
    {
        LOG(VB_RECORD, LOG_ERR,
            "PSIPGenerator Error: Bad context passed to read_stream()");
        return 0;
    }

    return generator->ReadStream(buf, size);
}

static int64_t seek_stream(void* opaque, int64_t offset, int whence)
{
    PSIPGenerator *generator = reinterpret_cast<PSIPGenerator*>(opaque);
    if (!generator)
    {
        LOG(VB_RECORD, LOG_ERR,
            "PSIPGenerator Error: Bad context passed to seek_stream()");
        return 0;
    }

    return generator->SeekStream(offset, whence);
}

PSIPGenerator::PSIPGenerator() :
    m_av_input_format(NULL), m_bio_ctx(NULL), m_buffer_pos(0),
    m_check_thresh(1024), m_detection_failed(false)
{
}

bool PSIPGenerator::Initialize()
{
    {
        QMutexLocker locker(avcodeclock);
        av_register_all();
    }

    m_av_input_format = av_find_input_format("mpegts");
    if (!m_av_input_format)
    {
        LOG(VB_RECORD, LOG_ERR,
            "PSIPGenerator::Initialize(): Failed to find mpegts input format. "
            "PSI packet generation will not be attempted.");
        return false;
    }

    m_bio_ctx = av_alloc_put_byte(m_avio_buffer, sizeof(m_avio_buffer), 0, this,
                                  read_stream, NULL, seek_stream);
    if (!m_bio_ctx)
    {
        LOG(VB_RECORD, LOG_ERR,
            "PSIPGenerator::Initialize(): Failed to allocate avio context. PSI "
            "packet generation will not be attempted.");
        return false;
    }

    return true;
}

void PSIPGenerator::Reset()
{
    m_buffer.clear();
    m_check_thresh = 1024;
    m_pat_packets.clear();
    m_pmt_packets.clear();
    m_pat_timer.stop();
    m_pmt_timer.stop();
    m_detection_failed = false;
}

bool PSIPGenerator::IsPSIAvailable() const
{
    return (m_pat_packets.size() > 0);
}

void PSIPGenerator::AddData(const unsigned char* data, uint data_size)
{
    if (m_pat_packets.size() == 0 && !m_detection_failed)
        m_buffer.append(reinterpret_cast<const char*>(data), data_size);
}

bool PSIPGenerator::CheckPSIAvailable()
{
    if (IsPSIAvailable())
        return true;

    if (m_buffer.length() < m_check_thresh)
        return false;

    LOG(VB_RECORD, LOG_DEBUG,
        QString("PSIPGenerator::CheckPSIAvailable(): Checking for programs at "
                "%1 bytes.").arg(m_buffer.length()));

    m_buffer_pos = 0;

    AVFormatContext *av_format_ctx = NULL;
    if (av_open_input_stream(&av_format_ctx, m_bio_ctx, "", m_av_input_format,
                             NULL) != 0)
    {
        LOG(VB_RECORD, LOG_DEBUG,
            "PSIPGenerator::CheckPSIAvailable(): Failed to open stream.");

        SetupRetry();
        return false;
    }

    if (av_find_stream_info(av_format_ctx) < 0)
    {
        LOG(VB_RECORD, LOG_DEBUG,
            "PSIPGenerator::CheckPSIAvailable(): Failed to find stream info.");

        av_close_input_stream(av_format_ctx);
        SetupRetry();
        return false;
    }

    vector<uint> stream_pids;
    vector<uint> stream_types;
    for (uint i = 0; i < av_format_ctx->nb_streams; ++i)
    {
        switch (av_format_ctx->streams[i]->codec->codec_id)
        {
        case CODEC_ID_MPEG1VIDEO:
            stream_types.push_back(StreamID::MPEG1Video);
            break;
        case CODEC_ID_MPEG2VIDEO:
            stream_types.push_back(StreamID::MPEG2Video);
            break;
        case CODEC_ID_MPEG4:
            stream_types.push_back(StreamID::MPEG4Video);
            break;
        case CODEC_ID_H264:
            stream_types.push_back(StreamID::H264Video);
            break;
        case CODEC_ID_VC1:
            stream_types.push_back(StreamID::VC1Video);
            break;
        case CODEC_ID_MP1:
            stream_types.push_back(StreamID::MPEG1Audio);
            break;
        case CODEC_ID_MP2:
            stream_types.push_back(StreamID::MPEG2Audio);
            break;
        case CODEC_ID_AAC:
            stream_types.push_back(StreamID::MPEG2AACAudio);
            break;
        case CODEC_ID_AC3:
            stream_types.push_back(StreamID::AC3Audio);
            break;
        case CODEC_ID_DTS:
            stream_types.push_back(StreamID::DTSAudio);
            break;
        case CODEC_ID_PROBE:
            /* At least one program has yet to be identified,
               so wait for more data. */
            LOG(VB_RECORD, LOG_DEBUG,
                "PSIPGenerator::CheckPSIAvailable(): Found programs, but some "
                "are unidentified.");

            av_close_input_stream(av_format_ctx);
            SetupRetry();
            return false;
        default:
            continue;
        }

        stream_pids.push_back(av_format_ctx->streams[i]->id);
    }

    vector<uint> programs;
    programs.push_back(1);
    vector<uint> program_pids;
    program_pids.push_back(1);

    ProgramAssociationTable* pat =
        ProgramAssociationTable::Create(0, 0, programs, program_pids);
    pat->GetAsTSPackets(m_pat_packets, 0);
    m_pat_packets.back().SetContinuityCounter(0);
    delete pat;

    ProgramMapTable* pmt =
        ProgramMapTable::Create(1, 1, TSPacket::kNullPacket->PID(), 0,
                                stream_pids, stream_types);
    pmt->GetAsTSPackets(m_pmt_packets, 0);
    m_pmt_packets.back().SetContinuityCounter(0);
    delete pmt;

    av_close_input_stream(av_format_ctx);

    m_buffer.clear();
    m_check_thresh = 1024;

    return true;
}

void PSIPGenerator::SetupRetry()
{
    if (m_buffer.size() > kMaxCheckSize)
    {
        LOG(VB_RECORD, LOG_WARNING,
            QString("PSIPGenerator::CheckPSIAvailable(): Not enough PSI data "
                    "found in %1 bytes. Giving up.").arg(m_buffer.size()));
        
        m_buffer.clear();
        m_detection_failed = true;
    }
    else
    {
        init_put_byte(m_bio_ctx, m_avio_buffer, sizeof(m_avio_buffer), 0, this,
                      read_stream, NULL, seek_stream);
        m_check_thresh *= 2;
    }
}

int PSIPGenerator::ReadStream(uint8_t* buf, int size)
{
    int copy_size = size < m_buffer.size() - m_buffer_pos ? size :
        m_buffer.size() - m_buffer_pos;

    memcpy(buf, m_buffer.data() + m_buffer_pos, copy_size);

    m_buffer_pos += copy_size;

    return copy_size;
}

int64_t PSIPGenerator::SeekStream(int64_t offset, int whence)
{
    switch (whence)
    {
    case SEEK_SET:
        m_buffer_pos = offset;
        break;
    case SEEK_CUR:
        m_buffer_pos += offset;
        break;
    case SEEK_END:
        m_buffer_pos = m_buffer.size() - offset;
        break;
    default:
        break;
    }

    if (m_buffer_pos < 0)
        m_buffer_pos = 0;
    else if (m_buffer_pos > m_buffer.length())
        m_buffer_pos = m_buffer.length();

    return m_buffer_pos;
}

/* The ATSC standard is a reasonable spec to mimic. It says a PAT must occur at
 * least once every 100 ms. A PMT must occur at least once every 400 ms. Let's
 * set the frequencies at 75% of the limits. */
static const int kPATTimeout = 75;
static const int kPMTTimeout = 300;

const vector<TSPacket>& PSIPGenerator::PATPackets()
{
    if (!IsPSIAvailable())
        return kNoPackets;

    if (!m_pat_timer.isRunning())
        m_pat_timer.start();
    else if (m_pat_timer.elapsed() < kPATTimeout)
        return kNoPackets;

    unsigned int cc = m_pat_packets.back().ContinuityCounter();

    for (vector<TSPacket>::iterator it = m_pat_packets.begin();
         it != m_pat_packets.end();
         ++it)
        it->SetContinuityCounter(++cc);

    return m_pat_packets;
}

const vector<TSPacket>& PSIPGenerator::PMTPackets()
{
    if (!IsPSIAvailable())
        return kNoPackets;

    if (!m_pmt_timer.isRunning())
        m_pmt_timer.start();
    else if (m_pmt_timer.elapsed() < kPMTTimeout)
        return kNoPackets;

    unsigned int cc = m_pmt_packets.back().ContinuityCounter();

    for (vector<TSPacket>::iterator it = m_pmt_packets.begin();
         it != m_pmt_packets.end();
         ++it)
        it->SetContinuityCounter(++cc);

    return m_pmt_packets;
}
