/// -*- Mode: c++ -*-

#ifndef _MYTHSYSTEM_WINDOWS_H_
#define _MYTHSYSTEM_WINDOWS_H_

#include <Windows.h>
#include <signal.h>

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

typedef QMap<HANDLE, MythSystemLegacyWindows *> MSMap_t;
typedef QMap<HANDLE, QBuffer *> PMap_t;
typedef QList<MythSystemLegacyWindows *> MSList_t;

class MythSystemLegacyIOHandler: public MThread
{
    public:
        MythSystemLegacyIOHandler(bool read);
        ~MythSystemLegacyIOHandler() { wait(); }
        void   run(void);

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
        MythSystemLegacyManager();
        ~MythSystemLegacyManager();
        void run(void);
        void append(MythSystemLegacyWindows *);
        void jumpAbort(void);

    private:
        void ChildListRebuild();

        int        m_childCount;
        HANDLE    *m_children;
        MSMap_t    m_pMap;
        QMutex     m_mapLock;

        bool       m_jumpAbort;
        QMutex     m_jumpLock;
};

class MythSystemLegacySignalManager : public MThread
{
    public:
        MythSystemLegacySignalManager();
        ~MythSystemLegacySignalManager() { wait(); }
        void run(void);
    private:
};


class MBASE_PUBLIC MythSystemLegacyWindows : public MythSystemLegacyPrivate
{
    Q_OBJECT

    public:
        MythSystemLegacyWindows(MythSystemLegacy *parent);
        ~MythSystemLegacyWindows();

        virtual void Fork(time_t timeout) MOVERRIDE;
        virtual void Manage(void) MOVERRIDE;

        virtual void Term(bool force=false) MOVERRIDE;
        virtual void Signal(int sig) MOVERRIDE;
        virtual void JumpAbort(void) MOVERRIDE;

        virtual bool ParseShell(const QString &cmd, QString &abscmd,
                                QStringList &args) MOVERRIDE;

        friend class MythSystemLegacyManager;
        friend class MythSystemLegacySignalManager;
        friend class MythSystemLegacyIOHandler;

    private:
        HANDLE      m_child;
        time_t      m_timeout;

        HANDLE      m_stdpipe[3];
};

#endif // _MYTHSYSTEM_WINDOWS_H_

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
