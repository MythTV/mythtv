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
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include <unistd.h> // for usleep()

#include <QtTest/QtTest>
#include <QTemporaryFile>
#include <QDateTime>

#include <iostream>
using namespace std;

//#define NEW_LOGGING
#ifdef NEW_LOGGING
#include "mythlogging_extra.h"
#include "debugloghandler.h"
#endif

#include "mythcorecontext.h"
#include "mythsystem.h"

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#define MSKIP(MSG) QSKIP(MSG, SkipSingle)
#else
#define MSKIP(MSG) QSKIP(MSG)
#endif

#ifdef NEW_LOGGING
static DebugLogHandler *console_dbg(void)
{
    return DebugLogHandler::Get("ConsoleLogHandler");
}
#endif

class TestMythSystem: public QObject
{
    Q_OBJECT

    QDateTime m_before;

  private slots:
    // called at the beginning of these sets of tests
    void initTestCase(void)
    {
        gCoreContext = new MythCoreContext("bin_version", NULL);
#ifdef NEW_LOGGING
        DebugLogHandler::AddReplacement("ConsoleLogHandler");
        myth_logging::initialize_logging(
            VB_GENERAL, LOG_INFO, kNoFacility, /*use_threads*/false,
            /*logfile*/ QString(), /*logprefix*/QString());
#endif
    }

