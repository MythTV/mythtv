/*
 *  Class TestMythIOWrapper
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
#include <unistd.h>
#include "libmythtv/io/mythiowrapper.h"
#include "test_mythiowrapper.h"

void TestMythIOWrapper::initTestCase(void)
{
}

void TestMythIOWrapper::local_directory_test(void)
{
    QSet<QString> known = QSet<QString> { "foo", "baz", "bar" };

    QString dirname = QString(TEST_SOURCE_DIR) + "/testfiles";
    int dirid = MythDirOpen(qPrintable(dirname));
    QVERIFY2(dirid != 0, "MythDirOpen failed");

    char *name = nullptr;
    QSet<QString> found;
    while ((name = MythDirRead(dirid)) != nullptr) {
        if (name[0] != '.')
            found += name;
        free(name);
    }
    QVERIFY(known == found);

    int res =  MythDirClose(dirid);
    QVERIFY2(res == 0, "MythDirClose failed");
}

void TestMythIOWrapper::cleanupTestCase(void)
{
}

QTEST_APPLESS_MAIN(TestMythIOWrapper)
