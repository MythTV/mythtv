#ifndef RINGBUFFER
#define RINGBUFFER

#include <qstring.h>
#include <qwaitcondition.h>
#include <qmutex.h>
#include <pthread.h>

class RemoteFile;
class RemoteEncoder;
class ThreadedFileWriter;
class DVDRingBufferPriv;
class LiveTVChain;

class RingBuffer
{
  public:
    // can explicitly disable the readahead thread here, or just by not
    // calling Start()
    RingBuffer(const QString &lfilename, bool write, bool usereadahead = true);
   ~RingBuffer();

    void OpenFile(const QString &lfilename, uint retryCount = 4);
    bool IsOpen(void);
    
    int Read(void *buf, int count);
    int Write(const void *buf, int count);
    void Sync(void);

    int DataInReadAhead(void);

    long long Seek(long long pos, int whence);
    long long WriterSeek(long long pos, int whence);
    void WriterFlush(void);

    void SetWriteBufferSize(int newSize);
    void SetWriteBufferMinWriteSize(int newMinSize);

    long long GetReadPosition(void);
    long long GetWritePosition(void);

    void Reset(bool full = false);

    void StopReads(void);
    void StartReads(void);
    bool GetStopReads(void) const { return stopreads; }

    bool LiveMode(void);
    void SetLiveMode(LiveTVChain *chain);

    const QString GetFilename(void) const { return filename; }

    bool IsIOBound(void);

    void Start(void);

    void Pause(void);
    void Unpause(void);
    bool isPaused(void);
    void WaitForPause(void);
    
    bool isDVD(void) const { return dvdPriv; }
    
    void CalcReadAheadThresh(int estbitrate);

    long long GetRealFileSize(void);

    void getPartAndTitle( int& title, int& part);
    void getDescForPos(QString& desc);
    
    void nextTrack(void);
    void prevTrack(void);
    
  protected:
    static void *startReader(void *type);
    void ReadAheadThread(void);

  private:
    void Init(void);
    
    int safe_read_dvd(void *data, unsigned sz);
    int safe_read(int fd, void *data, unsigned sz);
    int safe_read(RemoteFile *rf, void *data, unsigned sz);

    QString filename;

    ThreadedFileWriter *tfw;
    int fd2;

    bool writemode;
 
    long long readpos;
    long long writepos;

    bool stopreads;

    pthread_rwlock_t rwlock;

    int recorder_num;
    RemoteEncoder *remoteencoder;
    RemoteFile *remotefile;

    int ReadFromBuf(void *buf, int count);

    inline int ReadBufFree(void);
    inline int ReadBufAvail(void);

    void StartupReadAheadThread(void);
    void ResetReadAhead(long long newinternal);
    void KillReadAheadThread(void);

    QMutex readAheadLock;
    pthread_t reader;

    bool startreadahead;
    char *readAheadBuffer;
    bool readaheadrunning;
    bool readaheadpaused;
    bool pausereadthread;
    int rbrpos;
    int rbwpos;
    long long internalreadpos;
    bool ateof;
    bool readsallowed;
    bool wantseek;
    int fill_threshold;
    int fill_min;

    int readblocksize;

    QWaitCondition pauseWait;

    int wanttoread;
    QWaitCondition availWait;
    QMutex availWaitMutex;

    QWaitCondition readsAllowedWait;

    int numfailures;

    bool commserror;

    DVDRingBufferPriv *dvdPriv;

    bool oldfile;

    LiveTVChain *livetvchain;

    // constants
    static const uint kBufferSize;
    static const uint kReadTestSize;
};

#endif
