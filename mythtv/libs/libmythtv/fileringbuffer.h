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
    bool      IsOpen(void)          const override; // RingBuffer
    long long GetReadPosition(void) const override; // RingBuffer

    // General Commands
    bool OpenFile(const QString &lfilename,
                  uint retry_ms = kDefaultOpenTimeout) override; // RingBuffer
    bool ReOpen(QString newFilename = "") override; // RingBuffer

  protected:
    FileRingBuffer(const QString &lfilename,
                   bool write, bool readahead, int timeout_ms);

    int safe_read(void *data, uint sz) override // RingBuffer
    {
        if (m_remotefile)
            return safe_read(m_remotefile, data, sz);
        else if (m_fd2 >= 0)
            return safe_read(m_fd2, data, sz);

        errno = EBADF;
        return -1;
    }
    int safe_read(int fd, void *data, uint sz);
    int safe_read(RemoteFile *rf, void *data, uint sz);
    long long GetRealFileSizeInternal(void) const override; // RingBuffer
    long long SeekInternal(long long pos, int whence) override; // RingBuffer
};
