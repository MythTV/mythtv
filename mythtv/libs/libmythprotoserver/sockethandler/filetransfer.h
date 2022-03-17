#ifndef FILETRANSFER_H_
#define FILETRANSFER_H_

// C++ headers
#include <cstdint>
#include <vector>

#include <QMutex>
#include <QString>
#include <QWaitCondition>

#include "libmythbase/mythsocket.h"
#include "libmythprotoserver/sockethandler.h"

class ProgramInfo;
class MythMediaBuffer;

class FileTransfer : public SocketHandler
{
  public:
    FileTransfer(QString &filename, MythSocket *remote,
                 MythSocketManager *parent,
                 bool usereadahead, std::chrono::milliseconds timeout);
    FileTransfer(QString &filename, MythSocket *remote,
                 MythSocketManager *parent, bool write);

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

    volatile bool  m_readthreadlive {true};
    bool           m_readsLocked {false};
    QWaitCondition m_readsUnlockedCond;

    ProgramInfo *m_pginfo {nullptr};
    MythMediaBuffer  *m_rbuffer {nullptr};
    bool m_ateof {false};

    std::vector<char> m_requestBuffer;

    QMutex m_lock;

    bool m_writemode {false};
};

#endif
