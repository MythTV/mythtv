/// -*- Mode: c++ -*-

#ifndef _MYTHSYSTEM_WINDOWS_H_
#define _MYTHSYSTEM_WINDOWS_H_

#include <windows.h>
#include <csignal>

#include <QWaitCondition>
#include <QBuffer>
#include <QObject>
#include <QMutex>
#include <QList>
#include <QMap>

#include "mythsystemprivate.h"
#include "mythbaseexp.h"
#include "mythsystemlegacy.h"
#include "mthread.h"

class MythSystemLegacyWindows;

using MSMap_t  = QMap<HANDLE, MythSystemLegacyWindows *>;
using PMap_t   = QMap<HANDLE, QBuffer *>;
using MSList_t = QList<MythSystemLegacyWindows *>;

class MythSystemLegacyIOHandler: public MThread
{
    public:
        explicit MythSystemLegacyIOHandler(bool read);
        ~MythSystemLegacyIOHandler() { wait(); }
        void   run(void) override; // MThread

        void   insert(HANDLE h, QBuffer *buff);
        void   Wait(HANDLE h);
        void   remove(HANDLE h);
        void   wake();

    private:
        bool   HandleRead(HANDLE h, QBuffer *buff);
        bool   HandleWrite(HANDLE h, QBuffer *buff);

        QMutex          m_pWaitLock;
        QWaitCondition  m_pWait;
        QMutex          m_pLock;
        PMap_t          m_pMap;

        bool    m_read;
        char    m_readbuf[65536];
};

class MythSystemLegacyManager : public MThread
{
    public:
        MythSystemLegacyManager()
            : MThread("SystemManager") {}
        ~MythSystemLegacyManager();
        void run(void) override; // MThread
        void append(MythSystemLegacyWindows *);
        void jumpAbort(void);

    private:
        void ChildListRebuild();

        int        m_childCount {0};
        HANDLE    *m_children   {nullptr};
        MSMap_t    m_pMap;
        QMutex     m_mapLock;

        bool       m_jumpAbort  {false};
        QMutex     m_jumpLock;
};

// spawn separate thread for signals to prevent manager
// thread from blocking in some slot
class MythSystemLegacySignalManager : public MThread
{
    public:
        MythSystemLegacySignalManager()
            : MThread("SystemSignalManager") {}
        ~MythSystemLegacySignalManager() { wait(); }
        void run(void) override; // MThread
    private:
};


class MBASE_PUBLIC MythSystemLegacyWindows : public MythSystemLegacyPrivate
{
    Q_OBJECT

    public:
        explicit MythSystemLegacyWindows(MythSystemLegacy *parent);
        ~MythSystemLegacyWindows() = default;

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
        HANDLE      m_child   {nullptr};
        SystemTime  m_timeout {0s};

        HANDLE      m_stdpipe[3];
};

#endif // _MYTHSYSTEM_WINDOWS_H_

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
