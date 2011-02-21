#ifndef FIFOWRITER
#define FIFOWRITER

// POSIX headers
#include <pthread.h>

// Qt headers
#include <QString>
#include <QMutex>

// MythTV headers
#include "mythtvexp.h"

using namespace std;

class MTV_PUBLIC FIFOWriter
{
  public:
    FIFOWriter(int count, bool sync);
   ~FIFOWriter();

    int FIFOInit(int id, QString desc, QString name, long size, int num_bufs);
    void FIFOWrite(int id, void *buf, long size);
    void FIFODrain(void);

  private:
    void FIFOWriteThread(void);
    static void *FIFOStartThread(void *param);

    struct fifo_buf 
    {
        struct fifo_buf *next;
        unsigned char *data;
        long blksize;
     } **fifo_buf, **fb_inptr, **fb_outptr;

     pthread_t *fifothrds;
     pthread_mutex_t *fifo_lock;
     pthread_cond_t *full_cond, *empty_cond;

     QString *filename, *fbdesc;

     long *maxblksize;
     int *killwr, *fbcount;
     int num_fifos;
     bool usesync;
     int cur_id;
};

#endif

