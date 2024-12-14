/*
 *  Class TestMythBinaryPList
 *
 *  Copyright (C) David Hampton 2021
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
#include "mythbinaryplist.h"
#include "test_mythbinaryplist.h"

static QDateTime test_datetime;

// called at the beginning of these sets of tests
void TestMythBinaryPList::initTestCase(void)
{
    auto date = QDate(2021, 4, 21);
    auto time = QTime(13, 26, 03);
#if QT_VERSION < QT_VERSION_CHECK(6,5,0)
    test_datetime = QDateTime(date, time, Qt::UTC);
#else
    test_datetime = QDateTime(date, time, QTimeZone(QTimeZone::UTC));
#endif
}

// called at the end of these sets of tests
void TestMythBinaryPList::cleanupTestCase(void)
{
}

// called before each test case
void TestMythBinaryPList::init(void)
{
}

// called after each test case
void TestMythBinaryPList::cleanup(void)
{
}

void TestMythBinaryPList::plist_read(void)
{
    // Read the file
    QFile file(QStringLiteral(TEST_SOURCE_DIR) + "/info_mac_addl.bplist");
    QCOMPARE(file.exists(), true);
    QCOMPARE(file.open(QIODevice::ReadOnly), true);
    QByteArray file_data = file.readAll();
    //std::cerr << "file_data: " << file_data.constData() << std::endl;
    file.close();

    // Parse it
    MythBinaryPList plist(file_data);
    //std::cerr << "plist: " << qPrintable(plist.ToString()) << std::endl;

    // Check values
    QVariant variant = plist.GetValue("CFBundleIconFile");
    QVERIFY(variant.isValid());
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    QCOMPARE(variant.type(), QVariant::String);
#else
    QCOMPARE(variant.typeId(), QMetaType::QString);
#endif
    auto icon_name = variant.value<QString>();
    QCOMPARE(icon_name, QString("@ICON@"));

    variant = plist.GetValue("CFBundleDocumentTypes");
    QVERIFY(variant.isValid());
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    QCOMPARE(variant.type(), QVariant::List);
#else
    QCOMPARE(variant.typeId(), QMetaType::QVariantList);
#endif
    auto list = variant.value<QVariantList>();
    QCOMPARE(list.size(), 1);

    variant = list[0];
    QVERIFY(variant.isValid());
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    QCOMPARE(variant.type(), QVariant::Map);
#else
    QCOMPARE(variant.typeId(), QMetaType::QVariantMap);
#endif
    auto map = variant.value<QVariantMap>();
    QCOMPARE(map.size(), 5);

    QVERIFY(map.contains("CFBundleTypeExtensions"));
    QVariant variant2 = map["CFBundleTypeExtensions"];
    QVERIFY(variant2.isValid());
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    QCOMPARE(variant2.type(), QVariant::List);
#else
    QCOMPARE(variant2.typeId(), QMetaType::QVariantList);
#endif
    auto list2 = variant2.value<QVariantList>();
    QCOMPARE(list2.size(), 5);
    const QVariant& variant3 = list2[4];
    QVERIFY(variant3.isValid());
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    QCOMPARE(variant3.type(), QVariant::String);
#else
    QCOMPARE(variant3.typeId(), QMetaType::QString);
#endif
    auto ext_name = variant3.value<QString>();
    QCOMPARE(ext_name, QString("xhtml"));
    
    QVERIFY(map.contains("CFBundleTypeRole"));
    auto role_name = map["CFBundleTypeRole"].value<QString>();
    QCOMPARE(role_name, QString("Viewer"));

    // Test float twice. Catch conversion in place.
    variant = plist.GetValue("TestFloat");
    QVERIFY(variant.isValid());
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    QCOMPARE(variant.type(), QVariant::Double);
#else
    QCOMPARE(variant.typeId(), QMetaType::Double);
#endif
    auto pi = variant.value<double>();
    QCOMPARE(pi, 3.1415926545897932);
    variant = plist.GetValue("TestFloat");
    QVERIFY(variant.isValid());
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    QCOMPARE(variant.type(), QVariant::Double);
#else
    QCOMPARE(variant.typeId(), QMetaType::Double);
#endif
    pi = variant.value<double>();
    QCOMPARE(pi, 3.1415926545897932);
    variant = plist.GetValue("TestFloat2");
    QVERIFY(variant.isValid());
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    QCOMPARE(variant.type(), QVariant::Double);
#else
    QCOMPARE(variant.typeId(), QMetaType::Double);
#endif
    pi = variant.value<double>();
    QCOMPARE(pi, 3.1415926545897932);

    // Check dates
    variant = plist.GetValue("TestDate");
    QVERIFY(variant.isValid());
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    QCOMPARE(variant.type(), QVariant::DateTime);
#else
    QCOMPARE(variant.typeId(), QMetaType::QDateTime);
#endif
    auto when = variant.value<QDateTime>();
    QCOMPARE(when, test_datetime);
    variant = plist.GetValue("TestDate");
    QVERIFY(variant.isValid());
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    QCOMPARE(variant.type(), QVariant::DateTime);
#else
    QCOMPARE(variant.typeId(), QMetaType::QDateTime);
#endif
    when = variant.value<QDateTime>();
    QCOMPARE(when, test_datetime);
    variant = plist.GetValue("TestDate2");
    QVERIFY(variant.isValid());
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    QCOMPARE(variant.type(), QVariant::DateTime);
#else
    QCOMPARE(variant.typeId(), QMetaType::QDateTime);
#endif
    when = variant.value<QDateTime>();
    QCOMPARE(when, test_datetime);
}


QTEST_APPLESS_MAIN(TestMythBinaryPList)
