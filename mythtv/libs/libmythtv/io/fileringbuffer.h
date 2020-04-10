// Qt
#include <QCoreApplication>

// MythTV
#include "ringbuffer.h"

class MTV_PUBLIC FileRingBuffer : public RingBuffer
{
    Q_DECLARE_TR_FUNCTIONS(FileRingBuffer)

    friend class RingBuffer;

  public:
    ~FileRingBuffer() override;

    bool      IsOpen          (void) const override;
    long long GetReadPosition (void) const override;
    bool      OpenFile        (const QString &Filename, uint Retry = static_cast<uint>(kDefaultOpenTimeout)) override;
    bool      ReOpen          (const QString& Filename = "") override;

  protected:
    FileRingBuffer(const QString &Filename, bool Write, bool UseReadAhead, int Timeout);
    int       SafeRead       (void *Buffer, uint Size) override;
    int       SafeRead       (int FD, void *Buffer, uint Size);
    int       SafeRead       (RemoteFile *Remote, void *Buffer, uint Size);
    long long GetRealFileSizeInternal(void) const override;
    long long SeekInternal    (long long Position, int Whence) override;
};
