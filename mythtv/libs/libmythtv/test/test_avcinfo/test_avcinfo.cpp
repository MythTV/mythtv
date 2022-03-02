/*
 *  Class TestAvcInfo
 *
 *  Copyright (C) David Hampton 2018
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

#include <cstdio>
#include <iostream>
#include <random>
#include "test_avcinfo.h"
#include "libmythtv/recorders/avcinfo.h"

void TestAvcInfo::initTestCase(void)
{
}

void TestAvcInfo::test_guids(void)
{
    std::mt19937_64 gen (std::random_device{}());

    // Test a couple of known values.
    QString guid_string = guid_to_string(0x0249E6CB92FCC5C9LL);
    QVERIFY2(guid_string == "0249E6CB92FCC5C9", "Failed guid_to_string");
    uint64_t translated = string_to_guid(guid_string);
    QVERIFY2(translated == 0x0249E6CB92FCC5C9, "Failed string_to_guid");

    guid_string = guid_to_string(0x45E5A44466F07DD0);
    QVERIFY2(guid_string == "45E5A44466F07DD0", "Failed guid_to_string");
    translated = string_to_guid(guid_string);
    QVERIFY2(translated == 0x45E5A44466F07DD0, "Failed string_to_guid");

    guid_string = guid_to_string(0xF596480E2C60E9DC);
    QVERIFY2(guid_string == "F596480E2C60E9DC", "Failed guid_to_string");
    translated = string_to_guid(guid_string);
    QVERIFY2(translated == 0xF596480E2C60E9DC, "Failed string_to_guid");

    // Test some random values.
    for (int i = 0; i < 100; i++)
    {
        uint64_t guid = gen();
        guid_string = guid_to_string(guid);
        translated = string_to_guid(guid_string);
        QVERIFY(guid == translated);
    }
}

void TestAvcInfo::cleanupTestCase(void)
{
}

QTEST_APPLESS_MAIN(TestAvcInfo)
