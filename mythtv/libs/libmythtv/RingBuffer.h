#ifndef RINGBUFFER
#define RINGBUFFER

#include <qstring.h>
#include <pthread.h>

class MythContext;
class RemoteFile;

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
    pthread_t writer;
};


class RingBuffer
{
 public:
    RingBuffer(MythContext *context, const QString &lfilename, bool write,
               bool needevents = false);
    RingBuffer(MythContext *context, const QString &lfilename, long long size, 
               long long smudge, int recordernum = 0);
    
   ~RingBuffer();

    bool IsOpen(void) { return (tfw || fd2 > 0 || remotefile); }
    
    int Read(void *buf, int count);
    int Write(const void *buf, int count);

    // this should _only_ be used when transitioning from livetv->recording
    int WriteToDumpFile(const void *buf, int count);

    void TransitionToFile(const QString &lfilename);
    void TransitionToRing(void);

    long long Seek(long long pos, int whence);
    long long WriterSeek(long long pos, int whence);

    long long GetReadPosition(void);
    long long GetTotalReadPosition(void);
    long long GetWritePosition(void);
    long long GetTotalWritePosition(void);
    long long GetFileSize(void) { return filesize; }
    long long GetSmudgeSize(void) { return smudgeamount; }

    long long GetFileWritePosition(void);
    
    long long GetFreeSpace(void);

    long long GetFreeSpaceWithReadChange(long long readchange);

    void Reset(void);

    void StopReads(void) { stopreads = true; }
    void StartReads(void) { stopreads = false; }
    bool GetStopReads(void) { return stopreads; }

    bool LiveMode(void) { return !normalfile; }

    const QString GetFilename(void) { return filename; }

    bool IsIOBound(void);

    void Start(void);

 private:
    int safe_read(int fd, void *data, unsigned sz);
    int safe_read(RemoteFile *rf, void *data, unsigned sz);

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

    MythContext *m_context;
    int recorder_num;
    RemoteFile *remotefile;
};

#endif
