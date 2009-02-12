#ifndef REMOTEFILE_H_
#define REMOTEFILE_H_

#include <qstring.h>
#include <qmutex.h>

#include "mythexp.h"

class MythContext;
class MythSocket;

class MPUBLIC RemoteFile
{
  public:
    RemoteFile(const QString &url, bool usereadahead = true, int retries = -1);
   ~RemoteFile();

    bool Open();
    void Close(void);

    long long Seek(long long pos, int whence, long long curpos = -1);

    int Read(void *data, int size);
    void Reset(void);

    bool SaveAs(QByteArray &data);

    void SetTimeout(bool fast);

    bool isOpen(void) const
        { return sock && controlSock; }
    long long GetFileSize(void) const
        { return filesize; }

    const MythSocket *getSocket(void) const
        { return sock; }
    MythSocket *getSocket(void)
        { return sock; }

  private:
    MythSocket     *openSocket(bool control);

    QString         path;
    bool            usereadahead;
    int             retries;
    long long       filesize;
    bool            timeoutisfast;
    long long       readposition;
    int             recordernum;

    mutable QMutex  lock;
    MythSocket     *controlSock;
    MythSocket     *sock;
    QString         query;
};

#endif
