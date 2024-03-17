/*
 *  Class TestCommandLineParser
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
#include <string>
#include <vector>
#include "test_mythcommandlineparser.h"

void TestCommandLineParser::initTestCase()
{
    QVERIFY(m_testfile.open());
    m_testfile.write("plugh=xyzzy\n\"plover\"=\"plugh\"\n");
    m_testfile.close();
}

void TestCommandLineParser::cleanupTestCase()
{
}

void TestCommandLineParser::test_getOpt_data (void)
{
    QTest::addColumn<int>("argpos");
    QTest::addColumn<MythCommandLineParser::Result>("expectedResult");
    QTest::addColumn<QString>("expectedOpt");
    QTest::addColumn<QString>("expectedVal");

    QTest::newRow("end")      << 99 << MythCommandLineParser::Result::kEnd << "" << "";
    QTest::newRow("empty")    <<  1 << MythCommandLineParser::Result::kEmpty << "" << "";
    QTest::newRow("first")    <<  0 << MythCommandLineParser::Result::kOptOnly << "-h" << "";
    QTest::newRow("combo")    <<  2 << MythCommandLineParser::Result::kCombOptVal << "-zed" << "100";
    QTest::newRow("badcombo") <<  3 << MythCommandLineParser::Result::kInvalid << "-bad=100=101" << "";
    QTest::newRow("passthru") <<  4 << MythCommandLineParser::Result::kPassthrough << "" << "";
    QTest::newRow("argument") <<  5 << MythCommandLineParser::Result::kArg << "" << "foo";
    QTest::newRow("arg val1") <<  6 << MythCommandLineParser::Result::kOptVal << "-x" << "xray";
    QTest::newRow("arg val2") <<  8 << MythCommandLineParser::Result::kOptVal << "-y" << "-";
    QTest::newRow("arg noval")<< 10 << MythCommandLineParser::Result::kOptOnly << "-z" << "";
    QTest::newRow("arg noval")<< 11 << MythCommandLineParser::Result::kOptOnly << "-a" << "";
}

// \brief Parse individual arguments.  Reset the parser each time.
void TestCommandLineParser::test_getOpt (void)
{
    MythCommandLineParser cmdline("test");
    const std::vector<const char *> argv { "-h", "", "-zed=100", "-bad=100=101", "--", "foo", "-x", "xray", "-y", "-", "-z", "-a"};
    QString opt;
    QByteArray val;

    QFETCH(int, argpos);
    QFETCH(MythCommandLineParser::Result, expectedResult);
    QFETCH(QString, expectedOpt);
    QFETCH(QString, expectedVal);

    MythCommandLineParser::Result actualResult = cmdline.getOpt(argv.size(), argv.data(), argpos, opt, val);
    QCOMPARE(actualResult, expectedResult);
    QCOMPARE(opt, expectedOpt);
    QCOMPARE(QString(val), expectedVal);
}

// \brief Parse multiple arguments.  Use one single parser.
void TestCommandLineParser::test_getOpt_passthrough (void)
{
    MythCommandLineParser cmdline("test");
    const std::vector<const char *> argv { "-x", "xray", "-y", "--", "foo", "-z"};
    QString opt;
    QByteArray val;

    int argpos = 0;
    MythCommandLineParser::Result actualResult = cmdline.getOpt(argv.size(), argv.data(), argpos, opt, val);
    QCOMPARE(actualResult, MythCommandLineParser::Result::kOptVal);
    QCOMPARE(argpos, 1);
    QCOMPARE(opt, QString("-x"));
    QCOMPARE(val, QByteArray("xray"));

    argpos = 2;
    actualResult = cmdline.getOpt(argv.size(), argv.data(), argpos, opt, val);
    QCOMPARE(actualResult, MythCommandLineParser::Result::kOptOnly);
    QCOMPARE(argpos, 2);
    QCOMPARE(opt, QString("-y"));
    QCOMPARE(val, QByteArray(""));

    argpos = 3;
    actualResult = cmdline.getOpt(argv.size(), argv.data(), argpos, opt, val);
    QCOMPARE(actualResult, MythCommandLineParser::Result::kPassthrough);
    QCOMPARE(argpos, 3);
    QCOMPARE(opt, QString(""));
    QCOMPARE(val, QByteArray(""));

    argpos = 4;
    actualResult = cmdline.getOpt(argv.size(), argv.data(), argpos, opt, val);
    QCOMPARE(actualResult, MythCommandLineParser::Result::kArg);
    QCOMPARE(argpos, 4);
    QCOMPARE(opt, QString(""));
    QCOMPARE(val, QByteArray("foo"));

    argpos = 5;
    actualResult = cmdline.getOpt(argv.size(), argv.data(), argpos, opt, val);
    QCOMPARE(actualResult, MythCommandLineParser::Result::kArg);
    QCOMPARE(argpos, 5);
    QCOMPARE(opt, QString(""));
    QCOMPARE(val, QByteArray("-z"));

    argpos = 6;
    actualResult = cmdline.getOpt(argv.size(), argv.data(), argpos, opt, val);
    QCOMPARE(actualResult, MythCommandLineParser::Result::kEnd);
    QCOMPARE(argpos, 6);
    QCOMPARE(opt, QString(""));
    QCOMPARE(val, QByteArray(""));
}

void TestCommandLineParser::test_parse_help (void)
{
    MythCommandLineParser cmdline("test");
    const std::vector<const char *> argv { "thecommand", "-h"};

    // Capture stderr for length of test run
    std::stringstream buffer;
    std::streambuf* oldCoutBuffer = std::cerr.rdbuf(buffer.rdbuf());
    bool result = cmdline.Parse(argv.size(), argv.data());
    std::cerr.rdbuf(oldCoutBuffer);

    QCOMPARE(result, false);
    QVERIFY(cmdline.toBool("showhelp")); // Because of the unknown argument
    QString actualOutput = QString::fromStdString(buffer.str());
    QVERIFY(actualOutput.contains("Unhandled option"));
    QVERIFY(cmdline.m_namedArgs.contains("showhelp")); // Ditto

    // Test 2

    cmdline.addHelp();

    // Capture stderr for length of test run
    buffer.str("");
    oldCoutBuffer = std::cerr.rdbuf(buffer.rdbuf());
    result = cmdline.Parse(argv.size(), argv.data());
    std::cerr.rdbuf(oldCoutBuffer);

    QCOMPARE(result, true);
    QVERIFY(cmdline.toBool("showhelp"));
    QVERIFY(buffer.tellp() == 0);
    QVERIFY(cmdline.m_namedArgs.contains("showhelp"));
}

void TestCommandLineParser::test_overrides (void)
{
    MythCommandLineParser cmdline("test");
    cmdline.addSettingsOverride();
    const std::vector<const char *> argv { "thecommand", "-O", "plugh=xyzzy", "-O", R"("plover"="plugh")"};

    cmdline.Parse(argv.size(), argv.data());
    QVERIFY(cmdline.toBool("overridesettings"));
    QVERIFY(!cmdline.toBool("overridesettingsfile"));

    QMap<QString,QString> overrides = cmdline.GetSettingsOverride();
    QVERIFY(overrides.contains("plugh"));
    QCOMPARE(overrides["plugh"], QString("xyzzy"));
    QVERIFY(overrides.contains("plover"));
    QCOMPARE(overrides["plover"], QString("plugh"));
}

void TestCommandLineParser::test_override_file (void)
{
    MythCommandLineParser cmdline("test");
    cmdline.addSettingsOverride();
    // The result of qPrintable is ephermal.  Copy it somewhere.
    std::string filename = qPrintable(m_testfile.fileName());
    const std::vector<const char *> argv { "thecommand", "--override-settings-file",
                                           filename.c_str()};

    cmdline.Parse(argv.size(), argv.data());
    QVERIFY(!cmdline.toBool("overridesettings"));
    QVERIFY(cmdline.toBool("overridesettingsfile"));

    QMap<QString,QString> overrides = cmdline.GetSettingsOverride();
    QVERIFY(overrides.contains("plugh"));
    QCOMPARE(overrides["plugh"], QString("xyzzy"));
    QVERIFY(overrides.contains("plover"));
    QCOMPARE(overrides["plover"], QString("plugh"));
}

void TestCommandLineParser::test_parse_cmdline_data(void)
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QStringList>("expectedOutput");

    QTest::newRow("simple")
        << R"(This is a test string)"
        << QStringList({"This", "is", "a", "test", "string"});
    QTest::newRow("simplequotes")
        << R"(cmd "whatever" "goes" here)"
        << QStringList({R"(cmd)",
                        R"("whatever")",
                        R"("goes")",
                        R"(here)"});
    QTest::newRow("multiword")
        << R"(cmd "whatever" "multi-word argument" arg3)"
        << QStringList({R"(cmd)",
                        R"("whatever")",
                        R"("multi-word argument")",
                        R"(arg3)"});
    QTest::newRow("mixedargs")
        << R"(cmd --arg1="whatever" --arg2="multi-word argument" --arg3)"
        << QStringList({R"(cmd)",
                        R"(--arg1="whatever")",
                        R"(--arg2="multi-word argument")",
                        R"(--arg3)"});
    QTest::newRow("mixedquotes")
        << R"(cmd --arg1 first-value --arg2 "second 'value'")"
        << QStringList({R"(cmd)",
                        R"(--arg1)",
                        R"(first-value)",
                        R"(--arg2)",
                        R"("second 'value'")"});
    QTest::newRow("mixeduneven")
        << R"(cmd --arg1 first-value --arg2 "second 'value")"
        << QStringList({R"(cmd)",
                        R"(--arg1)",
                        R"(first-value)",
                        R"(--arg2)",
                        R"("second 'value")"});
    QTest::newRow("1escapedquote")
        << R"(cmd -d --arg1 first-value --arg2 \"second)"
        << QStringList({R"(cmd)",
                        R"(-d)",
                        R"(--arg1)",
                        R"(first-value)",
                        R"(--arg2)",
                        R"(\"second)"});
    QTest::newRow("nestedquotes")
        << R"(cmd --arg1 first-value --arg2 "second \"value\"")"
        << QStringList({R"(cmd)",
                        R"(--arg1)",
                        R"(first-value)",
                        R"(--arg2)",
                        R"("second \"value\"")"});
    QTest::newRow("unfinishedquote")
        << R"(cmd --arg1 first-value --arg2 "second \"value\")"
        << QStringList({R"(cmd)",
                        R"(--arg1)",
                        R"(first-value)",
                        R"(--arg2)",
                        R"("second \"value\")"});
}

void TestCommandLineParser::test_parse_cmdline(void)
{
    QFETCH(QString, input);
    QFETCH(QStringList, expectedOutput);

    QStringList output = MythCommandLineParser::MythSplitCommandString(input);
//  std::cerr << "Expected: " << qPrintable(expectedOutput.join("|")) << std::endl;
//  std::cerr << "Actual:   " << qPrintable(output.join("|")) << std::endl;
    QCOMPARE(output, expectedOutput);
}

QTEST_APPLESS_MAIN(TestCommandLineParser)
