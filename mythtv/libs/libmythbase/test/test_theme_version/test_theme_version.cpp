/*
 *  Class TestThemeVersion
 *
 *  Copyright (c) David Hampton 2025
 *
 *  See the file LICENSE_FSF for licensing information.
 */
#include <iostream>
#include "test_theme_version.h"

void TestThemeVersion::test_version_data(void)
{
    QTest::addColumn<QString>("version");
    QTest::addColumn<bool>("e_result");
    QTest::addColumn<bool>("e_devel");
    QTest::addColumn<int>("e_major");
    QTest::addColumn<int>("e_minor");

    QTest::newRow("devsimple")
        << "v29-Pre"
        << true << true << 29 << 0;
    QTest::newRow("devnormal")
        << "v29-pre-574-g92517f5"
        << true << true << 29 << 0;
    QTest::newRow("major")
        << "v29-21-ge26a33c"
        << true << false << 29 << 0;
    QTest::newRow("majmin")
        << "v29.1-21-ge26a33c"
        << true << false << 29 << 1;
    QTest::newRow("v34")
        << "v34.0-32-gd309161e2b-dirty"
        << true << false << 34 << 0;
    QTest::newRow("v1234")
        << "v1234.0-32-gd309161e2b-dirty"
        << true << false << 1234 << 0;
    QTest::newRow("point")
        << "v34.0.2-32-gd309161e2b-dirty"
        << true << false << 34 << 0;
    QTest::newRow("point-pre")
        << "v34.0.2-pre-32-gd309161e2b-dirty"
        << true << true << 34 << 0;

    QTest::newRow("no_v")
        << "34.0-32-gd309161e2b-dirty"
        << false << false << 0 << 0;
}

void TestThemeVersion::test_version(void)
{
    QFETCH(QString, version);
    QFETCH(bool, e_result);
    QFETCH(bool, e_devel);
    QFETCH(int,  e_major);
    QFETCH(int,  e_minor);

    uint a_major { 0 };
    uint a_minor { 0 };
    bool a_devel { false };
    bool result = ParseMythSourceVersion(a_devel, a_major, a_minor,
                                         qPrintable(version));

    QCOMPARE(result, e_result);
    QCOMPARE(a_devel, e_devel);
    QCOMPARE(a_major, static_cast<uint>(e_major));
    QCOMPARE(a_minor, static_cast<uint>(e_minor));
}

QTEST_APPLESS_MAIN(TestThemeVersion)
