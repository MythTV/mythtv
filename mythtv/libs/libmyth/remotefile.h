#ifndef REMOTEFILE_H_
#define REMOTEFILE_H_

#include <qstring.h>
#include <qmutex.h>

class MythContext;
class QSocket;

class RemoteFile
{
  public:
    RemoteFile(const QString &url, bool needevents = false, 
               int recordernum = -1);
   ~RemoteFile();

    QSocket *getSocket();
    bool isOpen(void);

    void Start(bool events = false);

    void Close(void);
    bool RequestBlock(int size);

    long long Seek(long long pos, int whence, long long curpos = -1);

    int Read(void *data, int size, bool singlefile = false);
    void Reset(void);

    bool SaveAs(QByteArray &data, bool events = true);

    long long GetFileSize(void);

  private:
    QSocket *openSocket(bool control, bool events = false);

    QString path;

    QSocket *sock;
    QSocket *controlSock;

    long long readposition;
    int recordernum;
    int type;

    QString query;
    QString append;

    QMutex lock;

    long long filesize;
};

#endif
