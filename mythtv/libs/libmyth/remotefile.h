#ifndef REMOTEFILE_H_
#define REMOTEFILE_H_

#include <qstring.h>
#include <qmutex.h>

class MythContext;
class QSocketDevice;

class RemoteFile
{
  public:
    RemoteFile(const QString &url, int recordernum = -1);
   ~RemoteFile();

    QSocketDevice *getSocket();
    bool isOpen(void);

    void Close(void);

    long long Seek(long long pos, int whence, long long curpos = -1);

    int Read(void *data, int size);
    void Reset(void);

    bool SaveAs(QByteArray &data);

    long long GetFileSize(void);

  private:
    QSocketDevice *openSocket(bool control);

    QString path;

    QSocketDevice *sock;
    QSocketDevice *controlSock;

    long long readposition;
    int recordernum;

    QString query;

    QMutex lock;

    long long filesize;
};

#endif
