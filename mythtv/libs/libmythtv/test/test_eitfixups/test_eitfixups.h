/*
 *  Class TestEITFixups
 *
 *  Copyright (C) Richard Hulme 2015
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

#include <QtTest/QtTest>

#include <eithelper.h> /* for FixupValue */
#include <programdata.h>

class TestEITFixups : public QObject
{
    Q_OBJECT

  private slots:
    static void testUKFixups1(void);
    static void testUKFixups2(void);
    static void testUKFixups3(void);
    static void testUKFixups4(void);
    static void testUKFixups5(void);
    static void testUKFixups6(void);
    static void testUKFixups7(void);
    static void testUKFixups8(void);
    static void testUKFixups9(void);
    static void testUKLawAndOrder(void);
    static void testUKMarvel(void);
    static void testUKXFiles(void);
    static void testDEPro7Sat1(void);
    static void testHTMLFixup(void);
    static void testSkyEpisodes(void);
    static void testUnitymedia(void);
    static void testDeDisneyChannel(void);
    static void testATV(void);
    static void test64BitEnum(void);

  private:
    static DBEventEIT *SimpleDBEventEIT (FixupValue fix, const QString& title, const QString& subtitle, const QString& description);
};
