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
    explicit RemoteFile(QString url = "",
               bool write = false,
               bool usereadahead = true,
               std::chrono::milliseconds timeout = 2s/*RingBuffer::kDefaultOpenTimeout*/,
               const QStringList *possibleAuxiliaryFiles = nullptr);
   ~RemoteFile();

    bool ReOpen(const QString& newFilename);

    long long Seek(long long pos, int whence, long long curpos = -1);

    static bool DeleteFile(const QString &url);
    static bool Exists(const QString &url, struct stat *fileinfo);
    static bool Exists(const QString &url);
    static QString GetFileHash(const QString &url);
    static QDateTime LastModified(const QString &url);
    QDateTime LastModified(void) const;
    static QString FindFile(const QString &filename, const QString &host,
                            const QString &storageGroup, bool useRegex = false,
                            bool allowFallback = false);

    static QStringList FindFileList(const QString &filename, const QString &host,
                                    const QString &storageGroup, bool useRegex = false,
                                    bool allowFallback = false);
    static bool CopyFile(const QString &src, const QString &dst,
                         bool overwrite = false, bool verify = false);
    static bool MoveFile(const QString &src, const QString &dst,
                         bool overwrite = false);

    int Write(const void *data, int size);
    int Read(void *data, int size);
    void Reset(void);
    bool SetBlocking(bool m_block = true);

    bool SaveAs(QByteArray &data);

    void SetTimeout(bool fast);

    bool isOpen(void) const;
    static bool isLocal(const QString &path);
    bool isLocal(void) const;
    long long GetFileSize(void) const;
    long long GetRealFileSize(void);

    QStringList GetAuxiliaryFiles(void) const
        { return m_auxFiles; }

  private:
    bool Open(void);
    bool OpenInternal(void);
    void Close(bool haslock = false);
    bool CheckConnection(bool repos = true);
    bool IsConnected(void);
    bool Resume(bool repos = true);
    long long SeekInternal(long long pos, int whence, long long curpos = -1);

    MythSocket     *openSocket(bool control);

    QString         m_path;
    bool            m_useReadAhead     {true};
    std::chrono::milliseconds m_timeoutMs {2s};
    long long       m_fileSize         {-1};
    bool            m_timeoutIsFast    {false};
    long long       m_readPosition     {0LL};
    long long       m_lastPosition     {0LL};
    bool            m_canResume        {false};
    int             m_recorderNum      {0};

    mutable QMutex  m_lock;
    MythSocket     *m_controlSock      {nullptr};
    MythSocket     *m_sock             {nullptr};
    QString         m_query            {"QUERY_FILETRANSFER %1"};

    bool            m_writeMode        {false};
    bool            m_completed        {false};
    MythTimer       m_lastSizeCheck;

    QStringList     m_possibleAuxFiles;
    QStringList     m_auxFiles;
    int             m_localFile        {-1};
    ThreadedFileWriter *m_fileWriter   {nullptr};
};

#endif
