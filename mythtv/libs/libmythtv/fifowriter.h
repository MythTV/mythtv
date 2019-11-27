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
    fifo_buf       **m_fifo_buf   {nullptr};
    fifo_buf       **m_fb_inptr   {nullptr};
    fifo_buf       **m_fb_outptr  {nullptr};

    FIFOThread      *m_fifothrds  {nullptr};
    QMutex          *m_fifo_lock  {nullptr};
    QWaitCondition  *m_full_cond  {nullptr};
    QWaitCondition  *m_empty_cond {nullptr};

    QString         *m_filename   {nullptr};
    QString         *m_fbdesc     {nullptr};

    long            *m_maxblksize {nullptr};
    int             *m_killwr     {nullptr};
    int             *m_fbcount    {nullptr};
    int             *m_fbmaxcount {nullptr};
    int              m_num_fifos;
    bool             m_usesync;
};

#endif

