/*
 *  Class TestLyrics
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

#include "test_lyrics.h"

void TestLyrics::initTestCase()
{
}

void TestLyrics::dump(const LyricsLineMap* lyrics)
{
    auto keys = lyrics->keys();
    for (auto key : std::as_const(keys))
    {
        auto *value = lyrics->value(key);
        QString valuestr = QString("%1:%2").arg(value->m_time.count()).arg(value->m_lyric);
        std::cerr << "Key: " << key.count() << " value: " << qPrintable(valuestr) << std::endl;
    }
}

void TestLyrics::test_notimes(void)
{
    QFile file {QStringLiteral(TEST_SOURCE_DIR) + "/samples/DemoTrack1.xml"};
    QVERIFY(file.exists());
    QVERIFY(file.open(QIODevice::ReadOnly));

    QTextStream in(&file);
    QString xmlData = in.readAll();
    QVERIFY(xmlData.size() > 0);

    auto *lyricsdata = new LyricsData;
    lyricsdata->loadLyrics(xmlData);

    QCOMPARE(lyricsdata->grabber(),      QString("EmbeddedLyrics"));
    QCOMPARE(lyricsdata->artist(),       QString("Robb Benson"));
    QCOMPARE(lyricsdata->album(),        QString("Demo Tracks"));
    QCOMPARE(lyricsdata->title(),        QString("Lone Rock"));
    QCOMPARE(lyricsdata->artist(),       QString("Robb Benson"));
    QCOMPARE(lyricsdata->syncronized(), true);

    const auto* lyrics = lyricsdata->lyrics();
    //dump(lyrics);
    QVERIFY(lyrics != nullptr);
    QCOMPARE(lyrics->size(), 19);
    QVERIFY (lyrics->contains( 0ms));
    QCOMPARE(lyrics->value   ( 0ms)->m_lyric, QString("One, two, three, four"));
    QVERIFY (lyrics->contains( 3ms));
    QCOMPARE(lyrics->value   ( 3ms)->m_lyric, QString("Silent"));
    QVERIFY (lyrics->contains(13ms));
    QCOMPARE(lyrics->value   (13ms)->m_lyric, QString("Memories count, memories count"));
    QVERIFY (lyrics->contains(15ms));
    QCOMPARE(lyrics->value   (15ms)->m_lyric, QString("Memories count, memories count"));
}

void TestLyrics::test_times(void)
{
    QFile file {QStringLiteral(TEST_SOURCE_DIR) + "/samples/DemoTrack2.xml"};
    QVERIFY(file.exists());
    QVERIFY(file.open(QIODevice::ReadOnly));

    QTextStream in(&file);
    QString xmlData = in.readAll();
    QVERIFY(xmlData.size() > 0);

    auto *lyricsdata = new LyricsData;
    lyricsdata->loadLyrics(xmlData);

    QCOMPARE(lyricsdata->grabber(),      QString("EmbeddedLyrics"));
    QCOMPARE(lyricsdata->artist(),       QString("Robb Benson"));
    QCOMPARE(lyricsdata->album(),        QString("Demo Tracks"));
    QCOMPARE(lyricsdata->title(),        QString("Lone Rock"));
    QCOMPARE(lyricsdata->artist(),       QString("Robb Benson"));
    QCOMPARE(lyricsdata->syncronized(), true);

    const auto* lyrics = lyricsdata->lyrics();
    //dump(lyrics);
    QVERIFY(lyrics != nullptr);
    QCOMPARE(lyrics->size(), 19);
    QVERIFY (lyrics->contains(5s));
    QCOMPARE(lyrics->value   (5s)->m_lyric, QString("One, two, three, four"));
    QVERIFY (lyrics->contains(25s));
    QCOMPARE(lyrics->value   (25s)->m_lyric, QString("Silent"));
    QVERIFY (lyrics->contains(1min + 45s));
    QCOMPARE(lyrics->value   (1min + 45s)->m_lyric, QString("Memories count, memories count"));
    QVERIFY (lyrics->contains(2min + 120ms));
    QCOMPARE(lyrics->value   (2min + 120ms)->m_lyric, QString("Memories count, memories count"));
}

void TestLyrics::test_offset1(void)
{
    QFile file {QStringLiteral(TEST_SOURCE_DIR) + "/samples/DemoTrack3.xml"};
    QVERIFY(file.exists());
    QVERIFY(file.open(QIODevice::ReadOnly));

    QTextStream in(&file);
    QString xmlData = in.readAll();
    QVERIFY(xmlData.size() > 0);

    auto *lyricsdata = new LyricsData;
    lyricsdata->loadLyrics(xmlData);

    QCOMPARE(lyricsdata->grabber(),      QString("EmbeddedLyrics"));
    QCOMPARE(lyricsdata->artist(),       QString("Robb Benson"));
    QCOMPARE(lyricsdata->album(),        QString("Demo Tracks"));
    QCOMPARE(lyricsdata->title(),        QString("Lone Rock"));
    QCOMPARE(lyricsdata->artist(),       QString("Robb Benson"));
    QCOMPARE(lyricsdata->syncronized(), true);

    const auto* lyrics = lyricsdata->lyrics();
    //dump(lyrics);
    QVERIFY(lyrics != nullptr);
    QCOMPARE(lyrics->size(), 19);
    QVERIFY (lyrics->contains(0s));
    QCOMPARE(lyrics->value   (0s)->m_lyric, QString("One, two, three, four"));
    QVERIFY (lyrics->contains(15s));
    QCOMPARE(lyrics->value   (15s)->m_lyric, QString("Silent"));
    QVERIFY (lyrics->contains(1min + 35s));
    QCOMPARE(lyrics->value   (1min + 35s)->m_lyric, QString("Memories count, memories count"));
    QVERIFY (lyrics->contains(1min + 50s + 120ms));
    QCOMPARE(lyrics->value   (1min + 50s + 120ms)->m_lyric, QString("Memories count, memories count"));
}

void TestLyrics::test_offset2(void)
{
    QFile file {QStringLiteral(TEST_SOURCE_DIR) + "/samples/DemoTrack4.xml"};
    QVERIFY(file.exists());
    QVERIFY(file.open(QIODevice::ReadOnly));

    QTextStream in(&file);
    QString xmlData = in.readAll();
    QVERIFY(xmlData.size() > 0);

    auto *lyricsdata = new LyricsData;
    lyricsdata->loadLyrics(xmlData);

    QCOMPARE(lyricsdata->grabber(),      QString("EmbeddedLyrics"));
    QCOMPARE(lyricsdata->artist(),       QString("Robb Benson"));
    QCOMPARE(lyricsdata->album(),        QString("Demo Tracks"));
    QCOMPARE(lyricsdata->title(),        QString("Lone Rock"));
    QCOMPARE(lyricsdata->artist(),       QString("Robb Benson"));
    QCOMPARE(lyricsdata->syncronized(), true);

    const auto* lyrics = lyricsdata->lyrics();
    //dump(lyrics);
    QVERIFY(lyrics != nullptr);
    QCOMPARE(lyrics->size(), 19);
    QVERIFY (lyrics->contains(15s));
    QCOMPARE(lyrics->value   (15s)->m_lyric, QString("One, two, three, four"));
    QVERIFY (lyrics->contains(35s));
    QCOMPARE(lyrics->value   (35s)->m_lyric, QString("Silent"));
    QVERIFY (lyrics->contains(1min + 55s));
    QCOMPARE(lyrics->value   (1min + 55s)->m_lyric, QString("Memories count, memories count"));
    QVERIFY (lyrics->contains(2min + 10s + 120ms));
    QCOMPARE(lyrics->value   (2min + 10s + 120ms)->m_lyric, QString("Memories count, memories count"));
}

void TestLyrics::cleanupTestCase()
{
}

QTEST_APPLESS_MAIN(TestLyrics)
