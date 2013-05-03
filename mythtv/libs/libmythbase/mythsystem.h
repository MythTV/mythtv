#ifndef MYTHSYSTEM_H_
#define MYTHSYSTEM_H_

#include "mythbaseexp.h"
#include "compat.h"
#include <signal.h>

typedef enum MythSystemMask {
    kMSNone               = 0x00000000,
    kMSDontBlockInputDevs = 0x00000001, ///< avoid blocking LIRC & Joystick Menu
    kMSDontDisableDrawing = 0x00000002, ///< avoid disabling UI drawing
    kMSRunBackground      = 0x00000004, ///< run child in the background
    kMSProcessEvents      = 0x00000008, ///< process events while waiting
    kMSInUi               = 0x00000010, ///< the parent is in the UI
    kMSStdIn              = 0x00000020, ///< allow access to stdin
    kMSStdOut             = 0x00000040, ///< allow access to stdout
    kMSStdErr             = 0x00000080, ///< allow access to stderr
    kMSBuffered           = 0x00000100, ///< buffer the IO channels
    kMSRunShell           = 0x00000200, ///< run process through shell
    kMSNoRunShell         = 0x00000400, ///< do NOT run process through shell
    kMSAnonLog            = 0x00000800, ///< anonymize the logs
    kMSAbortOnJump        = 0x00001000, ///< abort this process on a jumppoint
    kMSSetPGID            = 0x00002000, ///< set the process group id
    kMSAutoCleanup        = 0x00004000, ///< automatically delete if 
                                        ///  backgrounded
    kMSLowExitVal         = 0x00008000, ///< allow exit values 0-127 only
    kMSDisableUDPListener = 0x00010000, ///< disable MythMessage UDP listener
                                        ///  for the duration of application.
    kMSPropagateLogs      = 0x00020000, ///< add arguments for MythTV log 
                                        ///  propagation
} MythSystemFlag;

#ifdef __cplusplus
#include <QString>
#include <QStringList>
#include <QBuffer>
#include <QSemaphore>
#include <QMap>
#include <QPointer>

#include "referencecounter.h"

typedef QMap<QString, bool> Setting_t;

void ShutdownMythSystem(void);

class MythSystemPrivate;
class MBASE_PUBLIC MythSystem : public QObject
{
    Q_OBJECT

    public:
        MythSystem();
        MythSystem(const QString &, uint);
        MythSystem(const QString &, const QStringList &, uint);
        MythSystem(const MythSystem &other);
        ~MythSystem(void);

        void SetCommand(const QString &, uint);
        void SetCommand(const QString &, const QStringList &, uint);
        void SetDirectory(const QString &);
        bool SetNice(int nice);
        bool SetIOPrio(int prio);

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
        void Signal(int sig);

        void JumpAbort(void);

        bool isBackground()   { return GetSetting("RunInBackground"); };
        bool doAutoCleanup()  { return GetSetting("AutoCleanup"); };
        bool onlyLowExitVal() { return GetSetting("OnlyLowExitVal"); };
        void HandlePreRun();
        void HandlePostRun();

        uint GetStatus()             { return m_status; };
        void SetStatus(uint status)  { m_status = status; };

        QString& GetLogCmd()         { return m_logcmd; };
        QString& GetDirectory()      { return m_directory; };

        bool GetSetting(const char *setting) 
            { return m_settings.value(QString(setting)); };

        QString& GetCommand()        { return m_command; };
        void SetCommand(QString &cmd) { m_command = cmd; };

        QStringList &GetArgs()       { return m_args; };
        void SetArgs(QStringList &args)  { m_args = args; };

        int GetNice()                { return m_nice; };
        int GetIOPrio()              { return m_ioprio; };

        QBuffer *GetBuffer(int index) { return &m_stdbuff[index]; };

        void Unlock() { m_semReady.release(1); };

        friend class MythSystemPrivate;

        static QString ShellEscape(const QString &str);

    signals:
        void started();
        void finished();
        void error(uint status);
        void readDataReady(int fd);

    private:
        void initializePrivate(void);
        MythSystemPrivate *d;

    protected:
        void ProcessFlags(uint flags);

        uint        m_status;
        QSemaphore  m_semReady;

        QString     m_command;
        QString     m_logcmd;
        QStringList m_args;
        QString     m_directory;

        int         m_nice;
        int         m_ioprio;

        Setting_t   m_settings;
        QBuffer     m_stdbuff[3];
};

class MBASE_PUBLIC MythSystemPrivate : public QObject, public ReferenceCounter
{
    Q_OBJECT

    public:
        MythSystemPrivate(const QString &debugName);

        virtual void Fork(time_t timeout) = 0;
        virtual void Manage() = 0;

        virtual void Term(bool force=false) = 0;
        virtual void Signal(int sig) = 0;
        virtual void JumpAbort(void) = 0;

        virtual bool ParseShell(const QString &cmd, QString &abscmd,
                                QStringList &args) = 0;

    protected:
        QPointer<MythSystem> m_parent;

        uint GetStatus()             { return m_parent->GetStatus(); };
        void SetStatus(uint status)  { m_parent->SetStatus(status); };

        QString& GetLogCmd()         { return m_parent->GetLogCmd(); };
        QString& GetDirectory()      { return m_parent->GetDirectory(); };

        bool GetSetting(const char *setting) 
            { return m_parent->GetSetting(setting); };

        QString& GetCommand()        { return m_parent->GetCommand(); };
        void SetCommand(QString &cmd) { m_parent->SetCommand(cmd); };

        QStringList &GetArgs()       { return m_parent->GetArgs(); };
        void SetArgs(QStringList &args)  { m_parent->SetArgs(args); };

        QBuffer *GetBuffer(int index) { return m_parent->GetBuffer(index); };
        void Unlock() { m_parent->Unlock(); };

    signals:
        void started();
        void finished();
        void error(uint status);
        void readDataReady(int fd);
};


 MBASE_PUBLIC  unsigned int myth_system(const QString &command, 
                                 uint flags = kMSNone,
                                 uint timeout = 0);
 MBASE_PUBLIC  void myth_system_jump_abort(void);
extern "C" {
#endif

/* C prototype */
 MBASE_PUBLIC  unsigned int myth_system_c(char *command, uint flags, uint timeout);

#ifdef __cplusplus
}
#endif

#endif

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
