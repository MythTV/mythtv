// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson
#include "pespacket.h"
#include "mpegtables.h"
#include "mythcontext.h"

#include <vector>
#include <map>

using namespace std;

// return true if complete or broken
bool PESPacket::AddTSPacket(const TSPacket* packet)
{
    uint tlen = Length() + ((_pesdata-1) - _fullbuffer) + 4 /* CRC bytes */;

    if (!tsheader()->PayloadStart())
    {
        VERBOSE(VB_RECORD, "Error: We started a PES packet, "
                "without a payloadStart!");
        return false;
    }
    else if (!IsClone())
    {
        VERBOSE(VB_RECORD, "Error: Must clone initially to use addPackets()");
        return false;
    }

    const int cc = packet->ContinuityCounter();
    const int ccExp = (_ccLast + 1) & 0xf;
    uint payloadSize  = TSPacket::PAYLOAD_SIZE;
    uint payloadStart = TSPacket::HEADER_SIZE;

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
        VERBOSE(VB_RECORD, "AddTSPacket: Out of sync!!! "
                "Need to wait for next payloadStart");
        return true;
    }

    if (_pesdataSize >= tlen)
    {
        _badPacket = !VerifyCRC();
        return true;
    }

    return false;
}

/// These are pixel aspect ratios
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
    if (mem188.size() > 1)
    {
        vector<unsigned char*>::iterator it;
        for (it = mem188.begin(); it != mem188.end(); ++it)
            free(*it);
        mem188.clear();
        free188.clear();
        //cerr<<"freeing all 188 blocks"<<endl;
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
        cerr<<alloc4096.size()<<" 4096 blocks remain"<<endl;
        map<unsigned char*, bool>::iterator it;
        for (it = alloc4096.begin(); it != alloc4096.end(); ++it)
        {
            TSPacket *ts = (TSPacket*) it->first;
            cerr<<QString("PES Packet: pid(0x%1)").arg(ts->PID(),0,16);
            if (ts->PID() == 0x1ffb)
            {
                cerr<<QString(" tid(0x%1) ext(0x%2)")
                    .arg(PSIPTable::View(*ts).TableID(),0,16)
                    .arg(PSIPTable::View(*ts).TableIDExtension(),0,16);
            }
            cerr<<endl;
        }
#endif 

    // free the allocator only if more than 1 block was used
    if (mem4096.size() > 1)
    {
        vector<unsigned char*>::iterator it;
        for (it = mem4096.begin(); it != mem4096.end(); ++it)
            free(*it);
        mem4096.clear();
        free4096.clear();
        //cerr<<"freeing all 4096 blocks"<<endl;
    }
}

static QMutex pes_alloc_mutex;

unsigned char *pes_alloc(uint size)
{
    QMutexLocker locker(&pes_alloc_mutex);
    if (size <= 188)
        return get_188_block();
    else if (size <= 4096)
        return get_4096_block();
    return (unsigned char*) malloc(size);
}

void pes_free(unsigned char *ptr)
{
    QMutexLocker locker(&pes_alloc_mutex);
    if (is_188_block(ptr))
        return_188_block(ptr);
    else if (is_4096_block(ptr))
        return_4096_block(ptr);
    else
        free(ptr);
}
