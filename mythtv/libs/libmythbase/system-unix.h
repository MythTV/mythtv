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
#include <QPointer>
#include <sys/select.h>
#include "mythsystem.h"

class MythSystemUnix;

typedef QMap<pid_t, QPointer<MythSystemUnix> > MSMap_t;
typedef QMap<int, QBuffer *> PMap_t;
typedef QList<QPointer<MythSystemUnix> > MSList_t;

class MythSystemIOHandler: public QThread
{
    public:
        MythSystemIOHandler(bool read);
        void   run(void);

        void   insert(int fd, QBuffer *buff);
        void   remove(int fd);
        void   wake();

    private:
        void   HandleRead(int fd, QBuffer *buff);
        void   HandleWrite(int fd, QBuffer *buff);
        void   BuildFDs();

        QWaitCondition  m_pWait;
        QMutex          m_pLock;
        PMap_t          m_pMap;

        fd_set m_fds;
        int    m_maxfd;
        bool   m_read;
        char   m_readbuf[65536];
};

class MythSystemManager : public QThread
{
    public:
        MythSystemManager();
        void run(void);
        void append(MythSystemUnix *);
        void jumpAbort(void);
    private:
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


class MBASE_PUBLIC MythSystemUnix : public MythSystemPrivate
{
    Q_OBJECT

    public:
        MythSystemUnix(MythSystem *parent);
        ~MythSystemUnix();

        virtual void Fork(time_t timeout);
        virtual void Manage();

        virtual void Term(bool force=false);
        virtual void Signal(int sig);
        virtual void JumpAbort(void);

        friend class MythSystemManager;
        friend class MythSystemSignalManager;
        friend class MythSystemIOHandler;

    private:
        pid_t       m_pid;
        time_t      m_timeout;

        int         m_stdpipe[3];
};

#endif

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
