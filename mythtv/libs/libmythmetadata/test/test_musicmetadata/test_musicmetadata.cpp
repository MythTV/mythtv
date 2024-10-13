/*
 *  Class TestMusicMetadata
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
#include "test_musicmetadata.h"
#include <iostream>

void TestMusicMetadata::initTestCase()
{
    gCoreContext = new MythCoreContext("test_mythmusicdata_1.0", nullptr);
}

void TestMusicMetadata::dump(MusicMetadata *data)
{
    std::cerr << "Artist:                   " << qPrintable(data->Artist())                << std::endl;
    std::cerr << "Artist Sort:              " << qPrintable(data->ArtistSort())            << std::endl;
    std::cerr << "Compilation Artrist:      " << qPrintable(data->CompilationArtist())     << std::endl;
    std::cerr << "Compilation Artrist Sort: " << qPrintable(data->CompilationArtistSort()) << std::endl;
    std::cerr << "Album:                    " << qPrintable(data->Album())                 << std::endl;
    std::cerr << "Album Sort:               " << qPrintable(data->AlbumSort())             << std::endl;
    std::cerr << "Title:                    " << qPrintable(data->Title())                 << std::endl;
    std::cerr << "Title Sort:               " << qPrintable(data->TitleSort())             << std::endl;
    std::cerr << "Format Artist:            " << qPrintable(data->FormatArtist())          << std::endl;
    std::cerr << "Format Title:             " << qPrintable(data->FormatTitle())           << std::endl;
    std::cerr << "Genre:                    " << qPrintable(data->Genre())                 << std::endl;
    std::cerr << "Directory Id:             " << data->getDirectoryId()                    << std::endl;
    std::cerr << "Artist Id:                " << data->getArtistId()                       << std::endl;
    std::cerr << "Compilation Artist Id:    " << data->getCompilationArtistId()            << std::endl;
    std::cerr << "Album Id:                 " << data->getArtistId()                       << std::endl;
    std::cerr << "Genre Id:                 " << data->getGenreId()                        << std::endl;
    std::cerr << "Year:                     " << data->Year()                              << std::endl;
    std::cerr << "Track:                    " << data->Track()                             << std::endl;
    std::cerr << "Track Count:              " << data->GetTrackCount()                     << std::endl;
    std::cerr << "Length:                   " << data->Length().count()                    << std::endl;
    std::cerr << "Disc Number:              " << data->DiscNumber()                        << std::endl;
    std::cerr << "Disc Count:               " << data->DiscCount()                         << std::endl;
    std::cerr << "Play Count:               " << data->PlayCount()                         << std::endl;
    std::cerr << "Id:                       " << data->ID()                                << std::endl;
    std::cerr << "Filename:                 " << qPrintable(data->Filename())              << std::endl;
    std::cerr << "Hostname:                 " << qPrintable(data->Hostname())              << std::endl;
    std::cerr << "File Size:                " << data->FileSize()                          << std::endl;
    std::cerr << "Format:                   " << qPrintable(data->Format())                << std::endl;
    std::cerr << "Rating:                   " << data->Rating()                            << std::endl;
    std::cerr << "LastPlay:                 " << qPrintable(data->LastPlay().toString(Qt::ISODate)) << std::endl;
    std::cerr << "Compilation:              " << (data->Compilation() ? "true" : "false")  << std::endl;
}

void TestMusicMetadata::test_flac(void)
{
    MusicMetadata *data = MetaIO::readMetadata(QStringLiteral(TEST_SOURCE_DIR) +
                                               "/samples/silence.flac");
    QVERIFY(data != nullptr);
//  dump(data);
    QCOMPARE(data->Length(), 5000ms);
    QCOMPARE(data->Title(), QString("The Silence, #99"));
    QCOMPARE(data->TitleSort(), QString("silence, #99"));
    QCOMPARE(data->Artist(), QString("Mother Nature"));
    QCOMPARE(data->ArtistSort(), QString("mother nature"));
    QCOMPARE(data->Track(), 123);
    QCOMPARE(data->GetTrackCount(), 999999999);
    QCOMPARE(data->Year(), 2021);
}

void TestMusicMetadata::test_ogg(void)
{
    MusicMetadata *data = MetaIO::readMetadata(QStringLiteral(TEST_SOURCE_DIR) +
                                               "/samples/silence.ogg");
    QVERIFY(data != nullptr);
//  dump(data);
    QCOMPARE(data->Length(), 5000ms);
    QCOMPARE(data->Title(), QString("The Silence, #99"));
    QCOMPARE(data->TitleSort(), QString("silence, #99"));
    QCOMPARE(data->Artist(), QString("Mother Nature"));
    QCOMPARE(data->ArtistSort(), QString("mother nature"));
    QCOMPARE(data->Track(), 123);
    // metaiooggvorbis doesn't read track count
    // QCOMPARE(data->GetTrackCount(), 999999999);
    QCOMPARE(data->Year(), 2021);
}

void TestMusicMetadata::test_mp4(void)
{
    MusicMetadata *data = MetaIO::readMetadata(QStringLiteral(TEST_SOURCE_DIR) +
                                               "/samples/silence.m4a");
    QVERIFY(data != nullptr);
//  dump(data);
    QCOMPARE(data->Length(), 3140ms);
    QCOMPARE(data->Title(), QString("The Silence, #99"));
    QCOMPARE(data->TitleSort(), QString("silence, #99"));
    QCOMPARE(data->Artist(), QString("Mother Nature"));
    QCOMPARE(data->ArtistSort(), QString("mother nature"));

    // metaiomp4 doesn't seem to read track counts
    // QCOMPARE(data->Track(), 123);
    // QCOMPARE(data->GetTrackCount(), 999999999);

    // metaiomp4 doesn't seem to read year
    // QCOMPARE(data->Year(), 2021);
}

void TestMusicMetadata::test_mp3(void)
{
    MusicMetadata *data = MetaIO::readMetadata(QStringLiteral(TEST_SOURCE_DIR) +
                                               "/samples/silence.mp3");
    QVERIFY(data != nullptr);
//  dump(data);
    QCOMPARE(data->Length(), 5042ms);
    QCOMPARE(data->Title(), QString("The Silence, #99"));
    QCOMPARE(data->TitleSort(), QString("silence, #99"));
    QCOMPARE(data->Artist(), QString("Mother Nature"));
    QCOMPARE(data->ArtistSort(), QString("mother nature"));
    QCOMPARE(data->Track(), 123);
    QCOMPARE(data->GetTrackCount(), 999999999);
    QCOMPARE(data->Year(), 2021);
}

void TestMusicMetadata::test_wv(void)
{
    MusicMetadata *data = MetaIO::readMetadata(QStringLiteral(TEST_SOURCE_DIR) +
                                               "/samples/silence.wv");
    QVERIFY(data != nullptr);
//  dump(data);
    QCOMPARE(data->Length(), 3142ms);
    QCOMPARE(data->Title(), QString("The Silence, #99"));
    QCOMPARE(data->TitleSort(), QString("silence, #99"));
    QCOMPARE(data->Artist(), QString("Mother Nature"));
    QCOMPARE(data->ArtistSort(), QString("mother nature"));
    QCOMPARE(data->Track(), 123);
    // metaiowavpack doesn't read track count
    // QCOMPARE(data->GetTrackCount(), 999999999);
    QCOMPARE(data->Year(), 2021);
}

// Exercises metaioavfcomment.cpp
void TestMusicMetadata::test_aiff(void)
{
    MusicMetadata *data = MetaIO::readMetadata(QStringLiteral(TEST_SOURCE_DIR) +
                                               "/samples/silence.aiff");
    QVERIFY(data != nullptr);
//  dump(data);
    QCOMPARE(data->Length(), 2000ms);
    QCOMPARE(data->Title(), QString("The Silence, #99"));
    QCOMPARE(data->TitleSort(), QString("silence, #99"));
}

void TestMusicMetadata::cleanupTestCase()
{
}

QTEST_APPLESS_MAIN(TestMusicMetadata)
