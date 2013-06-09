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
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <QtTest/QtTest>

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#define MSKIP(MSG) QSKIP(MSG, SkipSingle)
#else
#define MSKIP(MSG) QSKIP(MSG)
#endif

class TestTemplate: public QObject
{
    Q_OBJECT

  private slots:
    // called at the beginning of these sets of tests
    void initTestCase(void)
    {
    }

    // called at the end of these sets of tests
    void cleanupTestCase(void)
    {
    }

    // called before each test case
    void init(void)
    {
    }

    // called after each test case
    void cleanup(void)
    {
    }

    // example passing test
    void example_passing_unit_test(void)
    {
        QVERIFY(true);
    }

    // example benchmark test
    void example_benchmark_test(void)
    {
        QBENCHMARK
        {
            int sum = 0;
            for (int i = 0; i < 999; i++)
                sum += i;
        }
    }

    // example skipped test
    void example_skipped_test(void)
    {
        MSKIP("this test should pass, but doesn't yet");
        QVERIFY(true); // yes this really would pass, but this is an example
    }
};
