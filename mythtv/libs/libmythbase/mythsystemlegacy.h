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

#ifndef MYTHSYSTEMLEGACY_H
#define MYTHSYSTEMLEGACY_H

#include "mythbaseexp.h"

// Note: The FIXME were added 2013-05-18, as part of a multi-pass
// cleanup of MythSystemLegacy.
//
// MythSystemLegacy exists because the Qt QProcess has caused us numerous
// headaches, most importantly:
//   http://code.mythtv.org/trac/ticket/7135
//   https://bugreports.qt-project.org/browse/QTBUG-5990
// QProcess also demands that the thread in which it is used
// and created run a Qt event loop, which MythSystemLegacy does not
// require. A number of MythTV threads do not run an event loop
// so this requirement is a bit onerous.
//
// MythSystemLegacy has grown a bit fat around the middle as functionality
// has been added and as a core functionality class is due for a code
// review and some cleanup.

// C headers
#include <array>
#include <cstdint>

#include "exitcodes.h"  // included for GENERIC_EXIT_OK
#include "mythchrono.h"
#include "mythsystem.h" // included for MythSystemFlag and MythSignal

#include <QStringList>
#include <QSemaphore>
#include <QBuffer>
#include <QObject>
#include <QString>
#include <QMap> // FIXME: This shouldn't be needed, Setting_t is not public

using Setting = QMap<QString, bool>;

void MBASE_PUBLIC ShutdownMythSystemLegacy(void);

// FIXME: Does MythSystemLegacy really need to inherit from QObject?
//        we can probably create a private class that inherits
//        from QObject to avoid exposing lots of thread-unsafe
//        methods & complicated life-time management.
class MythSystemLegacyPrivate;
class MBASE_PUBLIC MythSystemLegacy : public QObject
{
    Q_OBJECT

  public:
    explicit MythSystemLegacy(QObject *parent = nullptr);
    MythSystemLegacy(const QString &command, uint flags, QObject *parent = nullptr);
    MythSystemLegacy(const QString &command, const QStringList &args, uint flags,
                     QObject *parent = nullptr);
    ~MythSystemLegacy(void) override;

    // FIXME: We should not allow a MythSystemLegacy to be reused for a new command.
    void SetCommand(const QString &command, uint flags);
    // FIXME: We should not allow a MythSystemLegacy to be reused for a new command.
    void SetCommand(const QString &command, const QStringList &args, uint flags);
    // FIXME: This should only be in the constructor
    void SetDirectory(const QString &directory);
    // FIXME: This should only be in the constructor
    bool SetNice(int nice);
    // FIXME: This should only be in the constructor
    bool SetIOPrio(int prio);

    // FIXME: Forks, should be called Start() for consistency with MThread.
    // FIXME: Do we need this call at all?
    void Run(std::chrono::seconds timeout = 0s);
    // FIXME: This should just return a bool telling us if we hit the timeout.
    uint Wait(std::chrono::seconds timeout = 0s);

    int Write(const QByteArray &ba);
    QByteArray Read(int size);
    QByteArray ReadErr(int size);
    // FIXME: We should not return a reference here
    QByteArray& ReadAll();
    // FIXME: We should not return a reference here
    QByteArray& ReadAllErr();

    // FIXME: Can Term be wrapped into Signal?
    void Term(bool force = false);
    void Signal(MythSignal sig);

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
    // FIXME: Document what this actually returns.
    uint GetStatus(void) const       { return m_status; }
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
    void SetCommand(const QString &cmd)    { m_command = cmd; }

    // FIXME: We should not return a reference here
    // FIXME: Rename "GetArguments"
    QStringList &GetArgs(void)       { return m_args; }
    // FIXME: Eliminate. We should not allow setting the arguements
    //        after construcion.
    void SetArgs(const QStringList &args)  { m_args = args; }

    int GetNice(void) const          { return m_nice; }
    int GetIOPrio(void) const        { return m_ioprio; }

    // FIXME: We should not return a pointer to a QBuffer
    QBuffer *GetBuffer(int index)    { return &m_stdbuff[index]; }

    // FIXME: We should not make locking public
    void Unlock(void)                { m_semReady.release(1); }

    friend class MythSystemLegacyPrivate;

    // FIXME: This should be an independent function living elsewhere
    static QString ShellEscape(const QString &in);

    // FIXME: Do we really need to expose Qt signals?
    //        If so why are they lower case?
  signals:
    void started(void);
    void finished(void);
    void error(uint status);
    void readDataReady(int fd);

  private:
    Q_DISABLE_COPY(MythSystemLegacy)
    void initializePrivate(void);
    // NOLINTNEXTLINE(readability-identifier-naming)
    MythSystemLegacyPrivate *d {nullptr}; // FIXME we generally call this m_priv in MythTV

  protected:
    void ProcessFlags(uint flags);

    // FIXME if we already have a private helper, why all this?
    uint        m_status {GENERIC_EXIT_OK};
    QSemaphore  m_semReady;

    QString     m_command;
    QString     m_logcmd;
    QStringList m_args;
    QString     m_directory;

    int         m_nice   {0};
    int         m_ioprio {0};

    Setting     m_settings;
    std::array<QBuffer,3> m_stdbuff;
};

MBASE_PUBLIC uint myth_system(const QString &command,
                              uint flags = kMSNone,
                              std::chrono::seconds timeout = 0s);
MBASE_PUBLIC uint myth_system(const QString &Command, const QStringList& Args,
                              uint Flags = kMSNone, std::chrono::seconds Timeout = 0s);

#endif // MYTHSYSTEMLEGACY_H
/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