    // called at the end of these sets of tests
    void cleanupTestCase(void)
    {
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

    void constructed_command_is_run(void)
    {
        QScopedPointer<MythSystem> cmd(
            MythSystem::Create(
                QString("echo %1").arg(__FUNCTION__), kMSStdOut));
        cmd->Wait();
        QVERIFY(QString(cmd->GetStandardOutputStream()->readAll())
                .contains(__FUNCTION__));
    }

    // TODO kMSDontBlockInputDevs -- avoid blocking LIRC & Joystick Menu
    // TODO kMSDontDisableDrawing -- avoid disabling UI drawing

    // tests kMSRunBackground      -- run child in the background
    void run_in_background_works(void)
    {
        MSKIP("MythSystemLegacyPrivate calls MythSystem::Unlock"
              "after the instance is deleted");
        QScopedPointer<MythSystem> cmd(
            MythSystem::Create("sleep 0.5", kMSRunBackground));
        cmd->Wait();
        QVERIFY(m_before.msecsTo(QDateTime::currentDateTime()) < 400);
    }

    // TODO kMSProcessEvents      -- process events while waiting

    // kMSStdIn              -- allow access to stdin
    void stdin_works(void)
    {
        MSKIP("stdin_works -- currently blocks forever");
        QTemporaryFile tempfile;
        tempfile.open();
        QByteArray in = QString(__FUNCTION__).toLatin1();
        QScopedPointer<MythSystem> cmd(
            MythSystem::Create(QString("cat - > %1").arg(tempfile.fileName()),
                               kMSStdIn | kMSRunShell));
        cmd->GetStandardInputStream()->write(in);
        cmd->GetStandardInputStream()->close();
        cerr << "stdin_works -- Wait starting" << endl;
        cmd->Wait(0);
        QVERIFY(cmd->GetExitCode() == 0);
        QByteArray out = tempfile.readAll();
        QVERIFY(QString(out).contains(QString(in)));
    }

    // kMSStdOut             -- allow access to stdout
    void stdout_works(void)
    {
        QScopedPointer<MythSystem> cmd(
            MythSystem::Create(QString("echo %1").arg(__FUNCTION__),
                               kMSStdOut));
        cmd->Wait();
        QVERIFY(cmd->GetExitCode() == 0);
        QVERIFY(cmd->GetStandardOutputStream());
        QVERIFY(cmd->GetStandardOutputStream()->isOpen());
        QByteArray ba = cmd->GetStandardOutputStream()->readAll();
        QVERIFY(QString(ba).contains(__FUNCTION__));
    }

    // kMSStdErr             -- allow access to stderr
    void stderr_works(void)
    {
        QScopedPointer<MythSystem> cmd(
            MythSystem::Create(QString("echo %1 >&2").arg(__FUNCTION__),
                               kMSRunShell | kMSStdErr));
        cmd->Wait();
        QVERIFY(cmd->GetExitCode() == 0);
        QVERIFY(cmd->GetStandardErrorStream());
        QVERIFY(cmd->GetStandardErrorStream()->isOpen());
        QByteArray ba = cmd->GetStandardErrorStream()->readAll();
        QVERIFY(QString(ba).contains(__FUNCTION__));
    }

    // kMSRunShell           -- run process through shell
    void shell_used_when_requested(void)
    {
        QScopedPointer<MythSystem> cmd(
            MythSystem::Create("if [ x != y ] ; then echo X ; else echo Y ; fi",
                               kMSRunShell | kMSStdOut));
        cmd->Wait();
        QVERIFY(QString(cmd->GetStandardOutputStream()->readAll())
                .contains("X"));
    }

    void shell_not_used_when_not_requested(void)
    {
        QScopedPointer<MythSystem> cmd(
            MythSystem::Create("if [ x != y ] ; then echo X ; else echo Y ; fi",
                               kMSStdOut));
        cmd->Wait();
        QVERIFY(!QString(cmd->GetStandardOutputStream()->readAll())
                .contains("X"));
    }

    // kMSAnonLog            -- anonymize the logs
    void logs_anonymized_when_requested(void)
    {
#ifdef NEW_LOGGING
        console_dbg()->Clear();
        QScopedPointer<MythSystem> cmd(
            MythSystem::Create(QString("echo %1").arg(__FUNCTION__),
                               kMSAnonLog));
        cmd->Wait();
        DebugLogHandlerEntry l = console_dbg()->LastEntry(kHandleLog);
        QVERIFY(!l.entry().GetMessage().contains(__FUNCTION__));
#else
        MSKIP("Log inspection not supported in old logging.");
#endif
    }

    // kMSAutoCleanup        -- automatically delete if backgrounded
    void auto_cleanup_return_null(void)
    {
        MythSystem *ptr = MythSystem::Create(
            "sleep 10", kMSAutoCleanup | kMSRunBackground);
        QVERIFY(!ptr);
    }

    // TODO kMSDisableUDPListener -- disable MythMessage UDP listener
    //                               for the duration of application.
    // TODO kMSPropagateLogs      -- add arguments for MythTV log propagation

    void get_flags_returns_flags_sent(void)
    {
        QScopedPointer<MythSystem> cmd(
            MythSystem::Create("exit 5", kMSStdOut | kMSDontDisableDrawing));
        QVERIFY(cmd->GetFlags() == (kMSStdOut | kMSDontDisableDrawing));
    }

    void get_starting_path_returns_path_sent(void)
    {
        QScopedPointer<MythSystem> cmd(
            MythSystem::Create("exit 5", kMSNone, "/tmp"));
        QVERIFY(cmd->GetStartingPath() == "/tmp");
    }

    void get_starting_path_returns_a_path_when_none_sent(void)
    {
        MSKIP("Not working yet");
        QScopedPointer<MythSystem> cmd(
            MythSystem::Create("exit 5", kMSNone));
        QVERIFY(!cmd->GetStartingPath().isEmpty());
    }

    void get_cpu_priority_returns_priority_sent(void)
    {
        MSKIP("Not working yet");
        QScopedPointer<MythSystem> cmd(
            MythSystem::Create(
                "exit 5", kMSNone, QString(), MythSystem::kLowPriority));
        QVERIFY(cmd->GetCPUPriority() == MythSystem::kLowPriority);
    }

    void get_disk_priority_returns_priority_sent(void)
    {
        MSKIP("Not working yet");
        QScopedPointer<MythSystem> cmd(
            MythSystem::Create(
                "exit 5", kMSNone, QString(),
                MythSystem::kInheritPriority, MythSystem::kLowPriority));
        QVERIFY(cmd->GetDiskPriority() == MythSystem::kLowPriority);
    }

    void wait_returns_true_on_exit(void)
    {
        QScopedPointer<MythSystem> cmd(
            MythSystem::Create("exit 200", kMSRunShell));
        QVERIFY(cmd->Wait());
    }

    void wait_returns_false_on_timeout(void)
    {
        QScopedPointer<MythSystem> cmd(
            MythSystem::Create("sleep 2", kMSRunShell));
        QVERIFY(!cmd->Wait(1));
    }

    void getexitcode_returns_exit_code_when_non_zero(void)
    {
        QScopedPointer<MythSystem> cmd(
            MythSystem::Create("exit 200", kMSRunShell));
        cmd->Wait();
        QVERIFY(cmd->GetExitCode() == 200);
    }

    void getexitcode_returns_neg_1_when_signal_seen(void)
    {
        QScopedPointer<MythSystem> cmd(
            MythSystem::Create("sleep 0.5", kMSRunShell));
        usleep(50 * 1000);
        cmd->Signal(kSignalQuit);
        cmd->Wait();
        QVERIFY(cmd->GetExitCode() == -1);
    }

    void getexitcode_returns_neg_2_when_still_running(void)
    {
        QScopedPointer<MythSystem> cmd(
            MythSystem::Create("sleep 0.25", kMSRunShell));
        QVERIFY(cmd->GetExitCode() == -2);
    }
};
