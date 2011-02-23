#ifndef FIFOWRITER
#define FIFOWRITER

// Qt headers
#include <QString>
#include <QMutex>
#include <QWaitCondition>
#include <QThread>

// MythTV headers
#include "mythtvexp.h"

using namespace std;

class FIFOWriter;

class FIFOThread : public QThread
{
    Q_OBJECT
  public:
    FIFOThread() : m_parent(NULL), m_id(-1) {}
    void SetId(int id) { m_id = id; }
    void SetParent(FIFOWriter *parent) { m_parent = parent; }
    void run(void);
  private:
    FIFOWriter *m_parent;
    int m_id;
};

class MTV_PUBLIC FIFOWriter
{
    friend class FIFOThread;
  public:
    FIFOWriter(int count, bool sync);
   ~FIFOWriter();

    int FIFOInit(int id, QString desc, QString name, long size, int num_bufs);
    void FIFOWrite(int id, void *buf, long size);
    void FIFODrain(void);

  private:
    void FIFOWriteThread(int id);

    struct fifo_buf 
    {
        struct fifo_buf *next;
        unsigned char *data;
        long blksize;
     } **fifo_buf, **fb_inptr, **fb_outptr;

     FIFOThread     *fifothrds;
     QMutex         *fifo_lock;
     QWaitCondition *full_cond, *empty_cond;

     QString *filename, *fbdesc;

     long *maxblksize;
     int *killwr, *fbcount;
     int num_fifos;
     bool usesync;
};

#endif

