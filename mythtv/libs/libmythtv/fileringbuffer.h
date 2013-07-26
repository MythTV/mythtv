// Qt headers
#include <QCoreApplication>

// MythTV headers
#include "ringbuffer.h"

class MTV_PUBLIC FileRingBuffer : public RingBuffer
{
    Q_DECLARE_TR_FUNCTIONS(FileRingBuffer)

    friend class RingBuffer;
  public:
    ~FileRingBuffer();

    // Gets
    virtual bool      IsOpen(void)          const;
    virtual long long GetReadPosition(void) const;
    virtual long long GetRealFileSize(void) const;

    // General Commands
    virtual bool OpenFile(const QString &lfilename,
                          uint retry_ms = kDefaultOpenTimeout);
    virtual bool ReOpen(QString newFilename = "");
    virtual long long Seek(long long pos, int whence, bool has_lock);

  protected:
    FileRingBuffer(const QString &lfilename,
                   bool write, bool readahead, int timeout_ms);

    virtual int safe_read(void *data, uint sz)
    {
        if (remotefile)
            return safe_read(remotefile, data, sz);
        else if (fd2 >= 0)
            return safe_read(fd2, data, sz);

        errno = EBADF;
        return -1;
    }
    int safe_read(int fd, void *data, uint sz);
    int safe_read(RemoteFile *rf, void *data, uint sz);
};
