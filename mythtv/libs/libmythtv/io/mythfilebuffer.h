// Qt
#include <QCoreApplication>

// MythTV
#include "io/mythmediabuffer.h"

class MTV_PUBLIC MythFileBuffer : public MythMediaBuffer
{
    Q_DECLARE_TR_FUNCTIONS(MythFileBuffer)

    friend class MythMediaBuffer;

  public:
    ~MythFileBuffer() override;

    bool      IsOpen          (void) const override;
    long long GetReadPosition (void) const override;
    bool      OpenFile        (const QString &Filename, std::chrono::milliseconds Retry = kDefaultOpenTimeout) override;
    bool      ReOpen          (const QString& Filename = "") override;

  protected:
    MythFileBuffer(const QString &Filename, bool Write, bool UseReadAhead, std::chrono::milliseconds Timeout);
    int       SafeRead        (void *Buffer, uint Size) override;
    int       SafeRead        (int FD, void *Buffer, uint Size);
    int       SafeRead        (RemoteFile *Remote, void *Buffer, uint Size);
    long long GetRealFileSizeInternal(void) const override;
    long long SeekInternal    (long long Position, int Whence) override;
};
