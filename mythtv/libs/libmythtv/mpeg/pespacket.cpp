// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson
#include "mythlogging.h"
#include "pespacket.h"
#include "mpegtables.h"

extern "C" {
#include "mythconfig.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/crc.h"
#include "libavutil/bswap.h"
}

#include <vector>
#include <map>

using namespace std;

// return true if complete or broken
bool PESPacket::AddTSPacket(const TSPacket* packet, bool &broken)
{
    broken = true;
    if (!tsheader()->PayloadStart())
    {
        LOG(VB_RECORD, LOG_ERR,
            "Error: We started a PES packet, without a payloadStart!");
        return true;
    }
    else if (!IsClone())
    {
        LOG(VB_RECORD, LOG_ERR,
            "Error: Must clone initially to use addPackets()");
        return false;
    }

    const int cc = packet->ContinuityCounter();
    const int ccExp = (_ccLast + 1) & 0xf;
    uint payloadSize  = TSPacket::kPayloadSize;
    uint payloadStart = TSPacket::kHeaderSize;

    // If the next TS has an offset, we need to strip it out.
    // The offset will be used when a new PESPacket is created.
    if (packet->PayloadStart())
    {
        payloadSize--;
        payloadStart++;
    }

    if (ccExp == cc)
    {
        if (_pesdataSize + payloadSize >= _allocSize)
        {
            uint sz = (((_allocSize * 2) + 4095) / 4096) * 4096;
            unsigned char *nbuf = pes_alloc(sz);
            memcpy(nbuf, _fullbuffer, _pesdataSize);
            pes_free(_fullbuffer);
            _fullbuffer = nbuf;
            _pesdata    = _fullbuffer + _psiOffset + 1;
            _allocSize  = sz;
        }

        memcpy(_fullbuffer    + _pesdataSize,
               packet->data() + payloadStart,
               payloadSize);

        _ccLast = cc;
        _pesdataSize += payloadSize;
    }
    else if (int(_ccLast) == cc)
    {
        // do nothing with repeats
    }
    else
    {
        LOG(VB_RECORD, LOG_ERR,
            "AddTSPacket: Out of sync!!! Need to wait for next payloadStart" +
            QString(" PID: 0x%1, continuity counter: %2 (expected %3).")
                .arg(packet->PID(),0,16).arg(cc).arg(ccExp));
        return true;
    }

    // packet is correct or incomplete
    broken = false;
    // check if it's safe to call Length
    if ((_psiOffset + 1 + 3) <=  _pesdataSize)
    {
        // +3 = first 3 bytes of pespacket header, not included in Length()
        uint tlen = Length() + (_pesdata - _fullbuffer) +3;

        if (_pesdataSize >= tlen)
        {
            _badPacket = !VerifyCRC();
            return true;
        }
    }

    return false;
}

/** \fn PESPacket::GetAsTSPackets(vector<TSPacket>&,uint) const
 *  \brief Returns payload only PESPacket as series of TSPackets
 */
void PESPacket::GetAsTSPackets(vector<TSPacket> &output, uint cc) const
{
#define INCR_CC(_CC_) do { _CC_ = (_CC_ + 1) & 0xf; } while (0)
    uint last_byte_of_pesdata = Length() + 4 - 1;
    uint size = last_byte_of_pesdata + _pesdata - _fullbuffer;

    if (_pesdata == _fullbuffer)
    {
        LOG(VB_GENERAL, LOG_ERR, "WriteAsTSPackets _pesdata == _fullbuffer");
        output.resize(0);
        return;
    }

    output.resize(1);
    memcpy(output[0].data(), _fullbuffer, TSPacket::kSize);
    output[0].data()[3] = (output[0].data()[3] & 0xf0) | cc;
    if (size <= TSPacket::kSize)
        return;

    TSHeader header;
    header.data()[1] = 0x00;
    header.data()[2] = 0x00;
    header.data()[3] = 0x10; // adaptation field control == payload only
    header.SetPID(tsheader()->PID());

    const unsigned char *data = _fullbuffer + TSPacket::kSize;
    size -= TSPacket::kSize;
    while (size > 0)
    {
        INCR_CC(cc);
        header.SetContinuityCounter(cc);
        output.resize(output.size()+1);
        output[output.size()-1].InitHeader(header.data());
        uint write_size = min(size, TSPacket::kPayloadSize);
        output[output.size()-1].InitPayload(data, write_size);
        data += write_size;
        size -= write_size;
    }
#undef INCR_CC
}

