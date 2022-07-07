/*
 *  Class TestUnzip
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
#include "unziputil.h"
#include "test_unzip.h"
#include <QTemporaryDir>

QTemporaryDir *gTmpDir {nullptr};

void TestUnzip::initTestCase()
{
    gTmpDir = new QTemporaryDir();
    QVERIFY(gTmpDir != nullptr);
    gTmpDir->setAutoRemove(true);
}

// After each test case
void TestUnzip::cleanup()
{
    QDir dir { gTmpDir->path() };
    dir.removeRecursively();
    dir.mkpath(dir.absolutePath());
}

// After all test cases
void TestUnzip::cleanupTestCase()
{
    delete gTmpDir;
    gTmpDir = nullptr;
}

void TestUnzip::test_text_file(void)
{
    QString filename { QStringLiteral(TEST_SOURCE_DIR) +
        "/zipfiles/ipsum_lorem.zip" };
    bool result = extractZIP(filename, gTmpDir->path());
    QCOMPARE(result, true);

    auto fi = QFileInfo(gTmpDir->path() + "/ipsum_lorem_p1.txt");
    QCOMPARE(fi.exists(), true);
    QCOMPARE(fi.size(), 755);
    auto actualDateTime = QDateTime(QDate(2021,6,24),QTime(9,55,16));
    QCOMPARE(fi.lastModified(), actualDateTime);

    auto orig = QFile(QStringLiteral(TEST_SOURCE_DIR) +
                      "/data/ipsum_lorem_p1.txt");
    orig.open(QIODevice::ReadOnly);
    auto origData = orig.readAll();

    auto unzipped = QFile(gTmpDir->path() + "/ipsum_lorem_p1.txt");
    unzipped.open(QIODevice::ReadOnly);
    auto unzippedData = unzipped.readAll();
    QCOMPARE(origData, unzippedData);
}

void TestUnzip::test_theme_file(void)
{
    QString filename { QStringLiteral(TEST_SOURCE_DIR) +
        "/zipfiles/themes.zip" };
    bool result = extractZIP(filename, gTmpDir->path());
    QCOMPARE(result, true);

    auto fi = QFileInfo(gTmpDir->path() + "/trunk/Willi/themeinfo.xml");
    QCOMPARE(fi.exists(), true);
    QCOMPARE(fi.size(), 1461);
    auto actualDateTime = QDateTime(QDate(2013,7,14),QTime(16,00,56));
    QCOMPARE(fi.lastModified(), actualDateTime);

    auto orig = QFile(QStringLiteral(TEST_SOURCE_DIR) +
                      "/data/willi_themeinfo.xml");
    orig.open(QIODevice::ReadOnly);
    auto origData = orig.readAll();

    auto unzipped = QFile(gTmpDir->path() + "/trunk/Willi/themeinfo.xml");
    unzipped.open(QIODevice::ReadOnly);
    auto unzippedData = unzipped.readAll();
    QCOMPARE(origData, unzippedData);
}

QTEST_APPLESS_MAIN(TestUnzip)
