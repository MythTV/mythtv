/*
 *  Class TestMhegDsmcc
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
#include <iostream>
#include "test_mheg_dsmcc.h"

Dsmcc *g_dsmcc = nullptr;

ObjCarousel *g_obj1a = nullptr;
ObjCarousel *g_obj1b = nullptr;
ObjCarousel *g_obj1c = nullptr;
ObjCarousel *g_obj2a = nullptr;

static const uint16_t Car1 = 0x4242;
static const uint16_t Car2 = 0x4343;
static const uint16_t Car3 = 0x4444;
static const uint16_t Tag1a = 0x4242;
static const uint16_t Tag1b = 0x4200;
static const uint16_t Tag1c = 0x0042;
static const uint16_t Tag2a = 0x0001;
static const uint16_t Tag3a = 0x9876;

void TestMhegDsmcc::initTestCase()
{
    g_dsmcc = new Dsmcc;
    g_obj1a = g_dsmcc->AddTap(Tag1a, Car1);
    g_obj1b = g_dsmcc->AddTap(Tag1b, Car1);
    g_obj1c = g_dsmcc->AddTap(Tag1c, Car1);
    g_obj2a = g_dsmcc->AddTap(Tag2a, Car2);
}

void TestMhegDsmcc::test_objects(void)
{
    QCOMPARE(g_obj1a, g_obj1b);
    QCOMPARE(g_obj1a, g_obj1c);
    QVERIFY(g_obj1a != g_obj2a);
}

void TestMhegDsmcc::test_carousel_data(void)
{
    QTest::addColumn<uint16_t>("car");
    QTest::addColumn<uint16_t>("tag");
    QTest::addColumn<qulonglong>("expected");

    QTest::newRow("1a") << Car1 << Tag1a << (qulonglong)g_obj1a;
    QTest::newRow("1b") << Car1 << Tag1b << (qulonglong)g_obj1a;
    QTest::newRow("1c") << Car1 << Tag1c << (qulonglong)g_obj1a;
    QTest::newRow("1x") << Car1 << Tag2a << (qulonglong)g_obj1a;

    QTest::newRow("2a") << Car2 << Tag2a << (qulonglong)g_obj2a;
    QTest::newRow("2x") << Car2 << Tag3a << (qulonglong)g_obj2a;

    QTest::newRow("3a") << Car3 << Tag3a << (qulonglong)0;
    QTest::newRow("3x") << Car3 << Tag2a << (qulonglong)0;
}

void TestMhegDsmcc::test_carousel(void)
{
    QFETCH(uint16_t, car);
    QFETCH(uint16_t, tag); Q_UNUSED(tag);
    QFETCH(qulonglong, expected);

    auto *actual = g_dsmcc->GetCarouselById(car);
//  std::cerr << "Expected: " << qPrintable(QString("%1").arg(expected)) << std::endl;
//  std::cerr << "Actual:   " << qPrintable(QString("%1").arg((qulonglong)actual))   << std::endl;
    QCOMPARE(expected, (qulonglong)actual);
}

void TestMhegDsmcc::cleanupTestCase()
{
    delete g_dsmcc;
}

QTEST_APPLESS_MAIN(TestMhegDsmcc)
