#ifndef SYSTEM_UNIX_H_
#define SYSTEM_UNIX_H_

#include "mythbaseexp.h"
#include <signal.h>
#include <QObject>
#include <QMap>
#include <QList>
#include <QBuffer>
#include <QThread>
#include <QWaitCondition>
#include <QMutex>
#include "mythsystem.h"

class MythSystemWindows;

typedef QMap<HANDLE, MythSystemWindows *> MSMap_t;
typedef QMap<HANDLE, QBuffer *> PMap_t;
typedef QList<MythSystemWindows *> MSList_t;

class MythSystemIOHandler: public QThread
{
    public:
        MythSystemIOHandler(bool read);
        void   run(void);

        void   insert(HANDLE h, QBuffer *buff);
        void   remove(HANDLE h);
        void   wake();

    private:
        bool   HandleRead(HANDLE h, QBuffer *buff);
        bool   HandleWrite(HANDLE h, QBuffer *buff);

        QWaitCondition  m_pWait;
        QMutex          m_pLock;
        PMap_t          m_pMap;

        bool    m_read;
        char    m_readbuf[65536];
};

class MythSystemManager : public QThread
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

class MythSystemSignalManager : public QThread
{
    public:
        MythSystemSignalManager();
        void run(void);
    private:
};


class MBASE_PUBLIC MythSystemWindows : public MythSystemPrivate
{
    Q_OBJECT

    public:
        MythSystemWindows(MythSystem *parent);
        ~MythSystemWindows();

        virtual void Fork(time_t timeout);
        virtual void Manage();

        virtual void Term(bool force=false);
        virtual void Signal(int sig);
        virtual void JumpAbort(void);

        friend class MythSystemManager;
        friend class MythSystemSignalManager;
        friend class MythSystemIOHandler;

    private:
        HANDLE      m_child;
        time_t      m_timeout;

        HANDLE      m_stdpipe[3];
};

#endif

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
