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

#include "mythcorecontext.h"
#include "programinfo.h"
#include "programtypes.h"

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#define MSKIP(MSG) QSKIP(MSG, SkipSingle)
#else
#define MSKIP(MSG) QSKIP(MSG)
#endif

class TestProgramInfo : public QObject
{
    Q_OBJECT
  private:
    ProgramInfo mockMovie (QString const &inetref, QString const &programid, QString const &title, unsigned int year)
    {
        return ProgramInfo (
            title,          /* title */
            "",              /* subtitle */
            "Its a movie.", /* description */
            "", /* syndicated episode */
            "", /* category */
            (uint) -1, /* chanid */
            "", /* channum */
            "", /* chansign */
            "", /* channame */
            "", /* chan playback filters */
            MythDate::fromString ("2000-01-01 00:00:00"), /* start ts */
            MythDate::fromString ("2000-01-01 01:30:00"), /* end ts */
            MythDate::fromString ("2000-01-01 00:00:00"), /* rec start ts */
            MythDate::fromString ("2000-01-01 01:30:00"), /* rec end ts */
            "", /* series id */
            programid, /* program id */
            ProgramInfo::kCategoryMovie, /* cat type */
            0.0f, /* stars */
            year, /* year */
            (uint) 0, /* part number */
            (uint) 0, /* part total */
            QDate(), /* original air date */
            rsUnknown, /* rec status */
            (uint) -1, /* record id */
            kNotRecording, /* rec type */
            (uint) -1, /* find id */
            true, /* comm free */
            false, /* repeat */
            (uint) 0, /* video props */
            (uint) 0, /* audio props */
            (uint) 0, /* subtitle type */
            (uint) 0, /* season */
            (uint) 0, /* episode */
            (uint) 0, /* total episodes */
            ProgramList() /* sched List */
        );
    }

  private slots:
    /**
     * test for https://code.mythtv.org/trac/ticket/12049
     */
    void programFromVideo_test(void)
    {
        ProgramInfo program (
            "/pathname",
             "The plot",
             "The Title",
             "The Subtitle",
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

//        MSKIP ("tests that still fail");

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
};
