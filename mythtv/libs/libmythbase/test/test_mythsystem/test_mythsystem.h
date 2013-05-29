/*
 *  Class TestMythSystem
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
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <QtTest/QtTest>
#include <QTemporaryFile>
#include <QDateTime>

#include <iostream>
using namespace std;

#include "mythlogging_extra.h"
#include "debugloghandler.h"
#include "mythcorecontext.h"
#include "mythsystem.h"

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#define MSKIP(MSG) QSKIP(MSG, QTest::SkipSingle)
#else
#define MSKIP(MSG) QSKIP(MSG)
#endif

static DebugLogHandler *console_dbg(void)
{
    return DebugLogHandler::Get("ConsoleLogHandler");
}

class TestMythSystem: public QObject
{
    Q_OBJECT

  private slots:
    // called at the beginning of these sets of tests
    void initTestCase(void)
    {
        gCoreContext = new MythCoreContext("bin_version", NULL);
        DebugLogHandler::AddReplacement("ConsoleLogHandler");
        myth_logging::initialize_logging(
            VB_GENERAL, LOG_INFO, kNoFacility, /*use_threads*/false,
            /*logfile*/ QString(), /*logprefix*/QString());
    }

    // called at the end of these sets of tests
    void cleanupTestCase(void)
    {
        ShutdownMythSystem();
    }

    // called before each test case
    void init(void)
    {
    }

    // called after each test case
    void cleanup(void)
    {
    }

    void constructed_command_is_run(void)
    {
        MythSystem cmd(QString("echo %1").arg(__FUNCTION__), kMSStdOut);
        cmd.Run();
        int ret = cmd.Wait();
        QVERIFY(ret == 0);
        QVERIFY(QString(cmd.ReadAll()).contains(__FUNCTION__));
    }

    void exit_code_works(void)
    {
        MythSystem cmd("exit 34", kMSNone | kMSRunShell);
        cmd.Run();
        int ret = cmd.Wait();
        QVERIFY(ret == 34);
    }

    // TODO kMSDontBlockInputDevs -- avoid blocking LIRC & Joystick Menu
    // TODO kMSDontDisableDrawing -- avoid disabling UI drawing

    // tests kMSRunBackground      -- run child in the background
    void run_in_background_works(void)
    {
        MythSystem cmd("sleep 1", kMSRunBackground);
        QDateTime before = QDateTime::currentDateTime();
        cmd.Run();
        (void) cmd.Wait();
        QDateTime after = QDateTime::currentDateTime();
        QVERIFY(before.msecsTo(after) < 500);
    }

    // TODO kMSProcessEvents      -- process events while waiting
    // TODO kMSInUi               -- the parent is in the UI

    // kMSStdIn              -- allow access to stdin
    void stdin_works(void)
    {
        MSKIP("stdin_works -- currently blocks forever");
        QTemporaryFile tempfile;
        tempfile.open();
        QByteArray in = QString(__FUNCTION__).toLatin1();
        MythSystem cmd(QString("cat - > %1").arg(tempfile.fileName()),
                       kMSStdIn);
        cmd.Write(in);
        cmd.Run();
        int ret = cmd.Wait();
        QVERIFY(ret == 0);
        QByteArray out = tempfile.readAll();
        QVERIFY(QString(out).contains(QString(in)));
    }

    // kMSStdOut             -- allow access to stdout
    void stdout_works(void)
    {
        MythSystem cmd(QString("echo %1").arg(__FUNCTION__), kMSStdOut);
        cmd.Run();
        int ret = cmd.Wait();
        QVERIFY(ret == 0);
        QVERIFY(QString(cmd.ReadAll()).contains(__FUNCTION__));
    }

    // kMSStdErr             -- allow access to stderr
    void stderr_works(void)
    {
        MythSystem cmd(QString("echo %1 >&2").arg(__FUNCTION__),
                       kMSRunShell | kMSStdErr);
        cmd.Run();
        int ret = cmd.Wait();
        QVERIFY(ret == 0);
        QVERIFY(QString(cmd.ReadAllErr()).contains(__FUNCTION__));
    }
    // TODO kMSBuffered           -- buffer the IO channels

    // kMSRunShell           -- run process through shell
    void shell_used_when_requested(void)
    {
        MythSystem cmd("if [ x != y ] ; then echo X ; else echo Y ; fi",
                       kMSRunShell | kMSStdOut);
        cmd.Run();
        (void) cmd.Wait();
        QVERIFY(QString(cmd.ReadAll()).contains("X"));
    }

    void shell_not_used_when_not_requested(void)
    {
        MythSystem cmd("if [ x != y ] ; then echo X ; else echo Y ; fi",
                       kMSStdOut);
        cmd.Run();
        (void) cmd.Wait();
        QVERIFY(!QString(cmd.ReadAll()).contains("X"));
    }

    // no need to test kMSNoRunShell, it is a no-op
    // TODO delete flag
    // kMSNoRunShell         -- do NOT run process through shell

    // kMSAnonLog            -- anonymize the logs
    void logs_anonymized_when_requested(void)
    {
        console_dbg()->Clear();
        MythSystem cmd(QString("echo %1").arg(__FUNCTION__), kMSAnonLog);
        cmd.Run();
        (void) cmd.Wait();
        DebugLogHandlerEntry l = console_dbg()->LastEntry(kHandleLog);
        QVERIFY(!l.entry().GetMessage().contains(__FUNCTION__));
        QVERIFY(!cmd.GetLogCmd().contains(__FUNCTION__));
    }

    // TODO flags to test
    // TODO kMSAbortOnJump        -- abort this process on a jumppoint
    // TODO kMSSetPGID            -- set the process group id
    // TODO kMSAutoCleanup        -- automatically delete if backgrounded
    // TODO kMSLowExitVal         -- allow exit values 0-127 only
    // TODO kMSDisableUDPListener -- disable MythMessage UDP listener
    //                               for the duration of application.
    // TODO kMSPropagateLogs      -- add arguments for MythTV log propagation

};
