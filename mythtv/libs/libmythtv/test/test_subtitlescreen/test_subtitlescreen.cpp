/*
 *  Class TestSubtitleScreen
 *
 *  Copyright (C) David Hampton 2021
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

#include <iostream>
#include <vector>
#include "test_subtitlescreen.h"

#include "libmythbase/mythchrono.h"
#include "libmythtv/captions/subtitlescreen.h"

void TestSubtitleScreen::initTestCase()
{
}

void TestSubtitleScreen::cleanupTestCase()
{
}

void TestSubtitleScreen::testSRT()
{
    const QString dummy;
    QStringList text {
        "Line 0",
        "<b>Line 1</b>",
        "Line <em>2</em>",
        "<i>Line <u>3</u></i>",
        "<font color=\"#FFFFFF\">Line 4</font>",
        "<b>Line 5",
        "Line 6</b>",
    };

    auto *srt = new FormattedTextSubtitleSRT(dummy, QRect(), 0ms, 0ms, nullptr, text);

    QVERIFY(srt != nullptr);
    QCOMPARE(srt->m_lines.count(), 7);
    QCOMPARE(srt->m_lines[0].chunks.count(),   1);
    QCOMPARE(srt->m_lines[0].chunks[0].m_text, QString("Line 0"));
    QCOMPARE(srt->m_lines[1].chunks.count(),   1);
    QCOMPARE(srt->m_lines[1].chunks[0].m_text, QString("Line 1"));
    QCOMPARE(srt->m_lines[2].chunks.count(),   2);
    QCOMPARE(srt->m_lines[2].chunks[0].m_text, QString("Line "));
    QCOMPARE(srt->m_lines[2].chunks[1].m_text, QString("2"));
    QCOMPARE(srt->m_lines[3].chunks.count(),   2);
    QCOMPARE(srt->m_lines[3].chunks[0].m_text, QString("Line "));
    QCOMPARE(srt->m_lines[3].chunks[1].m_text, QString("3"));
    QCOMPARE(srt->m_lines[4].chunks.count(),   1);
    QCOMPARE(srt->m_lines[4].chunks[0].m_text, QString("Line 4"));
    QCOMPARE(srt->m_lines[5].chunks.count(),   1);
    QCOMPARE(srt->m_lines[5].chunks[0].m_text, QString("Line 5"));
    QCOMPARE(srt->m_lines[6].chunks.count(),   1);
    QCOMPARE(srt->m_lines[6].chunks[0].m_text, QString("Line 6"));
}

void TestSubtitleScreen::test608()
{
    const QString dummy;
    CC608Text line0 { "Line 0", 0, 0};
    CC608Text line1 { QChar(0x700E) + QString("Line 1") + QChar(0x7000), 0, 0};
    CC608Text line2 { QString("Line ") + QChar(0x700E) + QString("2") + QChar(0x7000), 0, 0};
    CC608Text line3 { QChar(0x700E) + QString("Line ") + QChar(0x700F) + QString("3") + QChar(0x7000), 0, 0};
    CC608Text line4 { QChar(0x7002) + QString("Line 4") + QChar(0x7000), 0, 0};
    CC608Text line5 { QChar(0x7001) + QString("Line 5"), 0, 0};
    CC608Text line6 { QString("Line 6") + QChar(0x7000), 0, 0};
    const std::vector<CC608Text*> buffers {
        &line0, &line1, &line2, &line3, &line4, &line5, &line6};

    auto *cc608 = new FormattedTextSubtitle608(buffers, dummy, QRect(), nullptr);
    QVERIFY(cc608 != nullptr);
    QCOMPARE(cc608->m_lines.count(), 7);
    QCOMPARE(cc608->m_lines[0].chunks.count(),   1);
    QCOMPARE(cc608->m_lines[0].chunks[0].m_text, QString("Line 0"));
    // Control character at end of string results in extra blank chunck
    QCOMPARE(cc608->m_lines[1].chunks.count(),   2);
    QCOMPARE(cc608->m_lines[1].chunks[0].m_text, QString("Line 1"));
    QCOMPARE(cc608->m_lines[2].chunks.count(),   3);
    QCOMPARE(cc608->m_lines[2].chunks[0].m_text, QString("Line "));
    QCOMPARE(cc608->m_lines[2].chunks[1].m_text, QString("2"));
    QCOMPARE(cc608->m_lines[3].chunks.count(),   3);
    QCOMPARE(cc608->m_lines[3].chunks[0].m_text, QString("Line "));
    QCOMPARE(cc608->m_lines[3].chunks[1].m_text, QString("3"));
    QCOMPARE(cc608->m_lines[4].chunks.count(),   2);
    QCOMPARE(cc608->m_lines[4].chunks[0].m_text, QString("Line 4"));
    QCOMPARE(cc608->m_lines[5].chunks.count(),   1);
    QCOMPARE(cc608->m_lines[5].chunks[0].m_text, QString("Line 5"));
    QCOMPARE(cc608->m_lines[6].chunks.count(),   2);
    QCOMPARE(cc608->m_lines[6].chunks[0].m_text, QString("Line 6"));
}


QTEST_APPLESS_MAIN(TestSubtitleScreen)
