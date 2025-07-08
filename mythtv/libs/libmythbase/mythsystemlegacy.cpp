/* -*- Mode: c++ -*-
 *  Class MythSystemLegacy
 *
 *  Copyright (C) Gavin Hurlbut 2012
 *  Copyright (C) Issac Richards 2008
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

// compat header
#include "compat.h"

// Own header
#include "mythsystemlegacy.h"

// C++/C headers
#include <cerrno>
#include <csignal> // for kill() and SIGXXX
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

// QT headers
#include <QtGlobal>
#include <QCoreApplication>

// libmythbase headers
#include "referencecounter.h"
#include "mythcorecontext.h"
#include "mythevent.h"
#include "mythlogging.h"

#ifdef Q_OS_WIN
#include "mythsystemwindows.h"
#else
#include "mythsystemunix.h"
#endif


/*******************************
 * MythSystemLegacy method defines
 ******************************/

void MythSystemLegacy::initializePrivate(void)
{
#ifdef Q_OS_WIN
    d = new MythSystemLegacyWindows(this);
#else
    d = new MythSystemLegacyUnix(this);
#endif
}

MythSystemLegacy::MythSystemLegacy(QObject *parent) :
    QObject(parent)
{
    setObjectName("MythSystemLegacy()");
    m_semReady.release(1);  // initialize
    initializePrivate();
}

MythSystemLegacy::MythSystemLegacy(const QString &command, uint flags,
                                   QObject *parent) :
    QObject(parent)
{
    setObjectName(QString("MythSystemLegacy(%1)").arg(command));
    m_semReady.release(1);  // initialize
    initializePrivate();
    SetCommand(command, flags);
}

/**
 *  \brief Resets an existing MythSystemLegacy object to a new command
 */
void MythSystemLegacy::SetCommand(const QString &command, uint flags)
{
    if (flags & kMSRunShell)
    {
        SetCommand(command, QStringList(), flags);
    }
    else
    {
        QString abscommand;
        QStringList args;
        if (!d->ParseShell(command, abscommand, args))
        {
            LOG(VB_GENERAL, LOG_ERR,
                    QString("MythSystemLegacy(%1) command not understood")
                            .arg(command));
            m_status = GENERIC_EXIT_INVALID_CMDLINE;
            return;
        }

        SetCommand(abscommand, args, flags);
    }

    if (m_settings["UseStdin"])
        m_stdbuff[0].open(QIODevice::WriteOnly);
    if (m_settings["UseStdout"])
        m_stdbuff[1].open(QIODevice::ReadOnly);
    if (m_settings["UseStderr"])
        m_stdbuff[2].open(QIODevice::ReadOnly);
}


MythSystemLegacy::MythSystemLegacy(const QString &command,
                                   const QStringList &args,
                                   uint flags,
                                   QObject *parent) :
    QObject(parent)
{
    m_semReady.release(1);  // initialize
    initializePrivate();
    SetCommand(command, args, flags);
}

/**
 *  \brief Resets an existing MythSystemLegacy object to a new command
 */
void MythSystemLegacy::SetCommand(const QString &command,
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
    if (!GetSetting("UseShell") && (access(command.toUtf8().constData(), X_OK)) != 0)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("MythSystemLegacy(%1) command not executable, ")
            .arg(command) + ENO);
        m_status = GENERIC_EXIT_CMD_NOT_FOUND;
    }

    m_logcmd = (m_command + " " + m_args.join(" ")).trimmed();

    if (GetSetting("AnonLog"))
    {
        m_logcmd.truncate(m_logcmd.indexOf(" "));
        m_logcmd.append(" (anonymized)");
    }
}

// QBuffers may also need freeing
MythSystemLegacy::~MythSystemLegacy(void)
{
    if (GetStatus() == GENERIC_EXIT_RUNNING)
    {
        Term(true);
        Wait();
    }
    d->DecrRef();
}

void MythSystemLegacy::SetDirectory(const QString &directory)
{
    m_settings["SetDirectory"] = true;
    m_directory = QString(directory);
}

bool MythSystemLegacy::SetNice(int nice)
{
    if (!d || (GetStatus() != GENERIC_EXIT_START))
        return false;

    m_nice = nice;
    return true;
}

bool MythSystemLegacy::SetIOPrio(int prio)
{
    if (!d || (GetStatus() != GENERIC_EXIT_START))
        return false;

    m_ioprio = prio;
    return true;
}

