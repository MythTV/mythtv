
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
#include <QThread>
#include <QMutex>
#include <QMap>
#include <QString>
#include <QStringList>

// libmythdb headers
#include "mythcorecontext.h"
#include "mythevent.h"
#include "mythverbose.h"
#include "exitcodes.h"

#if CONFIG_CYGWIN || defined(_WIN32)
#include "system-windows.h"
#else
#if 0
#include <sys/select.h>
#include <sys/wait.h>
#endif
#include "system-unix.h"
#endif


/*******************************
 * MythSystem method defines
 ******************************/

void MythSystem::initializePrivate(void)
{
#if CONFIG_CYGWIN || defined(_WIN32)
    d = new MythSystemWindows(this);
#else
    d = new MythSystemUnix(this);
#endif
}

MythSystem::MythSystem(const QString &command, uint flags)
{
    initializePrivate();
    SetCommand(command, flags);
}

/** \fn MythSystem::setCommand(const QString &command) 
 *  \brief Resets an existing MythSystem object to a new command
 */
void MythSystem::SetCommand(const QString &command, uint flags)
{
    SetCommand(command, QStringList(), flags | kMSRunShell);
}


MythSystem::MythSystem(const QString &command, 
                       const QStringList &args, uint flags)
{
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

    // check for execute rights
    if (!GetSetting("UseShell") && !access(command.toUtf8().constData(), X_OK))
        m_status = GENERIC_EXIT_CMD_NOT_FOUND;

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
}

// QBuffers may also need freeing
MythSystem::~MythSystem(void)
{
    delete d;
}


void MythSystem::SetDirectory(const QString &directory)
{
    m_settings["SetDirectory"] = true;
    m_directory = QString(directory);
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

    m_pmutex.lock();
    emit started();
    d->Manage();
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
            if( m_pmutex.tryLock(100) )
            {
                m_pmutex.unlock();
                break;
            }

            qApp->processEvents();
        }
    }
    else
    {
        if( timeout > 0 )
        {
            if( m_pmutex.tryLock(timeout*1000) )
                m_pmutex.unlock();
        }
        else
        {
            m_pmutex.lock();
            m_pmutex.unlock();
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
        VERBOSE(VB_IMPORTANT, QString("status: %1").arg(m_status));
        return;
    }

    if( flags & kMSRunBackground )
        m_settings["RunInBackground"] = true;

    if( m_command.endsWith("&") )
    {
        if (!GetSetting("RunInBackground"))
            VERBOSE(VB_GENERAL, "Adding background flag");

        // Remove the &
        m_command.chop(1);
        m_command = m_command.trimmed();
        m_settings["RunInBackground"] = true;
        m_settings["UseShell"]        = true;
    }

    m_settings["IsInUI"] = gCoreContext->HasGUI() && gCoreContext->IsUIThread();
    if( GetSetting("IsInUI") )
    {
        // Check for UI-only locks
        m_settings["BlockInputDevs"] = !(flags & kMSDontBlockInputDevs);
        m_settings["DisableDrawing"] = !(flags & kMSDontDisableDrawing);
        m_settings["ProcessEvents"]  = flags & kMSProcessEvents;
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

    VERBOSE(VB_GENERAL, "Triggering Abort on Jumppoint");
    d->JumpAbort();
}

uint myth_system(const QString &command, uint flags, uint timeout)
{
    flags |= kMSRunShell | kMSAutoCleanup;
    MythSystem *ms = new MythSystem(command, flags);
    ms->Run(timeout);
    uint result = ms->Wait(0);
    if (!(flags & kMSRunBackground))
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
