#ifndef FIFOWRITER
#define FIFOWRITER

// Qt headers
#include <QWaitCondition>
#include <QString>
#include <QMutex>

// MythTV headers
#include "mythtvexp.h"
#include "mthread.h"

using namespace std;

class FIFOWriter;

class FIFOThread : public MThread
{
  public:
    FIFOThread() : MThread("FIFOThread") {}
    virtual ~FIFOThread() { wait(); m_parent = nullptr; m_id = -1; }
    void SetId(int id) { m_id = id; }
    void SetParent(FIFOWriter *parent) { m_parent = parent; }
    void run(void) override; // MThread
  private:
    FIFOWriter *m_parent {nullptr};
    int         m_id     {-1};
};

class MTV_PUBLIC FIFOWriter
{
    friend class FIFOThread;
  public:
    FIFOWriter(int count, bool sync);
    FIFOWriter(const FIFOWriter& rhs);
   ~FIFOWriter(void);

    bool FIFOInit(int id, const QString& desc, const QString& name, long size, int num_bufs);
    void FIFOWrite(int id, void *buf, long size);
    void FIFODrain(void);

  private:
    void FIFOWriteThread(int id);

    struct fifo_buf
    {
        struct fifo_buf *next;
        unsigned char *data;
        long blksize;
    };
    fifo_buf       **m_fifoBuf    {nullptr};
    fifo_buf       **m_fbInptr    {nullptr};
    fifo_buf       **m_fbOutptr   {nullptr};

    FIFOThread      *m_fifoThrds  {nullptr};
    QMutex          *m_fifoLock   {nullptr};
    QWaitCondition  *m_fullCond   {nullptr};
    QWaitCondition  *m_emptyCond  {nullptr};

    QString         *m_filename   {nullptr};
    QString         *m_fbDesc     {nullptr};

    long            *m_maxBlkSize {nullptr};
    int             *m_killWr     {nullptr};
    int             *m_fbCount    {nullptr};
    int             *m_fbMaxCount {nullptr};
    int              m_numFifos   {0};
    bool             m_useSync;
};

#endif

