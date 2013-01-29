
// compat header
#include "compat.h"

// Own header
#include "mythsystem.h"

// C++/C headers
#include <cerrno>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>  // for kill()
#include <string.h>

// QT headers
#include <QCoreApplication>

// libmythbase headers
#include "referencecounter.h"
#include "mythcorecontext.h"
#include "mythevent.h"
#include "mythlogging.h"
#include "exitcodes.h"

#if CONFIG_CYGWIN || defined(_WIN32)
#include "system-windows.h"
#else
#include "system-unix.h"
#endif


/*******************************
 * MythSystem method defines
 ******************************/

void MythSystem::initializePrivate(void)
{
    m_nice = 0;
    m_ioprio = 0;
#if CONFIG_CYGWIN || defined(_WIN32)
    d = new MythSystemWindows(this);
#else
    d = new MythSystemUnix(this);
#endif
}

MythSystem::MythSystem()
{
    setObjectName("MythSystem()");
    m_semReady.release(1);  // initialize
    initializePrivate();
}

MythSystem::MythSystem(const QString &command, uint flags)
{
    setObjectName(QString("MythSystem(%1)").arg(command));
    m_semReady.release(1);  // initialize
    initializePrivate();
    SetCommand(command, flags);
}

/** \fn MythSystem::setCommand(const QString &command) 
 *  \brief Resets an existing MythSystem object to a new command
 */
void MythSystem::SetCommand(const QString &command, uint flags)
{
    if (flags & kMSRunShell)
        SetCommand(command, QStringList(), flags);
    else
    {
        QString abscommand;
        QStringList args;
        if (!d->ParseShell(command, abscommand, args))
        {
            LOG(VB_GENERAL, LOG_ERR,
                    QString("MythSystem(%1) command not understood")
                            .arg(command));
            m_status = GENERIC_EXIT_INVALID_CMDLINE;
            return;
        }

        SetCommand(abscommand, args, flags);
    }
}


MythSystem::MythSystem(const QString &command, 
                       const QStringList &args, uint flags)
{
    m_semReady.release(1);  // initialize
    initializePrivate();
    SetCommand(command, args, flags);
}

/** \fn MythSystem::setCommand(const QString &command, 
                               const QStringList &args)
 *  \brief Resets an existing MythSystem object to a new command
 */
void MythSystem::SetCommand(const QString &command, 
                            const QStringList &args, uint flags)
{
    m_status = GENERIC_EXIT_START;
    m_command = QString(command).trimmed();
    m_args = QStringList(args);

    ProcessFlags(flags);

    // add logging arguments
    if (GetSetting("PropagateLogs"))
    {
        if (GetSetting("UseShell") && m_args.isEmpty())
        {
            m_command += logPropagateArgs;
            if (!logPropagateQuiet())
                m_command += " --quiet";
        }
        else
        {
            m_args << logPropagateArgList;
            if (!logPropagateQuiet())
                m_args << "--quiet";
        }
    }

    // check for execute rights
    if (!GetSetting("UseShell") && access(command.toUtf8().constData(), X_OK))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("MythSystem(%1) command not executable, ")
                .arg(command) + ENO);
        m_status = GENERIC_EXIT_CMD_NOT_FOUND;
    }

    m_logcmd = (m_command + " " + m_args.join(" ")).trimmed();

    if( GetSetting("AnonLog") )
    {
        m_logcmd.truncate(m_logcmd.indexOf(" "));
        m_logcmd.append(" (anonymized)");
    }
}


MythSystem::MythSystem(const MythSystem &other) :
    d(other.d),
    m_status(other.m_status),

    m_command(other.m_command),
    m_logcmd(other.m_logcmd),
    m_args(other.m_args),
    m_directory(other.m_directory),

    m_settings(other.m_settings)
{
    m_semReady.release(other.m_semReady.available());
}

// QBuffers may also need freeing
MythSystem::~MythSystem(void)
{
    d->DecrRef();
}


void MythSystem::SetDirectory(const QString &directory)
{
    m_settings["SetDirectory"] = true;
    m_directory = QString(directory);
}

bool MythSystem::SetNice(int nice)
{
    if( !d || (GetStatus() != GENERIC_EXIT_START) )
        return false;

    m_nice = nice;
    return true;
}

bool MythSystem::SetIOPrio(int prio)
{
    if( !d || (GetStatus() != GENERIC_EXIT_START) )
        return false;

    m_ioprio = prio;
    return true;
}

