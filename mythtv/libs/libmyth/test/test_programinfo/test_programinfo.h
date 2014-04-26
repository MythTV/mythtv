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

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#define MSKIP(MSG) QSKIP(MSG, SkipSingle)
#else
#define MSKIP(MSG) QSKIP(MSG)
#endif

class TestProgramInfo : public QObject
{
    Q_OBJECT

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
};
