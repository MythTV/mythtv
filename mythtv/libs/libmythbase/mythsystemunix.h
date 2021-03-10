/// -*- Mode: c++ -*-

#ifndef MYTHSYSTEM_UNIX_H
#define MYTHSYSTEM_UNIX_H

#include <array>
#include <csignal>
#include <sys/select.h>

#include <QObject>
#include <QString>
#include <QStringList>
#include <QMap>
#include <QList>
#include <QBuffer>
#include <QWaitCondition>
#include <QMutex>
#include <QPointer>

#include "mythsystemprivate.h"
#include "mythbaseexp.h"
#include "mythchrono.h"
#include "mythsystemlegacy.h"
#include "mthread.h"

class MythSystemLegacyUnix;

using MSMap_t  = QMap<pid_t, QPointer<MythSystemLegacyUnix> >;
using PMap_t   = QMap<int, QBuffer *>;
using MSList_t = QList<QPointer<MythSystemLegacyUnix> >;

class MythSystemLegacyIOHandler: public MThread
{
    public:
        explicit MythSystemLegacyIOHandler(bool read)
            : MThread(QString("SystemIOHandler%1").arg(read ? "R" : "W")),
              m_read(read) {};
        ~MythSystemLegacyIOHandler() override { wait(); }
        void   run(void) override; // MThread

        void   insert(int fd, QBuffer *buff);
        void   Wait(int fd);
        void   remove(int fd);
        void   wake();

    private:
        void   HandleRead(int fd, QBuffer *buff);
        void   HandleWrite(int fd, QBuffer *buff);
        void   BuildFDs();

        QMutex          m_pWaitLock;
        QWaitCondition  m_pWait;
        QMutex          m_pLock;
        PMap_t          m_pMap;

        fd_set m_fds   {};
        int    m_maxfd {-1};
        bool   m_read  {true};
        std::array<char,65536> m_readbuf {};
};

// spawn separate thread for signals to prevent manager
// thread from blocking in some slot
class MythSystemLegacyManager : public MThread
{
    public:
        MythSystemLegacyManager()
            : MThread("SystemManager") {}
        ~MythSystemLegacyManager() override { wait(); }
        void run(void) override; // MThread
        void append(MythSystemLegacyUnix *ms);
        void jumpAbort(void);
    private:
        MSMap_t    m_pMap;
        QMutex     m_mapLock;
        bool       m_jumpAbort {false};
        QMutex     m_jumpLock;
        QWaitCondition m_wait;
};

class MythSystemLegacySignalManager : public MThread
{
    public:
        MythSystemLegacySignalManager()
            : MThread("SystemSignalManager") {}
        ~MythSystemLegacySignalManager() override { wait(); }
        void run(void) override; // MThread
    private:
};


class MBASE_PUBLIC MythSystemLegacyUnix : public MythSystemLegacyPrivate
{
    Q_OBJECT

    public:
        explicit MythSystemLegacyUnix(MythSystemLegacy *parent);
        ~MythSystemLegacyUnix() override = default;

        void Fork(std::chrono::seconds timeout) override; // MythSystemLegacyPrivate
        void Manage(void) override; // MythSystemLegacyPrivate

        void Term(bool force=false) override; // MythSystemLegacyPrivate
        void Signal(int sig) override; // MythSystemLegacyPrivate
        void JumpAbort(void) override; // MythSystemLegacyPrivate

        bool ParseShell(const QString &cmd, QString &abscmd,
                        QStringList &args) override; // MythSystemLegacyPrivate

        friend class MythSystemLegacyManager;
        friend class MythSystemLegacySignalManager;
        friend class MythSystemLegacyIOHandler;

    private:
        pid_t       m_pid     {0};
        SystemTime  m_timeout {0s};

        std::array<int,3> m_stdpipe {-1, -1, -1};
};

#endif // MYTHSYSTEM_UNIX_H

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
