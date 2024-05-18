/*
 *  Class TestDbCon
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

#include "test_mythdbcon.h"

#include "libmythbase/mythcorecontext.h"

void TestDbCon::initTestCase()
{
    gCoreContext = new MythCoreContext("test_mythdbcon_1.0", nullptr);
}

void TestDbCon::test_escapeAsQuery_data(void)
{
    QTest::addColumn<QString>("query");
    QTest::addColumn<QStringList>("values");
    QTest::addColumn<QString>("e_result");

    QTest::newRow("one")      << "INSERT INTO test (name) VALUES (:Name_1);"
                              << QStringList({":Name_1", "Fred Flinstone"})
                              << "INSERT INTO test (name) VALUES ('Fred Flinstone');";
    QTest::newRow("two")      << "INSERT INTO test (name1, name2) VALUES (:Name_1, :2_NaMe);"
                              << QStringList({":Name_1", "Fred Flinstone", ":2_NaMe", "Barney"})
                              << "INSERT INTO test (name1, name2) VALUES ('Fred Flinstone', 'Barney');";
    QTest::newRow("quotes")   << "INSERT INTO test (name) VALUES ('name');"
                              << QStringList({"'name'", "Fred Flinstone"})
                              << "INSERT INTO test (name) VALUES ('Fred Flinstone');";
    QTest::newRow("multi")    << "INSERT INTO test SET name=:name WHERE value=:value;"
                              << QStringList({":name", "Fred Flinstone", ":value", "Barney Rubble"})
                              << "INSERT INTO test SET name='Fred Flinstone' WHERE value='Barney Rubble';";

    QTest::newRow("unicode1") << "INSERT INTO test (name) VALUES (:bañana);"
                              << QStringList({":bañana", "Curious George"})
                              << "INSERT INTO test (name) VALUES ('Curious George');";
    QTest::newRow("unicode2") << "INSERT INTO test (name) VALUES ('bañana');"
                              << QStringList({"'bañana'", "Curious George"})
                              << "INSERT INTO test (name) VALUES ('Curious George');";
    
    // Negative test cases. Must have at least one character in binding name
    QTest::newRow("failme1")  << "INSERT INTO test (name) VALUES (:);"
                              << QStringList({":", "Fred Flinstone"})
                              << "INSERT INTO test (name) VALUES (:);";
    QTest::newRow("failme2")  << "INSERT INTO test (name) VALUES ('');"
                              << QStringList({"''", "Fred Flinstone"})
                              << "INSERT INTO test (name) VALUES ('');";
}

void TestDbCon::test_escapeAsQuery(void)
{
    QFETCH(QString, query);
    QFETCH(QStringList, values);
    QFETCH(QString, e_result);

    QVERIFY(values.size() % 2 == 0);
    MSqlBindings bindings {};
    for (int i = 0; i < values.size(); i += 2)
        bindings[values[i]] = QVariant(values[i+1]);

    MSqlEscapeAsAQuery(query, bindings);

    QCOMPARE(query, e_result);
}

void TestDbCon::cleanupTestCase()
{
}

QTEST_GUILESS_MAIN(TestDbCon)
