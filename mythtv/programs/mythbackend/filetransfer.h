#ifndef FILETRANSFER_H_
#define FILETRANSFER_H_

#include <qstring.h>
#include <pthread.h>

class RingBuffer;
class QSocket;

class FileTransfer
{
  public:
    FileTransfer(QString &filename, QSocket *remote);
   ~FileTransfer();

    QSocket *getSocket() { return sock; }

    bool isOpen(void);

    void Stop(void);

    void Pause(void);
    void Unpause(void);
    bool RequestBlock(int size);

    long long Seek(long long curpos, long long pos, int whence);

    long long GetFileSize(void);

  protected:
    void DoFTReadThread(void);
    static void *FTReadThread(void *param);

  private:
    int readrequest;
    bool readthreadlive;
    pthread_mutex_t readthreadLock;
    pthread_t readthread;

    RingBuffer *rbuffer;
    QSocket *sock;
    bool ateof;
};

#endif
