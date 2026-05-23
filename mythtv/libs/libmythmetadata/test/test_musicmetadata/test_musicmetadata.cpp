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

#include "libmythbase/mythcorecontext.h"

void TestMusicMetadata::initTestCase()
{
    gCoreContext = new MythCoreContext("test_mythmusicdata_1.0", nullptr);
}

void TestMusicMetadata::dump(MusicMetadata *data)
{
    std::cerr << "Artist:                   " << qPrintable(data->Artist())                << '\n';
    std::cerr << "Artist Sort:              " << qPrintable(data->ArtistSort())            << '\n';
    std::cerr << "Compilation Artrist:      " << qPrintable(data->CompilationArtist())     << '\n';
    std::cerr << "Compilation Artrist Sort: " << qPrintable(data->CompilationArtistSort()) << '\n';
    std::cerr << "Album:                    " << qPrintable(data->Album())                 << '\n';
    std::cerr << "Album Sort:               " << qPrintable(data->AlbumSort())             << '\n';
    std::cerr << "Title:                    " << qPrintable(data->Title())                 << '\n';
    std::cerr << "Title Sort:               " << qPrintable(data->TitleSort())             << '\n';
    std::cerr << "Format Artist:            " << qPrintable(data->FormatArtist())          << '\n';
    std::cerr << "Format Title:             " << qPrintable(data->FormatTitle())           << '\n';
    std::cerr << "Genre:                    " << qPrintable(data->Genre())                 << '\n';
    std::cerr << "Directory Id:             " << data->getDirectoryId()                    << '\n';
    std::cerr << "Artist Id:                " << data->getArtistId()                       << '\n';
    std::cerr << "Compilation Artist Id:    " << data->getCompilationArtistId()            << '\n';
    std::cerr << "Album Id:                 " << data->getArtistId()                       << '\n';
    std::cerr << "Genre Id:                 " << data->getGenreId()                        << '\n';
    std::cerr << "Year:                     " << data->Year()                              << '\n';
    std::cerr << "Track:                    " << data->Track()                             << '\n';
    std::cerr << "Track Count:              " << data->GetTrackCount()                     << '\n';
    std::cerr << "Length:                   " << data->Length().count()                    << '\n';
    std::cerr << "Disc Number:              " << data->DiscNumber()                        << '\n';
    std::cerr << "Disc Count:               " << data->DiscCount()                         << '\n';
    std::cerr << "Play Count:               " << data->PlayCount()                         << '\n';
    std::cerr << "Id:                       " << data->ID()                                << '\n';
    std::cerr << "Filename:                 " << qPrintable(data->Filename())              << '\n';
    std::cerr << "Hostname:                 " << qPrintable(data->Hostname())              << '\n';
    std::cerr << "File Size:                " << data->FileSize()                          << '\n';
    std::cerr << "Format:                   " << qPrintable(data->Format())                << '\n';
    std::cerr << "Rating:                   " << data->Rating()                            << '\n';
    std::cerr << "LastPlay:                 " << qPrintable(data->LastPlay().toString(Qt::ISODate)) << '\n';
    std::cerr << "Compilation:              " << (data->Compilation() ? "true" : "false")  << '\n';
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

#include "moc_test_musicmetadata.cpp"
