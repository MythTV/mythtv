#ifndef RINGBUFFER
#define RINGBUFFER

#include <string>

using namespace std;

class RingBuffer
{
 public:
    RingBuffer(const char *lfilename, bool actasnormalfile, bool write);
    RingBuffer(const char *lfilename, long long size);
    
   ~RingBuffer();

    bool IsOpen(void) { return (fd > 0); }
    
    int Read(void *buf, int count);
    int Write(const void *buf, int count);

    long long Seek(long pos, int whence);

 private:
    string filename;
    
    int fd, fd2;
    
    bool normalfile;
    bool writemode;
    
    long long writepos;
    long long totalwritepos;

    long long readpos;
    long long totalreadpos;

    long long filesize;

    long long wrapcount;
};

#endif
