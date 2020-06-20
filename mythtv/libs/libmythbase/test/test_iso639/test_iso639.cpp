/*
 *  Class TestISO639
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
#include "test_iso639.h"

void TestISO639::test_str2name_data(void)
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("expectedOutput");

    QTest::newRow("en")  << "en"  << "English";
    QTest::newRow("eng") << "eng" << "English";
    QTest::newRow("zh")  << "zh"  << "Chinese";
    QTest::newRow("zho") << "zho" << "Chinese";
    QTest::newRow("chi") << "chi" << "Chinese";
}

void TestISO639::test_str2name(void)
{
    QFETCH(QString, input);
    QFETCH(QString, expectedOutput);

    QString output = iso639_str_toName(reinterpret_cast<const uchar *>(qPrintable(input)));
//  std::cerr << "Expected: " << qPrintable(expectedOutput) << std::endl;
//  std::cerr << "Actual:   " << qPrintable(output) << std::endl;
    QCOMPARE(output, expectedOutput);
}

void TestISO639::test_key2str3_data(void)
{
    QTest::addColumn<int>("input");
    QTest::addColumn<QString>("expectedOutput");

    QTest::newRow("eng") << 0x656E67  << "eng";
    QTest::newRow("zho") << 0X7A686F  << "zho";
    QTest::newRow("cho") << 0x636869  << "chi";
}

void TestISO639::test_key2str3(void)
{
    QFETCH(int, input);
    QFETCH(QString, expectedOutput);

    QString output = iso639_key_to_str3(input);
//  std::cerr << "Expected: " << qPrintable(expectedOutput) << std::endl;
//  std::cerr << "Actual:   " << qPrintable(output) << std::endl;
    QCOMPARE(output, expectedOutput);
}

void TestISO639::test_str3_to_key_data(void)
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<int>("expectedOutput");
    QTest::addColumn<bool>("expectedDefined");

    QTest::newRow("aaa") << "aaa" << 0x616161 << true;
    QTest::newRow("eng") << "eng" << 0x656E67 << true;
    QTest::newRow("zho") << "zho" << 0X7A686F << true;
    QTest::newRow("cho") << "chi" << 0x636869 << true;
    QTest::newRow("und") << "und" << 0x756E64 << false;
}

void TestISO639::test_str3_to_key(void)
{
    QFETCH(QString, input);
    QFETCH(int, expectedOutput);
    QFETCH(bool, expectedDefined);

    int actualOutput = iso639_str3_to_key(input);
//  std::cerr << "Expected: " << qPrintable(expectedOutput) << std::endl;
//  std::cerr << "Actual:   " << qPrintable(output) << std::endl;
    QCOMPARE(actualOutput, expectedOutput);
    bool actualDefined = !iso639_is_key_undefined(actualOutput);
    QCOMPARE(actualDefined, expectedDefined);
}

void TestISO639::test_key2name_data(void)
{
    QTest::addColumn<int>("input");
    QTest::addColumn<QString>("expectedOutput");

    QTest::newRow("eng") << 0x656E67  << "English";
    QTest::newRow("zho") << 0X7A686F  << "Unknown";
    QTest::newRow("cho") << 0x636869  << "Chinese";
}

void TestISO639::test_key2name(void)
{
    QFETCH(int, input);
    QFETCH(QString, expectedOutput);

    QString output = iso639_key_toName(input);
//  std::cerr << "Expected: " << qPrintable(expectedOutput) << std::endl;
//  std::cerr << "Actual:   " << qPrintable(output) << std::endl;
    QCOMPARE(output, expectedOutput);
}

void TestISO639::test_str2_to_str3_data(void)
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("expectedOutput");

    QTest::newRow("eng") << "en" << "eng";
    QTest::newRow("chi") << "zh" << "chi";
    QTest::newRow("xyz") << "xy" << "und";
}

void TestISO639::test_str2_to_str3(void)
{
    QFETCH(QString, input);
    QFETCH(QString, expectedOutput);

    QString output = iso639_str2_to_str3(input);
//  std::cerr << "Expected: " << qPrintable(expectedOutput) << std::endl;
//  std::cerr << "Actual:   " << qPrintable(output) << std::endl;
    QCOMPARE(output, expectedOutput);
}

void TestISO639::test_key_to_cankey_data(void)
{
    QTest::addColumn<int>("input");
    QTest::addColumn<int>("expectedOutput");

    QTest::newRow("zho")
        << (('z' << 16) | ('h' << 8) | 'o')
        << (('c' << 16) | ('h' << 8) | 'i');
    QTest::newRow("xyz")
        << (('x' << 16) | ('y' << 8) | 'z')
        << (('x' << 16) | ('y' << 8) | 'z');
}

void TestISO639::test_key_to_cankey(void)
{
    QFETCH(int, input);
    QFETCH(int, expectedOutput);

    int output = iso639_key_to_canonical_key(input);
//  std::cerr << "Expected: " << qPrintable(expectedOutput) << std::endl;
//  std::cerr << "Actual:   " << qPrintable(output) << std::endl;
    QCOMPARE(output, expectedOutput);
}


QTEST_APPLESS_MAIN(TestISO639)
