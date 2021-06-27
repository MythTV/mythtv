/*
 *  Class TestFrequencies
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
#include <iostream>
#include <string>
#include "test_frequencies.h"

void TestFrequencies::initTestCase()
{
}

void TestFrequencies::test_gchanlists_data(void)
{
    QTest::addColumn<QString>("country");
    QTest::addColumn<bool>("expected");

    QTest::newRow("us-bcast") << "us-bcast" << true;
    QTest::newRow("france") << "france" << true;
    QTest::newRow("bogus") << "bogus" << false;
}

void TestFrequencies::test_gchanlists(void)
{
    QFETCH(QString, country);
    QFETCH(bool, expected);

    std::string country2 = country.toStdString();
    auto it = std::find_if(gChanLists.cbegin(),gChanLists.cend(),
                           [country2] (const CHANLISTS &chanlist) -> bool
                               { return country2 == chanlist.name; });
    QCOMPARE(it != gChanLists.end(), expected);
}

void TestFrequencies::test_get_tables_data(void)
{
    QTest::addColumn<QString>("format");
    QTest::addColumn<QString>("modulation");
    QTest::addColumn<QString>("country");
    QTest::addColumn<int>("count");
    QTest::addColumn<int>("freqid");
    QTest::addColumn<int>("center");
    QTest::addColumn<int>("frequency");

    // Entries build directly
    QTest::newRow("dbvt_ofdm_gb") << "dvbt" << "ofdm" << "gb" << 1 << 21 << 474000000;
    QTest::newRow("dbvt_ofdm_au") << "dvbt" << "ofdm" << "au" << 2 << 68 << 809500000;
    QTest::newRow("dbvc_qam_de")  << "dvbc" << "qam"  << "de" << 5 << 22 << 314000000;

    // Entries build in a loop
    QTest::newRow("atsc uscable")   << "atsc" << "vsb8"   << "uscable" << 8 <<  3 <<  63000000;
    QTest::newRow("qam256 uscable") << "atsc" << "qam256" << "uscable" << 8 << 98 << 111000000;

    // Entries build from old tables
    QTest::newRow("us bcast")     << "analog" << "analog" << "us-bcast"     <<  35 << 12 << 207000000;
    QTest::newRow("us cable hrc") << "analog" << "analog" << "us-cable-hrc" << 125 << 99 << 109755000;
    QTest::newRow("france")       << "analog" << "analog" << "france"       <<  94 << 28 << 305000000;

    // Invalid
    QTest::newRow("dbvc_qam_xx")     << "dvbc" << "qam"     << "xx"      << 0 <<  0 <<         0;
}

void TestFrequencies::test_get_tables(void)
{
    QFETCH(QString, format);
    QFETCH(QString, modulation);
    QFETCH(QString, country);
    QFETCH(int, count);
    QFETCH(int, freqid);
    QFETCH(int, center);

    auto tables = get_matching_freq_tables(format, modulation, country);
//  std::cerr << "Data: " << qPrintable(QString("%1_%2_%3").arg(format)
//                                      .arg(modulation).arg(country)) << std::endl;
//  std::cerr << "Count:  " << count << std::endl;
//  std::cerr << "Actual: " << tables.size() << std::endl;
    QCOMPARE(tables.size(), (size_t)count);
    if (count > 0)
    {
        long long actual_center = get_center_frequency(format, modulation, country, freqid);
//      std::cerr << "Center: " << center << std::endl;
//      std::cerr << "Actual: " << actual_center << std::endl;
        QCOMPARE(actual_center, center);

        int actual_freq = get_closest_freqid(format, modulation, country, center);
//      std::cerr << "FreqId: " << freqid << std::endl;
//      std::cerr << "Actual: " << actual_freq << std::endl;
        QCOMPARE(actual_freq, freqid);
    }
}

void TestFrequencies::test_get_closest_data(void)
{
    QTest::addColumn<QString>("format");
    QTest::addColumn<QString>("modulation");
    QTest::addColumn<QString>("country");
    QTest::addColumn<int>("frequency");
    QTest::addColumn<int>("freqid");

    // Entries build directly
    QTest::newRow("dbvt_ofdm_gb 1") << "dvbt" << "ofdm" << "gb" << 473999999 << -1;
    QTest::newRow("dbvt_ofdm_gb 2") << "dvbt" << "ofdm" << "gb" << 474000000 << 21;
    QTest::newRow("dbvt_ofdm_au 1") << "dvbt" << "ofdm" << "au" << 809499999 << 67;
    QTest::newRow("dbvt_ofdm_au 2") << "dvbt" << "ofdm" << "au" << 809500000 << 68;
    QTest::newRow("dbvc_qam_de 1")  << "dvbc" << "qam"  << "de" << 313999999 << 21;
    QTest::newRow("dbvc_qam_de 2")  << "dvbc" << "qam"  << "de" << 314000000 << 22;

    // Entries build in a loop
    QTest::newRow("atsc uscable 1")   << "atsc" << "vsb8"   << "uscable" <<  62999999 <<  2;
    QTest::newRow("atsc uscable 2")   << "atsc" << "vsb8"   << "uscable" <<  63000000 <<  3;
    QTest::newRow("qam256 uscable 1") << "atsc" << "qam256" << "uscable" << 110999999 << 97;
    QTest::newRow("qam256 uscable 2") << "atsc" << "qam256" << "uscable" << 111000000 << 98;

    // Entries build from old tables
    QTest::newRow("us bcast 1")     << "analog" << "analog" << "us-bcast"     << 206999999 << 11;
    QTest::newRow("us bcast 2")     << "analog" << "analog" << "us-bcast"     << 207000000 << 12;
    QTest::newRow("us cable hrc 1") << "analog" << "analog" << "us-cable-hrc" << 109754999 << 98;
    QTest::newRow("us cable hrc 2") << "analog" << "analog" << "us-cable-hrc" << 109755000 << 99;
}

void TestFrequencies::test_get_closest(void)
{
    QFETCH(QString, format);
    QFETCH(QString, modulation);
    QFETCH(QString, country);
    QFETCH(int, frequency);
    QFETCH(int, freqid);

    int actual_freq = get_closest_freqid(format, modulation, country, frequency);
//  std::cerr << "FreqId: " << freqid << std::endl;
//  std::cerr << "Actual: " << actual_freq << std::endl;
    QCOMPARE(actual_freq, freqid);
}

void TestFrequencies::cleanupTestCase()
{
}

QTEST_APPLESS_MAIN(TestFrequencies)