uint PESPacket::CalcCRC(void) const
{
    if (Length() < 1)
        return 0xffffffff;
    return av_bswap32(av_crc(av_crc_get_table(AV_CRC_32_IEEE), (uint32_t) -1,
                             _pesdata, Length() - 1));
}

bool PESPacket::VerifyCRC(void) const
{
    bool ret = !HasCRC() || (CalcCRC() == CRC());
    if (!ret)
    {
        LOG(VB_SIPARSER, LOG_INFO,
            QString("PESPacket: Failed CRC check 0x%1 != 0x%2 "
                    "for StreamID = 0x%3")
                .arg(CRC(),8,16,QLatin1Char('0')).arg(CalcCRC(),8,16,QLatin1Char('0')).arg(StreamID(),0,16));
    }
    return ret;
}

// These are pixel aspect ratios
const float SequenceHeader::mpeg1_aspect[16] =
{
    0.0000f,       1.0000f,       0.6735f,       0.7031f,
    0.7615f,       0.8055f,       0.8437f,       0.8935f,
    0.9157f,       0.9815f,       1.0255f,       1.0695f,
    1.0950f,       1.1575f,       1.2015f,       0.0000f,
};

/// The negative values are screen aspect ratios,
/// while the positive ones are pixel aspect ratios
const float SequenceHeader::mpeg2_aspect[16] =
{
    0.0000f,       1.0000f,       -3.0/4.0,     -9.0/16.0,
    -1.0/2.21,     0.0000f,       0.0000f,       0.0000f,
    0.0000f,       0.0000f,       0.0000f,       0.0000f,
    0.0000f,       0.0000f,       0.0000f,       0.0000f,
};

const float SequenceHeader::mpeg2_fps[16] =
{
    0.0f,          24000/1001.0f, 24.0f,        25.0f,
    30000/1001.0f, 30.0f,         50.0f,        60000/1001.0f,
    60.0f,         1.0f,          1.0f,         1.0f,
    1.0f,          1.0f,          1.0f,         1.0f,
};

/// Returns the screen aspect ratio
float SequenceHeader::aspect(bool mpeg1) const
{
    if (!height())
        return 1.0f; // avoid segfaults on broken seq data

    uint  index  = aspectNum();
    float aspect = (mpeg1) ? mpeg1_aspect[index] : mpeg2_aspect[index];

    float retval = 0.0f;
    retval = (aspect >  0.0f) ? width() / (aspect * height()) : retval;
    retval = (aspect <  0.0f) ? -1.0f   /  aspect             : retval;
    retval = (retval <= 0.0f) ? width() * 1.0f / height()     : retval;
    return retval;
}



/////////////////////////////////////////////////////////////////////////
// Memory allocator to avoid malloc global lock and waste less memory. //
/////////////////////////////////////////////////////////////////////////

static vector<unsigned char*> mem188;
static vector<unsigned char*> free188;
static map<unsigned char*, bool> alloc188;

static vector<unsigned char*> mem4096;
static vector<unsigned char*> free4096;
static map<unsigned char*, bool> alloc4096;

