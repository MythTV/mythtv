/*
 *  Class TestXineDemux
 *
 *  Copyright (c) David Hampton 2020
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
#include "test_xine_demux.h"

void TestXineDemux::initTestCase()
{
}


void TestXineDemux::open_file(const QString& filename,
                              demux_sputext_t& sub_data,
                              bool& loaded)
{
    QFile file(QStringLiteral(TEST_SOURCE_DIR) + "/" + filename);
    QCOMPARE(file.exists(), true);
    QCOMPARE(file.open(QIODevice::ReadOnly | QIODevice::Text), true);

    QByteArray ba = file.readAll();
    sub_data.rbuffer_text = ba.data();
    sub_data.rbuffer_len = ba.size();

    loaded = sub_read_file(&sub_data);
}

void TestXineDemux::test_captions_microdvd(void)
{
    demux_sputext_t sub_data {};
    bool loaded = false;

    open_file("samples/microdvd.sub", sub_data, loaded);
    QCOMPARE(sub_data.format, FORMAT_MICRODVD);
    QCOMPARE(loaded, true);
    QCOMPARE(sub_data.num, 17);
    QCOMPARE(sub_data.subtitles[0].text.size(), static_cast<size_t>(1));
    QCOMPARE(sub_data.subtitles[14].text.size(), static_cast<size_t>(2));
    QCOMPARE(sub_data.subtitles[15].text.size(), static_cast<size_t>(2));
    QCOMPARE(sub_data.subtitles[16].text.size(), static_cast<size_t>(1));
}

void TestXineDemux::test_captions_srt(void)
{
    demux_sputext_t sub_data {};
    bool loaded = false;

    open_file("samples/subrip.srt", sub_data, loaded);
    QCOMPARE(sub_data.format, FORMAT_SUBRIP);
    QCOMPARE(loaded, true);
    QCOMPARE(sub_data.num, 4);
    QCOMPARE(sub_data.subtitles[3].text.size(), static_cast<size_t>(3));
    QVERIFY(!QString::fromStdString(sub_data.subtitles[3].text[1]).contains("{\\i1}"));
    QVERIFY(!QString::fromStdString(sub_data.subtitles[3].text[1]).contains("</i>"));
}

void TestXineDemux::test_captions_subviewer_data(void)
{
    QTest::addColumn<QString>("filename");
    QTest::addColumn<int>("format");

    QTest::newRow("subviewer")  << "samples/subviewer.sub"  << FORMAT_SUBVIEWER;
    QTest::newRow("subviewer2") << "samples/subviewer2.sub" << FORMAT_SUBVIEWER2;
}

void TestXineDemux::test_captions_subviewer(void)
{
    QFETCH(QString, filename);
    QFETCH(int, format);

    demux_sputext_t sub_data {};
    bool loaded = false;

    open_file(filename, sub_data, loaded);
    QCOMPARE(sub_data.format, format);
    QCOMPARE(loaded, true);
    QCOMPARE(sub_data.num, 4);
    QCOMPARE(sub_data.subtitles[1].text.size(), static_cast<size_t>(2));
    QCOMPARE(sub_data.subtitles[2].text.size(), static_cast<size_t>(3));
    QCOMPARE(sub_data.subtitles[3].text.size(), static_cast<size_t>(1));
    QVERIFY(QString::fromStdString(sub_data.subtitles[2].text[0]).contains("String3"));
}

void TestXineDemux::test_captions_smi(void)
{
    demux_sputext_t sub_data {};
    bool loaded = false;

    open_file("samples/sami.smi", sub_data, loaded);
    // Loses first blank line.
    // Loses last line (regardless of if blank).
    QCOMPARE(sub_data.format, FORMAT_SAMI);
    QCOMPARE(loaded, true);
    QCOMPARE(sub_data.num, 4);
    QCOMPARE(sub_data.subtitles[3].text.size(), static_cast<size_t>(3));
    QVERIFY(!QString::fromStdString(sub_data.subtitles[3].text[1]).contains("</i>"));
}

void TestXineDemux::test_captions_vplayer(void)
{
    demux_sputext_t sub_data {};
    bool loaded = false;

    open_file("samples/vplayer.txt", sub_data, loaded);
    QCOMPARE(sub_data.format, FORMAT_VPLAYER);
    QCOMPARE(loaded, true);
    QCOMPARE(sub_data.num, 5);
    QCOMPARE(sub_data.subtitles[3].text.size(), static_cast<size_t>(1));
}

void TestXineDemux::test_captions_rt(void)
{
    demux_sputext_t sub_data {};
    bool loaded = false;

    open_file("samples/realtext.txt", sub_data, loaded);
    QCOMPARE(sub_data.format, FORMAT_RT);
    QCOMPARE(loaded, true);
    QCOMPARE(sub_data.num, 8);
}

void TestXineDemux::test_captions_ssa_ass_data(void)
{
    QTest::addColumn<QString>("filename");
    QTest::addColumn<int>("expectedLines");

    QTest::newRow("subtitles.v3.ssa")  << "samples/substationalpha.v3.ssa" << 5;
    QTest::newRow("subtitles.v4.ssa")  << "samples/substationalpha.v4.ssa" << 3;
    QTest::newRow("subtitles.ass")     << "samples/advancedssa.ass"        << 3;
}

void TestXineDemux::test_captions_ssa_ass(void)
{
    QFETCH(QString, filename);
    QFETCH(int, expectedLines);

    demux_sputext_t sub_data {};
    bool loaded = false;

    open_file(filename, sub_data, loaded);
    // Loses first blank line.
    // Loses last line (regardless of if blank).
    QCOMPARE(sub_data.format, FORMAT_SSA);
    QCOMPARE(loaded, true);
    QCOMPARE(sub_data.num, expectedLines);
}

// Phoenix Japanimation Society
void TestXineDemux::test_captions_pjs(void)
{
    demux_sputext_t sub_data {};
    bool loaded = false;

    open_file("samples/phoenixjapanimationsociety.pjs", sub_data, loaded);
    QCOMPARE(sub_data.format, FORMAT_PJS);
    QCOMPARE(loaded, true);
    QCOMPARE(sub_data.num, 2);
}

void TestXineDemux::test_captions_mpsub(void)
{
    demux_sputext_t sub_data {};
    bool loaded = false;

    open_file("samples/mpsub.sub", sub_data, loaded);
    QCOMPARE(sub_data.format, FORMAT_MPSUB);
    QCOMPARE(loaded, true);
    QCOMPARE(sub_data.num, 3);
    QCOMPARE(sub_data.subtitles[0].text.size(), static_cast<size_t>(1));
    QCOMPARE(sub_data.subtitles[1].text.size(), static_cast<size_t>(2));
}

void TestXineDemux::test_captions_aqtitle(void)
{
    demux_sputext_t sub_data {};
    bool loaded = false;

    open_file("samples/aqtitles.aqt.sub", sub_data, loaded);
    QCOMPARE(sub_data.format, FORMAT_AQTITLE);
    QCOMPARE(loaded, true);
    QCOMPARE(sub_data.num, 6);
    QCOMPARE(sub_data.subtitles[0].text.size(), static_cast<size_t>(1));
    QCOMPARE(sub_data.subtitles[1].text.size(), static_cast<size_t>(1));
    QCOMPARE(sub_data.subtitles[2].text.size(), static_cast<size_t>(2));
    QCOMPARE(sub_data.subtitles[3].text.size(), static_cast<size_t>(1));
    QCOMPARE(sub_data.subtitles[4].text.size(), static_cast<size_t>(3));
    QCOMPARE(sub_data.subtitles[5].text.size(), static_cast<size_t>(1));
}

void TestXineDemux::test_captions_jaco(void)
{
    demux_sputext_t sub_data {};
    bool loaded = false;

    open_file("samples/jaco.sub", sub_data, loaded);
    QCOMPARE(sub_data.format, FORMAT_JACOBSUB);
    QCOMPARE(loaded, true);
    QCOMPARE(sub_data.num, 37);
    // Lines containing the "RLB" directive are skipped, altering the
    // line numbers compared to the original file.
    QCOMPARE(sub_data.subtitles[0].text.size(), static_cast<size_t>(3));
    QCOMPARE(sub_data.subtitles[4].text.size(), static_cast<size_t>(1));
    QVERIFY(QString::fromStdString(sub_data.subtitles[4].text[0]).startsWith("(And"));
    QCOMPARE(sub_data.subtitles[5].text.size(), static_cast<size_t>(3));
    QCOMPARE(sub_data.subtitles[6].text.size(), static_cast<size_t>(2));
}

void TestXineDemux::test_captions_subrip09(void)
{
    demux_sputext_t sub_data {};
    bool loaded = false;

    open_file("samples/subrip09.srt", sub_data, loaded);
    QCOMPARE(sub_data.format, FORMAT_SUBRIP09);
    QCOMPARE(loaded, true);
    QCOMPARE(sub_data.num, 4);
    QCOMPARE(sub_data.subtitles[2].text.size(), static_cast<size_t>(3));
    QCOMPARE(sub_data.subtitles[3].text.size(), static_cast<size_t>(2));
    QVERIFY(QString::fromStdString(sub_data.subtitles[3].text[1]).contains("fleece"));
}

void TestXineDemux::test_captions_mpl2(void) // MPL
{
    demux_sputext_t sub_data {};
    bool loaded = false;

    open_file("samples/mpl2.mpl.sub", sub_data, loaded);
    QCOMPARE(sub_data.format, FORMAT_MPL2);
    QCOMPARE(loaded, true);
    QCOMPARE(sub_data.num, 6);
    QCOMPARE(sub_data.subtitles[0].text.size(), static_cast<size_t>(1));
    QCOMPARE(sub_data.subtitles[1].text.size(), static_cast<size_t>(2));
    QCOMPARE(sub_data.subtitles[2].text.size(), static_cast<size_t>(1));
    QCOMPARE(sub_data.subtitles[5].text.size(), static_cast<size_t>(2));
}

void TestXineDemux::cleanupTestCase()
{
}

QTEST_APPLESS_MAIN(TestXineDemux)
