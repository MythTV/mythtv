#ifndef REMOTEFILE_H_
#define REMOTEFILE_H_

#include <sys/stat.h>

#include <QDateTime>
#include <QStringList>
#include <QMutex>

#include "mythbaseexp.h"
#include "mythtimer.h"

class MythSocket;
class QFile;
class ThreadedFileWriter;

class MBASE_PUBLIC RemoteFile
{
  public:
    RemoteFile(const QString &url = "",
               bool write = false,
               bool usereadahead = true,
               int timeout_ms = 2000/*RingBuffer::kDefaultOpenTimeout*/,
               const QStringList *possibleAuxiliaryFiles = NULL);
   ~RemoteFile();

    bool ReOpen(QString newFilename);

    long long Seek(long long pos, int whence, long long curpos = -1);

    static bool DeleteFile(const QString &url);
    static bool Exists(const QString &url, struct stat *fileinfo);
    static bool Exists(const QString &url);
    static QString GetFileHash(const QString &url);
    static QDateTime LastModified(const QString &url);
    QDateTime LastModified(void) const;
    static QString FindFile(const QString &filename, const QString &host, const QString &storageGroup);
    static bool CopyFile(const QString &src, const QString &dest);

    int Write(const void *data, int size);
    int Read(void *data, int size);
    void Reset(void);
    bool SetBlocking(bool block = true);

    bool SaveAs(QByteArray &data);

    void SetTimeout(bool fast);

    bool isOpen(void) const;
    static bool isLocal(const QString &path);
    bool isLocal(void) const;
    long long GetFileSize(void) const;
    long long GetRealFileSize(void);

    QStringList GetAuxiliaryFiles(void) const
        { return auxfiles; }

  private:
    bool Open(void);
    bool OpenInternal(void);
    void Close(bool haslock = false);
    bool CheckConnection(bool repos = true);
    bool IsConnected(void);
    bool Resume(bool repos = true);
    long long SeekInternal(long long pos, int whence, long long curpos = -1);

    MythSocket     *openSocket(bool control);

    QString         path;
    bool            usereadahead;
    int             timeout_ms;
    long long       filesize;
    bool            timeoutisfast;
    long long       readposition;
    long long       lastposition;
    bool            canresume;
    int             recordernum;

    mutable QMutex  lock;
    MythSocket     *controlSock;
    MythSocket     *sock;
    QString         query;

    bool            writemode;
    bool            completed;
    MythTimer       lastSizeCheck;

    QStringList     possibleauxfiles;
    QStringList     auxfiles;
    QFile          *localFile;
    ThreadedFileWriter *fileWriter;
};

#endif
