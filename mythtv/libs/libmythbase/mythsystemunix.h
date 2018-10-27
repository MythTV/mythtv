/// -*- Mode: c++ -*-

#ifndef _MYTHSYSTEM_UNIX_H_
#define _MYTHSYSTEM_UNIX_H_

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
#include "mythsystemlegacy.h"
#include "mthread.h"

class MythSystemLegacyUnix;

typedef QMap<pid_t, QPointer<MythSystemLegacyUnix> > MSMap_t;
typedef QMap<int, QBuffer *> PMap_t;
typedef QList<QPointer<MythSystemLegacyUnix> > MSList_t;

class MythSystemLegacyIOHandler: public MThread
{
    public:
        explicit MythSystemLegacyIOHandler(bool read);
        ~MythSystemLegacyIOHandler() { wait(); }
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

        fd_set m_fds;
        int    m_maxfd;
        bool   m_read;
        char   m_readbuf[65536];
};

class MythSystemLegacyManager : public MThread
{
    public:
        MythSystemLegacyManager();
        ~MythSystemLegacyManager() { wait(); }
        void run(void) override; // MThread
        void append(MythSystemLegacyUnix *);
        void jumpAbort(void);
    private:
        MSMap_t    m_pMap;
        QMutex     m_mapLock;
        bool       m_jumpAbort;
        QMutex     m_jumpLock;
        QWaitCondition m_wait;
};

class MythSystemLegacySignalManager : public MThread
{
    public:
        MythSystemLegacySignalManager();
        ~MythSystemLegacySignalManager() { wait(); }
        void run(void) override; // MThread
    private:
};


class MBASE_PUBLIC MythSystemLegacyUnix : public MythSystemLegacyPrivate
{
    Q_OBJECT

    public:
        explicit MythSystemLegacyUnix(MythSystemLegacy *parent);
        ~MythSystemLegacyUnix() = default;

        void Fork(time_t timeout) override; // MythSystemLegacyPrivate
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
        pid_t       m_pid;
        time_t      m_timeout;

        int         m_stdpipe[3];
};

#endif // _MYTHSYSTEM_UNIX_H_

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