/** \fn MythSystem::Run()
 *  \brief Runs a command inside the /bin/sh shell. Returns immediately
 */
void MythSystem::Run(time_t timeout)
{
    if( !d )
        m_status = GENERIC_EXIT_NO_HANDLER;

    if( GetStatus() != GENERIC_EXIT_START )
    {
        emit error(GetStatus());
        return;
    }

    // Handle any locking of drawing, etc
    HandlePreRun();

    d->Fork(timeout);

    if( GetStatus() == GENERIC_EXIT_RUNNING )
    {
        m_semReady.acquire(1);
        emit started();
        d->Manage();
    }
    else
    {
        emit error(GetStatus());
    }
}

// should there be a separate 'getstatus' call? or is using
// Wait() for that purpose sufficient?
uint MythSystem::Wait(time_t timeout)
{
    if( !d )
        m_status = GENERIC_EXIT_NO_HANDLER;

    if( (GetStatus() != GENERIC_EXIT_RUNNING) || GetSetting("RunInBackground") )
        return GetStatus();

    if( GetSetting("ProcessEvents") )
    {
        if( timeout > 0 )
            timeout += time(NULL);

        while( !timeout || time(NULL) < timeout )
        {
            // loop until timeout hits or process ends
            if( m_semReady.tryAcquire(1,100) )
            {
                m_semReady.release(1);
                break;
            }

            qApp->processEvents();
        }
    }
    else
    {
        if( timeout > 0 )
        {
            if( m_semReady.tryAcquire(1, timeout*1000) )
                m_semReady.release(1);
        }
        else
        {
            m_semReady.acquire(1);
            m_semReady.release(1);
        }
    }
    return GetStatus();
}

void MythSystem::Term(bool force)
{
    if( !d )
        m_status = GENERIC_EXIT_NO_HANDLER;

    if( m_status != GENERIC_EXIT_RUNNING )
        return;

    d->Term(force);
}

void MythSystem::Signal( int sig )
{
    if( !d )
        m_status = GENERIC_EXIT_NO_HANDLER;

    if( m_status != GENERIC_EXIT_RUNNING )
        return;

    d->Signal(sig);
}


void MythSystem::ProcessFlags(uint flags)
{
    if( m_status != GENERIC_EXIT_START )
    {
        LOG(VB_SYSTEM, LOG_DEBUG, QString("status: %1").arg(m_status));
        return;
    }

    m_settings["IsInUI"] = gCoreContext->HasGUI() && gCoreContext->IsUIThread();

    if( flags & kMSRunBackground )
        m_settings["RunInBackground"] = true;

    if( m_command.endsWith("&") )
    {
        if (!GetSetting("RunInBackground"))
            LOG(VB_SYSTEM, LOG_DEBUG, "Adding background flag");

        // Remove the &
        m_command.chop(1);
        m_command = m_command.trimmed();
        m_settings["RunInBackground"] = true;
        m_settings["UseShell"]        = true;
        m_settings["IsInUI"]          = false;
    }

    if( GetSetting("IsInUI") )
    {
        // Check for UI-only locks
        m_settings["BlockInputDevs"] = !(flags & kMSDontBlockInputDevs);
        m_settings["DisableDrawing"] = !(flags & kMSDontDisableDrawing);
        m_settings["ProcessEvents"]  = flags & kMSProcessEvents;
        m_settings["DisableUDP"]     = flags & kMSDisableUDPListener;
    }

    if( flags & kMSStdIn )
        m_settings["UseStdin"] = true;
    if( flags & kMSStdOut )
        m_settings["UseStdout"] = true;
    if( flags & kMSStdErr )
        m_settings["UseStderr"] = true;
    if( flags & kMSRunShell )
        m_settings["UseShell"] = true;
    if( flags & kMSNoRunShell ) // override for use with myth_system
        m_settings["UseShell"] = false;
    if( flags & kMSAbortOnJump )
        m_settings["AbortOnJump"] = true;
    if( flags & kMSSetPGID )
        m_settings["SetPGID"] = true;
    if( flags & kMSAutoCleanup && GetSetting("RunInBackground") )
        m_settings["AutoCleanup"] = true;
    if( flags & kMSAnonLog )
        m_settings["AnonLog"] = true;
    if( flags & kMSLowExitVal )
        m_settings["OnlyLowExitVal"] = true;
    if( flags & kMSPropagateLogs )
        m_settings["PropagateLogs"] = true;
}

