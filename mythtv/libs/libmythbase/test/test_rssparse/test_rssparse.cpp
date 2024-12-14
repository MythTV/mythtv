/*
 *  Class TestRSSParse
 *
 *  Copyright (c) David Hampton 2021
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
#include <string>
#include "test_rssparse.h"

Parse *gParse {nullptr};

void TestRSSParse::initTestCase()
{
    gParse = new Parse;

#if 0
    QList<QByteArray> timezones = QTimeZone::availableTimeZoneIds();

    std::cerr << "Time Zone Names:" << std::endl;
    for (const auto & name : std::as_const(timezones))
        std::cerr << "  " << name.data() << std::endl;
#endif
}

void TestRSSParse::test_rfc822_data(void)
{
    QTest::addColumn<QString>("dateString");
    QTest::addColumn<QDate>("expectedDate");
    QTest::addColumn<QTime>("expectedTime");

    QTest::newRow("EST") << "Wed, 02 Oct 2002 08:00:00 EST"
                         << QDate(2002,10,02) << QTime(13,0,0);
    QTest::newRow("GMT") << "Wed, 02 Oct 2002 13:00:00 GMT"
                         << QDate(2002,10,02) << QTime(13,0,0);
    QTest::newRow("EST") << "Wed, 02 Oct 2002 15:00:00 +0200"
                         << QDate(2002,10,02) << QTime(13,0,0);
}

void TestRSSParse::test_rfc822(void)
{
    QFETCH(QString, dateString);
    QFETCH(QDate, expectedDate);
    QFETCH(QTime, expectedTime);

    QDateTime result = Parse::RFC822TimeToQDateTime(dateString);
#if QT_VERSION < QT_VERSION_CHECK(6,5,0)
    QDateTime expectedResult(expectedDate, expectedTime, Qt::UTC);
#else
    QDateTime expectedResult(expectedDate, expectedTime,
                             QTimeZone(QTimeZone::UTC));
#endif
    QCOMPARE(result, expectedResult);
}

void TestRSSParse::test_rfc3339_data(void)
{
    QTest::addColumn<QString>("dateString");
    QTest::addColumn<QDate>("expectedDate");
    QTest::addColumn<QTime>("expectedTime");

    // Examples from RFC3339
    QTest::newRow("1985")
        << "1985-04-12T23:20:50.52Z"
        << QDate(1985,04,12) << QTime(23,20,50,520);
    QTest::newRow("1996 UTC")
        << "1996-12-20T00:39:57Z"
        << QDate(1996,12,20) << QTime(00,39,57,0);
    QTest::newRow("1996 Pacific")
        << "1996-12-19T16:39:57-08:00"
        << QDate(1996,12,20) << QTime(00,39,57,0);

    // Qt doesn't handle leap seconds
    // QTest::newRow("LeapSec UTC")     << "1990-12-31T23:59:60Z"
    //     << QDate(1990,12,31) << QTime(23,59,60,0);
    // QTest::newRow("LeapSec Pacific") << "1990-12-31T15:59:60-08:00"
    //     << QDate(1990,12,31) << QTime(23,59,60,0);

    // Other Examples
    QTest::newRow("lowercase")
        << "2021-02-17t14:45:23Z"
        << QDate(2021,02,17) << QTime(14,45,23,0);
    QTest::newRow("space")
        << "2021-02-17 14:45:23.123+04:00"
        << QDate(2021,02,17) << QTime(10,45,23,123);
}

void TestRSSParse::test_rfc3339(void)
{
    QFETCH(QString, dateString);
    QFETCH(QDate, expectedDate);
    QFETCH(QTime, expectedTime);

    QDateTime result = Parse::FromRFC3339(dateString);
#if QT_VERSION < QT_VERSION_CHECK(6,5,0)
    QDateTime expectedResult(expectedDate, expectedTime, Qt::UTC);
#else
    QDateTime expectedResult(expectedDate, expectedTime,
                             QTimeZone(QTimeZone::UTC));
#endif
    QCOMPARE(result, expectedResult);
}


void TestRSSParse::cleanupTestCase()
{
}

QTEST_APPLESS_MAIN(TestRSSParse)
