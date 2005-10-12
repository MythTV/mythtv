// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson
#include "pespacket.h"
#include "mpegtables.h"
#include "mythcontext.h"

#include <vector>
#include <map>

using namespace std;

static inline uint calcOffset(uint cnt)
{
    return (cnt * TSPacket::PAYLOAD_SIZE) + TSPacket::HEADER_SIZE;
}

// return true if complete or broken
bool PESPacket::AddTSPacket(const TSPacket* packet)
{
    uint tlen = Length() + (_pesdata - _fullbuffer) + 4 /* CRC bytes */;

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
    if (ccExp == cc)
    {
        if (calcOffset(_cnt+1) >= _allocSize)
        {
            uint sz = (((_allocSize * 2) + 4095) / 4096) * 4096;
            unsigned char *nbuf = pes_alloc(sz);
            memcpy(nbuf, _fullbuffer, _allocSize);
            pes_free(_fullbuffer);
            _fullbuffer = nbuf;
            _allocSize  = sz;
        }
        memcpy(_fullbuffer    + calcOffset(_cnt),
               packet->data() + TSPacket::HEADER_SIZE,
               TSPacket::PAYLOAD_SIZE);

        _ccLast = cc;
        _cnt++;
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

    if (calcOffset(_cnt) >= tlen)
    {
        if (CalcCRC()==CRC())
        {
            _badPacket=false;
            return true;
        }

        return true;
    }

    return false;
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

#define BLOCKS188 1000
static unsigned char* get_188_block()
{
    if (!free188.size())
    {
        mem188.push_back((unsigned char*) malloc(188 * BLOCKS188));
        free188.reserve(BLOCKS188);
        for (uint i = 0; i < BLOCKS188; ++i)
            free188.push_back(i*188 + mem188.back());
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
    if (alloc188.size())
        free188.push_back(ptr);
    else
    {
        vector<unsigned char*>::iterator it;
        for (it = mem188.begin(); it != mem188.end(); ++it)
            free(*it);
        mem188.clear();
        free188.clear();
        //cerr<<"freeing all 188 blocks"<<endl;
    }
}

#define BLOCKS4096 256
static unsigned char* get_4096_block()
{
    if (!free4096.size())
    {
        mem4096.push_back((unsigned char*) malloc(4096 * BLOCKS4096));
        free4096.reserve(BLOCKS4096);
        for (uint i = 0; i < BLOCKS4096; ++i)
            free4096.push_back(i*4096 + mem4096.back());
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
    if (alloc4096.size())
    {
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
    }
    else
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
