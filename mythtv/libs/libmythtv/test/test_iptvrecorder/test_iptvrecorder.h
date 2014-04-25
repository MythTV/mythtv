/*
 *  Class TestIPTVRecorder
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

#include "iptvtuningdata.h"
#include "channelscan/iptvchannelfetcher.h"

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#define MSKIP(MSG) QSKIP(MSG, SkipSingle)
#else
#define MSKIP(MSG) QSKIP(MSG)
#endif

class TestIPTVRecorder: public QObject
{
    Q_OBJECT

  private slots:
    /**
     * Test if supported Urls really are considered valid.
     */
    void TuningData(void)
    {
        IPTVTuningData tuning;

        /* test url from #11949 without port, free.fr */
        tuning.m_data_url = QUrl (QString("rtsp://mafreebox.freebox.fr/fbxtv_pub/stream?namespace=1&service=203&flavour=sd"));
        QVERIFY (tuning.IsValid());

        /* test url from #11949 with port, free.fr */
        tuning.m_data_url = QUrl (QString("rtsp://mafreebox.freebox.fr:554/fbxtv_pub/stream?namespace=1&service=203&flavour=sd"));
        QVERIFY (tuning.IsValid());

        /* test url from #11852 with port, telekom.de */
        tuning.m_data_url = QUrl (QString("rtp://@239.35.10.1:10000"));
        QVERIFY (tuning.IsValid());
        /* test url from das-erste.de with port, telekom.de */
        tuning.m_data_url = QUrl (QString("rtp://239.35.10.4:10000"));
        QVERIFY (tuning.IsValid());

        /* test url from #11847 with port, Dreambox */
        tuning.m_data_url = QUrl (QString("http://yourdreambox:8001/1:0:1:488:3FE:22F1:EEEE0000:0:0:0:"));
        QVERIFY (tuning.IsValid());
    }


    /**
     * Test if the expectation "if the Url works with VLC it should work with MythTV" is being met.
     */
    void TuningDataVLCStyle(void)
    {
        MSKIP ("Do we want to support non-conformant urls that happen to work with VLC?");
        IPTVTuningData tuning;

        /* test url from http://www.tldp.org/HOWTO/VideoLAN-HOWTO/x549.html */
        tuning.m_data_url = QUrl (QString("udp:@239.255.12.42"));
        QVERIFY (tuning.IsValid());
        /* test url from http://www.tldp.org/HOWTO/VideoLAN-HOWTO/x1245.html */
        tuning.m_data_url = QUrl (QString("udp:@[ff08::1]"));
        QVERIFY (tuning.IsValid());
        /* test url from http://www.tldp.org/HOWTO/VideoLAN-HOWTO/x1245.html */
        tuning.m_data_url = QUrl (QString("udp:[ff08::1%eth0]"));
        QVERIFY (tuning.IsValid());
    }

    /**
     * Test parsing a playlist
     */
    void ParseChanInfo(void)
    {
        /* #12077 - DVB with Neutrino - Sweden */
        QString rawdataHTTP ("#EXTM3U\n"
                             "#EXTINF:0,1 - SVT1 HD Mitt\n"
                             "#EXTMYTHTV:xmltvid=svt1hd.svt.se\n"
                             "#EXTVLCOPT:program=1330\n"
                             "http://192.168.0.234:8001/1:0:19:532:6:22F1:EEEE0000:0:0:0:\n");

        /* #11949 - FreeboxTV - France - Free */
        QString rawdataRTSP ("#EXTM3U\n"
                             "#EXTINF:0,2 - France 2 (bas débit)\n"
                             "rtsp://mafreebox.freebox.fr/fbxtv_pub/stream?namespace=1&service=201&flavour=ld\n"
                             "#EXTINF:0,2 - France 2 (HD)\n"
                             "rtsp://mafreebox.freebox.fr/fbxtv_pub/stream?namespace=1&service=201&flavour=hd\n"
                             "#EXTINF:0,2 - France 2 (standard)\n"
                             "rtsp://mafreebox.freebox.fr/fbxtv_pub/stream?namespace=1&service=201&flavour=sd\n"
                             "#EXTINF:0,2 - France 2 (auto)\n"
                             "rtsp://mafreebox.freebox.fr/fbxtv_pub/stream?namespace=1&service=201");

        /* #11963 - Movistar TV - Spain - Telefonica */
        QString rawdataUDP ("#EXTM3U\n"
                            "#EXTINF:0,001 - La 1\n"
                            "udp://239.0.0.76:8208\n");

        fbox_chan_map_t chanmap;

        /* test plain old MPEG-2 TS over multicast playlist */
        chanmap = IPTVChannelFetcher::ParsePlaylist (rawdataUDP, NULL);
        QCOMPARE (chanmap["001"].m_name, QString ("La 1"));
        QVERIFY (chanmap["001"].IsValid ());
        QVERIFY (chanmap["001"].m_tuning.IsValid ());
        QCOMPARE (chanmap["001"].m_tuning.m_data_url.toString(), QString ("udp://239.0.0.76:8208"));
        QCOMPARE (chanmap["001"].m_tuning.GetDataURL().toString(), QString ("udp://239.0.0.76:8208"));

        /* test playlist for Neutrino STBs */
        chanmap = IPTVChannelFetcher::ParsePlaylist (rawdataHTTP, NULL);
        QVERIFY (chanmap["1"].IsValid ());
        QVERIFY (chanmap["1"].m_tuning.IsValid ());
        QCOMPARE (chanmap["1"].m_name, QString ("SVT1 HD Mitt"));
        QCOMPARE (chanmap["1"].m_xmltvid, QString ("svt1hd.svt.se"));
        QCOMPARE (chanmap["1"].m_programnumber, (uint) 1330);
        QCOMPARE (chanmap["1"].m_tuning.m_data_url.toString(), QString ("http://192.168.0.234:8001/1:0:19:532:6:22F1:EEEE0000:0:0:0:"));

        /* test playlist for FreeboxTV, last channel in playlist "wins" */
        chanmap = IPTVChannelFetcher::ParsePlaylist (rawdataRTSP, NULL);
        QVERIFY (chanmap["2"].IsValid ());
        QVERIFY (chanmap["2"].m_tuning.IsValid ());
        QCOMPARE (chanmap["2"].m_name, QString ("France 2 (auto)"));
        QCOMPARE (chanmap["2"].m_tuning.GetDataURL().toString(), QString ("rtsp://mafreebox.freebox.fr/fbxtv_pub/stream?namespace=1&service=201"));
    }
};
