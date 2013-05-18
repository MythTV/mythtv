/// -*- Mode: c++ -*-

#ifndef MYTHSYSTEM_H_
#define MYTHSYSTEM_H_

#include "mythbaseexp.h"

// Note: The FIXME were added 2013-05-18, as part of a multi-pass
// cleanup of MythSystem.
//
// MythSystem exists because the Qt QProcess has caused us numerous
// headaches, most importantly:
//   http://code.mythtv.org/trac/ticket/7135
//   https://bugreports.qt-project.org/browse/QTBUG-5990
// QProcess also demands that the thread in which it is used
// and created run a Qt event loop, which MythSystem does not
// require. A number of MythTV threads do not run an event loop
// so this requirement is a bit onerous.
// 
// MythSystem has grown a bit fat around the middle as functionality
// has been added and as a core functionality class is due for a code
// review and some cleanup.

// FIXME: compat.h && signal.h should not be pulled into a header file.
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
#include <QMap> // FIXME: This shouldn't be needed, Setting_t is not public
#include <QPointer> // FIXME: QPointer is deprecated

#include "referencecounter.h"

// FIXME: _t is not how we name types in MythTV...
typedef QMap<QString, bool> Setting_t;

void ShutdownMythSystem(void);

// FIXME: Does MythSystem really need to inherit from QObject?
//        we can probably create a private class that inherits
//        from QObject to avoid exposing lots of thread-unsafe
//        methods & complicated life-time management.
class MythSystemPrivate;
class MBASE_PUBLIC MythSystem : public QObject
{
    Q_OBJECT

  public:
    MythSystem();
    MythSystem(const QString &, uint);
    MythSystem(const QString &, const QStringList &, uint);
    // FIXME: Do we really need to expose a public copy-constructor?
    // FIXME: Should operator=() also be provided or banned?
    MythSystem(const MythSystem &other);
    ~MythSystem(void);

    // FIXME: We should not allow a MythSystem to be reused for a new command.
    void SetCommand(const QString &, uint);
    // FIXME: We should not allow a MythSystem to be reused for a new command.
    void SetCommand(const QString &, const QStringList &, uint);
    // FIXME: This should only be in the constructor
    void SetDirectory(const QString &);
    // FIXME: This should only be in the constructor
    bool SetNice(int nice);
    // FIXME: This should only be in the constructor
    bool SetIOPrio(int prio);

    void Run(time_t timeout = 0);
    uint Wait(time_t timeout = 0);

    int Write(const QByteArray&);
    QByteArray Read(int size);
    QByteArray ReadErr(int size);
    // FIXME: We should not return a reference here
    QByteArray& ReadAll();
    // FIXME: We should not return a reference here
    QByteArray& ReadAllErr();

    // FIXME: Replace these with a single Signal() that accepts
    //        an enum typedef defined in this header.
    void Term(bool force = false);
    void Kill(void)   { Signal(SIGKILL); }
    void Stop(void)   { Signal(SIGSTOP); }
    void Cont(void)   { Signal(SIGCONT); }
    void HangUp(void) { Signal(SIGHUP);  }
    void USR1(void)   { Signal(SIGUSR1); }
    void USR2(void)   { Signal(SIGUSR2); }
    void Signal(int sig);

    // FIXME: Should be documented, and maybe should be called AbortJump()
    void JumpAbort(void);

    // FIXME: Should be IsBackground() + documented
    bool isBackground(void)   { return GetSetting("RunInBackground"); }
    // FIXME: Should be IsAutoCleanupProcess() + documented
    bool doAutoCleanup(void)  { return GetSetting("AutoCleanup"); }
    // FIXME: No idea what this is querying but should be StudlyCase
    //        and should be documented
    bool onlyLowExitVal(void) { return GetSetting("OnlyLowExitVal"); }
    // FIXME: Why is this public?
    void HandlePreRun(void);
    // FIXME: Why is this public?
    void HandlePostRun(void);

    // FIXME: Rename "GetExitStatus" and document that this does not
    //        block until an exit status exists.
    uint GetStatus(void)             { return m_status; }
    // FIXME Make private.
    void SetStatus(uint status)      { m_status = status; }

