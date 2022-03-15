/*
 *  Class TestMythDate
 *
 *  Copyright (c) David Hampton 2022
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
#include "test_mythdate.h"

void TestMythDate::formatting_data(void)
{
    QTest::addColumn<std::chrono::milliseconds>("msecs");
    QTest::addColumn<QString>("fmt");
    QTest::addColumn<QString>("expected");

    QTest::newRow(    "m:ss")       <<     192876ms <<     "m:ss" <<      "3:12";
    QTest::newRow(   "mm:ss")       <<     192876ms <<    "mm:ss" <<     "03:12";
    QTest::newRow(   "mm:ss oflow") <<   17112876ms <<    "mm:ss" <<    "285:12";
    QTest::newRow(" H:mm:ss")       <<   17112876ms <<  "H:mm:ss" <<   "4:45:12";
    QTest::newRow("HH:mm:ss")       <<   17112876ms << "HH:mm:ss" <<  "04:45:12";
    QTest::newRow(" H:mm:ss oflow") << 1169112876ms <<  "H:mm:ss" << "324:45:12";
    QTest::newRow("HH:mm:ss oflow") << 1169112876ms << "HH:mm:ss" << "324:45:12";

    QTest::newRow("HH:mm:ss.")      << 17112876ms << "HH:mm:ss."    << "04:45:12.";
    QTest::newRow("HH:mm:ss.z")     << 17112876ms << "HH:mm:ss.z"   << "04:45:12.8";
    QTest::newRow("HH:mm:ss.zz")    << 17112876ms << "HH:mm:ss.zz"  << "04:45:12.87";
    QTest::newRow("HH:mm:ss.zzz")   << 17112876ms << "HH:mm:ss.zzz" << "04:45:12.876";
    QTest::newRow("HH:mm:ss.000")   << 17112000ms << "HH:mm:ss.zzz" << "04:45:12.000";
    QTest::newRow("HH:mm:ss.05")    << 17112050ms << "HH:mm:ss.zz"  << "04:45:12.05";

    // Precison maxes out at 3. This is a millisecond function.
    QTest::newRow("HH:mm:ss.zzzzzz")<< 17112876ms   << "HH:mm:ss.zzzzzz"  << "04:45:12.876";
    QTest::newRow("silly overflow") << 1169112876ms << "HHHHHHHH:mm:ss.z" << "00000324:45:12.8";
    QTest::newRow("random chars")   << 1169112876ms << "!HH@mm#ss%zzzzzz" << "!324@45#12%876";
}

void TestMythDate::formatting(void)
{
    QFETCH(std::chrono::milliseconds, msecs);
    QFETCH(QString, fmt);
    QFETCH(QString, expected);

    QString actual = MythDate::formatTime(std::chrono::milliseconds(msecs), fmt);
    QCOMPARE(actual, expected);
}

QTEST_APPLESS_MAIN(TestMythDate)
