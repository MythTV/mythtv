#ifndef FILETRANSFER_H_
#define FILETRANSFER_H_

#include <qstring.h>
#include <pthread.h>
#include <qmutex.h>

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
    int RequestBlock(int size);

    long long Seek(long long curpos, long long pos, int whence);

    long long GetFileSize(void);

  private:
    bool readthreadlive;
    QMutex readthreadLock;

    RingBuffer *rbuffer;
    QSocket *sock;
    bool ateof;

    char requestBuffer[256001];
};

#endif
