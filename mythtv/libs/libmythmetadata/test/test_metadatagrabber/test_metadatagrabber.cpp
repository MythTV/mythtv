/*
 *  Class TestMetadataGrabber
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

#include "test_metadatagrabber.h"

void TestMetadataGrabber::initTestCase()
{
    gCoreContext = new MythCoreContext("test_mythmetadatagrabber_1.0", nullptr);

    if (QDir::currentPath().endsWith("test_metadatagrabber"))
        QDir::setCurrent("../../..");
    QDir::setCurrent("../programs/scripts");
}

void TestMetadataGrabber::dump(void)
{
}

void TestMetadataGrabber::test_inetref(void)
{
    QCOMPARE(MetaGrabberScript::CleanedInetref("A.B_LMNOP"),   QString("LMNOP"));
    QCOMPARE(MetaGrabberScript::CleanedInetref("a.BC_LMNOP"),  QString("LMNOP"));
    QCOMPARE(MetaGrabberScript::CleanedInetref("0.Bcd_lmnop"), QString("lmnop"));
    QCOMPARE(MetaGrabberScript::CleanedInetref("_.B01:lmnop"), QString("lmnop"));
    QCOMPARE(MetaGrabberScript::CleanedInetref("-.B_LMNOP"),   QString("LMNOP"));
    QCOMPARE(MetaGrabberScript::CleanedInetref("..BCD:LMNOP"), QString("LMNOP"));
    // Negative test
    QCOMPARE(MetaGrabberScript::CleanedInetref("A.BCDE_WXYZ"), QString("A.BCDE_WXYZ"));
}

void TestMetadataGrabber::test_fromInetref(void)
{
#if 0
    MetaGrabberScript script;
    script = MetaGrabberScript::FromInetref("tvmaze.py_1234", false);

    QCOMPARE(script.GetCommand(), QString("tvmaze.py"));
#endif
}

void TestMetadataGrabber::cleanupTestCase()
{
}

QTEST_APPLESS_MAIN(TestMetadataGrabber)
