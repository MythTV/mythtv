#ifndef REMOTEFILE_H_
#define REMOTEFILE_H_

#include <qstring.h>
#include <pthread.h>

class MythContext;
class QSocket;

class RemoteFile
{
  public:
    RemoteFile(QString &url, bool needevents = false, int recordernum = -1);
   ~RemoteFile();

    QSocket *getSocket();
    bool isOpen(void);

    void Start(void);

    void Close(void);
    bool RequestBlock(int size);

    long long Seek(long long pos, int whence, long long curpos = -1);

    int Read(void *data, int size);
    void Reset(void);

  private:
    QSocket *openSocket(bool control);

    QString path;

    QSocket *sock;
    QSocket *controlSock;

    long long readposition;
    int recordernum;
    int type;

    QString query;
    QString append;

    pthread_mutex_t lock;
};

#endif
