/*
 *  Class TestLcdDevice
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
#include "test_lcddevice.h"

void TestLcdDevice::test_singleton (void)
{
    LCD *lcd1 = LCD::Get();
    QVERIFY(lcd1 == nullptr);

    LCD::m_enabled = true;
    LCD *lcd2 = LCD::Get();
    LCD *lcd3 = LCD::Get();
    QVERIFY(lcd2 != nullptr);
    QVERIFY(lcd3 != nullptr);
    // Make clang-tidy NullDereference check happy with this next test.
    if ((lcd2 == nullptr) || (lcd3 == nullptr))
        return;
    QVERIFY(lcd2->m_retryTimer->parent() == lcd3->m_ledTimer->parent());
}

void TestLcdDevice::test_quotedString_data (void)
{
    QTest::addColumn<QString>("string");
    QTest::addColumn<QString>("result");

    QTest::newRow("empty")  << R"()"
                            << R"("")";
    QTest::newRow("single") << R"(")"
                            << R"("""")";
    QTest::newRow("none")   << R"(Test)"
                            << R"("Test")";
    QTest::newRow("middle") << R"(A "test" case.)"
                            << R"("A ""test"" case.")";
    QTest::newRow("start")  << R"("Testing"... one... two)"
                            << R"("""Testing""... one... two")";
}

void TestLcdDevice::test_quotedString (void)
{
    QFETCH(QString, string);
    QFETCH(QString, result);

    QCOMPARE(LCD::quotedString(string), result);
}

QTEST_APPLESS_MAIN(TestLcdDevice)
