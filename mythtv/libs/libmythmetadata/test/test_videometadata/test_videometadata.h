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

#include <QtTest/QtTest>

#include <videometadata.h>

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#define MSKIP(MSG) QSKIP(MSG, SkipSingle)
#else
#define MSKIP(MSG) QSKIP(MSG)
#endif

class Testvideometadata: public QObject
{
    Q_OBJECT

  private slots:

    void NonTVFilenameNoSubtitle(void)
    {
        // With Spaces as separator
        TestMetadata(QString("A Movie Title.mpg"),
                     QString("A Movie Title"),
                     QString(""),
                     0,
                     0);
    }
    void NonTVFilenameNoSubtitleDotSeparator(void)
    {
        // With Dots as separator
        TestMetadata(QString("A.Movie.Title.mpg"),
                     QString("A Movie Title"),
                     QString(""),
                     0,
                     0);
    }
    void NonTVFilenameWithYear(void)
    {
        // With Spaces as separator
        TestMetadata(QString("A Movie Title 1984.mpg"),
                     QString("A Movie Title 1984"),
                     QString(""),
                     0,
                     0);
    }
    void NonTVFilenameWithYearInBrackets(void)
    {
        // With Spaces as separator
        TestMetadata(QString("A Movie Title (1984).mpg"),
                     QString("A Movie Title"),
                     QString(""),
                     0,
                     0);
    }
    void TVFilenameSESyntaxLower(void)
    {
        TestMetadata(QString("Title s01e02 subtitle.mpg"),
                     QString("Title"),
                     QString("subtitle"),
                     1,
                     2);
    }
    void TVFilenameSESyntaxLowerDotSeparator(void)
    {
        TestMetadata(QString("Title.s01e02.subtitle.mpg"),
                     QString("Title"),
                     QString("subtitle"),
                     1,
                     2);
    }
    void TVFilenameSESyntaxUpper(void)
    {
        TestMetadata(QString("Title S01E02 subtitle.mpg"),
                     QString("Title"),
                     QString("subtitle"),
                     1,
                     2);
    }
    void TVFilenameXSyntax(void)
    {
        TestMetadata(QString("Title 1x2 subtitle.mpg"),
                     QString("Title"),
                     QString("subtitle"),
                     1,
                     2);
    }
    void TVFilenameXSyntaxUpper(void)
    {
        TestMetadata(QString("Title 1X2 subtitle.mpg"),
                     QString("Title"),
                     QString("subtitle"),
                     1,
                     2);
    }

    void TVFilenameXSyntaxUpperDoubleZeroPadded(void)
    {
        TestMetadata(QString("Title 001X002 subtitle.mpg"),
                     QString("Title"),
                     QString("subtitle"),
                     1,
                     2);
    }
    void TVFilenameSeasonEpisodeSyntax(void)
    {
        TestMetadata(QString("Title Season 1 Episode 2 subtitle.mpg"),
                     QString("Title"),
                     QString("subtitle"),
                     1,
                     2);
    }
    void TVFilenameSeasonEpisodeSyntaxUpper(void)
    {
        TestMetadata(QString("Title SEASON 1 EPISODE 2 subtitle.mpg"),
                     QString("Title"),
                     QString("subtitle"),
                     1,
                     2);
    }

    void TVFilenameSeasonEpisodeNoSpaceSyntaxUpper(void)
    {
        TestMetadata(QString("Title SEASON1EPISODE2 subtitle.mpg"),
                     QString("Title"),
                     QString("subtitle"),
                     1,
                     2);
    }

    void TVFullPath(void)
    {
        TestMetadata(QString("Title/Season 1/02 Subtitle.mpg"),
                     QString("Title"),
                     QString("Subtitle"),
                     1,
                     2);
    }

    void TVFullPathSESyntax(void)
    {
        TestMetadata(QString("Title/Season 2/S02E03 Subtitle.mpg"),
                     QString("Title"),
                     QString("Subtitle"),
                     2,
                     3);
    }

    void TVFullPathXSyntax(void)
    {
        TestMetadata(QString("Title/Season 2/2x03 Subtitle.mpg"),
                     QString("Title"),
                     QString("Subtitle"),
                     2,
                     3);
    }
    void TVFullPathXSyntaxNoSubtitle(void)
    {
        TestMetadata(QString("Title/Season 2/2x03.mpg"),
                     QString("Title"),
                     QString(""),
                     2,
                     3);
    }

    void TVFullPathSeasonEpisodeSyntax(void)
    {
        TestMetadata(QString("Title/Season 1/Season 2 episode 3.mpg"),
                     QString("Title"),
                     QString(""),
                     2,
                     3);
    }

    void TVFullPathWithSeasonXSyntax(void)
    {
        TestMetadata(QString("Title Season 2/2x03.mpg"),
                     QString("Title"),
                     QString(""),
                     2,
                     3);
    }

    void TVFullPathWithSeasonAndTitleXSyntax(void)
    {
        TestMetadata(QString("Title Season 2/Title Overide 2x03.mpg"),
                     QString("Title Overide"),
                     QString(""),
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
};