#define BLOCKS188 512
static unsigned char* get_188_block()
{
    if (free188.empty())
    {
        mem188.push_back((unsigned char*) malloc(188 * BLOCKS188));
        free188.reserve(BLOCKS188);
        unsigned char* block_start = mem188.back();
        for (uint i = 0; i < BLOCKS188; ++i)
            free188.push_back(i*188 + block_start);
    }

    unsigned char *ptr = free188.back();
    free188.pop_back();
    alloc188[ptr] = true;
    return ptr;
}
#undef BLOCKS188

static bool is_188_block(unsigned char* ptr)
{
    return alloc188.find(ptr) != alloc188.end();
}

static void return_188_block(unsigned char* ptr)
{
    alloc188.erase(ptr);
    free188.push_back(ptr);
    // free the allocator only if more than 1 block was used
    if (alloc188.empty() && mem188.size() > 1)
    {
        vector<unsigned char*>::iterator it;
        for (it = mem188.begin(); it != mem188.end(); ++it)
            free(*it);
        mem188.clear();
        free188.clear();
#if 0
        LOG(VB_GENERAL, LOG_DEBUG, "freeing all 188 blocks");
#endif
    }
}

#define BLOCKS4096 128
static unsigned char* get_4096_block()
{
    if (free4096.empty())
    {
        mem4096.push_back((unsigned char*) malloc(4096 * BLOCKS4096));
        free4096.reserve(BLOCKS4096);
        unsigned char* block_start = mem4096.back();
        for (uint i = 0; i < BLOCKS4096; ++i)
            free4096.push_back(i*4096 + block_start);
    }

    unsigned char *ptr = free4096.back();
    free4096.pop_back();
    alloc4096[ptr] = true;
    return ptr;
}
#undef BLOCKS4096

static bool is_4096_block(unsigned char* ptr)
{
    return alloc4096.find(ptr) != alloc4096.end();
}

static void return_4096_block(unsigned char* ptr)
{
    alloc4096.erase(ptr);
    free4096.push_back(ptr);

#if 0 // enable this to debug memory leaks
        LOG(VB_GENERAL, LOG_DEBUG, QString("%1 4096 blocks remain")
            .arg(alloc4096.size()));
        map<unsigned char*, bool>::iterator it;
        for (it = alloc4096.begin(); it != alloc4096.end(); ++it)
        {
            TSPacket *ts = (TSPacket*) it->first;
            LGO(VB_GENERAL, LOG_DEBUG, QString("PES Packet: pid(0x%1)")
                .arg(ts->PID(),0,16));
            if (ts->PID() == 0x1ffb)
            {
                LOG(VB_GENERAL, LOG_DEBUG, QString(" tid(0x%1) ext(0x%2)")
                    .arg(PSIPTable(*ts).TableID(),0,16)
                    .arg(PSIPTable(*ts).TableIDExtension(),0,16));
            }
        }
#endif

    // free the allocator only if more than 1 block was used
    if (alloc4096.empty() && mem4096.size() > 1)
    {
        vector<unsigned char*>::iterator it;
        for (it = mem4096.begin(); it != mem4096.end(); ++it)
            free(*it);
        mem4096.clear();
        free4096.clear();
#if 0
        LOG(VB_GENERAL, LOG_DEBUG, "freeing all 4096 blocks");
#endif
    }
}

static QMutex pes_alloc_mutex;

unsigned char *pes_alloc(uint size)
{
    QMutexLocker locker(&pes_alloc_mutex);
#ifndef USING_VALGRIND
    if (size <= 188)
        return get_188_block();
    else if (size <= 4096)
        return get_4096_block();
#endif // USING_VALGRIND
    return (unsigned char*) malloc(size);
}

void pes_free(unsigned char *ptr)
{
    QMutexLocker locker(&pes_alloc_mutex);
#ifndef USING_VALGRIND
    if (is_188_block(ptr))
        return_188_block(ptr);
    else if (is_4096_block(ptr))
        return_4096_block(ptr);
    else
#endif // USING_VALGRIND
        free(ptr);
}
