#ifndef RINGBUFFER
#define RINGBUFFER

#include <qstring.h>
#include <pthread.h>

class ThreadedFileWriter
{
public:
    ThreadedFileWriter(const char *filename, int flags, mode_t mode);
    ~ThreadedFileWriter();                 /* commits all writes and closes the file. */

    long long Seek(long long pos, int whence);
    unsigned Write(const void *data, unsigned count);

    unsigned BufUsed();  /* # of bytes queued for write by the write thread */
    unsigned BufFree();  /* # of bytes that can be written, without blocking */

protected:
    static void *boot_writer(void *);
    void DiskLoop(); /* The thread that actually calls write(). */

private:
    int fd;
    char *buf;
    unsigned rpos,wpos;
    pthread_mutex_t buflock;
    int in_dtor;
    int child_live;
    pthread_t writer;
};


class RingBuffer
{
 public:
    RingBuffer(const QString &lfilename, bool write);
    RingBuffer(const QString &lfilename, long long size, long long smudge);
    
   ~RingBuffer();

    bool IsOpen(void) { return (tfw || fd2 > 0); }
    
    int Read(void *buf, int count);
    int Write(const void *buf, int count);

    // this should _only_ be used when transitioning from livetv->recording
    int WriteToDumpFile(const void *buf, int count);

    void TransitionToFile(const QString &lfilename);
    void TransitionToRing(void);

    long long Seek(long long pos, int whence);
    long long WriterSeek(long long pos, int whence);

    long long GetReadPosition(void) { return readpos; }
    long long GetTotalReadPosition(void) { return totalreadpos; }
    long long GetWritePosition(void) { return writepos; }
    long long GetTotalWritePosition(void) { return totalwritepos; }
    long long GetFileSize(void) { return filesize; }
    long long GetSmudgeSize(void) { return smudgeamount; }

    long long GetFileWritePosition(void);
    
    long long GetFreeSpace(void);

    long long GetFreeSpaceWithReadChange(long long readchange);

    void Reset(void);

    void StopReads(void) { stopreads = true; }
    bool LiveMode(void) { return !normalfile; }

    const QString GetFilename(void) { return filename; }

    bool IsIOBound(void);

 private:
    QString filename;

    ThreadedFileWriter *tfw, *dumpfw;
    int fd2;
 
    bool normalfile;
    bool writemode;
    
    long long writepos;
    long long totalwritepos;
    long long dumpwritepos;

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
