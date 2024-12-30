/*
 *  Class TestRecordingExtender
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
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>

#include "libmythbase/mythconfig.h"
#if CONFIG_SQLITE3
#include <QSqlDriver>
#include <sqlite3.h>
#endif

#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdb.h"
#include "libmythtv/dbcheck.h"
#include "dummyscheduler.h"
#include "test_recordingextender.h"

static constexpr char const * const TESTNAME = "test_recordingextender";
static constexpr char const * const TESTVERSION = "test_recordingextender_1.0";
bool gSqliteRegexpEnabled = false;
bool gForceLocalUrl {false};

static std::size_t replace_all(std::string& inout, std::string_view what, std::string_view with)
{
    std::size_t count{};
    for (std::string::size_type pos{};
         std::string::npos != (pos = inout.find(what, pos));
         pos += with.length(), ++count) {
        inout.replace(pos, what.length(), with);
    }
    return count;
}

static void convertToSqlite (DBUpdates& updates)
{
    for (std::string& s : updates)
    {
      replace_all(s, R"(\\)", R"(\)");
      replace_all(s, "ENGINE=MyISAM DEFAULT CHARSET=utf8", "");
      replace_all(s, "UNIQUE(provider,key1(25),key2(50))",
                     "UNIQUE(provider,key1,key2)");
    }
}

static bool enableSqliteRegex (void)
{
#if CONFIG_SQLITE3
    MSqlQueryInfo info = MSqlQuery::InitCon();

    // Is support already enabled?
    MSqlQuery query(info);
    query.prepare("SELECT 'MythTV!' REGEXP '.*';");
    if (query.exec())
        return true;

    // Gaah. Have to manually enable extension loading. If it exists.
    QVariant v = info.qsqldb.driver()->handle();
    if (!v.isValid() || (qstrcmp(v.typeName(), "sqlite3*") != 0)) {
        qWarning("Can't find sqldb driver.");
        return false;
    }

    // v.data() returns a pointer to the handle
    sqlite3 *handle = *static_cast<sqlite3 **>(v.data());
    if (handle == nullptr) {
        qWarning("Can't find sqldb driver handle.");
        return false;
    }

    if (sqlite3_enable_load_extension(handle, 1) != SQLITE_OK) {
        qWarning("Cannot enable sqlite extensions.");
        return false;
    }

    char *errtext {nullptr};
    // The location on Debian/Ubuntu.
#if defined(__FreeBSD__)
    static constexpr char const * const pcre = "/usr/local/libexec/sqlite-ext/pcre.so";
#else
    static constexpr char const * const pcre = "/usr/lib/sqlite3/pcre.so";
#endif
    if (sqlite3_load_extension(handle, pcre, nullptr, &errtext) != SQLITE_OK) {
        QString errmsg = QString("Cannot load the PCRE extension: %1.").arg(errtext);
        qWarning("%s", qPrintable(errmsg));
        return false;
    }

    gSqliteRegexpEnabled = true;
    return true;
#else
    return false;
#endif
}

RecExtDataPage* TestRecExtEspnDataSource::newPage(const QJsonDocument& doc)
{
    return new TestRecExtEspnDataPage(this, doc);
}

QUrl TestRecExtEspnDataSource::makeInfoUrl(const SportInfo& info, const QDateTime& dt)
{
    if (!gForceLocalUrl)
        return RecExtEspnDataSource::makeInfoUrl(info, dt);

    QDateTime dt2 = getNow();
    QString path = "file://" + QStringLiteral(TEST_SOURCE_DIR) +
        QString("/data/espn_%1_%2_%3xx.json")
        .arg(info.sport, info.league, dt2.toString("yyyyMM"));
    return {path};
}

QUrl TestRecExtEspnDataSource::makeGameUrl(const ActiveGame& game, const QString& str)
{
    SportInfo info = game.getInfo();
    QString path = "file://" + QStringLiteral(TEST_SOURCE_DIR) +
        QString("/data/espn_%1_%2_game_%3.json")
        .arg(info.sport, info.league, str);
    return {path};
}

RecExtDataPage* TestRecExtMlbDataSource::newPage(const QJsonDocument& doc)
{
    return new TestRecExtMlbDataPage(this, doc);
}

//////////////////////////////////////////////////

TestRecordingExtender::TestRecordingExtender()
{
    m_scheduler = new TestScheduler;
}

// Before all test cases
void TestRecordingExtender::initTestCase()
{
    gCoreContext = new MythCoreContext(TESTVERSION, nullptr);

    if (!QSqlDatabase::drivers().contains("QSQLITE"))
        QSKIP("This test requires the SQLITE database driver.");
    GetMythTestDB(TESTNAME);
    (void)enableSqliteRegex();

    // Start with 1. There is no recording table for 0 to munge.
    for (int updateNum = 1; ; updateNum++)
    {
        DBUpdates updates = getRecordingExtenderDbInfo(updateNum);
        if (updates.empty())
            break;
        convertToSqlite(updates);
        QString error =
            QString("Unable to initialize database, step %1.").arg(updateNum);
        QVERIFY2(performUpdateSeries("MythtTV", updates), qPrintable(error));
    }
}

// After all test cases
void TestRecordingExtender::cleanupTestCase()
{
}

// Before each test case
void TestRecordingExtender::init()
{
}

// After each test case
void TestRecordingExtender::cleanup()
{
    gForceLocalUrl = false;
}

// This is a testing version of RecExtDataSource::create
RecExtDataSource* TestRecordingExtender::createDataSource(AutoExtendType type)
{
    switch (type)
    {
      default:
        return nullptr;
      case AutoExtendType::ESPN:
        return new TestRecExtEspnDataSource(this);
      case AutoExtendType::MLB:
        return new TestRecExtMlbDataSource(this);
    }
}


void TestRecordingExtender::test_findKnownSport_data(void)
{
    if (!gSqliteRegexpEnabled)
        QSKIP("This test requires SQLITE regexp support.");

    QStringList fifaQualLeagues
        {"fifa.worldq.uefa", "fifa.worldq.concacaf", "fifa.worldq.conmebol",
         "fifa.worldq.afc", "fifa.worldq.caf", "fifa.worldq.ofc"};

    QTest::addColumn<QString>("title");
    QTest::addColumn<int>("autoExtendType");
    QTest::addColumn<QString>("expectedSport");
    QTest::addColumn<int>("expectedLeagueSize");
    QTest::addColumn<QStringList>("expectedLeagues");

    QTest::newRow("invalid")   << "Goofy Title"  << 1
                               << "" << 0 << QStringList();
    QTest::newRow("baseball1") << "MLB Baseball" << 1
                               << "baseball" << 1 << QStringList("mlb");
    QTest::newRow("baseball2") << "MLB Baseball" << 2
                               << "baseball" << 1 << QStringList("mlb");
    // Must be current year. (For the purposes of these tests, the
    // current year is 2022.)
    QTest::newRow("baseball3") << "2020 World Series" << 1
                               << "" << 0 << QStringList();
    QTest::newRow("baseball4") << "2022 World Series" << 1
                               << "baseball" << 1 << QStringList("mlb");
    QTest::newRow("football1") << "NFL Football" << 1
                               << "football" << 1 << QStringList("nfl");
    QTest::newRow("football2") << "Super Bowl LVI" << 1
                               << "football" << 1 << QStringList("nfl");
    QTest::newRow("football3") << "Super Bowl LVILVILVI" << 1
                               << "football" << 1 << QStringList("nfl");
    QTest::newRow("football4") << "Super Bowl CII" << 1
                               << "football" << 1 << QStringList("nfl");
    QTest::newRow("football5") << "Super Bowl DII" << 1
                               << "" << 0 << QStringList();
    //QTest::newRow("football6") << "Super Bowl" << 1
    //                           << "football" << 1 << QStringList("nfl");
    QTest::newRow("football7") << "College Football" << 1
                               << "football" << 1 << QStringList("college-football");
    QTest::newRow("football8") << "Rose Bowl" << 1
                               << "football" << 1 << QStringList("college-football");
    QTest::newRow("soccer1")   << "English Premier League" << 1
                               << "soccer" << 1 << QStringList("eng.1");
    QTest::newRow("soccer2")   << "Premier League Soccer" << 1
                               << "soccer" << 1 << QStringList("eng.1");
    QTest::newRow("soccer3")   << "EPL Soccer" << 1
                               << "soccer" << 1 << QStringList("eng.1");
    QTest::newRow("soccer4")   << "German Bundesliga Soccer" << 1
                               << "soccer" << 1 << QStringList("ger.1");
    QTest::newRow("soccer5")   << "Bundesliga Soccer" << 1
                               << "soccer" << 1 << QStringList("ger.1");
    QTest::newRow("soccer6")   << "German Bundesliga Soccer" << 1
                               << "soccer" << 1 << QStringList("ger.1");
    QTest::newRow("soccer7")   << "German 2. Bundesliga Soccer" << 1
                               << "soccer" << 1 << QStringList("ger.2");
    QTest::newRow("soccer8")   << "German 2. Bundesliga Soccer" << 1
                               << "soccer" << 1 << QStringList("ger.2");
    QTest::newRow("soccer97")  << "FIFA World Cup Qualifying" << 1
                               << "soccer" << 1 << fifaQualLeagues;
    QTest::newRow("soccer98")  << "FIFA World Cup 2022 Qualifying Soccer" << 1
                               << "soccer" << 1 << fifaQualLeagues;
    QTest::newRow("soccer99")  << "FIFA Eliminatorias Copa Mundial 2022" << 1
                               << "soccer" << 1 << fifaQualLeagues;

}

void TestRecordingExtender::test_findKnownSport(void)
{
    QFETCH(QString, title);
    QFETCH(int, autoExtendType);
    QFETCH(QString, expectedSport);
    QFETCH(int, expectedLeagueSize);
    QFETCH(QStringList, expectedLeagues);

    // Force the year for testing purposes.
    m_forcedYearforTesting = 2022;

    SportInfoList infoList;
    bool present = findKnownSport(title, autoExtendTypeFromInt(autoExtendType),
                                  infoList);
    bool expectedPresent = (expectedLeagueSize != 0);
    QCOMPARE(present, expectedPresent);
    if (!present)
        return;
    QCOMPARE(infoList[0].sport, expectedSport);
    QCOMPARE(infoList.size(), expectedLeagues.size());

    // Eliminate items that are in both the "expected and "found" sets.
    QSet<QString> expectedSet;
    expectedSet.reserve(expectedLeagues.size());
    for (const auto& league : std::as_const(expectedLeagues))
        expectedSet.insert(league);
    QSet<QString> foundSet;
    foundSet.reserve(infoList.size());
    for (const auto& info : std::as_const(infoList))
        foundSet.insert(info.league);
    auto intersection = foundSet.intersect(expectedSet);
    foundSet = foundSet.subtract(intersection);
    expectedSet = expectedSet.subtract(intersection);

    // Anything remaining in either set is an error.
    if (!expectedSet.isEmpty())
    {
        QString string;
        for (const auto& str : std::as_const(expectedSet))
            string += str + ' ';
        QFAIL(qPrintable(QString("Missing results: %1").arg(string)));
    }
    if (!foundSet.isEmpty())
    {
        QString string;
        for (const auto& str : std::as_const(foundSet))
            string += str + ' ';
        QFAIL(qPrintable(QString("Unexpected results: %1").arg(string)));
    }
}

void TestRecordingExtender::test_nameCleanup_data(void)
{
    if (!gSqliteRegexpEnabled)
        QSKIP("This test requires SQLITE regexp support.");

    QTest::addColumn<QString>("title");
    QTest::addColumn<int>("autoExtendType");
    QTest::addColumn<QString>("sport");
    QTest::addColumn<QString>("team");
    QTest::addColumn<QString>("expectedTeam");

    QTest::newRow("mlb1")     << "MLB Baseball" << 1 << "baseball"
                              << "Washington\t\tNationals"
                              << "Washington Nationals";
    QTest::newRow("mlb2")     << "MLB Baseball" << 1 << "baseball"
                              << "Wáshïngtòn  Ñatiônals"
                              << "Washington Nationals";
    QTest::newRow("mlb3")     << "MLB Baseball" << 1 << "baseball"
                              << "Washington Nationals FC"
                              << "Washington Nationals FC";
    QTest::newRow("soccer1")  << "MLB Baseball" << 1 << "baseball"
                              << "Washington Nationals FC"
                              << "Washington Nationals FC";
    QTest::newRow("soccer2")  << "Soccer" << 1 << "soccer"
                              << "AFC Richmond"
                              << "Richmond";
    QTest::newRow("soccer3")  << "Soccer" << 1 << "soccer"
                              << "A.F.C. Richmond"
                              << "Richmond";
    QTest::newRow("soccer4")  << "Soccer" << 1 << "soccer"
                              << "A.F.C. Richmond S.C."
                              << "Richmond";
    QTest::newRow("soccer5")  << "Soccer" << 1 << "soccer"
                              << "  ÅFÇ   Rîċhmǫñd   "
                              << "Richmond";
    // Don't delete entire string.
    QTest::newRow("soccer6")  << "Soccer" << 1 << "soccer"
                              << "ÅFÇ"
                              << "AFC";
    // Don't delete entire string. Its order dependent.
    QTest::newRow("soccer7")  << "Soccer" << 1 << "soccer"
                              << "AFC SA"
                              << "SA";
    QTest::newRow("soccer8")  << "Soccer" << 1 << "soccer"
                              << "Inglaterra vs. Andorra"
                              << "England vs. Andorra";
    QTest::newRow("soccer9")  << "Soccer" << 1 << "soccer"
                              << "Inter Miami CF"
                              << "Inter Miami";
    QTest::newRow("soccer10") << "Soccer" << 1 << "soccer"
                              << "F.C.Some Team Name"
                              << "Some Team Name";
    QTest::newRow("soccer11") << "Soccer" << 1 << "soccer"
                              << "Los Angeles FC"
                              << "Los Angeles";
    QTest::newRow("soccer12") << "Soccer" << 1 << "soccer"
                              << "LA Galaxy"
                              << "LA Galaxy";
    QTest::newRow("soccer13") << "Soccer" << 1 << "soccer"
                              << "Central Córdoba (SE)"
                              << "Central Cordoba";
    QTest::newRow("basketball1") << "Basketball" << 1 << "basketball"
                                 << "Alabama-Birmingham"
                                 << "UAB";
    QTest::newRow("basketball2") << "Basketball" << 1 << "basketball"
                                 << "Connecticut"
                                 << "UConn";
    QTest::newRow("basketball3") << "Basketball" << 1 << "basketball"
                                 << "Cal State Bakersfield"
                                 << "CSU Bakersfield";
    QTest::newRow("basketball4") << "Basketball" << 1 << "basketball"
                                 << "Cal State Fullerton"
                                 << "CSU Fullerton";
    QTest::newRow("basketball5") << "Basketball" << 1 << "basketball"
                                 << "UT-Chattanooga"
                                 << "Chattanooga";
    QTest::newRow("basketball6") << "Basketball" << 1 << "basketball"
                                 << "Hawaii"
                                 << "Hawai'i";
    QTest::newRow("basketball7") << "Basketball" << 1 << "basketball"
                                 << "LIU"
                                 << "Long Island University";
    QTest::newRow("basketball8") << "Basketball" << 1 << "basketball"
                                 << "Loyola-Chicago"
                                 << "Loyola Chicago";
    QTest::newRow("basketball9") << "Basketball" << 1 << "basketball"
                                 << "Loyola (Md.)"
                                 << "Loyola (MD)";
    QTest::newRow("basketball10") << "Basketball" << 1 << "basketball"
                                  << "Miami (Ohio)"
                                  << "Miami (OH)";
    QTest::newRow("basketball11") << "Basketball" << 1 << "basketball"
                                  << "Texas-Arlington"
                                  << "UT Arlington";
    QTest::newRow("basketball12") << "Basketball" << 1 << "basketball"
                                  << "Texas-El Paso"
                                  << "UTEP";
    QTest::newRow("basketball13") << "Basketball" << 1 << "basketball"
                                  << "Texas-Rio Grande Valley"
                                  << "UT Rio Grande Valley";
    QTest::newRow("basketball14") << "Basketball" << 1 << "basketball"
                                  << "Massachusetts"
                                  << "UMass";
    QTest::newRow("basketball15") << "Basketball" << 1 << "basketball"
                                  << "UNC-Asheville"
                                  << "UNC Asheville";
    QTest::newRow("basketball16") << "Basketball" << 1 << "basketball"
                                  << "UNC-Wilmington"
                                  << "UNC Wilmington";
    QTest::newRow("basketball17") << "Basketball" << 1 << "basketball"
                                  << "UT-Martin"
                                  << "UT Martin";
    QTest::newRow("basketball18") << "Basketball" << 1 << "basketball"
                                  << "Grambling State"
                                  << "Grambling";
    QTest::newRow("basketball19") << "Basketball" << 1 << "basketball"
                                  << "McNeese State"
                                  << "McNeese";
}

void TestRecordingExtender::test_nameCleanup(void)
{
    QFETCH(QString, title);
    QFETCH(int, autoExtendType);
    QFETCH(QString, sport);
    QFETCH(QString, team);
    QFETCH(QString, expectedTeam);

    QString dummy;
    SportInfo info {title, autoExtendTypeFromInt(autoExtendType), sport, dummy};
    nameCleanup(info, team, dummy);
    QCOMPARE(team, expectedTeam);
}

void TestRecordingExtender::test_parseProgramInfo_data(void)
{
    QTest::addColumn<QString>("subtitle");
    QTest::addColumn<QString>("description");
    QTest::addColumn<QString>("expectedTeam1");
    QTest::addColumn<QString>("expectedTeam2");
    QTest::newRow("subtitle1") << "Washington Nationals vs. Miami Marlins"
                               << "ipsum lorem"
                               << "Washington Nationals"
                               << "Miami Marlins";
    QTest::newRow("subtitle2") << "Washington Nationals vs Miami Marlins"
                               << "ipsum lorem"
                               << "Washington Nationals"
                               << "Miami Marlins";
    QTest::newRow("subtitle3") << "Washington Nationals at Miami Marlins"
                               << "ipsum lorem"
                               << "Washington Nationals"
                               << "Miami Marlins";
    QTest::newRow("subtitle4") << "Washington Nationals @ Miami Marlins"
                               << "ipsum lorem"
                               << "Washington Nationals"
                               << "Miami Marlins";
    QTest::newRow("badsubtitle1") << "Washington Nationals"
                                  << "ipsum lorem"
                                  << ""
                                  << "";
    QTest::newRow("badsubtitle2") << "Washington Nationals @ Miami Marlins vs. Denver Broncos"
                                  << "ipsum lorem"
                                  << ""
                                  << "";
    QTest::newRow("description1") << "ipsum lorem"
                                  << "Washington Nationals vs. Miami Marlins"
                                  << "Washington Nationals"
                                  << "Miami Marlins";
    QTest::newRow("description2") << "ipsum lorem"
                                  << "Washington Nationals vs Miami Marlins"
                                  << "Washington Nationals"
                                  << "Miami Marlins";
    QTest::newRow("description3") << "ipsum lorem"
                                  << "Washington Nationals at Miami Marlins"
                                  << "Washington Nationals"
                                  << "Miami Marlins";
    QTest::newRow("description4") << "ipsum lorem"
                                  << "Washington Nationals @ Miami Marlins"
                                  << "Washington Nationals"
                                  << "Miami Marlins";
    QTest::newRow("description5") << "ipsum lorem"
                                  << "Washington Nationals @ Miami Marlins. The matchup of a lifetime... or at least this week."
                                  << "Washington Nationals"
                                  << "Miami Marlins";
    QTest::newRow("description6") << "ipsum lorem"
                                  << "Washington Nationals vs. Miami Marlins: The matchup of a lifetime... or at least this week."
                                  << "Washington Nationals"
                                  << "Miami Marlins";
    QTest::newRow("baddescription1") << "ipsum lorem"
                                     << "The matchup of a lifetime... or at least this week."
                                     << ""
                                     << "";
    QTest::newRow("collegebowl1") << "Quick Lane Bowl: Western Michigan vs. Nevada"
                                  << "The Wolf Pack play the Broncos in the Quick Lane Bowl. Western Michigan seeks just its second bowl game triumph in program history. Nevada quarterback Carson Strong finished the season ranked sixth in the country with 4,186 passing yards and 36 TDs."
                                  << "Western Michigan"
                                  << "Nevada";
    QTest::newRow("collegebowl2") << "R+L Carriers New Orleans Bowl: Louisiana vs. Marshall"
                                  << "The No. 23 Ragin' Cajuns (12-1) take on the Thundering Herd (7-5) in the New Orleans Bowl. Louisiana Lafayette has won 12 in a row after capturing the Sun Belt title against Appalachian State on Dec. 4. Marshall is 7-2 in bowl games since 2009."
                                  << "Louisiana"
                                  << "Marshall";
    QTest::newRow("collegebowl3") << "New Era Pinstripe Bowl: Maryland vs. Virginia Tech"
                                  << "The 6-6 Terrapins face off against the 6-6 Hokies in the Pinstripe Bowl. Both teams became bowl eligible with season-ending victories Nov. 27. Maryland seeks its first postseason win since 2010. Virginia Tech's last bowl celebration came in 2016."
                                  << "Maryland"
                                  << "Virginia Tech";
    QTest::newRow("collegebowl4") << "Lockheed Martin Armed Forces Bowl: Missouri vs. Army"
                                  << "The Tigers play the Black Knights in the Armed Forces Bowl in Fort Worth, Texas. Army has won this postseason contest three times, most recently in 2018 when it thrashed Houston 70-14. Missouri seeks its first bowl victory since the 2014 Citrus Bowl."
                                  << "Missouri"
                                  << "Army";
    QTest::newRow("collegebowl5") << "Servpro First Responder Bowl: Air Force vs. Louisville"
                                  << "The Falcons (9-3) face Malik Cunningham and the Cardinals (6-6) in the First Responder Bowl. Cunningham led all quarterbacks with 968 rushing yards this season, adding 19 touchdowns on the ground. Air Force won its final three regular season games."
                                  << "Air Force"
                                  << "Louisville";
    QTest::newRow("collegeplayoff1") << "NCAA Division III Tournament: Mary Hardin-Baylor at Wisconsin-Whitewater"
                                     << "All the action from NCAA college football."
                                     << "Mary Hardin-Baylor"
                                     << "Wisconsin-Whitewater";
    QTest::newRow("collegeplayoff2") << "NCAA Division III Tournament: North Central (Ill.) at Mount Union"
                                     << "From Mount Union Stadium in Alliance, Ohio."
                                     << "North Central (Ill.)"
                                     << "Mount Union";
    QTest::newRow("collegeplayoff3") << "NCAA Division II Championship: Valdosta State vs. Ferris State"
                                     << "The Bulldogs try to complete a perfect season when they face the Blazers in the NCAA Division II championship. Ferris State (13-0) outscored its playoff foes 150-47 to reach the final. Valdosta State (12-1) beat Ferris State for the 2018 title."
                                     << "Valdosta State"
                                     << "Ferris State";
    QTest::newRow("collegeplayoff4") << "NCAA Division III Championship: North Central (Ill.) vs. Mary Hardin-Baylor"
                                     << "The Cardinals (13-0) face the Crusaders (14-0) in the NCAA Division III championship. Mary-Hardin Baylor beat Wisconsin-Whitewater 24-7 in the semifinals. North Central doubled up Mount Union 26-13. North Central claimed the last D-III title in 2019."
                                     << "North Central (Ill.)"
                                     << "Mary Hardin-Baylor";
}

void TestRecordingExtender::test_parseProgramInfo(void)
{
    QFETCH(QString, subtitle);
    QFETCH(QString, description);
    QFETCH(QString, expectedTeam1);
    QFETCH(QString, expectedTeam2);

    QString team1;
    QString team2;
    bool success = parseProgramInfo(subtitle, description, team1, team2);

    QCOMPARE(success, !expectedTeam1.isEmpty());
    QCOMPARE(team1, expectedTeam1);
    QCOMPARE(team2, expectedTeam2);
}

void TestRecordingExtender::test_parseJson(void)
{
    m_nowForTest = QDateTime::fromString("2021-09-22T23:59:00Z", Qt::ISODate);
    auto source = std::make_unique<TestRecExtMlbDataSource>(this);

    SportInfo info {"MLB Baseball", AutoExtendType::MLB, "", ""};
    ActiveGame game(0, "MLB Baseball", info);
    QString path = "file://" + QStringLiteral(TEST_SOURCE_DIR) +
        "/data/mlb_baseball_20210921_1720.json";
    game.setInfoUrl(path);
    game.setTeams("Washington Nationals", "Miami Marlins");

    // Test loading info page
    auto* page = source->loadPage(game, game.getInfoUrl());
    QCOMPARE(page != nullptr, true);

    QJsonObject json = page->getDoc().object();
    QVERIFY(!json.isEmpty());
    QCOMPARE(json.size(), 6);
    QVERIFY(!json.contains("idontexist"));
    QVERIFY(json.contains("totalItems"));
    QJsonValue value = json.value("totalItems");
    QVERIFY(value.isDouble());
    QCOMPARE(value.toDouble(), 42.0); // Make Ubuntu 18.04 happy

    QJsonArray arrValue;
    QVERIFY(page->getJsonArray(json, "dates", arrValue));
    QCOMPARE(arrValue.size(), 3);

    QJsonObject dateObject = arrValue[0].toObject();
    QVERIFY(!dateObject.isEmpty());
    QString strValue;
    QVERIFY(page->getJsonString(dateObject, "date", strValue));

    int intValue {0};
    QStringList pathList = { "dates[0]", "games[4]", "gamePk" };
    QVERIFY(page->getJsonInt(json, pathList, intValue));
    QCOMPARE(intValue, 632408);
    QVERIFY(page->getJsonInt(json, "dates[0]/games[4]/teams/away/leagueRecord/wins", intValue));
    QCOMPARE(intValue,61);

    QStringList pathList2 = { "dates[0]", "games[4]", "status", "detailedState" };
    QVERIFY(page->getJsonString(json, pathList2, strValue));
    QCOMPARE(strValue, QString("Final"));
    QVERIFY(page->getJsonString(json, "dates[0]/games[4]/teams/away/leagueRecord/pct", strValue));
    QCOMPARE(strValue, QString(".407"));
}

void TestRecordingExtender::test_parseEspn_data(void)
{
    QTest::addColumn<QString>("sport");
    QTest::addColumn<QString>("league");
    QTest::addColumn<QString>("infoFile");
    QTest::addColumn<QString>("subtitle");
    QTest::addColumn<QString>("nowForTest");
    QTest::addColumn<QString>("expectedGameTime");
    QTest::addColumn<QString>("expectedGameURL");
    QTest::addColumn<int>    ("expectedPeriod");
    QTest::addColumn<bool>   ("expectedFinished");
    QTest::addColumn<QString>("expectedTextState");

    QTest::newRow("1") << "baseball" << "mlb"
                       << "espn_baseball_mlb_20210921_1955.json"
                       << "Washington Nationals at Miami Marlins"
                       << "2021-09-21T12:38:00Z" // now
                       << "" // gameTime
                       << "" << 9 << true << "";
    QTest::newRow("2") << "baseball" << "mlb"
                       << "espn_baseball_mlb_20210921_1955.json"
                       << "Washington Nationals at Miami Marlins"
                       << "2021-09-21T22:00:00Z" // now
                       << "2021-09-21T22:40:00Z" // gameTime
                       << "espn_baseball_mlb_game_401229321.json"
                       << 5 << false << "Top 5th";
    // Test API returning longer names
    QTest::newRow("3") << "baseball" << "mlb"
                       << "espn_baseball_mlb_20210921_1955.json"
                       << "Nationals at Miami"
                       << "2021-09-21T22:00:00Z" // now
                       << "2021-09-21T22:40:00Z" // gameTime
                       << "espn_baseball_mlb_game_401229321.json"
                       << 5 << false << "Top 5th";
    // Test API returning shorter names
    QTest::newRow("4") << "baseball" << "mlb"
                       << "espn_baseball_mlb_20210921_1955.json"
                       << "New York Mets at Boston Red Sox"
                       << "2021-09-21T23:10:00Z" // now
                       << "2021-09-21T23:10:00Z" // gameTime
                       << "espn_baseball_mlb_game_401229314.json"
                       << 3 << false << "Top 3rd";
    // Test diacritical marks in TV listings
    QTest::newRow("5") << "baseball" << "mlb"
                       << "espn_baseball_mlb_20210921_1955.json"
                       << "St. Lôùís Cârdiñals at Milwåukèé Brėwers"
                       << "2021-09-21T23:30:00Z" // now
                       << "2021-09-21T23:40:00Z" // gameTime
                       << "espn_baseball_mlb_game_401229318.json"
                       << 1 << false << "Bottom 1st";
    QTest::newRow("6") << "baseball" << "mlb"
                       << "espn_baseball_mlb_20210921_1955.json"
                       << "Chicago White Sox at Detroit Tigers"
                       << "2021-09-21T20:00:00Z" // now
                       << "2021-09-21T17:10:00Z" // gameTime
                       << "espn_baseball_mlb_game_401229309.json"
                       << 9 << true << "Final";
    QTest::newRow("7") << "soccer" << "womens_college"
                       << "espn_soccer_womens_college_20211010.json"
                       << "Tennessee at South Carolina"
                       << "2021-10-10T20:00:00Z" // now
                       << "2021-10-10T18:00:00Z" // gameTime
                       << "espn_soccer_womens_college_game_617628.json"
                       << 2 << false << "Second Half: 87'";
}

void TestRecordingExtender::test_parseEspn(void)
{
    QFETCH(QString, sport);
    QFETCH(QString, league);
    QFETCH(QString, infoFile);
    QFETCH(QString, subtitle);
    QFETCH(QString, nowForTest);
    QFETCH(QString, expectedGameTime);
    QFETCH(QString, expectedGameURL);
    QFETCH(int,     expectedPeriod);
    QFETCH(bool,    expectedFinished);
    QFETCH(QString, expectedTextState);

    m_nowForTest = QDateTime::fromString(nowForTest, Qt::ISODate);
    auto source = std::make_unique<TestRecExtEspnDataSource>(this);

    SportInfo info {"MLB Baseball", AutoExtendType::ESPN, sport, league};
    ActiveGame game(0, "MLB Baseball", info);
    QString path = "file://" + QStringLiteral(TEST_SOURCE_DIR) +
        "/data/" + infoFile;
    game.setInfoUrl(path);

    // Previous case tested parsing of team names.
    QString team1;
    QString team2;
    bool success = parseProgramInfo(subtitle, "", team1, team2);
    QCOMPARE(success, true);
    game.setTeams(team1, team2);
    RecordingExtender::nameCleanup(info, team1, team2);
    game.setTeamsNorm(team1, team2);

    // Test loading info page
    auto* page = source->loadPage(game, game.getInfoUrl());
    QCOMPARE(page != nullptr, true);

    // Test finding game in info page
    success = page->findGameInfo(game);
    if (expectedGameTime.isEmpty())
    {
        QCOMPARE(success, false);
        return;
    }
    QCOMPARE(success, true);
    QCOMPARE(game.getStartTimeAsString(), expectedGameTime);
    QVERIFY(game.getGameUrl().url().endsWith(expectedGameURL));

    // Test loading games status page (same as info page for ESPN)
    page = source->loadPage(game, game.getGameUrl());
    QCOMPARE(page != nullptr, true);

    // Test finding game in status page
    auto gameState = page->findGameScore(game);
    QCOMPARE(gameState.isValid(), true);
    QCOMPARE(gameState.getPeriod(), expectedPeriod);
    QCOMPARE(gameState.isFinished(), expectedFinished);
    QCOMPARE(gameState.getTextState(), expectedTextState);
}

void TestRecordingExtender::test_parseMlb_data(void)
{
    QTest::addColumn<QString>("infoFile");
    QTest::addColumn<QString>("subtitle");
    QTest::addColumn<QString>("nowForTest");
    QTest::addColumn<QString>("expectedGameTime");
    QTest::addColumn<QString>("expectedGameURL");
    QTest::addColumn<QString>("gameFile");
    QTest::addColumn<int>    ("expectedPeriod");
    QTest::addColumn<bool>   ("expectedFinished");
    QTest::addColumn<QString>("expectedTextState");

    QTest::newRow("1") << "mlb_baseball_20210921_1720.json"
                       << "Washington Nationals at Miami Marlins"
                       << "2021-09-21T12:38:00Z" // now
                       << "" // gameTime
                       << "" << "" << 9 << true << "";
    QTest::newRow("2") << "mlb_baseball_20210921_1720.json"
                       << "Washington Nationals at Miami Marlins"
                       << "2021-09-20T22:00:00Z" // now
                       << "2021-09-20T22:40:00Z" // gameTime
                       << "/api/v1.1/game/632408/feed/live"
                       << "mlb_game_632408.json"
                       << 10 << true << "game over";
    QTest::newRow("3") << "mlb_baseball_20210921_1720.json"
                       << "Washington Nationals at Miami Marlins"
                       << "2021-09-21T22:38:00Z" // now
                       << "2021-09-21T22:40:00Z" // gameTime
                       << "/api/v1.1/game/632379/feed/live"
                       << "mlb_game_632379.json"
                       << 9 << true << "game over";
    QTest::newRow("4") << "mlb_baseball_20210921_1720.json"
                       << "Washington Nationals at Miami Marlins"
                       << "2021-09-22T23:59:00Z" // now
                       << "2021-09-22T22:40:00Z" // gameTime
                       << "/api/v1.1/game/632385/feed/live"
                       << "mlb_game_632385.json"
                       << 1 << false << "Pre-Game";
}

void TestRecordingExtender::test_parseMlb(void)
{
    QFETCH(QString, infoFile);
    QFETCH(QString, subtitle);
    QFETCH(QString, nowForTest);
    QFETCH(QString, expectedGameTime);
    QFETCH(QString, expectedGameURL);
    QFETCH(QString, gameFile);
    QFETCH(int,     expectedPeriod);
    QFETCH(bool,    expectedFinished);
    QFETCH(QString, expectedTextState);

    m_nowForTest = QDateTime::fromString(nowForTest, Qt::ISODate);
    auto source = std::make_unique<TestRecExtMlbDataSource>(this);

    SportInfo info {"MLB Baseball", AutoExtendType::MLB, "sport", "league"};
    ActiveGame game(0, "MLB Baseball", info);
    QString path = "file://" + QStringLiteral(TEST_SOURCE_DIR) +
        "/data/" + infoFile;
    game.setInfoUrl(path);

    // Previous case tested parsing of team names.
    QString team1;
    QString team2;
    bool success = parseProgramInfo(subtitle, "", team1, team2);
    QCOMPARE(success, true);
    game.setTeams(team1, team2);
    RecordingExtender::nameCleanup(info, team1, team2);
    game.setTeamsNorm(team1, team2);

    // Test loading info page
    auto* page = source->loadPage(game, game.getInfoUrl());
    QCOMPARE(page != nullptr, true);

    // Test finding game in info page
    success = page->findGameInfo(game);
    if (expectedGameTime.isEmpty())
    {
        QCOMPARE(success, false);
        return;
    }
    QCOMPARE(success, true);
    QCOMPARE(game.getStartTimeAsString(), expectedGameTime);
    QVERIFY(game.getGameUrl().url().endsWith(expectedGameURL));

    // Test loading games status page
    path = "file://" + QStringLiteral(TEST_SOURCE_DIR) + "/data/" + gameFile;
    game.setGameUrl(path);
    page = source->loadPage(game, game.getGameUrl());
    QCOMPARE(page != nullptr, true);

    // Test finding game in status page
    auto gameState = page->findGameScore(game);
    QCOMPARE(gameState.isValid(), true);
    QCOMPARE(gameState.getPeriod(), expectedPeriod);
    QCOMPARE(gameState.isFinished(), expectedFinished);
    QCOMPARE(gameState.getTextState(), expectedTextState);
}

void TestRecordingExtender::test_processNewRecordings_data(void)
{
    if (!gSqliteRegexpEnabled)
        QSKIP("This test requires SQLITE regexp support.");

    QTest::addColumn<int>("recordedID");
    QTest::addColumn<QString>("title");
    QTest::addColumn<QString>("subtitle");
    QTest::addColumn<QString>("nowForTest");
    QTest::addColumn<QString>("expectedInfoUrl");
    QTest::addColumn<QString>("expectedGameUrl");

    QTest::newRow("fifa1") << 1
                           << "FIFA World Cup 2022 Qualifying"
                           << "Syria vs. Lebanon" // AFC
                           << "2021-10-12T16:00Z" // now
                           << "espn_soccer_fifa.worldq.afc_202110xx.json"
                           << "espn_soccer_fifa.worldq.afc_game_611285.json";
    QTest::newRow("fifa2") << 2
                           << "FIFA Eliminatorias Copa Mundial 2022"
                           << "Morocco at Guinea" // CAF
                           << "2021-10-12T19:00Z" // now
                           << "espn_soccer_fifa.worldq.caf_202110xx.json"
                           << "espn_soccer_fifa.worldq.caf_game_602035.json";
    QTest::newRow("fifa3") << 3
                           << "FIFA World Cup 2022 Qualifying"
                           << "United States vs. Costa Rica" // CONCACAF
                           << "2021-10-13T23:00Z" // now
                           << "espn_soccer_fifa.worldq.concacaf_202110xx.json"
                           << "espn_soccer_fifa.worldq.concacaf_game_606073.json";
    QTest::newRow("fifa4") << 4
                           << "FIFA Eliminatorias Copa Mundial 2022"
                           << "Colombia vs. Ecuador" // CONMEBOL
                           << "2021-10-14T21:00Z" // now
                           << "espn_soccer_fifa.worldq.conmebol_202110xx.json"
                           << "espn_soccer_fifa.worldq.conmebol_game_561039.json";
    QTest::newRow("fifa5") << 5
                           << "FIFA World Cup 2022 Qualifying"
                           << "New Zealand at Solomon Islands" // OFC
                           << "2017-09-05T03:00Z" // now
                           << "espn_soccer_fifa.worldq.ofc_201709xx.json"
                           << "espn_soccer_fifa.worldq.ofc_game_495540.json";
    QTest::newRow("fifa6") << 6
                           << "FIFA World Cup 2022 Qualifying"
                           << "Denmark vs. Austria" // UEFA
                           << "2021-10-12T18:45Z" // now
                           << "espn_soccer_fifa.worldq.uefa_202110xx.json"
                           << "espn_soccer_fifa.worldq.uefa_game_590254.json";
}

void TestRecordingExtender::test_processNewRecordings(void)
{
    QFETCH(int, recordedID);
    QFETCH(QString, title);
    QFETCH(QString, subtitle);
    QFETCH(QString, nowForTest);
    QFETCH(QString, expectedInfoUrl);
    QFETCH(QString, expectedGameUrl);

    m_nowForTest = QDateTime::fromString(nowForTest, Qt::ISODate);

    // Make sure nothing left from previous test case.
    m_newRecordings.clear();
    m_activeGames.clear();

    // Create fake recording
    RecordingInfo *ri = m_scheduler->GetRecording(recordedID);
    RecordingRule *rr = ri->GetRecordingRule(); // owned by ri
    ri->m_title        = title;
    ri->m_subtitle     = subtitle;
    ri->m_description  = "ipsum lorem";
    ri->m_recStatus    = RecStatus::Recording;
    rr->m_autoExtend   = AutoExtendType::ESPN;

    // Force a couple of items for testing purposes.
    m_forcedYearforTesting = 2022;
    gForceLocalUrl = true;

    // Kick RecordingExtender and cross fingers.
    m_newRecordings.append(recordedID);
    processNewRecordings();
    QCOMPARE(m_activeGames.size(), 1);
    ActiveGame game = m_activeGames[0];
    QVERIFY(game.getInfoUrl().url().endsWith(expectedInfoUrl));
    QVERIFY(game.getGameUrl().url().endsWith(expectedGameUrl));
}

QTEST_GUILESS_MAIN(TestRecordingExtender)
