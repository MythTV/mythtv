#ifndef REMOTEFILE_H_
#define REMOTEFILE_H_

#include <sys/stat.h>

#include <QDateTime>
#include <QStringList>
#include <QMutex>

#include "mythbaseexp.h"

class MythSocket;

class MBASE_PUBLIC RemoteFile
{
  public:
    RemoteFile(const QString &url = "",
               bool write = false,
               bool usereadahead = true,
               int timeout_ms = 2000/*RingBuffer::kDefaultOpenTimeout*/,
               const QStringList *possibleAuxiliaryFiles = NULL);
   ~RemoteFile();

    bool Open();
    bool ReOpen(QString newFilename);
    void Close(void);

    long long Seek(long long pos, int whence, long long curpos = -1);

    static bool DeleteFile(const QString &url);
    static bool Exists(const QString &url, struct stat *fileinfo);
    static bool Exists(const QString &url);
    static QString GetFileHash(const QString &url);
    static QDateTime LastModified(const QString &url);

    int Write(const void *data, int size);
    int Read(void *data, int size);
    void Reset(void);

    bool SaveAs(QByteArray &data);

    void SetURL(const QString &url) { path = url; }
    void SetTimeout(bool fast);

    bool isOpen(void) const
        { return sock && controlSock; }
    long long GetFileSize(void) const
        { return filesize; }

    QStringList GetAuxiliaryFiles(void) const
        { return auxfiles; }

  private:
    MythSocket     *openSocket(bool control);

    QString         path;
    bool            usereadahead;
    int             timeout_ms;
    long long       filesize;
    bool            timeoutisfast;
    long long       readposition;
    int             recordernum;

    mutable QMutex  lock;
    MythSocket     *controlSock;
    MythSocket     *sock;
    QString         query;

    bool            writemode;

    QStringList     possibleauxfiles;
    QStringList     auxfiles;
};

#endif