    // FIXME: We should not return a reference here, add a Set if necessary
    // Do we even need this function? Should it then be better named?
    QString& GetLogCmd(void)         { return m_logcmd; }
    // FIXME: We should not return a reference here
    QString& GetDirectory(void)      { return m_directory; }

    // FIXME: Eliminate or make private, we don't allow any settings
    //        that can not be enumerated.
    bool GetSetting(const char *setting) 
    { return m_settings.value(QString(setting)); }

    // FIXME: We should not return a reference here
    QString& GetCommand(void)        { return m_command; }
    // FIXME: Eliminate. We should not allow setting the command
    //        after construcion.
    void SetCommand(QString &cmd)    { m_command = cmd; }

    // FIXME: We should not return a reference here
    // FIXME: Rename "GetArguments"
    QStringList &GetArgs(void)       { return m_args; }
    // FIXME: Eliminate. We should not allow setting the arguements
    //        after construcion.
    void SetArgs(QStringList &args)  { m_args = args; }

    int GetNice(void)                { return m_nice; }
    int GetIOPrio(void)              { return m_ioprio; }

    // FIXME: We should not return a pointer to a QBuffer
    QBuffer *GetBuffer(int index)    { return &m_stdbuff[index]; }

    // FIXME: We should not make locking public
    void Unlock(void)                { m_semReady.release(1); }

    friend class MythSystemPrivate;

    // FIXME: This should be an independent function living elsewhere
    static QString ShellEscape(const QString &str);

    // FIXME: Do we really need to expose Qt signals?
    //        If so why are they lower case?
  signals:
    void started(void);
    void finished(void);
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

// FIXME: do we really need reference counting?
// it shouldn't be difficult to track the lifetime of a private object.
// FIXME: This should not live in the same header as MythSystem
class MythSystemPrivate : public QObject, public ReferenceCounter
{
    Q_OBJECT

  public:
    MythSystemPrivate(const QString &debugName);

    virtual void Fork(time_t timeout) = 0;
    virtual void Manage(void) = 0;

    virtual void Term(bool force=false) = 0;
    virtual void Signal(int sig) = 0;
    virtual void JumpAbort(void) = 0;

    virtual bool ParseShell(const QString &cmd, QString &abscmd,
                            QStringList &args) = 0;

  protected:
    QPointer<MythSystem> m_parent;

    uint GetStatus(void)             { return m_parent->GetStatus(); }
    void SetStatus(uint status)      { m_parent->SetStatus(status); }

    // FIXME: We should not return a reference here
    QString& GetLogCmd(void)         { return m_parent->GetLogCmd(); }
    // FIXME: We should not return a reference here
    QString& GetDirectory(void)      { return m_parent->GetDirectory(); }

    bool GetSetting(const char *setting) 
        { return m_parent->GetSetting(setting); }

    // FIXME: We should not return a reference here
    QString& GetCommand(void)        { return m_parent->GetCommand(); }
    void SetCommand(QString &cmd)    { m_parent->SetCommand(cmd); }

    // FIXME: We should not return a reference here
    // FIXME: Rename "GetArguments"
    QStringList &GetArgs(void)       { return m_parent->GetArgs(); }
    // FIXME: Rename "SetArguments"
    void SetArgs(QStringList &args)  { m_parent->SetArgs(args); }

    // FIXME: This is likely a bad idea, but possibly manageable
    //        since this is a private class.
    QBuffer *GetBuffer(int index)    { return m_parent->GetBuffer(index); }
    // FIXME: This is likely a bad idea, but possibly manageable
    //        since this is a private class.
    void Unlock(void)                { m_parent->Unlock(); }

  signals:
    void started(void);
    void finished(void);
    void error(uint status);
    void readDataReady(int fd);
};


MBASE_PUBLIC uint myth_system(const QString &command, 
                              uint flags = kMSNone,
                              uint timeout = 0);
MBASE_PUBLIC void myth_system_jump_abort(void);
extern "C" {
#endif

/* C prototype */
MBASE_PUBLIC uint myth_system_c(char *command, uint flags, uint timeout);

#ifdef __cplusplus
}
#endif

#endif

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
