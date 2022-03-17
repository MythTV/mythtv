#ifndef FIFOWRITER
#define FIFOWRITER

// Qt
#include <QWaitCondition>
#include <QString>
#include <QMutex>

// MythTV
#include "libmythbase/mthread.h"
#include "libmythtv/mythtvexp.h"

class MythFIFOWriter;

class MythFIFOThread : public MThread
{
  public:
    MythFIFOThread();
   ~MythFIFOThread() override;

    void SetId(int Id);
    void SetParent(MythFIFOWriter *Parent);

    void run(void) override;

  private:
    MythFIFOWriter *m_parent { nullptr };
    int         m_id     { -1      };
};

class MTV_PUBLIC MythFIFOWriter
{
    friend class MythFIFOThread;

  public:
    MythFIFOWriter(uint Count, bool Sync);
   ~MythFIFOWriter(void);

    bool FIFOInit (uint Id, const QString& Desc, const QString& Name, long Size, int NumBufs);
    void FIFOWrite(uint Id, void *Buffer, long Size);
    void FIFODrain(void);

  private:
    Q_DISABLE_COPY(MythFIFOWriter)
    void FIFOWriteThread(int Id);

    struct MythFifoBuffer
    {
        struct MythFifoBuffer *m_next;
        unsigned char         *m_data;
        long                   m_blockSize;
    };

    MythFifoBuffer **m_fifoBuf    { nullptr };
    MythFifoBuffer **m_fbInptr    { nullptr };
    MythFifoBuffer **m_fbOutptr   { nullptr };

    MythFIFOThread  *m_fifoThrds  { nullptr };
    QMutex          *m_fifoLock   { nullptr };
    QWaitCondition  *m_fullCond   { nullptr };
    QWaitCondition  *m_emptyCond  { nullptr };

    QString         *m_filename   { nullptr };
    QString         *m_fbDesc     { nullptr };

    long            *m_maxBlkSize { nullptr };
    int             *m_killWr     { nullptr };
    int             *m_fbCount    { nullptr };
    int             *m_fbMaxCount { nullptr };
    uint             m_numFifos   { 0       };
    bool             m_useSync;
};

#endif

