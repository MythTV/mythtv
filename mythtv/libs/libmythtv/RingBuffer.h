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
    RingBuffer(const QString &lfilename, bool write,
               bool usereadahead = true, uint read_retries = 4);
   ~RingBuffer();

    // Sets
    void SetWriteBufferSize(int newSize);
    void SetWriteBufferMinWriteSize(int newMinSize);
    void CalcReadAheadThresh(uint estbitrate);

    // Gets
    /// Returns name of file used by this RingBuffer
    QString   GetFilename(void)      const { return filename; }
    /// Returns ReadBufAvail(void)
    int       DataInReadAhead(void)  const { return ReadBufAvail(); }
    /// Returns value of stopreads
    /// \sa StartReads(void), StopReads(void)
    bool      GetStopReads(void)     const { return stopreads; }
    /// Returns false iff read-ahead is not
    /// running and read-ahead is not paused.
    bool      isPaused(void)         const
        { return (!readaheadrunning) ? true : readaheadpaused; }
    long long GetReadPosition(void)  const;
    long long GetWritePosition(void) const;
    long long GetRealFileSize(void)  const;
    bool      IsOpen(void)           const;

    // General Commands
    void OpenFile(const QString &lfilename, uint retryCount = 4);
    int  Read(void *buf, int count);
    void Reset(bool full = false);

    // Seeks
    long long Seek(long long pos, int whence);

    // Pause commands
    void Pause(void);
    void Unpause(void);
    void WaitForPause(void);
    
    // Start/Stop commands
    void Start(void);
    void StopReads(void);
    void StartReads(void);

    // LiveTVChain support
    bool LiveMode(void) const;
    void SetLiveMode(LiveTVChain *chain);
    /// Tells RingBuffer whether to igonre the end-of-file
    void IgnoreLiveEOF(bool ignore) { ignoreliveeof = ignore; }

    // ThreadedFileWriter proxies
    int  Write(const void *buf, uint count);
    bool IsIOBound(void) const;
    void WriterFlush(void);
    void Sync(void);
    long long WriterSeek(long long pos, int whence);

    // DVDRingBuffer proxies
    /// Returns true if this is a DVD backed RingBuffer.
    bool isDVD(void) const { return dvdPriv; }
    void getPartAndTitle(int &title, int &part);
    void getDescForPos(QString &desc);
    void nextTrack(void);
    void prevTrack(void);
    
  protected:
    static void *StartReader(void *type);
    void ReadAheadThread(void);

  private:
    int safe_read_dvd(void *data, uint sz);
    int safe_read(int fd, void *data, uint sz);
    int safe_read(RemoteFile *rf, void *data, uint sz);

    int ReadFromBuf(void *buf, int count);

    int ReadBufFree(void) const;
    int ReadBufAvail(void) const;

    void StartupReadAheadThread(void);
    void ResetReadAhead(long long newinternal);
    void KillReadAheadThread(void);

  private:
    QString filename;

    ThreadedFileWriter *tfw;
    int fd2;

    bool writemode;
 
    long long readpos;
    long long writepos;

    bool stopreads;

    mutable pthread_rwlock_t rwlock;

    int recorder_num;
    RemoteEncoder *remoteencoder;
    RemoteFile *remotefile;

    mutable QMutex readAheadLock;
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
    bool ignoreliveeof;

    // constants
    static const uint kBufferSize;
    static const uint kReadTestSize;
};

#endif