QByteArray  MythSystem::Read(int size)
{ 
    return m_stdbuff[1].read(size); 
}

QByteArray  MythSystem::ReadErr(int size)
{ 
    return m_stdbuff[2].read(size); 
}

QByteArray& MythSystem::ReadAll()
{
    return m_stdbuff[1].buffer();
}

QByteArray& MythSystem::ReadAllErr()
{
    return m_stdbuff[2].buffer();
}

int MythSystem::Write(const QByteArray &ba)
{
    if (!GetSetting("UseStdin"))
        return 0;

    m_stdbuff[0].buffer().append(ba.constData());
    return ba.size();
}

void MythSystem::HandlePreRun()
{
    // This needs to be a send event so that the MythUI locks the input devices
    // immediately instead of after existing events are processed
    // since this function could be called inside one of those events.
    if( GetSetting("BlockInputDevs") )
    {
        QEvent event(MythEvent::kLockInputDevicesEventType);
        QCoreApplication::sendEvent(gCoreContext->GetGUIObject(), &event);
    }

    // This needs to be a send event so that the listener is disabled
    // immediately instead of after existing events are processed, since the
    // listen server must be terminated before the spawned application tries
    // to start its own
    if( GetSetting("DisableUDP") )
    {
        QEvent event(MythEvent::kDisableUDPListenerEventType);
        QCoreApplication::sendEvent(gCoreContext->GetGUIObject(), &event);
    }

    // This needs to be a send event so that the MythUI m_drawState change is
    // flagged immediately instead of after existing events are processed
    // since this function could be called inside one of those events.
    if( GetSetting("DisableDrawing") )
    {
        QEvent event(MythEvent::kPushDisableDrawingEventType);
        QCoreApplication::sendEvent(gCoreContext->GetGUIObject(), &event);
    }
}

void MythSystem::HandlePostRun()
{
    // Since this is *not* running in the UI thread (but rather the signal
    // handler thread), we need to use postEvents
    if( GetSetting("DisableDrawing") )
    {
        QEvent *event = new QEvent(MythEvent::kPopDisableDrawingEventType);
        QCoreApplication::postEvent(gCoreContext->GetGUIObject(), event);
    }

    // This needs to be a post event so we do not try to start listening on
    // the UDP ports before the child application has stopped and terminated
    if( GetSetting("DisableUDP") )
    {
        QEvent event(MythEvent::kEnableUDPListenerEventType);
        QCoreApplication::sendEvent(gCoreContext->GetGUIObject(), &event);
    }

    // This needs to be a post event so that the MythUI unlocks input devices
    // after all existing (blocked) events are processed and ignored.
    if( GetSetting("BlockInputDevs") )
    {
        QEvent *event = new QEvent(MythEvent::kUnlockInputDevicesEventType);
        QCoreApplication::postEvent(gCoreContext->GetGUIObject(), event);
    }
}

void MythSystem::JumpAbort(void)
{
    if (!d)
        return;

    LOG(VB_SYSTEM, LOG_DEBUG, "Triggering Abort on Jumppoint");
    d->JumpAbort();
}

QString MythSystem::ShellEscape(const QString &in)
{
    QString out = in;

    if (out.contains("\""))
        out = out.replace("\"", "\\\"");

    if (out.contains("\'"))
        out = out.replace("\'", "\\\'");

    if (out.contains(" "))
    {
        out.prepend("\"");
        out.append("\"");
    }

    return out;
}

MythSystemPrivate::MythSystemPrivate(const QString &debugName) :
    ReferenceCounter(debugName)
{
}

uint myth_system(const QString &command, uint flags, uint timeout)
{
    flags |= kMSRunShell | kMSAutoCleanup;
    MythSystem *ms = new MythSystem(command, flags);
    ms->Run(timeout);
    uint result = ms->Wait(0);
    if (!ms->GetSetting("RunInBackground"))
        delete ms;

    return result;
}

void myth_system_jump_abort(void)
{
    MythSystem ms;
    ms.JumpAbort();
}

extern "C" {
    unsigned int myth_system_c(char *command, uint flags, uint timeout)
    {
        QString cmd(command);
        return myth_system(cmd, flags, timeout);
    }
}

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
