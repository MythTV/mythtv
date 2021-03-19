#ifndef FILETRANSFER_H_
#define FILETRANSFER_H_

// C++ headers
#include <cstdint>
#include <vector>

// Qt headers
#include <QMutex>
#include <QWaitCondition>

// MythTV headers
#include "referencecounter.h"

class ProgramInfo;
class MythMediaBuffer;
class MythSocket;
class QString;

class FileTransfer : public ReferenceCounter
{
    friend class QObject; // quiet OSX gcc warning

  public:
    FileTransfer(QString &filename, MythSocket *remote,
                 bool usereadahead, std::chrono::milliseconds timeout);
    FileTransfer(QString &filename, MythSocket *remote, bool write);

    MythSocket *getSocket() { return m_sock; }

    bool isOpen(void);
    bool ReOpen(const QString& newFilename = "");

    void Stop(void);

    void Pause(void);
    void Unpause(void);
    int RequestBlock(int size);
    int WriteBlock(int size);

    long long Seek(long long curpos, long long pos, int whence);

    uint64_t GetFileSize(void);
    QString GetFileName(void);

    void SetTimeout(bool fast);

  private:
   ~FileTransfer() override;

    volatile bool   m_readthreadlive    {true};
    bool            m_readsLocked       {false};
    QWaitCondition  m_readsUnlockedCond;

    ProgramInfo    *m_pginfo            {nullptr};
    MythMediaBuffer* m_rbuffer          {nullptr};
    MythSocket     *m_sock              {nullptr};
    bool            m_ateof             {false};

    std::vector<char> m_requestBuffer;

    QMutex          m_lock;

    bool            m_writemode         {false};
};

#endif
