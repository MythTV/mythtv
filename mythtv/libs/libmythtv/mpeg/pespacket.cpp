// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson
#include "libmythbase/mythlogging.h"
#include "libmythbase/sizetliteral.h"
#include "pespacket.h"
#include "mpegtables.h"

extern "C" {
#include "libmythbase/mythconfig.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/crc.h"
#include "libavutil/bswap.h"
}

#include <vector>
#include <map>

// return true if complete or broken
bool PESPacket::AddTSPacket(const TSPacket* packet, int cardid, bool &broken)
{
    broken = true;
    if (!tsheader()->PayloadStart())
    {
        LOG(VB_RECORD, LOG_ERR,
            "Error: We started a PES packet, without a payloadStart!");
        return true;
    }
    if (!IsClone())
    {
        LOG(VB_RECORD, LOG_ERR,
            "Error: Must clone initially to use addPackets()");
        return false;
    }

    const int cc = packet->ContinuityCounter();
    const int ccExp = (m_ccLast + 1) & 0xf;
    uint payloadSize  = TSPacket::kPayloadSize;
    uint payloadStart = TSPacket::kHeaderSize;

    // If the next TS has an offset, we need to strip it out.
    // The offset will be used when a new PESPacket is created.
    if (packet->PayloadStart())
    {
        payloadSize--;
        payloadStart++;
    }
    else
    {
        // Skip past the adaptation field if present
        if (packet->HasAdaptationField())
        {
            uint delta = packet->AdaptationFieldSize() + 1;

            // Adaptation field size is max 182 (control is '11') or 183 (control is '10').
            // Do not skip beyond end of packet when the adaptation field size is wrong.
            if (delta > TSPacket::kPayloadSize)
            {
                delta = TSPacket::kPayloadSize;
                LOG(VB_GENERAL, LOG_ERR, QString("PESPacket[%1] Invalid adaptation field size:%2  control:%3")
                    .arg(cardid).arg(packet->AdaptationFieldSize()).arg(packet->AdaptationFieldControl()));
            }
            payloadSize -= delta;
            payloadStart += delta;
        }
    }

    if (ccExp == cc)
    {
        if (m_pesDataSize + payloadSize >= m_allocSize)
        {
            uint sz = (((m_allocSize * 2) + 4095) / 4096) * 4096;
            unsigned char *nbuf = pes_alloc(sz);
            memcpy(nbuf, m_fullBuffer, m_pesDataSize);
            pes_free(m_fullBuffer);
            m_fullBuffer = nbuf;
            m_pesData    = m_fullBuffer + m_psiOffset + 1;
            m_allocSize  = sz;
        }

        memcpy(m_fullBuffer   + m_pesDataSize,
               packet->data() + payloadStart,
               payloadSize);

        m_ccLast = cc;
        m_pesDataSize += payloadSize;
    }
    else if (int(m_ccLast) == cc)
    {
        // Do nothing with repeats
        if (VERBOSE_LEVEL_CHECK(VB_RECORD, LOG_DEBUG))
        {
            LOG(VB_RECORD, LOG_ERR,
                QString("AddTSPacket[%1]: Repeat packet!! ").arg(cardid) +
                QString("PID: 0x%1, continuity counter: %2 ").arg(packet->PID(),0,16).arg(cc) +
                QString("(expected %1)").arg(ccExp));
        }
        return true;
    }
    else
    {
        // Even if the packet is out of sync it is still the last packet received
        m_ccLast = cc;

        LOG(VB_RECORD, LOG_ERR,
            QString("AddTSPacket[%1]: Out of sync!!! Need to wait for next payloadStart ").arg(cardid) +
            QString("PID: 0x%1, continuity counter: %2 ").arg(packet->PID(),0,16).arg(cc) +
            QString("(expected %1)").arg(ccExp));
        return true;
    }

    // packet is correct or incomplete
    broken = false;
    // check if it's safe to call Length
    if ((m_psiOffset + 1 + 3) <=  m_pesDataSize)
    {
        // +3 = first 3 bytes of pespacket header, not included in Length()
        uint tlen = Length() + (m_pesData - m_fullBuffer) +3;

        if (m_pesDataSize >= tlen)
        {
            m_badPacket = !VerifyCRC(cardid, packet->PID());
            return true;
        }
    }

    return false;
}

/** \fn PESPacket::GetAsTSPackets(vector<TSPacket>&,uint) const
 *  \brief Returns payload only PESPacket as series of TSPackets
 */
void PESPacket::GetAsTSPackets(std::vector<TSPacket> &output, uint cc) const
{
    uint last_byte_of_pesdata = Length() + 4 - 1;
    uint size = last_byte_of_pesdata + m_pesData - m_fullBuffer;

    if (m_pesData == m_fullBuffer)
    {
        LOG(VB_GENERAL, LOG_ERR, "WriteAsTSPackets m_pesData == m_fullBuffer");
        output.resize(0);
        return;
    }

    output.resize(1);
    memcpy(output[0].data(), m_fullBuffer, TSPacket::kSize);
    output[0].data()[3] = (output[0].data()[3] & 0xf0) | cc;
    if (size <= TSPacket::kSize)
        return;

    TSHeader header;
    header.data()[1] = 0x00;
    header.data()[2] = 0x00;
    header.data()[3] = 0x10; // adaptation field control == payload only
    header.SetPID(tsheader()->PID());

    const unsigned char *data = m_fullBuffer + TSPacket::kSize;
    size -= TSPacket::kSize;
    while (size > 0)
    {
        cc = (cc + 1) & 0xF;
        header.SetContinuityCounter(cc);
        output.resize(output.size()+1);
        output[output.size()-1].InitHeader(header.data());
        uint write_size = std::min(size, TSPacket::kPayloadSize);
        output[output.size()-1].InitPayload(data, write_size);
        data += write_size;
        size -= write_size;
    }
}

