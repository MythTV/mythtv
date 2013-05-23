/// -*- Mode: c++ -*-

#ifndef _MYTHSYSTEM_WINDOWS_H_
#define _MYTHSYSTEM_WINDOWS_H_

#include <signal.h>

#include <QWaitCondition>
#include <QBuffer>
#include <QObject>
#include <QMutex>
#include <QList>
#include <QMap>

#include "mythsystemprivate.h"
#include "mythbaseexp.h"
#include "mythsystem.h"
#include "mthread.h"

class MythSystemWindows;

typedef QMap<HANDLE, MythSystemWindows *> MSMap_t;
typedef QMap<HANDLE, QBuffer *> PMap_t;
typedef QList<MythSystemWindows *> MSList_t;

class MythSystemIOHandler: public MThread
{
    public:
        MythSystemIOHandler(bool read);
        ~MythSystemIOHandler() { wait(); }
        void   run(void);

        void   insert(HANDLE h, QBuffer *buff);
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

class MythSystemManager : public MThread
{
    public:
        MythSystemManager();
        ~MythSystemManager();
        void run(void);
        void append(MythSystemWindows *);
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

class MythSystemSignalManager : public MThread
{
    public:
        MythSystemSignalManager();
        ~MythSystemSignalManager() { wait(); }
        void run(void);
    private:
};


class MBASE_PUBLIC MythSystemWindows : public MythSystemPrivate
{
    Q_OBJECT

    public:
        MythSystemWindows(MythSystem *parent);
        ~MythSystemWindows();

        virtual void Fork(time_t timeout) MOVERRIDE;
        virtual void Manage(void) MOVERRIDE;

        virtual void Term(bool force=false) MOVERRIDE;
        virtual void Signal(int sig) MOVERRIDE;
        virtual void JumpAbort(void) MOVERRIDE;

        virtual bool ParseShell(const QString &cmd, QString &abscmd,
                                QStringList &args) MOVERRIDE;

        friend class MythSystemManager;
        friend class MythSystemSignalManager;
        friend class MythSystemIOHandler;

    private:
        HANDLE      m_child;
        time_t      m_timeout;

        HANDLE      m_stdpipe[3];
};

#endif // _MYTHSYSTEM_WINDOWS_H_

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