/// \brief Runs a command inside the /bin/sh shell. Returns immediately
void MythSystemLegacy::Run(std::chrono::seconds timeout)
{
    if (!d)
        m_status = GENERIC_EXIT_NO_HANDLER;

    if (GetStatus() != GENERIC_EXIT_START)
    {
        emit error(GetStatus());
        return;
    }

    // Handle any locking of drawing, etc
    HandlePreRun();

    d->Fork(timeout);

    if (GetStatus() == GENERIC_EXIT_RUNNING)
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
uint MythSystemLegacy::Wait(std::chrono::seconds timeout)
{
    if (!d)
        m_status = GENERIC_EXIT_NO_HANDLER;

    if ((GetStatus() != GENERIC_EXIT_RUNNING) || GetSetting("RunInBackground"))
        return GetStatus();

    if (GetSetting("ProcessEvents"))
    {
        auto tt = (timeout > 0s)
            ? SystemClock::now() + timeout
            : SystemTime::max();
        while (SystemClock::now() < tt)
        {
            // loop until timeout hits or process ends
            if (m_semReady.tryAcquire(1,100))
            {
                m_semReady.release(1);
                break;
            }

            qApp->processEvents();
        }
    }
    else
    {
        if (timeout > 0s)
        {
            auto msec = duration_cast<std::chrono::milliseconds>(timeout);
            if (m_semReady.tryAcquire(1, msec.count()))
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

void MythSystemLegacy::Term(bool force)
{
    if (!d)
        m_status = GENERIC_EXIT_NO_HANDLER;

    if (m_status != GENERIC_EXIT_RUNNING)
        return;

    d->Term(force);
}

void MythSystemLegacy::Signal(MythSignal sig)
{
    if (!d)
        m_status = GENERIC_EXIT_NO_HANDLER;

    if (m_status != GENERIC_EXIT_RUNNING)
        return;

#ifndef SIGTRAP /* For Mingw */
#define SIGTRAP -1
#endif
    int posix_signal = SIGTRAP;
    switch (sig)
    {
        case kSignalNone:
        case kSignalUnknown:
            break;
        case kSignalHangup: posix_signal = SIGHUP; break;
        case kSignalInterrupt: posix_signal = SIGINT; break;
        case kSignalContinue: posix_signal = SIGCONT; break;
        case kSignalQuit: posix_signal = SIGQUIT; break;
        case kSignalSegfault: posix_signal = SIGSEGV; break;
        case kSignalKill: posix_signal = SIGKILL; break;
        case kSignalUser1: posix_signal = SIGUSR1; break;
        case kSignalUser2: posix_signal = SIGUSR2; break;
        case kSignalTerm: posix_signal = SIGTERM; break;
        case kSignalStop: posix_signal = SIGSTOP; break;
    }

    // The default less switch above will cause a compiler warning
    // if someone adds a signal without updating the switch, but in
    // case that is missed print out a message.
    if (SIGTRAP == posix_signal)
    {
        LOG(VB_SYSTEM, LOG_ERR,
            QString("Programmer error: Unknown signal %1").arg(sig));
        return;
    }

    d->Signal(posix_signal);
}


void MythSystemLegacy::ProcessFlags(uint flags)
{
    if (m_status != GENERIC_EXIT_START)
    {
        LOG(VB_SYSTEM, LOG_DEBUG, QString("status: %1").arg(m_status));
        return;
    }

    m_settings["IsInUI"] = gCoreContext->HasGUI() && gCoreContext->IsUIThread();

    if (flags & kMSRunBackground)
        m_settings["RunInBackground"] = true;

    if (m_command.endsWith("&"))
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

    if (GetSetting("IsInUI"))
    {
        // Check for UI-only locks
        m_settings["BlockInputDevs"] = ((flags & kMSDontBlockInputDevs) == 0U);
        m_settings["DisableDrawing"] = ((flags & kMSDontDisableDrawing) == 0U);
        m_settings["ProcessEvents"]  = ((flags & kMSProcessEvents)      != 0U);
        m_settings["DisableUDP"]     = ((flags & kMSDisableUDPListener) != 0U);
    }

    if (flags & kMSStdIn)
        m_settings["UseStdin"] = true;
    if (flags & kMSStdOut)
        m_settings["UseStdout"] = true;
    if (flags & kMSStdErr)
        m_settings["UseStderr"] = true;
    if (flags & kMSRunShell)
        m_settings["UseShell"] = true;
    if (flags & kMSAutoCleanup && GetSetting("RunInBackground"))
        m_settings["AutoCleanup"] = true;
    if (flags & kMSAnonLog)
        m_settings["AnonLog"] = true;
    if (flags & kMSLowExitVal)
        m_settings["OnlyLowExitVal"] = true;
    if (flags & kMSPropagateLogs)
        m_settings["PropagateLogs"] = true;
}

QByteArray  MythSystemLegacy::Read(int size)
{
    return m_stdbuff[1].read(size);
}

QByteArray  MythSystemLegacy::ReadErr(int size)
{
    return m_stdbuff[2].read(size);
}

QByteArray& MythSystemLegacy::ReadAll(void)
{
    return m_stdbuff[1].buffer();
}

QByteArray& MythSystemLegacy::ReadAllErr(void)
{
    return m_stdbuff[2].buffer();
}

/** This writes to the standard input of the program being run.
 *  All calls to this must be done before Run() is called.
 *  All calls after Run() is called are silently ignored.
 */
int MythSystemLegacy::Write(const QByteArray &ba)
{
    if (!GetSetting("UseStdin"))
        return 0;

    return m_stdbuff[0].write(ba.constData());
}

void MythSystemLegacy::HandlePreRun(void)
{
    // This needs to be a send event so that the MythUI locks the input devices
    // immediately instead of after existing events are processed
    // since this function could be called inside one of those events.
    if (GetSetting("BlockInputDevs"))
    {
        QEvent event(MythEvent::kLockInputDevicesEventType);
        QCoreApplication::sendEvent(gCoreContext->GetGUIObject(), &event);
    }

    // This needs to be a send event so that the listener is disabled
    // immediately instead of after existing events are processed, since the
    // listen server must be terminated before the spawned application tries
    // to start its own
    if (GetSetting("DisableUDP"))
    {
        QEvent event(MythEvent::kDisableUDPListenerEventType);
        QCoreApplication::sendEvent(gCoreContext->GetGUIObject(), &event);
    }

    // This needs to be a send event so that the MythUI m_drawState change is
    // flagged immediately instead of after existing events are processed
    // since this function could be called inside one of those events.
    if (GetSetting("DisableDrawing"))
    {
        QEvent event(MythEvent::kPushDisableDrawingEventType);
        QCoreApplication::sendEvent(gCoreContext->GetGUIObject(), &event);
    }
}

void MythSystemLegacy::HandlePostRun(void)
{
    // Since this is *not* running in the UI thread (but rather the signal
    // handler thread), we need to use postEvents
    if (GetSetting("DisableDrawing"))
    {
        auto *event = new QEvent(MythEvent::kPopDisableDrawingEventType);
        QCoreApplication::postEvent(gCoreContext->GetGUIObject(), event);
    }

    // This needs to be a post event so we do not try to start listening on
    // the UDP ports before the child application has stopped and terminated
    if (GetSetting("DisableUDP"))
    {
        auto *event = new QEvent(MythEvent::kEnableUDPListenerEventType);
        QCoreApplication::postEvent(gCoreContext->GetGUIObject(), event);
    }

    // This needs to be a post event so that the MythUI unlocks input devices
    // after all existing (blocked) events are processed and ignored.
    if (GetSetting("BlockInputDevs"))
    {
        auto *event = new QEvent(MythEvent::kUnlockInputDevicesEventType);
        QCoreApplication::postEvent(gCoreContext->GetGUIObject(), event);
    }
}

QString MythSystemLegacy::ShellEscape(const QString &in)
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

MythSystemLegacyPrivate::MythSystemLegacyPrivate(const QString &debugName) :
    ReferenceCounter(debugName)
{
}

uint myth_system(const QString &command, uint flags, std::chrono::seconds timeout)
{
    flags |= kMSRunShell | kMSAutoCleanup;
    auto *ms = new MythSystemLegacy(command, flags);
    ms->Run(timeout);
    uint result = ms->Wait(0s);
    if (!ms->GetSetting("RunInBackground"))
        delete ms;

    return result;
}

uint myth_system(const QString &Command, const QStringList& Args, uint Flags,
                 std::chrono::seconds Timeout)
{
    Flags |= kMSRunShell | kMSAutoCleanup;
    auto *ms = new MythSystemLegacy(Command, Args, Flags);
    ms->Run(Timeout);
    uint result = ms->Wait(0s);
    if (!ms->GetSetting("RunInBackground"))
        delete ms;

    return result;
}

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
