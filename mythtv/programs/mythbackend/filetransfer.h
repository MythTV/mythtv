#ifndef FILETRANSFER_H_
#define FILETRANSFER_H_

// POSIX headers
#include <pthread.h>

// C++ headers
#include <vector>
using namespace std;

// Qt headers
#include <qstring.h>
#include <qmutex.h>

class RingBuffer;
class RefSocket;

class FileTransfer
{
  public:
    FileTransfer(QString &filename, RefSocket *remote);
    FileTransfer(QString &filename, RefSocket *remote,
                 bool usereadahead, int retries);

    RefSocket *getSocket() { return sock; }

    bool isOpen(void);

    void Stop(void);

    void UpRef(void);
    bool DownRef(void);

    void Pause(void);
    void Unpause(void);
    int RequestBlock(int size);

    long long Seek(long long curpos, long long pos, int whence);

    long long GetFileSize(void);

    void SetTimeout(bool fast);

  private:
   ~FileTransfer();

    bool readthreadlive;
    QMutex readthreadLock;

    RingBuffer *rbuffer;
    RefSocket *sock;
    bool ateof;

    vector<char> requestBuffer;

    int refCount;
};

#endif
