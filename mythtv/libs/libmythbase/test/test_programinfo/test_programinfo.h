/*
 *  Class TestProgramInfo
 *
 *  Copyright (C) Karl Dietz 2014
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

#include <array>
#include <iostream>
#include <QTest>

#include "libmythbase/mythcorecontext.h"
#include "libmythbase/programtypes.h"
#include "libmythbase/programinfo.h"

#define DEBUG 0

class TestProgramInfo : public QObject
{
    Q_OBJECT
  private:
    static ProgramInfo mockMovie (QString const &inetref, QString const &programid, QString const &title, unsigned int year)
    {
        return ProgramInfo (
            (uint) 0, /* recordedid */
            title,          /* title */
            "",              /* sortTitle */
            "",              /* subtitle */
            "",              /* sortSubtitle */
            "Its a movie.", /* description */
            (uint) 0, /* season */
            (uint) 0, /* episode */
            (uint) 0, /* total episodes */
            "", /* syndicated episode */
            "", /* category */

            UINT_MAX, /* chanid */
            "", /* channum */
            "", /* chansign */
            "", /* channame */
            "", /* chan playback filters */

            "", /* recgroup */
            "", /* playgroup */

            "", /* pathname */

            "", /* hostname */
            "", /* storagegroup */

            "", /* series id */
            programid, /* program id */
            inetref, /* inetref */
            ProgramInfo::kCategoryMovie, /* cat type */

            (int) 0, /* rec priority */

            (uint64_t) 0, /* filesize */

            MythDate::fromString ("2000-01-01 00:00:00"), /* start ts */
            MythDate::fromString ("2000-01-01 01:30:00"), /* end ts */
            MythDate::fromString ("2000-01-01 00:00:00"), /* rec start ts */
            MythDate::fromString ("2000-01-01 01:30:00"), /* rec end ts */

            0.0F, /* stars */

            year, /* year */
            (uint) 0, /* part number */
            (uint) 0, /* part total */

            QDate(), /* original air date */
            QDateTime(), /* last modified */

            RecStatus::Unknown, /* rec status */

            UINT_MAX, /* record id */

            RecordingDupInType::kDupsUnset, /* dupin */
            RecordingDupMethodType::kDupCheckUnset, /* dupmethod */

            UINT_MAX, /* find id */

            (uint) 0, /* programflags */
            (uint) 0, /* audio props */
            (uint) 0, /* video props */
            (uint) 0, /* subtitle type */
            "", /* inputname */
            QDateTime() /* bookmark update */
        );
    }

    QString m_draculaList = "Dracula||Its a movie.|0|0|0|||4294967295|||||0|"
        "946684800|946690200|4294967295||0|0|0|0|0|4294967295|0|0|0|946684800|"
        "946690200|0|Default|||tt0051554|11868|4294967295|0||Default|0|0|"
        "Default|0|0|0|1958|0|0|1|0||4294967295";
    QString m_flash34List = "The Flash (2014)|The New Rogues|Barry continues to "
        "train Jesse ...|3|4|23|syndicatedepisode|Drama|1514|514|WNUVDT|"
        "WNUBDT (WNUV-DT)|/recordings/1514_20161025235800.ts|6056109800|"
        "1477440000|1477443600|20141007|localhost|0|0|0|7|0|19890314|0|15|8|1477439880|"
        "1477443720|131600|Default||EP01922936|EP019229360055|ttvdb4.py_279121|"
        "1477444354|0.95|2016-10-26|Default|0|0|Default|8|1090|1|2016|50|133|4|715|"
        "Prime A-1|4294967295";
    QString m_supergirl23List = "Supergirl|Welcome to Earth|An attack is made "
        "on the President as hot-button...|2|3|23|syndicatedepisode|Drama|"
        "1514|514|WNUVDT|WNUBDT (WNUV-DT)|/recordings/1514_20161024235800.ts|"
        "6056109670|1477353600|1477357200|20151026|localhost|0|0|0|-1|0|19660922|0|15|8|"
        "1477353480|1477357320|1538|Default||EP02185451|EP021854510025|"
        "ttvdb4.py_295759|1477444354|0.94|2016-10-25|Default|0|0|Default|33|33|8|"
        "2016|23|106|4|66247|Prime A-0|4294967295";
    InfoMap m_flash34Map = {
        {"00x00", "3x04"},
        {"audioproperties", "8"},
        {"audioproperties_names", "DOLBY"},
        {"callsign", "WNUVDT"},
        {"card", "-"},
        {"catType", "tvshow"},
        {"category", "Drama"},
        {"chanid", "1514"},
        {"channame", "WNUBDT (WNUV-DT)"},
        {"channel", "514 WNUVDT"},
        {"channum", "514"},
        {"commfree", "0"},
        {"description", "Barry continues to train Jesse ..."},
        {"description0", "Barry continues to train Jesse ..."},
        {"enddate", "Wed 26 October"},
        {"endtime", "1:00 AM"},
        {"endts", "1477443600"},
        {"endyear", "2016"},
        {"episode", "4"},
        {"filesize", "6,056,109,800"},
        {"filesize_str", "5.64 GB"},
        {"inetref", "ttvdb4.py_279121"},
        {"input", "-"},
        {"inputname", "Prime A-1"},
        {"lastmodified", "Wed 26 October, 1:12 AM"},
        {"lastmodifieddate", "Wed 26 October"},
        {"lastmodifiedtime", "1:12 AM"},
        {"lenmins", "64 minute(s)"},
        {"lentime", "1 hour(s) 4 minute(s)"},
        {"longchannel", "514 WNUBDT (WNUV-DT)"},
        {"mediatype", "recording"},
        {"mediatypestring", "Recording"},
        {"numstars", "10"},
        {"originalairdate", "Wed 26 October"},
        {"partnumber", "50"},
        {"parttotal", "133"},
        {"playgroup", "Default"},
        {"programflags", "131600"},
        {"programflags_names", "BOOKMARK|WATCHED|IGNORELASTPLAYPOS"},
        {"programid", "EP019229360055"},
        {"recenddate", "Wed 26"},
        {"recendtime", "1:02 AM"},
        {"recordedid", "715"},
        {"recordinggroup", "Default"},
        {"recpriority", "7"},
        {"recpriority2", "0"},
        {"recstartdate", "Tue 25"},
        {"recstarttime", "11:58 PM"},
        {"recstatus", "Not Recording"},
        {"recstatuslong", "This showing is not scheduled to record"},
        {"rectype", "Not Recording"},
        {"rectypechar", " "},
        {"rectypestatus", "Not Recording"},
        {"s00e00", "s03e04"},
        {"season", "3"},
        {"seriesid", "EP01922936"},
        {"shortenddate", "Wed 26"},
        {"shortoriginalairdate", "Wed 26"},
        {"shortstartdate", "Wed 26"},
        {"shortstarttimedate", "Tue 25, 11:58 PM"},
        {"shorttimedate", "Tue 25, 11:58 PM - 1:02 AM"},
        {"sortsubtitle", "new rogues"},
        {"sorttitle", "flash (2014)"},
        {"sorttitlesubtitle", "flash (2014) - \"new rogues\""},
        {"stars", "10 star(s)"},
        {"startdate", "Wed 26 October"},
        {"starttime", "12:00 AM"},
        {"starttimedate", "Tue 25 October, 11:58 PM"},
        {"startts", "1477440000"},
        {"startyear", "2016"},
        {"storagegroup", "Default"},
        {"subtitle", "The New Rogues"},
        {"subtitleType", "1"},
        {"subtitleType_names", "HARDHEAR"},
        {"syndicatedepisode", "syndicatedepisode"},
        {"timedate", "Tue 25 October, 11:58 PM - 1:02 AM"},
        {"title", "The Flash (2014)"},
        {"titlesubtitle", "The Flash (2014) - \"The New Rogues\""},
        {"totalepisodes", "23"},
        {"videoproperties", "1090"},
        {"videoproperties_names", "HDTV|1080|DAMAGED"},
        {"year", "2016"},
        {"yearstars", "(2016, 10 star(s))"},
    };
    InfoMap m_supergirl23Map = {
        {"00x00", "2x03"},
        {"audioproperties", "33"},
        {"audioproperties_names", "STEREO|VISUALIMPAIR"},
        {"callsign", "WNUVDT"},
        {"card", "-"},
        {"catType", "tvshow"},
        {"category", "Drama"},
        {"chanid", "1514"},
        {"channame", "WNUBDT (WNUV-DT)"},
        {"channel", "514 WNUVDT"},
        {"channum", "514"},
        {"commfree", "0"},
        {"description", "An attack is made on the President as hot-button..."},
        {"description0", "An attack is made on the President as hot-button..."},
        {"enddate", "Tue 25 October"},
        {"endtime", "1:00 AM"},
        {"endts", "1477357200"},
        {"endyear", "2016"},
        {"episode", "3"},
        {"filesize", "6,056,109,670"},
        {"filesize_str", "5.64 GB"},
        {"inetref", "ttvdb4.py_295759"},
        {"input", "-"},
        {"inputname", "Prime A-0"},
        {"lastmodified", "Wed 26 October, 1:12 AM"},
        {"lastmodifieddate", "Wed 26 October"},
        {"lastmodifiedtime", "1:12 AM"},
        {"lenmins", "64 minute(s)"},
        {"lentime", "1 hour(s) 4 minute(s)"},
        {"longchannel", "514 WNUBDT (WNUV-DT)"},
        {"mediatype", "recording"},
        {"mediatypestring", "Recording"},
        {"numstars", "9"},
        {"originalairdate", "Tue 25 October"},
        {"outputfilters", ""},
        {"partnumber", "23"},
        {"parttotal", "106"},
        {"playgroup", "Default"},
        {"programflags", "1538"},
        {"programflags_names", "CUTLIST|WATCHED|PRESERVED"},
        {"programid", "EP021854510025"},
        {"recenddate", "Tue 25"},
        {"recendtime", "1:02 AM"},
        {"recordedid", "66247"},
        {"recordinggroup", "Default"},
        {"recpriority", "-1"},
        {"recpriority2", "0"},
        {"recstartdate", "Mon 24"},
        {"recstarttime", "11:58 PM"},
        {"recstatus", "Not Recording"},
        {"recstatuslong", "This showing is not scheduled to record"},
        {"rectype", "Not Recording"},
        {"rectypechar", " "},
        {"rectypestatus", "Not Recording"},
        {"s00e00", "s02e03"},
        {"season", "2"},
        {"seriesid", "EP02185451"},
        {"shortenddate", "Tue 25"},
        {"shortoriginalairdate", "Tue 25"},
        {"shortstartdate", "Tue 25"},
        {"shortstarttimedate", "Mon 24, 11:58 PM"},
        {"shorttimedate", "Mon 24, 11:58 PM - 1:02 AM"},
        {"sortsubtitle", "welcome to earth"},
        {"sorttitle", "supergirl"},
        {"sorttitlesubtitle", "supergirl - \"welcome to earth\""},
        {"stars", "9 star(s)"},
        {"startdate", "Tue 25 October"},
        {"starttime", "12:00 AM"},
        {"starttimedate", "Mon 24 October, 11:58 PM"},
        {"startts", "1477353600"},
        {"startyear", "2016"},
        {"storagegroup", "Default"},
        {"subtitle", "Welcome to Earth"},
        {"subtitleType", "8"},
        {"subtitleType_names", "SIGNED"},
        {"syndicatedepisode", "syndicatedepisode"},
        {"timedate", "Mon 24 October, 11:58 PM - 1:02 AM"},
        {"title", "Supergirl"},
        {"titlesubtitle", "Supergirl - \"Welcome to Earth\""},
        {"totalepisodes", "23"},
        {"videoproperties", "33"},
        {"videoproperties_names", "WIDESCREEN|720"},
        {"year", "2016"},
        {"yearstars", "(2016, 9 star(s))"},
    };

    ProgramInfo m_dracula;
    ProgramInfo m_flash34;
    ProgramInfo m_supergirl23;

    QMap<QString,int> m_intOverrides {};

  private slots:
    void initTestCase()
    {
        gCoreContext = new MythCoreContext("test_programinfo_1.0", nullptr);
        m_intOverrides["AlwaysStreamFiles"] = 0;
        gCoreContext->setTestIntSettings(m_intOverrides);

        m_dracula = ProgramInfo(mockMovie ("11868", "tt0051554", "Dracula", 1958));
        m_flash34 = ProgramInfo
            (715,
             "The Flash (2014)", "",
             "The New Rogues", "",
             "Barry continues to train Jesse ...",
             3, 4, 23, "syndicatedepisode", "Drama",
             1514, "514", "WNUVDT", "WNUBDT (WNUV-DT)", "",
             QString("Default"), QString("Default"),
             "/recordings/1514_20161025235800.ts",
             "localhost", "Default",
             "EP01922936", "EP019229360055", "ttvdb4.py_279121",
             ProgramInfo::kCategoryTVShow, 7, 6056109800,
             MythDate::fromString("2016-10-26 00:00:00"),
             MythDate::fromString("2016-10-26 01:00:00"),
             MythDate::fromString("2016-10-25 23:58:00"),
             MythDate::fromString("2016-10-26 01:02:00"),
             0.95, 2016, 50, 133, QDate(2016,10,26),
             MythDate::fromString("2016-10-26 01:12:34"),
             RecStatus::Unknown, 19890314, // recordid
             kDupsInAll, kDupCheckSubThenDesc,
             20141007, // findId
             FL_IGNORELASTPLAYPOS | FL_WATCHED | FL_BOOKMARK,
             AUD_DOLBY,
             VID_DAMAGED | VID_1080 | VID_HDTV,
             SUB_HARDHEAR,
             "Prime A-1",
             QDateTime());
        m_supergirl23 = ProgramInfo
            (65536+711,
             "Supergirl", "",
             "Welcome to Earth", "",
             "An attack is made on the President as hot-button...",
             2, 3, 23, "syndicatedepisode", "Drama",
             1514, "514", "WNUVDT", "WNUBDT (WNUV-DT)", "",
             "Default", "Default",
             "/recordings/1514_20161024235800.ts",
             "localhost", "Default",
             "EP02185451", "EP021854510025", "ttvdb4.py_295759",
             ProgramInfo::kCategoryTVShow, -1, 6056109670,
             MythDate::fromString("2016-10-25 00:00:00"),
             MythDate::fromString("2016-10-25 01:00:00"),
             MythDate::fromString("2016-10-24 23:58:00"),
             MythDate::fromString("2016-10-25 01:02:00"),
             0.94, 2016, 23, 106, QDate(2016,10,25),
             MythDate::fromString("2016-10-26 01:12:34"),
             RecStatus::Unknown, 19660922, // recordid
             kDupsInAll, kDupCheckSubThenDesc,
             20151026, // findId
             FL_PRESERVED | FL_WATCHED | FL_CUTLIST,
             AUD_VISUALIMPAIR | AUD_STEREO,
             VID_720 | VID_WIDESCREEN,
             SUB_SIGNED,
             "Prime A-0",
             QDateTime());
    }

    void cleanupTestCase()
    {
    }

    /**
     * test for https://code.mythtv.org/trac/ticket/12049
     */
    static void programFromVideo_test(void)
    {
        ProgramInfo program (
            "/pathname",
             "The plot",
             "The Title", "title, the",
             "The Subtitle", "subtitle, the",
             "Director",
             1,
             2,
             "inetref",
             90min,
             1968,
             "MV000"
        );

        QCOMPARE (program.IsVideo(), true);
        QCOMPARE (program.GetPathname(), QString("/pathname"));
        QCOMPARE (program.GetDescription(), QString("The plot"));
        QCOMPARE (program.GetTitle(), QString("The Title"));
        QCOMPARE (program.GetSubtitle(), QString("The Subtitle"));
        /* no getDirector() */
        QCOMPARE (program.GetSeason(), (unsigned int)1);
        QCOMPARE (program.GetEpisode(), (unsigned int)2);
        QCOMPARE (program.GetInetRef(), QString("inetref"));
        QCOMPARE (program.GetSecondsInRecording(), 90 * 60s); // Ubuntu1804 has trouble with '90min'
        QCOMPARE (program.GetYearOfInitialRelease(), (unsigned int)1968);
        QCOMPARE (program.GetProgramID(), QString("MV000"));
    }

    /**
     * test comparing programmes
     */
    static void movieComparison_test(void)
    {
        /* tt0021814 - Dracula (1931) */
        ProgramInfo programA (mockMovie ("128", "tt0021814", "Dracula", 1931));

        /* tt0051554 - Dracula (1958) */
        ProgramInfo programB (mockMovie ("11868", "tt0051554", "Dracula", 1958));

        /* both movies have the same name, but are not the same movie */
        QVERIFY (!programA.IsSameProgram (programB));

//        QSKIP ("tests that still fail");

        /* german theatrical title */
        ProgramInfo programC (mockMovie ("79548", "tt1838544", "Gone", 2012));
        /* german tv title */
        ProgramInfo programD (mockMovie ("79548", "tt1838544", "Gone - Ich muss dich finden", 2012));

        /* both programs are the same movie, but with different titles */
//        QVERIFY (programC.IsSameProgram (programD));

        /* the same movie, identical title, but IDs from different namespaces */
        ProgramInfo programE (mockMovie ("", "domainA/oneid", "Gone", 2012));
        ProgramInfo programF (mockMovie ("", "domainB/anotherid", "Gone", 2012));
        QVERIFY (programE.IsSameProgram (programF));

        /* the same movie, identical title */
        ProgramInfo programG (mockMovie ("", "", "Gone", 2012));
        ProgramInfo programH (mockMovie ("", "", "Gone", 2012));
        QVERIFY (programG.IsSameProgram (programH));
    }

    void programToStringList_test(void)
    {
        QStringList program_list;
        m_dracula.ToStringList(program_list);
        QVERIFY(program_list.size() == NUMPROGRAMLINES);
        QVERIFY(program_list.join('|') == m_draculaList);
        ProgramInfo alucard(program_list);
        QVERIFY(m_dracula == alucard);

        program_list.clear();
        m_flash34.ToStringList(program_list);
        QVERIFY(program_list.size() == NUMPROGRAMLINES);
        QVERIFY(program_list.join('|') == m_flash34List);
        ProgramInfo hsalf34(program_list);
        QVERIFY(m_flash34 == hsalf34);

        program_list.clear();
        m_supergirl23.ToStringList(program_list);
        QVERIFY(program_list.size() == NUMPROGRAMLINES);
        QVERIFY(program_list.join('|') == m_supergirl23List);
        ProgramInfo lrigrepus23(program_list);
        QVERIFY(m_supergirl23 == lrigrepus23);

        // Test accepting an empty string for an invalid QDateTime
        program_list.clear();
        m_supergirl23.ToStringList(program_list);
        QVERIFY(program_list.size() == NUMPROGRAMLINES);
        program_list[51] = "";
        ProgramInfo lrigrepus23b(program_list);
        QVERIFY(m_supergirl23 == lrigrepus23b);

        // Test a failure
        program_list.clear();
        m_supergirl23.ToStringList(program_list);
        program_list[1] = "Welcome to Earth 2";
        ProgramInfo lrigrepus23c(program_list);
        QEXPECT_FAIL("", "Intentionally changed title.", Abort);
        QVERIFY(m_supergirl23 == lrigrepus23c);
    }

    void printList (const QStringList& list)
    {
        Q_UNUSED(list);
#if DEBUG
        std::cerr << qPrintable(list.join('|')) << std::endl;
#endif
    }

    void programSorting_test(void)
    {
        QStringList program_list;
        ProgramInfo programB (mockMovie ("11868", "tt0051554", "Dracula", 1958));
        QString dracula2 = "Dracula||Its a movie.|0|0|0|||4294967295|||||0|"
            "946684800|946690200|4294967295||0|0|0|0|0|4294967295|0|0|0|"
            "946684800|946690200|0|Default|||tt0051554|11868|4294967295|0||"
            "Default|0|0|Default|0|0|0|1958|0|0|1|0||4294967295";
        programB.ToStringList(program_list);
        QVERIFY (program_list.join('|') == dracula2);
        program_list.clear();

        m_flash34.ToStringList(program_list);
        printList(program_list);
        QVERIFY (program_list.join('|') == m_flash34List);
        program_list.clear();

        m_supergirl23.ToStringList(program_list);
        printList(program_list);
        QVERIFY (program_list.join('|') == m_supergirl23List);
        program_list.clear();

        QCOMPARE (m_flash34.GetSortTitle(), QString("flash (2014)"));
        QCOMPARE (m_flash34.GetSortSubtitle(), QString("new rogues"));
        QVERIFY (m_flash34.GetSortTitle() < m_supergirl23.GetSortTitle());
    }

    static void SubstituteMatches_compile_test(void)
    {
        QDateTime startTs    {MythDate::current(true)};
        QDateTime endTs      {startTs};
        QDateTime recStartTs {startTs};
        QDateTime recEndTs   {startTs};

        static const std::array<const QString,4> s_timeStr
            { "STARTTIME", "ENDTIME", "PROGSTART", "PROGEND", };
        static const std::array<const QDateTime *,4> time_dtr
            { &recStartTs, &recEndTs, &startTs, &endTs, };
        QCOMPARE(s_timeStr.size(), (size_t)4);
        QCOMPARE(time_dtr.size(), (size_t)4);
    }

    static void SubstituteMatches_test_data(void)
    {
        QTest::addColumn<QString>("input");
        QTest::addColumn<QString>("expected");

        QTest::newRow("title")    << R"(%TITLE%)" << "The Flash (2014)";
        QTest::newRow("subtitle") << R"(%SUBTITLE%)" << "The New Rogues";
        QTest::newRow("episode")  << R"(S%SEASON%E%EPISODE%)" << "S3E4";
        QTest::newRow("times1utc") << R"(%STARTTIMEUTC% to %ENDTIMEUTC%)"
                                   << "20161025235800 to 20161026010200";
        QTest::newRow("times1isoutc") << R"(%STARTTIMEISOUTC% to %ENDTIMEISOUTC%)"
                                      << "2016-10-25T23:58:00Z to 2016-10-26T01:02:00Z";
        QTest::newRow("times2utc") << R"(%PROGSTARTUTC% to %PROGENDUTC%)"
                                   << "20161026000000 to 20161026010000";
        QTest::newRow("times2isoutc") << R"(%PROGSTARTISOUTC% to %PROGENDISOUTC%)"
                                      << "2016-10-26T00:00:00Z to 2016-10-26T01:00:00Z";
        QTest::newRow("dirfile") << R"(%DIR% %FILE%)"
                                 << "myth://localhost/1514_20161025235800.ts 1514_20161025235800.ts";


    }

    void SubstituteMatches_test(void)
    {
        QFETCH(QString, input);
        QFETCH(QString, expected);

        QString output = input;
        m_flash34.SubstituteMatches(output);
        QCOMPARE(output, expected);
    }

    static void SubstituteMatches2_test_data(void)
    {
        QTest::addColumn<QString>("input");
        QTest::addColumn<QString>("expected");

        QTest::newRow("title")    << R"(%TITLE%)" << "Supergirl";
        QTest::newRow("subtitle") << R"(%SUBTITLE%)" << "Welcome to Earth";
        QTest::newRow("episode")  << R"(S%SEASON%E%EPISODE%)" << "S2E3";
        QTest::newRow("times1utc") << R"(%STARTTIMEUTC% to %ENDTIMEUTC%)"
                                   << "20161024235800 to 20161025010200";
        QTest::newRow("times1isoutc") << R"(%STARTTIMEISOUTC% to %ENDTIMEISOUTC%)"
                                      << "2016-10-24T23:58:00Z to 2016-10-25T01:02:00Z";
        QTest::newRow("times2utc") << R"(%PROGSTARTUTC% to %PROGENDUTC%)"
                                   << "20161025000000 to 20161025010000";
        QTest::newRow("times2isoutc") << R"(%PROGSTARTISOUTC% to %PROGENDISOUTC%)"
                                      << "2016-10-25T00:00:00Z to 2016-10-25T01:00:00Z";
        QTest::newRow("dirfile") << R"(%DIR% %FILE%)"
                                 << "myth://localhost/1514_20161024235800.ts 1514_20161024235800.ts";


    }

    void SubstituteMatches2_test(void)
    {
        QFETCH(QString, input);
        QFETCH(QString, expected);

        QString output = input;
        m_supergirl23.SubstituteMatches(output);
        QCOMPARE(output, expected);
    }

    void checkMap (const InfoMap& actual, InfoMap expected)
    {
        for (auto it = actual.constKeyValueBegin(); it != actual.constKeyValueEnd(); it++)
        {
            auto [key, actualValue] = *it;
            if (expected.contains(key))
            {
                QVERIFY2(actualValue == expected[key],
                         qPrintable(QString("Key '%1': actual '%2' is not expected '%3'")
                                    .arg(key, actualValue, expected[key])));
                expected.remove(key);
            }
            else
            {
                QVERIFY2(actualValue == QString(""),
                         qPrintable(QString("Key '%1': actual '%2' is not empty")
                                    .arg(key, actualValue)));
            }
        }
        QStringList missingKeys = expected.keys();
        QCOMPARE(QString(""), missingKeys.join('|'));
    }

    void printMap (const QString& title, const QString& subtitle, const InfoMap& progMap)
    {
        Q_UNUSED(title);
        Q_UNUSED(subtitle);
        Q_UNUSED(progMap);
#if DEBUG
        std::cerr << qPrintable(QString("progMap for title '%1' subtitle '%2'")
                                .arg(title).arg(subtitle))
                  << std::endl;
        auto keys = progMap.keys();
        keys.sort();
        for (auto key : std::as_const(keys))
        {
            if (progMap[key].size() > 1)
                std::cerr << qPrintable(QString("  %1: string [%2]").arg(key).arg(progMap[key])) << std::endl;
            else if (progMap[key].size() == 1)
                std::cerr << qPrintable(QString("  %1: NUMBER [%2]").arg(key).arg(progMap[key].at(0).unicode())) << std::endl;
            else
                std::cerr << qPrintable(QString("  %1: [<empty>]").arg(key)) << std::endl;
        }
#endif
    }

    void test_toMap (void)
    {
        InfoMap progMap;
        m_flash34.ToMap(progMap, false, 10, MythDate::kOverrideUTC);
        printMap(m_flash34.GetTitle(), m_flash34.GetSubtitle(), progMap);
        checkMap(progMap, m_flash34Map);

        m_supergirl23.ToMap(progMap, false, 10, MythDate::kOverrideUTC);
        printMap(m_supergirl23.GetTitle(), m_supergirl23.GetSubtitle(), progMap);
        checkMap(progMap, m_supergirl23Map);
    }
};
