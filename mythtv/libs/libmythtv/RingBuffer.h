#ifndef RINGBUFFER
#define RINGBUFFER

#include <string>
#include <pthread.h>

using namespace std;

class RingBuffer
{
 public:
    RingBuffer(const string &lfilename, bool write);
    RingBuffer(const string &lfilename, long long size, long long smudge);
    
   ~RingBuffer();

    bool IsOpen(void) { return (fd > 0 || fd2 > 0); }
    
    int Read(void *buf, int count);
    int Write(const void *buf, int count);

    void TransitionToFile(const string &lfilename);
    void TransitionToRing(void);

    long long Seek(long long pos, int whence);

    long long GetReadPosition(void) { return readpos; }
    long long GetTotalReadPosition(void) { return totalreadpos; }
    long long GetWritePosition(void) { return writepos; }
    long long GetTotalWritePosition(void) { return totalwritepos; }
    long long GetFileSize(void) { return filesize; }

    long long GetFreeSpace(void);

    long long GetFreeSpaceWithReadChange(long long readchange);

    void Reset(void);

    void StopReads(void) { stopreads = true; }

 private:
    string filename;

    int fd, fd2;
    int dumpfd;   
 
    bool normalfile;
    bool writemode;
    
    long long writepos;
    long long totalwritepos;

    long long readpos;
    long long totalreadpos;

    long long filesize;
    long long smudgeamount;

    long long wrapcount;

    bool stopreads;

    pthread_rwlock_t rwlock;

    long long transitionpoint;
};

#endif
