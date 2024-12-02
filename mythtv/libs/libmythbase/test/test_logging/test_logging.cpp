/*
 *  Class TestLogging
 *
 *  Copyright (c) David Hampton 2020
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
#include "test_logging.h"

void TestLogging::initialize (void)
{
    QCOMPARE(logLevelGet("force initialization"), LOG_UNKNOWN);
}

void TestLogging::test_syslogGetFacility_data (void)
{
    QTest::addColumn<QString>("string");
    QTest::addColumn<int>("expected");

#ifdef _WIN32
#elif defined(Q_OS_ANDROID)
#else
    QTest::newRow("auth")   << "auth"   << LOG_AUTH;
    QTest::newRow("user")   << "user"   << LOG_USER;
    QTest::newRow("local7") << "local7" << LOG_LOCAL7;
    QTest::newRow("random") << "random" << -1;
    QTest::newRow("empty")  << ""       << -1;
#endif
}

void TestLogging::test_syslogGetFacility (void)
{
    QFETCH(QString, string);
    QFETCH(int, expected);

    int actual = syslogGetFacility(string);
#ifdef _WIN32
    QCOMPARE(actual, -2);
#elif defined(Q_OS_ANDROID)
    QCOMPARE(actual, -2);
#else
    QCOMPARE(actual, expected);
#endif
}

void TestLogging::test_logLevelGet_data (void)
{
    QTest::addColumn<QString>("string");
    QTest::addColumn<int>("result");

    QTest::newRow("any")     << "any"      << static_cast<int>(LOG_ANY);
    QTest::newRow("alert")   << "alert"  << static_cast<int>(LOG_ALERT);
    QTest::newRow("debug")   << "debug"  << static_cast<int>(LOG_DEBUG);
    QTest::newRow("unknown") << "-"      << static_cast<int>(LOG_UNKNOWN);
    QTest::newRow("random")  << "~"      << static_cast<int>(LOG_UNKNOWN);
    QTest::newRow("empty")   << ""       << static_cast<int>(LOG_UNKNOWN);
    QTest::newRow("long")    << "ABCDE"  << static_cast<int>(LOG_UNKNOWN);
}

void TestLogging::test_logLevelGet (void)
{
    QFETCH(QString, string);
    QFETCH(int, result);

    QCOMPARE(static_cast<int>(logLevelGet(string)), result);
}

void TestLogging::test_logLevelGetName_data (void)
{
    QTest::addColumn<int>("value");
    QTest::addColumn<QString>("result");

    QTest::newRow("any")     << static_cast<int>(LOG_ANY)     << "any";
    QTest::newRow("alert")   << static_cast<int>(LOG_ALERT)   << "alert" ;
    QTest::newRow("debug")   << static_cast<int>(LOG_DEBUG)   << "debug";
    QTest::newRow("unknown") << static_cast<int>(LOG_UNKNOWN) << "unknown";
    QTest::newRow("random")  << -42                           << "unknown";
}

void TestLogging::test_logLevelGetName (void)
{
    QFETCH(int, value);
    QFETCH(QString, result);

    QCOMPARE(logLevelGetName(static_cast<LogLevel_t>(value)), result);
}

void TestLogging::test_verboseArgParse_kwd_data (void)
{
    QTest::addColumn<QString>("argument");
    QTest::addColumn<int>("expectedExit");
    QTest::addColumn<QString>("expectedOutput");

    QTest::newRow("empty")     << ""          << (int)GENERIC_EXIT_OK << "";
    QTest::newRow("nextarg")   << "-x"        << (int)GENERIC_EXIT_INVALID_CMDLINE << "Invalid or missing";
    QTest::newRow("help")      << "help"      << (int)GENERIC_EXIT_INVALID_CMDLINE << "Verbose debug levels";
    QTest::newRow("important") << "important" << (int)GENERIC_EXIT_OK << R"("important" log mask)";
    QTest::newRow("extra")     << "extra"     << (int)GENERIC_EXIT_OK << R"("extra" log mask)";
    QTest::newRow("random")    << "random"    << (int)GENERIC_EXIT_INVALID_CMDLINE << "Unknown argument";
}

void TestLogging::test_verboseArgParse_kwd (void)
{
    QFETCH(QString, argument);
    QFETCH(int, expectedExit);
    QFETCH(QString, expectedOutput);

    // Capture stderr for length of test run
    std::stringstream buffer;
    std::streambuf* oldCoutBuffer = std::cerr.rdbuf(buffer.rdbuf());
    resetLogging();
    int actualExit = verboseArgParse(argument);
    std::cerr.rdbuf(oldCoutBuffer);

    // Check results
    QCOMPARE(expectedExit, actualExit);
    if (expectedOutput.isEmpty()) {
        QVERIFY(buffer.tellp() == 0);
    } else {
        QString actualOutput = QString::fromStdString(buffer.str());
        QVERIFY(actualOutput.contains(expectedOutput));
    }
}

void TestLogging::test_verboseArgParse_twice (void)
{
    // Capture stderr for length of test run
    std::stringstream buffer;
    std::streambuf* oldCoutBuffer = std::cerr.rdbuf(buffer.rdbuf());
    resetLogging();
    int actualExit1 = verboseArgParse("general,system,socket");
    int actualExit2 = verboseArgParse("help");
    std::cerr.rdbuf(oldCoutBuffer);

    // Check results
    QCOMPARE((int)GENERIC_EXIT_OK, actualExit1);
    QCOMPARE((int)GENERIC_EXIT_INVALID_CMDLINE, actualExit2);
    QString actualOutput = QString::fromStdString(buffer.str());
    QVERIFY(actualOutput.contains("-v general,system,socket"));
}

void TestLogging::test_verboseArgParse_class_data (void)
{
    QTest::addColumn<QString>("argument");
    QTest::addColumn<uint64_t>("expectedVMask");
    QTest::addColumn<QString>("expectedVString");

    QTest::newRow("general")         << "general"
                                     << static_cast<uint64_t>(VB_GENERAL) << "general";
    QTest::newRow("general,system")  << "general,system"
                                     << static_cast<uint64_t>(VB_SYSTEM|VB_GENERAL) << "general system";
    QTest::newRow("system,general")  << "system,general"
                                     << static_cast<uint64_t>(VB_SYSTEM|VB_GENERAL) << "general system";
    QTest::newRow("most,system")     << "most,system"
                                     << static_cast<uint64_t>(VB_MOST) << "most";
    QTest::newRow("most,nocommflag") << "most,nocommflag"
                                     << static_cast<uint64_t>(VB_MOST & ~VB_COMMFLAG) << "most nocommflag";
    QTest::newRow("none")            << "none"
                                     << static_cast<uint64_t>(VB_NONE) << "none";
    QTest::newRow("general,none")    << "general,none"
                                     << static_cast<uint64_t>(VB_NONE) << "none";
    QTest::newRow("default")         << "default"
                                     << static_cast<uint64_t>(VB_GENERAL) << "general";
    QTest::newRow("record,default")  << "record,default"
                                     << static_cast<uint64_t>(VB_GENERAL) << "general";
}

void TestLogging::test_verboseArgParse_class (void)
{
    QFETCH(QString, argument);
    QFETCH(uint64_t, expectedVMask);
    QFETCH(QString, expectedVString);

    // Capture stderr for length of test run
    std::stringstream buffer;
    std::streambuf* oldCoutBuffer = std::cerr.rdbuf(buffer.rdbuf());
    resetLogging();
    int actualExit = verboseArgParse(argument);
    std::cerr.rdbuf(oldCoutBuffer);

    // Check results
    QCOMPARE((int)GENERIC_EXIT_OK, actualExit);
    QString actualOutput = QString::fromStdString(buffer.str());
    QVERIFY(actualOutput.isEmpty());
    QCOMPARE(verboseMask, expectedVMask);
    QCOMPARE(verboseString.trimmed(), expectedVString);
}

void TestLogging::test_verboseArgParse_level_data (void)
{
    QTest::addColumn<QString>("argument");
    QTest::addColumn<uint64_t>("expectedVMask");
    QTest::addColumn<uint64_t>("expectedGroup1");
    QTest::addColumn<int>("expectedLevel1");
    QTest::addColumn<uint64_t>("expectedGroup2");
    QTest::addColumn<int>("expectedLevel2");
    QTest::addColumn<QString>("expectedVString");

    // The oddity here is that even though "info" is the default
    // level, these first two tests create different settings in the
    // program even though they should produce identical output.
    QTest::newRow("general")         << "general"
                                     << static_cast<uint64_t>(VB_GENERAL)
                                     << static_cast<uint64_t>(0) << 0
                                     << static_cast<uint64_t>(0) << 0
                                     << "general";
    QTest::newRow("general:info")    << "general:info"
                                     << static_cast<uint64_t>(VB_GENERAL)
                                     << static_cast<uint64_t>(VB_GENERAL) << static_cast<int>(LOG_INFO)
                                     << static_cast<uint64_t>(0) << 0
                                     << "general";
    QTest::newRow("general:notice")  << "general:notice"
                                     << static_cast<uint64_t>(VB_GENERAL)
                                     << static_cast<uint64_t>(VB_GENERAL) << static_cast<int>(LOG_NOTICE)
                                     << static_cast<uint64_t>(0) << 0
                                     << "general";
    QTest::newRow("general:notice,file:debug")
                                     << "general:notice,file:debug"
                                     << static_cast<uint64_t>(VB_GENERAL|VB_FILE)
                                     << static_cast<uint64_t>(VB_GENERAL)
                                     << static_cast<int>(LOG_NOTICE)
                                     << static_cast<uint64_t>(VB_FILE)
                                     << static_cast<int>(LOG_DEBUG)
                                     << "general file";
}

void TestLogging::test_verboseArgParse_level (void)
{
    QFETCH(QString, argument);
    QFETCH(uint64_t, expectedVMask);
    QFETCH(uint64_t, expectedGroup1);
    QFETCH(int, expectedLevel1);
    QFETCH(uint64_t, expectedGroup2);
    QFETCH(int, expectedLevel2);
    QFETCH(QString, expectedVString);

    // Capture stderr for length of test run
    std::stringstream buffer;
    std::streambuf* oldCoutBuffer = std::cerr.rdbuf(buffer.rdbuf());
    resetLogging();
    int actualExit = verboseArgParse(argument);
    std::cerr.rdbuf(oldCoutBuffer);

    // Check results
    QCOMPARE((int)GENERIC_EXIT_OK, actualExit);
    QString actualOutput = QString::fromStdString(buffer.str());
    QVERIFY(actualOutput.isEmpty());
    QCOMPARE(verboseMask, expectedVMask);

    if (expectedGroup1) {
        QVERIFY(componentLogLevel.contains(expectedGroup1));
        QCOMPARE(static_cast<int>(componentLogLevel[expectedGroup1]), expectedLevel1);
        if (expectedGroup2) {
            QVERIFY(componentLogLevel.contains(expectedGroup2));
            QCOMPARE(static_cast<int>(componentLogLevel[expectedGroup2]), expectedLevel2);
        }
    }
        
    QCOMPARE(verboseString.trimmed(), expectedVString);
}

void TestLogging::test_logPropagateCalc_data (void)
{
    QTest::addColumn<QString>("argument");
    QTest::addColumn<int>("quiet");
    QTest::addColumn<int>("facility");
    QTest::addColumn<bool>("propagate");
    QTest::addColumn<QString>("expectedArgs");

    QTest::newRow("plain")   << "general"
                             << 0 << -1
                             << false
                             << "--verbose general --loglevel info";
    QTest::newRow("path")    << "general"
                             << 0 << -1
                             << true
                             << "--verbose general --logpath /tmp --loglevel info";
    QTest::newRow("quiet")   << "general"
                             << 2 << -1
                             << false
                             << "--verbose general --loglevel info --quiet --quiet";
#if !defined(_WIN32) && !defined(Q_OS_ANDROID)
    QTest::newRow("syslog")  << "general"
                             << 0 << LOG_DAEMON
                             << false
                             << "--verbose general --loglevel info --syslog daemon";
#if CONFIG_SYSTEMD_JOURNAL
    QTest::newRow("systemd") << "general"
                             << 0 << SYSTEMD_JOURNAL_FACILITY
                             << false
                             << "--verbose general --loglevel info --systemd-journal";
#endif
#endif
    QTest::newRow("muddle")  << "general,schedule"
                             << 2 << LOG_LOCAL0
                             << true
                             << "--verbose general,schedule --logpath /tmp --loglevel info --quiet --quiet --syslog local0";
    QTest::newRow("muddle2") << "schedule:debug,general:warn"
                             << 2 << LOG_LOCAL0
                             << true
                             << "--verbose general,schedule --logpath /tmp --loglevel info --quiet --quiet --syslog local0";
}

void TestLogging::test_logPropagateCalc (void)
{
    QFETCH(QString, argument);
    QFETCH(int, quiet);
    QFETCH(int, facility);
    QFETCH(bool, propagate);
    QFETCH(QString, expectedArgs);

    // Capture stderr for length of test run
    std::stringstream buffer;
    std::streambuf* oldCoutBuffer = std::cerr.rdbuf(buffer.rdbuf());
    resetLogging();
    int actualExit = verboseArgParse(argument);
    logStart("/tmp/test", false, quiet, facility, LOG_INFO, propagate, false, true);
    std::cerr.rdbuf(oldCoutBuffer);

    // Check results
    QCOMPARE((int)GENERIC_EXIT_OK, actualExit);
    QString actualOutput = QString::fromStdString(buffer.str());
    QVERIFY(actualOutput.isEmpty());

    QCOMPARE(logPropagateArgs.trimmed(), expectedArgs);
}

QTEST_APPLESS_MAIN(TestLogging)
