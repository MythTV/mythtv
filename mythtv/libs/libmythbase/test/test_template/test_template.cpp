/*
 *  Class TestTemplate
 *
 *  Copyright (C) AUTHOR_GOES_HERE 2013
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

#include "test_template.h"
#include "iso639.h"

// called at the beginning of these sets of tests
void TestTemplate::initTestCase(void)
{
}

// called at the end of these sets of tests
void TestTemplate::cleanupTestCase(void)
{
}

// called before each test case
void TestTemplate::init(void)
{
}

// called after each test case
void TestTemplate::cleanup(void)
{
}

// example passing test
void TestTemplate::example_passing_unit_test(void)
{
    QVERIFY(true);
}

// example benchmark test
void TestTemplate::example_benchmark_test(void)
{
    QBENCHMARK
    {
        [[maybe_unused]] int sum = 0;
        for (int i = 0; i < 999; i++)
            sum += i;
    }
}

// example skipped test
void TestTemplate::example_skipped_test(void)
{
    QSKIP("this test should pass, but doesn't yet");
    QVERIFY(true); // yes this really would pass, but this is an example
}

// example test with data function
void TestTemplate::example_repeated_test_data(void)
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("expectedOutput");

    QTest::newRow("en")  << "en"  << "English";
    QTest::newRow("zh")  << "zh"  << "Chinese";
}

void TestTemplate::example_repeated_test(void)
{
    QFETCH(QString, input);
    QFETCH(QString, expectedOutput);

    // Process the input data here
    // actualOutput = do_something(input);
    QString actualOutput =
        iso639_str_toName((uchar*)input.toLatin1().constData());
    QCOMPARE(actualOutput, expectedOutput);
}

QTEST_APPLESS_MAIN(TestTemplate)
