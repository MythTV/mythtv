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

#include <QtTest/QtTest>

#include "programinfo.h"
#include "programtypes.h"

class TestProgramInfo : public QObject
{
    Q_OBJECT
  private:
    ProgramInfo mockMovie (QString const &inetref, QString const &programid, QString const &title, unsigned int year)
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

            (uint) -1, /* chanid */
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

            (uint) -1, /* record id */

            RecordingDupInType::kDupsUnset, /* dupin */
            RecordingDupMethodType::kDupCheckUnset, /* dupmethod */

            (uint) -1, /* find id */

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
        "1477439880|1477443720|0|localhost|0|0|0|0|0|0|0|15|8|1477439880|"
        "1477443720|0|Default||EP01922936|EP019229360055|ttvdb.py_279121|"
        "1477444354|0|2016-10-25|Default|0|0|Default|0|0|0|2016|0|0|4|715|"
        "Prime A-1|4294967295";
    QString m_supergirl23List = "Supergirl|Welcome to Earth|An attack is made "
        "on the President as hot-button...|2|3|23|syndicatedepisode|Drama|"
        "1514|514|WNUVDT|WNUBDT (WNUV-DT)|/recordings/1514_20161024235800.ts|"
        "6056109670|1477353480|1477357320|0|localhost|0|0|0|0|0|0|0|15|8|"
        "1477353480|1477357320|0|Default||EP02185451|EP021854510025|"
        "ttvdb.py_295759|1477444354|0|2016-10-24|Default|0|0|Default|0|0|0|"
        "2016|0|0|4|711|Prime A-0|4294967295";
    ProgramInfo m_dracula;
    ProgramInfo m_flash34;
    ProgramInfo m_supergirl23;

  private slots:
    void initTestCase()
    {
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
             "EP01922936", "EP019229360055", "ttvdb.py_279121",
             ProgramInfo::kCategoryTVShow, 0, 6056109800,
             MythDate::fromString("2016-10-25 23:58:00"),
             MythDate::fromString("2016-10-26 01:02:00"),
             MythDate::fromString("2016-10-25 23:58:00"),
             MythDate::fromString("2016-10-26 01:02:00"),
             0.0, 2016, 0, 0, QDate(2016,10,25),
             MythDate::fromString("2016-10-26 01:12:34"),
             RecStatus::Unknown, 0,
             kDupsInAll, kDupCheckSubThenDesc,
             0, 0, 0, 0, 0, "Prime A-1",
             QDateTime());
        m_supergirl23 = ProgramInfo
            (711,
             "Supergirl", "",
             "Welcome to Earth", "",
             "An attack is made on the President as hot-button...",
             2, 3, 23, "syndicatedepisode", "Drama",
             1514, "514", "WNUVDT", "WNUBDT (WNUV-DT)", "",
             "Default", "Default",
             "/recordings/1514_20161024235800.ts",
             "localhost", "Default",
             "EP02185451", "EP021854510025", "ttvdb.py_295759",
             ProgramInfo::kCategoryTVShow, 0, 6056109670,
             MythDate::fromString("2016-10-24 23:58:00"),
             MythDate::fromString("2016-10-25 01:02:00"),
             MythDate::fromString("2016-10-24 23:58:00"),
             MythDate::fromString("2016-10-25 01:02:00"),
             0.0, 2016, 0, 0, QDate(2016,10,24),
             MythDate::fromString("2016-10-26 01:12:34"),
             RecStatus::Unknown, 0,
             kDupsInAll, kDupCheckSubThenDesc,
             0, 0, 0, 0, 0, "Prime A-0",
             QDateTime());
    }

    void cleanupTestCase()
    {
    }

    /**
     * test for https://code.mythtv.org/trac/ticket/12049
     */
    void programFromVideo_test(void)
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
             90,
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
        QCOMPARE (program.GetSecondsInRecording(), (unsigned int)90*60);
        QCOMPARE (program.GetYearOfInitialRelease(), (unsigned int)1968);
        QCOMPARE (program.GetProgramID(), QString("MV000"));
    }

    /**
     * test comparing programmes
     */
    void movieComparison_test(void)
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

#if QT_VERSION >= QT_VERSION_CHECK(5,8,0)
        // Test accepting an empty string for an invalid QDateTime
        program_list.clear();
        m_supergirl23.ToStringList(program_list);
        QVERIFY(program_list.size() == NUMPROGRAMLINES);
        program_list[51] = "";
        ProgramInfo lrigrepus23b(program_list);
        QVERIFY(m_supergirl23 == lrigrepus23b);
#endif

        // Test a failure
        program_list.clear();
        m_supergirl23.ToStringList(program_list);
        program_list[1] = "Welcome to Earth 2";
        ProgramInfo lrigrepus23c(program_list);
        QEXPECT_FAIL("", "Intentionally changed title.", Abort);
        QVERIFY(m_supergirl23 == lrigrepus23c);
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
        QVERIFY (program_list.join('|') == m_flash34List);
        program_list.clear();

        m_supergirl23.ToStringList(program_list);
        QVERIFY (program_list.join('|') == m_supergirl23List);
        program_list.clear();

        QCOMPARE (m_flash34.GetSortTitle(), QString("flash (2014)"));
        QCOMPARE (m_flash34.GetSortSubtitle(), QString("new rogues"));
        QVERIFY (m_flash34.GetSortTitle() < m_supergirl23.GetSortTitle());
    }
};
