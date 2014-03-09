#ifndef FILETRANSFER_H_
#define FILETRANSFER_H_

// ANSI C headers
#include <stdint.h>

using namespace std;

#include <QMutex>
#include <QString>
#include <QWaitCondition>

#include <vector>

#include "mythsocket.h"
#include "sockethandler.h"

class ProgramInfo;
class RingBuffer;

class FileTransfer : public SocketHandler
{
  public:
    FileTransfer(QString &filename, MythSocket *remote,
                 MythSocketManager *parent,
                 bool usereadahead, int timeout_ms);
    FileTransfer(QString &filename, MythSocket *remote,
                 MythSocketManager *parent, bool write);

    bool isOpen(void);
    bool ReOpen(QString newFilename = "");

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
   ~FileTransfer();

    volatile bool  readthreadlive;
    bool           readsLocked;
    QWaitCondition readsUnlockedCond;

    ProgramInfo *pginfo;
    RingBuffer *rbuffer;
    bool ateof;

    vector<char> requestBuffer;

    QMutex lock;

    bool writemode;
};

#endif
