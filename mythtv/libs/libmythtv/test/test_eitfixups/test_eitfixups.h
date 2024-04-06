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

#include <QTest>

#include "libmythtv/eithelper.h" /* for FixupValue */
#include "libmythtv/programdata.h"

class TestEITFixups : public QObject
{
    Q_OBJECT

    static void printEvent(const DBEventEIT& event);
    static QString getSubtitleType(unsigned char type);
    static QString getAudioProps(unsigned char props);
    static QString getVideoProps(unsigned char props);
    static void checkCast(const DBEventEIT& event,
                          const QStringList& e_actors,
                          const QStringList& e_directors = QStringList(),
                          const QStringList& e_hosts = QStringList(),
                          const QStringList& e_presenters = QStringList(),
                          const QStringList& e_commentators = QStringList(),
                          const QStringList& e_producers = QStringList(),
                          const QStringList& e_writers = QStringList());
    static void checkRating(const DBEventEIT& event,
                            const QString& system, const QString& rating);

  private slots:
    static void initTestCase();
    static void testParseRoman(void);
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
    static void testRTLEpisodes(void);
    static void testUnitymedia(void);
    static void testDeDisneyChannel(void);
    static void testATV(void);
    static void test64BitEnum(void);
    static void testDvbEitAuthority_data();
    static void testDvbEitAuthority();
    static void testGenericTitle_data(void);
    static void testGenericTitle(void);
    static void testUKTitlePropsFixups();
    static void testUKTitleDescriptionFixups_data(void);
    static void testUKTitleDescriptionFixups(void);
    static void testUKSubtitleFixups_data(void);
    static void testUKSubtitleFixups(void);
    static void testUKSeriesFixups_data(void);
    static void testUKSeriesFixups(void);
    static void testUKPartFixups_data();
    static void testUKPartFixups();
    static void testUKStarringFixups_data();
    static void testUKStarringFixups();
    static void testBellExpress_data();
    static void testBellExpress();
    static void testBellExpressActors();
    static void testPBS();
    static void testComHem_data();
    static void testComHem();
    static void testComHem2_data();
    static void testComHem2();
    static void testComHem3_data();
    static void testComHem3();
    static void testAUStar_data();
    static void testAUStar();
    static void testAUDescription_data();
    static void testAUDescription();
    static void testAUNine_data();
    static void testAUNine();
    static void testAUSeven_data();
    static void testAUSeven();
    static void testAUFreeview_data();
    static void testAUFreeview();
    static void testMCA_data();
    static void testMCA();
    static void testMCA2_data();
    static void testMCA2();
    static void testRTL_data();
    static void testRTL();
    static void testFI_data();
    static void testFI();
    static void testNL_data();
    static void testNL();
    static void testCategory_data();
    static void testCategory();
    static void testNO_data();
    static void testNO();
    static void testNRK_data();
    static void testNRK();
    static void testDK_data();
    static void testDK();
    static void testDK2_data();
    static void testDK2();
    static void testGreekSubtitle_data();
    static void testGreekSubtitle();
    static void testGreek_data();
    static void testGreek();
    static void testGreek2_data();
    static void testGreek2();
    static void testGreek3();
    static void testGreekCategories_data();
    static void testGreekCategories();
    static void cleanupTestCase();

  private:
    static DBEventEIT *SimpleDBEventEIT (FixupValue fix, const QString& title, const QString& subtitle, const QString& description);
};