uint PESPacket::CalcCRC(void) const
{
    if (Length() < 1)
        return kTheMagicNoCRCCRC;
    return av_bswap32(av_crc(av_crc_get_table(AV_CRC_32_IEEE), UINT32_MAX,
                             m_pesData, Length() - 1));
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

bool PESPacket::VerifyCRC(int cardid, int pid) const
{
    bool ret = !HasCRC() || (CalcCRC() == CRC());
    if (!ret)
    {
        LOG(VB_RECORD, LOG_INFO,
            QString("PESPacket[%1] pid(0x%2): ").arg(cardid).arg(pid,0,16) +
            QString("Failed CRC check 0x%1 != 0x%2 for ID = 0x%3")
                .arg(CRC(),8,16,QLatin1Char('0')).arg(CalcCRC(),8,16,QLatin1Char('0')).arg(StreamID(),0,16));
    }
    return ret;
}

// These are pixel aspect ratios
const AspectArray SequenceHeader::kMpeg1Aspect
{
    0.0000F,       1.0000F,       0.6735F,       0.7031F,
    0.7615F,       0.8055F,       0.8437F,       0.8935F,
    0.9157F,       0.9815F,       1.0255F,       1.0695F,
    1.0950F,       1.1575F,       1.2015F,       0.0000F,
};

/// The negative values are screen aspect ratios,
/// while the positive ones are pixel aspect ratios
const AspectArray SequenceHeader::kMpeg2Aspect
{
    0.0000F,       1.0000F,      -3.0F/4.0F,    -9.0F/16.0F,
   -1.0F/2.21F,    0.0000F,       0.0000F,       0.0000F,
    0.0000F,       0.0000F,       0.0000F,       0.0000F,
    0.0000F,       0.0000F,       0.0000F,       0.0000F,
};

const AspectArray SequenceHeader::kMpeg2Fps
{
    0.0F,          24000/1001.0F, 24.0F,        25.0F,
    30000/1001.0F, 30.0F,         50.0F,        60000/1001.0F,
    60.0F,         1.0F,          1.0F,         1.0F,
    1.0F,          1.0F,          1.0F,         1.0F,
};

/// Returns the screen aspect ratio
float SequenceHeader::aspect(bool mpeg1) const
{
    if (!height())
        return 1.0F; // avoid segfaults on broken seq data

    uint  index  = aspectNum();
    float aspect = (mpeg1) ? kMpeg1Aspect[index] : kMpeg2Aspect[index];

    float retval = 0.0F;
    retval = (aspect >  0.0F) ? width() / (aspect * height()) : retval;
    retval = (aspect <  0.0F) ? -1.0F   /  aspect             : retval;
    retval = (retval <= 0.0F) ? width() * 1.0F / height()     : retval;
    return retval;
}



/////////////////////////////////////////////////////////////////////////
// Memory allocator to avoid malloc global lock and waste less memory. //
/////////////////////////////////////////////////////////////////////////

#ifndef USING_VALGRIND
static std::vector<unsigned char*> mem188;
static std::vector<unsigned char*> free188;
static std::map<unsigned char*, bool> alloc188;

static std::vector<unsigned char*> mem4096;
static std::vector<unsigned char*> free4096;
static std::map<unsigned char*, bool> alloc4096;

static constexpr size_t BLOCKS188 { 512 };
static unsigned char* get_188_block()
{
    if (free188.empty())
    {
        mem188.push_back((unsigned char*) malloc(188_UZ * BLOCKS188));
        free188.reserve(BLOCKS188);
        unsigned char* block_start = mem188.back();
        for (size_t i = 0; i < BLOCKS188; ++i)
            free188.push_back((i * 188_UZ) + block_start);
    }

    unsigned char *ptr = free188.back();
    free188.pop_back();
    alloc188[ptr] = true;
    return ptr;
}

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
        std::vector<unsigned char*>::iterator it;
        for (it = mem188.begin(); it != mem188.end(); ++it)
            free(*it);
        mem188.clear();
        free188.clear();
#if 0
        LOG(VB_GENERAL, LOG_DEBUG, "freeing all 188 blocks");
#endif
    }
}

static constexpr size_t BLOCKS4096 { 128 };
static unsigned char* get_4096_block()
{
    if (free4096.empty())
    {
        mem4096.push_back((unsigned char*) malloc(4096_UZ * BLOCKS4096));
        free4096.reserve(BLOCKS4096);
        unsigned char* block_start = mem4096.back();
        for (size_t i = 0; i < BLOCKS4096; ++i)
            free4096.push_back((i * 4096_UZ) + block_start);
    }

    unsigned char *ptr = free4096.back();
    free4096.pop_back();
    alloc4096[ptr] = true;
    return ptr;
}

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
        std::vector<unsigned char*>::iterator it;
        for (it = mem4096.begin(); it != mem4096.end(); ++it)
            free(*it);
        mem4096.clear();
        free4096.clear();
#if 0
        LOG(VB_GENERAL, LOG_DEBUG, "freeing all 4096 blocks");
#endif
    }
}
#endif

static QMutex pes_alloc_mutex;

unsigned char *pes_alloc(uint size)
{
    QMutexLocker locker(&pes_alloc_mutex);
#ifndef USING_VALGRIND
    if (size <= 188)
        return get_188_block();
    if (size <= 4096)
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
