/*
 *  Class TestSortHelper
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
#include "test_mythsorthelper.h"

void TestSortHelper::Singleton(void)
{
    auto sh1 = getMythSortHelper();
    QVERIFY(sh1.use_count() == 2);
    auto sh2 = getMythSortHelper();
    QVERIFY(sh2.use_count() == 3);
    auto sh3 = getMythSortHelper();
    QVERIFY(sh3.use_count() == 4);
    sh2 = nullptr;
    QVERIFY(sh3.use_count() == 3);
    sh1 = nullptr;
    QVERIFY(sh3.use_count() == 2);
}

void TestSortHelper::titles_timing_data(void)
{
    QTest::addColumn<int>("sensitive");
    QTest::addColumn<int>("disposition");
    QTest::addColumn<QString>("exclusions");

    QTest::newRow("Case Sensitive, Keep Prefix, No Exclsions")   << 0 << 0 << "";
    QTest::newRow("Case Sensitive, Remove Prefix, No Exclsions") << 0 << 1 << "";
    QTest::newRow("Case Sensitive, Prefix to end, No Exclsions") << 0 << 2 << "";

    QTest::newRow("Case Insensitive, Keep Prefix, No Exclsions")   << 1 << 0 << "";
    QTest::newRow("Case Insensitive, Remove Prefix, No Exclsions") << 1 << 1 << "";
    QTest::newRow("Case Insensitive, Prefix to end, No Exclsions") << 1 << 2 << "";

    QTest::newRow("Case Insensitive, Remove Prefix, Exclsions") << 1 << 1 << "A to Z; The The";
}

void TestSortHelper::titles_timing(void)
{
    QFETCH(int, sensitive);
    QFETCH(int, disposition);
    QFETCH(QString, exclusions);
    auto *sh = new MythSortHelper(static_cast<Qt::CaseSensitivity>(sensitive),
                                  static_cast<SortPrefixMode>(disposition),
                                  exclusions);
    QBENCHMARK {
        static_cast<void>(sh->doTitle("The Flash"));
    }
    delete sh;
}

void TestSortHelper::pathnames_timing_data(void)
{
    titles_timing_data();
}

void TestSortHelper::pathnames_timing(void)
{
    QFETCH(int, sensitive);
    QFETCH(int, disposition);
    QFETCH(QString, exclusions);

    auto *sh = new MythSortHelper(static_cast<Qt::CaseSensitivity>(sensitive),
                                  static_cast<SortPrefixMode>(disposition),
                                  exclusions);
    QBENCHMARK {
        static_cast<void>(sh->doPathname("dvds/The Flash/Season 2/The Flash - S02E01.ts"));
    }
    delete sh;
}

void TestSortHelper::Variations_test(void)
{
    // Case sensitive, keep prefixes (aka no-op)
    auto *sh = new MythSortHelper(Qt::CaseSensitive, SortPrefixKeep, "");
    QVERIFY(sh->doTitle("The Blob") == "The Blob");
    QVERIFY(sh->doTitle("The Blob") != "the blob");
    QVERIFY(sh->doTitle("The Blob") != "Blob");
    QVERIFY(sh->doTitle("The Blob") != "blob");
    QVERIFY(sh->doTitle("The Blob") != "Blob, The");
    QVERIFY(sh->doTitle("The Blob") != "blob, the");
    QVERIFY(sh->doTitle("Any Given Sunday") == "Any Given Sunday");
    QVERIFY(sh->doPathname("/video/recordings/The Flash/Season 1/The Flash - S01E01.ts")
            == "/video/recordings/The Flash/Season 1/The Flash - S01E01.ts");
    delete sh;

    // Case insensitive, keep prefixes
    sh = new MythSortHelper(Qt::CaseInsensitive, SortPrefixKeep, "");
    QVERIFY(sh->doTitle("The Blob") != "The Blob");
    QVERIFY(sh->doTitle("The Blob") == "the blob");
    QVERIFY(sh->doTitle("The Blob") != "Blob");
    QVERIFY(sh->doTitle("The Blob") != "blob");
    QVERIFY(sh->doTitle("The Blob") != "Blob, The");
    QVERIFY(sh->doTitle("The Blob") != "blob, the");
    QVERIFY(sh->doTitle("Any Given Sunday") == "any given sunday");
    QVERIFY(sh->doPathname("/video/recordings/The Flash/Season 1/The Flash - S01E01.ts")
            == "/video/recordings/the flash/season 1/the flash - s01e01.ts");
    delete sh;

    // Case sensitive, strip prefixes
    sh = new MythSortHelper(Qt::CaseSensitive, SortPrefixRemove, "");
    QVERIFY(sh->doTitle("The Sting") != "The Sting");
    QVERIFY(sh->doTitle("The Sting") != "the sting");
    QVERIFY(sh->doTitle("The Sting") == "Sting");
    QVERIFY(sh->doTitle("The Sting") != "sting");
    QVERIFY(sh->doTitle("The Sting") != "Sting, The");
    QVERIFY(sh->doTitle("The Sting") != "sting, the");
    QVERIFY(sh->doTitle("Any Given Sunday") == "Any Given Sunday");
    QVERIFY(sh->doPathname("/video/recordings/The Flash/Season 1/The Flash - S01E01.ts")
            == "/video/recordings/Flash/Season 1/Flash - S01E01.ts");
    delete sh;

    // Case insensitive, strip prefixes
    sh = new MythSortHelper(Qt::CaseInsensitive, SortPrefixRemove, "");
    QVERIFY(sh->doTitle("The Thing") != "The Thing");
    QVERIFY(sh->doTitle("The Thing") != "the thing");
    QVERIFY(sh->doTitle("The Thing") != "Thing");
    QVERIFY(sh->doTitle("The Thing") == "thing");
    QVERIFY(sh->doTitle("The Thing") != "Thing, The");
    QVERIFY(sh->doTitle("The Thing") != "thing, the");
    QVERIFY(sh->doTitle("Any Given Sunday") == "any given sunday");
    QVERIFY(sh->doPathname("/video/recordings/The Flash/Season 1/The Flash - S01E01.ts")
            == "/video/recordings/flash/season 1/flash - s01e01.ts");
    delete sh;

    // Case insensitive, prefixes to end
    sh = new MythSortHelper(Qt::CaseInsensitive, SortPrefixToEnd, "");
    QVERIFY(sh->doTitle("The Flash") != "The Flash");
    QVERIFY(sh->doTitle("The Flash") == "flash, the");
    QVERIFY(sh->doTitle("The Flash") != "Flash");
    QVERIFY(sh->doTitle("The Flash") != "flash");
    QVERIFY(sh->doTitle("Any Given Sunday") == "any given sunday");
    QVERIFY(sh->doPathname("/video/recordings/The Flash/Season 1/The Flash - S01E01.ts")
            == "/video/recordings/flash, the/season 1/flash - s01e01.ts, the");
    delete sh;

    // Case insensitive, strip prefixes, exclusions
    sh = new MythSortHelper(Qt::CaseInsensitive, SortPrefixRemove, "A to Z;The Jetsons");
    QVERIFY(sh->doTitle("A to Z") == "a to z");
    QVERIFY(sh->doTitle("The Jetsons") == "the jetsons");
    QVERIFY(sh->doTitle("The Flinstones") == "flinstones");

    QVERIFY(sh->doPathname("/video/recordings/A to Z/Season 1/A to Z - S01E01.ts")
            == "/video/recordings/a to z/season 1/a to z - s01e01.ts");
    QVERIFY(sh->doPathname("/video/recordings/The Jetsons/Season 1/The Jetsons - S01E01.ts")
            == "/video/recordings/the jetsons/season 1/the jetsons - s01e01.ts");
    QVERIFY(sh->doPathname("/video/recordings/The Flinstones/Season 1/The Flinstones - S01E01.ts")
            == "/video/recordings/flinstones/season 1/flinstones - s01e01.ts");
    delete sh;
}

QTEST_APPLESS_MAIN(TestSortHelper)
