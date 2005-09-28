// -*- Mode: c++ -*-

#include <qstring.h>
#include <qmutex.h>
#include <qwaitcondition.h>

class RingBuffer;

class RingBufferInfo
{
  public:
    RingBufferInfo() :
        // Basic RingBuffer info
        ringBuffer(NULL),      filename(""),
        fileextension("nuv"),
        // Data Socket stuff for streaming
        dataSocket(NULL),      requestBuffer(NULL),
        live(false),           paused(false),
        lock(false)
    {
        requestBuffer = new char[kRequestBufferSize+64];
    }
   ~RingBufferInfo()
    {
        SetRingBuffer(NULL);
        if (requestBuffer)
            delete[] requestBuffer;
    }

    // Sets
    void SetDataSocket(QSocket *sock);
    void SetRingBuffer(RingBuffer*);
    void SetFilename(QString fn)      { filename = fn;        }
    void SetFileExtension(QString fe) { fileextension = fe;   }
    inline void SetFilename(QString path, int cardnum);
    inline void SetFilename(QString path, QString file);
    void SetAlive(bool live);

    // Gets
    QSocket *GetDataSocket(void);
    RingBuffer *GetRingBuffer(void)   { return ringBuffer;    }
    QString GetFilename(void) const   { return filename;      }
    QString GetUIFilename(void) const { return filename.section("/", -1); }
    QString GetFileExtension() const  { return fileextension; }
    bool IsAlive(void) const          { return live;          }
    bool IsPaused(void) const         { return paused > 0;    }

    // Commands
    void UnlinkRingBufferFile(void)
        { if (!GetFilename().isEmpty()) unlink(GetFilename().ascii());
          SetFilename(""); }
    int  RequestBlock(uint size);
    long long Seek(long long curpos, long long pos, int whence);

    // Pausing reads
    void Pause(void);
    void Unpause(void);

  private:
    // Locking
    void GetLock(bool requireUnpaused);
    void ReturnLock(void);

  private:
    // Basic RingBuffer info
    RingBuffer    *ringBuffer;
    QString        filename;
    QString        fileextension;

    // Data Socket stuff for streaming
    QSocket       *dataSocket;
    char          *requestBuffer;
    bool           live;
    int            paused;
    QWaitCondition unpausedWait;
    QMutex         lock;


    /** Milliseconds to wait for whole amount of data requested to be
     *  sent in RequestBlock if we have not hit the end of the file.
     */
    static const uint kStreamedFileReadTimeout;
    /** The request buffer should be big enough to make disk reads
     *  efficient, but small enough so that we don't block too long
     *  waiting for the disk read in LiveTV mode.
     */
    static const uint kRequestBufferSize;
};

inline void RingBufferInfo::SetFilename(QString path, int cardnum)
{
    SetFilename(QString("%1/ringbuf%2.nuv").arg(path).arg(cardnum));
}

inline void RingBufferInfo::SetFilename(QString path, QString file)
{
    SetFilename(QString("%1/%2").arg(path).arg(file));
}
