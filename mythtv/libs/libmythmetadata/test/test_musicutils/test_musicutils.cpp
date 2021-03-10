/*
 *  Class TestMusicUtils
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

#include "test_musicutils.h"

void TestMusicUtils::initTestCase()
{
    gCoreContext = new MythCoreContext("test_mythmusicutils_1.0", nullptr);

    QDir::setCurrent("libmythmetadata/test/test_musicutils");
}

void TestMusicUtils::dump(void)
{
}

void TestMusicUtils::test_filenames(void)
{
    QCOMPARE(fixFilename    ( R"(a/b\c:d'e"f?g|h)" ), QString("a_b_c_d_e_f_g_h"));
    QCOMPARE(fixFileToken_sl( R"(a/b\c:d'e"f?g|h)" ), QString("a/b_c_d_e_f_g_h"));
}

void TestMusicUtils::test_nameFromMetadata_data(void)
{
    QTest::addColumn<QString>("genre");
    QTest::addColumn<QString>("artist");
    QTest::addColumn<QString>("album");
    QTest::addColumn<QString>("title");
    QTest::addColumn<int>("tracknum");
    QTest::addColumn<int>("year");
    QTest::addColumn<QString>("filenameTemplate");
    QTest::addColumn<bool>("noWhitespace");
    QTest::addColumn<QString>("e_filename");

    QTest::newRow("nada")     << "Jazz" << "The Rippingtons" << "20th Anniversary" << "Celebrate" << 2 << 2006
                              << "Everything should have the same name" << true
                              << "2 - Celebrate";
    QTest::newRow("ws")       << "Jazz" << "The Rippingtons" << "20th Anniversary" << "Celebrate" << 2 << 2006
                              << "Filename is GENRE ARTIST ALBUM TRACK TITLE YEAR" << false
                              << "Filename is Jazz The Rippingtons 20th Anniversary 02 Celebrate 2006";
    QTest::newRow("nows")     << "Jazz" << "The Rippingtons" << "20th Anniversary" << "Celebrate" << 2 << 2006
                              << "Filename is GENRE ARTIST ALBUM TRACK TITLE YEAR" << true
                              << "Filename_is_Jazz_The_Rippingtons_20th_Anniversary_02_Celebrate_2006";
    QTest::newRow("nogenre")  << "" << "The Rippingtons" << "20th Anniversary" << "Celebrate" << 123 << 2006
                              << "Filename_is_GENRE_ARTIST_ALBUM_TRACK_TITLE_YEAR" << false
                              << "Filename_is_Unknown Genre_The Rippingtons_20th Anniversary_123_Celebrate_2006";
    QTest::newRow("noartist") << "Jazz" << "" << "20th Anniversary" << "Celebrate" << 12 << 2006
                              << "Filename_is_GENRE_ARTIST_ALBUM_TRACK_TITLE_YEAR" << false
                              << "Filename_is_Jazz_Unknown Artist_20th Anniversary_12_Celebrate_2006";
    QTest::newRow("noalbum")  << "Jazz" << "The Rippingtons" << "" << "Celebrate" << 2 << 2006
                              << "Filename_is_GENRE_ARTIST_ALBUM_TRACK_TITLE_YEAR" << false
                              << "Filename_is_Jazz_The Rippingtons_Unknown Album_02_Celebrate_2006";
    QTest::newRow("notitle")  << "Jazz" << "The Rippingtons" << "20th Anniversary" << "" << 2 << 2006
                              << "Filename_is_GENRE_ARTIST_ALBUM_TRACK_TITLE_YEAR" << false
                              << "Filename_is_Jazz_The Rippingtons_20th Anniversary_02__2006";
    QTest::newRow("notrack")  << "Jazz" << "The Rippingtons" << "20th Anniversary" << "Celebrate" << 0 << 2006
                              << "Filename_is_GENRE_ARTIST_ALBUM_TRACK_TITLE_YEAR" << false
                              << "Filename_is_Jazz_The Rippingtons_20th Anniversary_00_Celebrate_2006";
    QTest::newRow("noyear")   << "Jazz" << "The Rippingtons" << "20th Anniversary" << "Celebrate" << 2 << -1
                              << "Filename_is_GENRE_ARTIST_ALBUM_TRACK_TITLE_YEAR" << false
                              << "Filename_is_Jazz_The Rippingtons_20th Anniversary_02_Celebrate_";
    QTest::newRow("null")     << "Jazz" << "The Rippingtons" << "20th Anniversary" << "Celebrate" << 2 << -1
                              << "" << false
                              << "2 - Celebrate";
    QTest::newRow("stuff")    << "Jazz" << "The Rippingtons" << "20th Anniversary" << "Celebrate" << 2 << -1
                              << "Filename_is_genre_GENRE_artist_ARTIST_ALBUM_TRACK_TITLE_YEAR" << false
                              << "Filename_is_genre_Jazz_artist_The Rippingtons_20th Anniversary_02_Celebrate_";
}

void TestMusicUtils::test_nameFromMetadata(void)
{
    QFETCH(QString, genre);
    QFETCH(QString, artist);
    QFETCH(QString, album);
    QFETCH(QString, title);
    QFETCH(int,     tracknum);
    QFETCH(int,     year);
    QFETCH(QString, filenameTemplate);
    QFETCH(bool,    noWhitespace);
    QFETCH(QString, e_filename);

    QMap<QString,QString> sOverrides;
    sOverrides["FilenameTemplate"] = filenameTemplate;
    gCoreContext->setTestStringSettings(sOverrides);
    QMap<QString,int> iOverrides;
    iOverrides["NoWhitespace"] = static_cast<int>(noWhitespace);
    gCoreContext->setTestIntSettings(iOverrides);

    auto* track = new MusicMetadata("", artist, "", album, title,
                                    genre, year, tracknum);
    
    QString actual = filenameFromMetadata(track);

    QCOMPARE(actual, e_filename);
}

void TestMusicUtils::cleanupTestCase()
{
}

QTEST_APPLESS_MAIN(TestMusicUtils)
