/*
 *  Class TestMythSystemLegacy
 *
 *  Copyright (C) Daniel Kristjansson 2013
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

#include <unistd.h> // for usleep()

#include <QTest>
#include <QTemporaryFile>
#include <QDateTime>

#include <iostream>

//#define NEW_LOGGING
#ifdef NEW_LOGGING
#include "mythlogging_extra.h"
#include "debugloghandler.h"
#endif

#include "mythcorecontext.h"
#include "mythsystemlegacy.h"

#ifdef NEW_LOGGING
static DebugLogHandler *console_dbg(void)
{
    return DebugLogHandler::Get("ConsoleLogHandler");
}
#endif

class TestMythSystemLegacy: public QObject
{
    Q_OBJECT

    QDateTime m_before;

    static void Go(MythSystemLegacy &s)
    {
        s.Run();
        (void) s.Wait();
    }

  private slots:
    // called at the beginning of these sets of tests
    static void initTestCase(void)
    {
        gCoreContext = new MythCoreContext("bin_version", nullptr);
#ifdef NEW_LOGGING
        DebugLogHandler::AddReplacement("ConsoleLogHandler");
        myth_logging::initialize_logging(
            VB_GENERAL, LOG_INFO, kNoFacility, /*use_threads*/false,
            /*logfile*/ QString(), /*logprefix*/QString());
#endif
    }

    // called at the end of these sets of tests
    static void cleanupTestCase(void)
    {
        ShutdownMythSystemLegacy();
    }

    // called before each test case
    void init(void)
    {
        m_before = QDateTime::currentDateTime();
    }

    // called after each test case
    void cleanup(void)
    {
        m_before = QDateTime();
    }

    static void constructed_command_is_run(void)
    {
        MythSystemLegacy cmd(QString("echo %1; sleep 1").arg(__FUNCTION__),
                             kMSStdOut | kMSRunShell);
        Go(cmd);
        QVERIFY(QString(cmd.ReadAll()).contains(__FUNCTION__));
    }

    static void wait_returns_exit_code(void)
    {
        MythSystemLegacy cmd("exit 200", kMSNone | kMSRunShell);
        cmd.Run();
        QVERIFY(cmd.Wait() == 200);
    }

    static void getstatus_returns_exit_code(void)
    {
        MythSystemLegacy cmd("exit 200", kMSNone | kMSRunShell);
        Go(cmd);
        QVERIFY(cmd.GetStatus() == 200);
    }

    // TODO kMSDontBlockInputDevs -- avoid blocking LIRC & Joystick Menu
    // TODO kMSDontDisableDrawing -- avoid disabling UI drawing

    // tests kMSRunBackground      -- run child in the background
    void run_in_background_works(void)
    {
        MythSystemLegacy cmd("sleep 2", kMSRunBackground);
        Go(cmd);
        QVERIFY(m_before.msecsTo(QDateTime::currentDateTime()) < 1500);
    }

    // TODO kMSProcessEvents      -- process events while waiting

    // kMSStdIn              -- allow access to stdin
    static void stdin_works(void)
    {
        QTemporaryFile tempfile;
        tempfile.open();
        QByteArray in = QString(__FUNCTION__).toLatin1();
        MythSystemLegacy cmd(QString("cat - > %1").arg(tempfile.fileName()),
                             kMSStdIn | kMSRunShell);
        cmd.Write(in);
        cmd.Run();
        cmd.Wait();
        QVERIFY(cmd.GetStatus() == 0);
        QByteArray out = tempfile.readAll();
        QVERIFY(QString(out).contains(QString(in)));
    }

    // kMSStdOut             -- allow access to stdout
    static void stdout_works(void)
    {
        MythSystemLegacy cmd(QString("echo %1; sleep 1").arg(__FUNCTION__),
                             kMSStdOut | kMSRunShell);
        Go(cmd);
        QVERIFY(cmd.GetStatus() == 0);
        QVERIFY(QString(cmd.ReadAll()).contains(__FUNCTION__));
    }

    // kMSStdErr             -- allow access to stderr
    static void stderr_works(void)
    {
        MythSystemLegacy cmd(QString("echo %1 >&2; sleep 1").arg(__FUNCTION__),
                       kMSRunShell | kMSStdErr);
        Go(cmd);
        QVERIFY(cmd.GetStatus() == 0);
        QVERIFY(QString(cmd.ReadAllErr()).contains(__FUNCTION__));
    }

    // kMSRunShell           -- run process through shell
    static void shell_used_when_requested(void)
    {
        MythSystemLegacy cmd("if [ x != y ] ; then echo X ; else echo Y ; fi; sleep 1",
                       kMSRunShell | kMSStdOut);
        Go(cmd);
        QVERIFY(QString(cmd.ReadAll()).contains("X"));
    }

    static void shell_not_used_when_not_requested(void)
    {
        MythSystemLegacy cmd("if [ x != y ] ; then echo X ; else echo Y ; fi; sleep 1",
                       kMSStdOut);
        Go(cmd);
        QVERIFY(!QString(cmd.ReadAll()).contains("X"));
    }

    // kMSAnonLog            -- anonymize the logs
    static void logs_anonymized_when_requested(void)
    {
#ifdef NEW_LOGGING
        console_dbg()->Clear();
        MythSystemLegacy cmd(QString("echo %1").arg(__FUNCTION__), kMSAnonLog);
        Go(cmd);
        DebugLogHandlerEntry l = console_dbg()->LastEntry(kHandleLog);
        QVERIFY(!l.entry().GetMessage().contains(__FUNCTION__));
        QVERIFY(!cmd.GetLogCmd().contains(__FUNCTION__));
#else
        QSKIP("Log inspection not supported in old logging.");
#endif
    }

    // TODO flags to test
    // TODO kMSAutoCleanup        -- automatically delete if backgrounded
    // TODO kMSDisableUDPListener -- disable MythMessage UDP listener
    //                               for the duration of application.
    // TODO kMSPropagateLogs      -- add arguments for MythTV log propagation

    // TODO test current GetStatus() results.
};
