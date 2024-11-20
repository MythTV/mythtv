/*
 *  Class Testvideometadata
 *
 *  Copyright (C) Martin Thwaites 2013
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

#include "libmythbase/programinfo.h"
#include "libmythmetadata/metadatafactory.h"
#include "libmythmetadata/videometadata.h"
#include "libmythtv/recordinginfo.h"

class Testvideometadata: public QObject
{
    Q_OBJECT

  private slots:

    static void NonTVFilenameNoSubtitle(void)
    {
        // With Spaces as separator
        TestMetadata(QString("A Movie Title.mpg"),
                     QString("A Movie Title"),
                     QString(""),
                     0,
                     0);
    }
    static void NonTVFilenameNoSubtitleDotSeparator(void)
    {
        // With Dots as separator
        TestMetadata(QString("A.Movie.Title.mpg"),
                     QString("A Movie Title"),
                     QString(""),
                     0,
                     0);
    }
    static void NonTVFilenameWithYear(void)
    {
        // With Spaces as separator
        TestMetadata(QString("A Movie Title 1984.mpg"),
                     QString("A Movie Title 1984"),
                     QString(""),
                     0,
                     0);
    }
    static void NonTVFilenameWithYearInBrackets(void)
    {
        // With Spaces as separator
        TestMetadata(QString("A Movie Title (1984).mpg"),
                     QString("A Movie Title (1984)"),
                     QString(""),
                     0,
                     0);
    }
    static void TVFilenameSESyntaxLower(void)
    {
        TestMetadata(QString("Title s01e02 subtitle.mpg"),
                     QString("Title"),
                     QString("subtitle"),
                     1,
                     2);
    }
    static void TVFilenameSESyntaxLowerDotSeparator(void)
    {
        TestMetadata(QString("Title.s01e02.subtitle.mpg"),
                     QString("Title"),
                     QString("subtitle"),
                     1,
                     2);
    }
    static void TVFilenameSESyntaxUpper(void)
    {
        TestMetadata(QString("Title S01E02 subtitle.mpg"),
                     QString("Title"),
                     QString("subtitle"),
                     1,
                     2);
    }
    static void TVFilenameXSyntax(void)
    {
        TestMetadata(QString("Title 1x2 subtitle.mpg"),
                     QString("Title"),
                     QString("subtitle"),
                     1,
                     2);
    }
    static void TVFilenameXSyntaxUpper(void)
    {
        TestMetadata(QString("Title 1X2 subtitle.mpg"),
                     QString("Title"),
                     QString("subtitle"),
                     1,
                     2);
    }

    static void TVFilenameXSyntaxUpperDoubleZeroPadded(void)
    {
        TestMetadata(QString("Title 001X002 subtitle.mpg"),
                     QString("Title"),
                     QString("subtitle"),
                     1,
                     2);
    }
    static void TVFilenameSeasonEpisodeSyntax(void)
    {
        TestMetadata(QString("Title Season 1 Episode 2 subtitle.mpg"),
                     QString("Title"),
                     QString("subtitle"),
                     1,
                     2);
    }
    static void TVFilenameSeasonEpisodeSyntaxUpper(void)
    {
        TestMetadata(QString("Title SEASON 1 EPISODE 2 subtitle.mpg"),
                     QString("Title"),
                     QString("subtitle"),
                     1,
                     2);
    }

    static void TVFilenameSeasonEpisodeNoSpaceSyntaxUpper(void)
    {
        TestMetadata(QString("Title SEASON1EPISODE2 subtitle.mpg"),
                     QString("Title"),
                     QString("subtitle"),
                     1,
                     2);
    }

    static void TVFullPath(void)
    {
        TestMetadata(QString("Title/Season 1/02 Subtitle.mpg"),
                     QString("Title"),
                     QString("Subtitle"),
                     1,
                     2);
    }

    static void TVFullPathSESyntax(void)
    {
        TestMetadata(QString("Title/Season 2/S02E03 Subtitle.mpg"),
                     QString("Title"),
                     QString("Subtitle"),
                     2,
                     3);
    }

    static void TVFullPathXSyntax(void)
    {
        TestMetadata(QString("Title/Season 2/2x03 Subtitle.mpg"),
                     QString("Title"),
                     QString("Subtitle"),
                     2,
                     3);
    }
    static void TVFullPathXSyntaxNoSubtitle(void)
    {
        TestMetadata(QString("Title/Season 2/2x03.mpg"),
                     QString("Title"),
                     QString(""),
                     2,
                     3);
    }

    static void TVFullPathSeasonEpisodeSyntax(void)
    {
        TestMetadata(QString("Title/Season 1/Season 2 episode 3.mpg"),
                     QString("Title"),
                     QString(""),
                     2,
                     3);
    }

    static void TVFullPathWithSeasonXSyntax(void)
    {
        TestMetadata(QString("Title Season 2/2x03.mpg"),
                     QString("Title"),
                     QString(""),
                     2,
                     3);
    }

    static void TVFullPathWithSeasonAndTitleXSyntax(void)
    {
        TestMetadata(QString("Title Season 2/Title Overide 2x03.mpg"),
                     QString("Title Overide"),
                     QString(""),
                     2,
                     3);
    }

    static void MovieWithMinus ()
    {
        QSKIP ("Minus is handled between parts of the title, but not as part of the title itself.");
        TestMetadata (QString ("A-Movie-Title.ts"),
                      QString ("A Movie Title"),
                      QString (""),
                      0,
                      0);
        TestMetadata (QString ("A-Movie-Title-2000.ts"),
                      QString ("A Movie Title 2000"),
                      QString (""),
                      0,
                      0);
        TestMetadata (QString ("Titan-A.E..ts"),
                      QString ("Titan A E"),
                      QString (""),
                      0,
                      0);
        TestMetadata (QString ("I-do-&-I-don't.ts"),
                      QString ("I do & I don't"),
                      QString (""),
                      0,
                      0);
    }

    static void MovieWithUnderscore ()
    {
        TestMetadata (QString ("A_Movie_Title.ts"),
                      QString ("A Movie Title"),
                      QString (""),
                      0,
                      0);
        TestMetadata (QString ("A_Movie_Title_2000.ts"),
                      QString ("A Movie Title 2000"),
                      QString (""),
                      0,
                      0);
        TestMetadata (QString ("Titan_A.E..ts"),
                      QString ("Titan A E"),
                      QString (""),
                      0,
                      0);
        TestMetadata (QString ("I_do_&_I_don't.ts"),
                      QString ("I do & I don't"),
                      QString (""),
                      0,
                      0);
    }

    static void MovieWithPeriod ()
    {
        TestMetadata (QString ("A.Movie.Title.ts"),
                      QString ("A Movie Title"),
                      QString (""),
                      0,
                      0);
        TestMetadata (QString ("A.Movie.Title.2000.ts"),
                      QString ("A Movie Title 2000"),
                      QString (""),
                      0,
                      0);
        TestMetadata (QString ("Titan.A.E..ts"),
                      QString ("Titan A E"),
                      QString (""),
                      0,
                      0);
        TestMetadata (QString ("I.do.&.I.don't.ts"),
                      QString ("I do & I don't"),
                      QString (""),
                      0,
                      0);
        TestMetadata (QString ("A.Movie.Title.2000.ts.orig"),
                      QString ("A Movie Title 2000 ts"),
                      QString (""),
                      0,
                      0);
    }

    static void MovieWithAMix ()
    {
        TestMetadata (QString ("A_Movie.Title.ts"),
                      QString ("A Movie Title"),
                      QString (""),
                      0,
                      0);
        TestMetadata (QString ("A.Movie_Title: 2000.ts"),
                      QString ("A Movie Title: 2000"),
                      QString (""),
                      0,
                      0);
        TestMetadata (QString ("Titan  A.E. .ts"),
                      QString ("Titan  A E"),
                      QString (""),
                      0,
                      0);
        TestMetadata (QString ("I_do_-_I_don't.ts"),
                      QString ("I do - I don't"),
                      QString (""),
                      0,
                      0);
    }

    static void SeriesWithAMix ()
    {
        TestMetadata (QString ("Series Title/Season 1/Season Title 02x03 Episode Title.mp4"),
                      QString ("Season Title"),
                      QString ("Episode Title"),
                      2,
                      3);
        TestMetadata (QString ("Series.Title/Season.1/Season.Title_-_02x03_-_Episode_Title.mp4"),
                      QString ("Season Title"),
                      QString ("Episode Title"),
                      2,
                      3);
        TestMetadata (QString ("Series.Title/Season.1/Season%20Title_-_02x03_-_Episode%20Title.mp4"),
                      QString ("Season Title"),
                      QString ("Episode Title"),
                      2,
                      3);
    }

    static void TestMetadata(const QString &filename,
                             const QString &expectedTitle,
                             const QString &expectedSubtitle,
                             int expectedSeason,
                             int expectedEpisode){
        QString title = VideoMetadata::FilenameToMeta(filename, 1);
        int season = VideoMetadata::FilenameToMeta(filename, 2).toInt();
        int episode = VideoMetadata::FilenameToMeta(filename, 3).toInt();
        QString subtitle = VideoMetadata::FilenameToMeta(filename, 4);
        QCOMPARE(title, expectedTitle);
        QCOMPARE(subtitle, expectedSubtitle);
        QCOMPARE(season, expectedSeason);
        QCOMPARE(episode, expectedEpisode);
    }

    /* TODO move into own test folder */
    static void ProgramWithInetref ()
    {
        QSKIP ("Might connect to the database or call the installed metadata grabbers.");
        ProgramInfo proginfo = ProgramInfo ("", "", "Test Movie", "", "", "",
                                            "", 0, 0, "tmdb3.py_1234", 0min, 0, "");
        RecordingInfo recinfo(proginfo);
        QCOMPARE (recinfo.GetInetRef(), QString("tmdb3.py_1234"));
        QCOMPARE (GuessLookupType (&recinfo), kProbableMovie);

        proginfo = ProgramInfo ("", "", "Test Series", "", "Test Episode", "",
                                "", 1, 15, "ttvdb4.py_1234", 0min, 0, "");
        recinfo = proginfo;
        QCOMPARE (recinfo.GetInetRef(), QString("ttvdb4.py_1234"));
        QCOMPARE (GuessLookupType (&recinfo), kProbableTelevision);

        //QCOMPARE (GuessLookupType (QString ("tmdb3.py_1234")), kProbableMovie);
        //QCOMPARE (GuessLookupType (QString ("ttvdb4.py_1234")), kProbableTelevision);
    }

    static void testEmbeddedFilnameToMetadata ()
    {
        QSKIP ("Tries to connect to the database.");
        VideoMetadata obj = VideoMetadata(QString ("Series Title/Season 1/02x03 Episode Title.mp4"));
        QCOMPARE (obj.GetTitle(), QString ("Series Title"));
        QCOMPARE (obj.GetSortTitle(), QString ("series title"));
        QCOMPARE (obj.GetSubtitle(), QString ("Episode Title"));
        QCOMPARE (obj.GetSortSubtitle(), QString ("episode title"));
        QCOMPARE (obj.GetSeason(), 2);
        QCOMPARE (obj.GetEpisode(), 3);

        obj = VideoMetadata(QString ("The Series Title/Season 1/02x03 The Episode Title.mp4"));
        QCOMPARE (obj.GetTitle(), QString ("The Series Title"));
        QCOMPARE (obj.GetSortTitle(), QString ("series title"));
        QCOMPARE (obj.GetSubtitle(), QString ("The Episode Title"));
        QCOMPARE (obj.GetSortSubtitle(), QString ("episode title"));
        QCOMPARE (obj.GetSeason(), 2);
        QCOMPARE (obj.GetEpisode(), 3);
    }
};
