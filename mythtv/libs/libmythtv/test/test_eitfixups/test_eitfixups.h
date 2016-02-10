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

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#define MSKIP(MSG) QSKIP(MSG, SkipSingle)
#else
#define MSKIP(MSG) QSKIP(MSG)
#endif

#include <eithelper.h> /* for FixupValue */
#include <programdata.h>

class TestEITFixups : public QObject
{
    Q_OBJECT

  private slots:
    void testUKFixups1(void);
    void testUKFixups2(void);
    void testUKFixups3(void);
    void testUKFixups4(void);
    void testUKFixups5(void);
    void testUKFixups6(void);
    void testUKFixups7(void);
    void testUKFixups8(void);
    void testUKFixups9(void);
    void testUKLawAndOrder(void);
    void testUKMarvel(void);
    void testUKXFiles(void);
    void testDEPro7Sat1(void);
    void testHTMLFixup(void);
    void testSkyEpisodes(void);
    void testUnitymedia(void);

  private:
    static DBEventEIT *SimpleDBEventEIT (FixupValue fix, QString title, QString subtitle, QString description);
};
