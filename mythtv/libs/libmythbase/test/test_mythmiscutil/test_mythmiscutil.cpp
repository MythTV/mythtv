/*
 *  Class TestMiscUtil
 *
 *  Copyright (c) David Hampton 2018
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
#include <iostream>
#include "test_mythmiscutil.h"

void TestMiscUtil::test_parse_cmdline_data(void)
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

void TestMiscUtil::test_parse_cmdline(void)
{
    QFETCH(QString, input);
    QFETCH(QStringList, expectedOutput);

    QStringList output = MythSplitCommandString(input);
//  std::cerr << "Expected: " << qPrintable(expectedOutput.join("|")) << std::endl;
//  std::cerr << "Actual:   " << qPrintable(output.join("|")) << std::endl;
    QCOMPARE(output, expectedOutput);
}


QTEST_APPLESS_MAIN(TestMiscUtil)
