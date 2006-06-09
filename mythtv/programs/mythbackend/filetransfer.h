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
class MythSocket;

class FileTransfer
{
  public:
    FileTransfer(QString &filename, MythSocket *remote);
    FileTransfer(QString &filename, MythSocket *remote,
                 bool usereadahead, int retries);

    MythSocket *getSocket() { return sock; }

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
    MythSocket *sock;
    bool ateof;

    vector<char> requestBuffer;

    int refCount;
};

#endif
