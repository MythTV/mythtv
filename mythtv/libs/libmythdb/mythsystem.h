#ifndef MYTHSYSTEM_H_
#define MYTHSYSTEM_H_

#include "mythexp.h"
#include <signal.h>

typedef enum MythSystemMask {
    kMSNone               = 0x00000000,
    kMSDontBlockInputDevs = 0x00000001, //< avoid blocking LIRC & Joystick Menu
    kMSDontDisableDrawing = 0x00000002, //< avoid disabling UI drawing
    kMSRunBackground      = 0x00000004, //< run child in the background
    kMSProcessEvents      = 0x00000008, //< process events while waiting
    kMSInUi               = 0x00000010, //< the parent is in the UI
    kMSStdIn              = 0x00000020, //< allow access to stdin
    kMSStdOut             = 0x00000040, //< allow access to stdout
    kMSStdErr             = 0x00000080, //< allow access to stderr
    kMSBuffered           = 0x00000100, //< buffer the IO channels
    kMSRunShell           = 0x00000200, //< run process through shell
    kMSNoRunShell         = 0x00000400, //< do NOT run process through shell
    kMSAnonLog            = 0x00000800, //< anonymize the logs
    kMSAbortOnJump        = 0x00001000, //< abort this process on a jumppoint
    kMSSetPGID            = 0x00002000, //< set the process group id
    kMSAutoCleanup        = 0x00004000, //< automatically delete if backgrounded
} MythSystemFlag;

#ifdef __cplusplus
#include <QString>
#include <QStringList>
#include <QBuffer>
#include <QMutex>
#include <QMap>
#include <QThread>
#include <QWaitCondition>
#include <unistd.h>  // for pid_t
#include <sys/select.h>

class MythSystem;

typedef QMap<pid_t, MythSystem *> MSMap_t;
typedef QMap<int, QBuffer *> PMap_t;
typedef QList<MythSystem *> MSList_t;

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
        void append(MythSystem *);
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


class MPUBLIC MythSystem : public QObject
{
    Q_OBJECT

    public:
        MythSystem() {};
        MythSystem(const QString &, uint);
        MythSystem(const QString &, const QStringList &, uint);
        MythSystem(const MythSystem &other);
        ~MythSystem(void);

        void SetCommand(const QString &, uint);
        void SetCommand(const QString &, const QStringList &, uint);
        void SetDirectory(const QString &);

        void Run(time_t timeout = 0);
        uint Wait(time_t timeout = 0);

        int Write(const QByteArray&);
        QByteArray Read(int size);
        QByteArray ReadErr(int size);
        QByteArray& ReadAll();
        QByteArray& ReadAllErr();

        void Term(bool force=false);
        void Kill()   { Signal(SIGKILL); };
        void Stop()   { Signal(SIGSTOP); };
        void Cont()   { Signal(SIGCONT); };
        void HangUp() { Signal(SIGHUP);  };
        void USR1()   { Signal(SIGUSR1); };
        void USR2()   { Signal(SIGUSR2); };

        bool isBackground()  { return m_runinbackground; };
        bool doAutoCleanup() { return m_autocleanup; };

        friend class MythSystemManager;
        friend class MythSystemSignalManager;

    signals:
        void started();
        void finished();
        void error(uint status);

    private:
        void ProcessFlags(uint);
        void HandlePreRun();
        void HandlePostRun();
        void Fork();
        void Signal(int sig);

        uint   m_status;
        pid_t  m_pid;
        QMutex m_pmutex;
        time_t m_timeout;

        QString     m_command;
        QString     m_logcmd;
        QStringList m_args;
        QString     m_directory;

        int     m_stdpipe[3]; // should there be a means of hitting these directly?
        QBuffer m_stdbuff[3]; // do these need to be allocated?

        // move to a struct to keep things clean?
        // perhaps allow overloaded input using the struct
        //   rather than the bitwise
        bool  m_runinbackground;
        bool  m_isinui;
        bool  m_blockinputdevs;
        bool  m_disabledrawing;
        bool  m_processevents;
        bool  m_usestdin;
        bool  m_usestdout;
        bool  m_usestderr;
        bool  m_useshell;
        bool  m_setdirectory;
        bool  m_abortonjump;
        bool  m_setpgid;
        bool  m_autocleanup;
};

MPUBLIC unsigned int myth_system(const QString &command, 
                                 uint flags = kMSNone,
                                 uint timeout = 0);
MPUBLIC void myth_system_jump_abort(void);
extern "C" {
#endif

/* C prototype */
MPUBLIC unsigned int myth_system_c(char *command, uint flags, uint timeout);

#ifdef __cplusplus
}
#endif

#endif

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
