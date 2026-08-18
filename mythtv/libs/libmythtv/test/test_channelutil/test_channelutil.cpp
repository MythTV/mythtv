#include "test_channelutil.h"

#include <QTest>

Q_DECLARE_METATYPE(ChannelChangeDirection);

// Add shorthand values for ChannelChangeDirection and
// ChannelVisibleType
#define UP   CHANNEL_DIRECTION_UP
#define DOWN CHANNEL_DIRECTION_DOWN
#define SAME CHANNEL_DIRECTION_SAME

#define VIS_YES kChannelVisible
#define VIS_NO  kChannelNotVisible
#define VIS_NEV kChannelNeverVisible

// Called once before any tests are started.
void TestChannelUtil::initTestCase(void)
{
    m_channels.reserve(30);

    // Using this ChannelInfo constructor:
    //    ChannelInfo(QString _channum, QString _callsign,
    //          uint _chanid, uint _major_chan, uint _minor_chan,
    //          uint _mplexid, ChannelVisibleType _visible,
    //          QString _name, QString _icon,
    //          uint _sourceid);

    // channum | callsign | chanid | atsc_major | atsc_minor | mplexid | visible | name | icon | sourceid
    m_channels.emplace_back("11_1",  "WBAL-SD",         21101, 11, 1, 1, VIS_YES, "WBAL-SD", "", 2);
    m_channels.emplace_back("11_1",  "WBAL-DT",         41101, 11, 1, 22, VIS_YES, "WBAL-DT", "", 4);
    m_channels.emplace_back("11_1A", "WBAL-HD",         21100, 11, 1, 8, VIS_YES, "WBAL-HD", "", 2);
    m_channels.emplace_back("11_2",  "WBAL Me",         21102, 11, 2, 8, VIS_YES, "WBAL Me", "", 2);
    m_channels.emplace_back("11_2",  "MeTV",            41102, 11, 2, 22, VIS_YES, "MeTV", "", 4);
    m_channels.emplace_back("11_3",  "Story",           41103, 11, 3, 22, VIS_YES, "Story", "", 4);
    m_channels.emplace_back("11_4",  "TheGrio",         41104, 11, 4, 22, VIS_NO, "TheGrio", "", 4);
    m_channels.emplace_back("11_5",  "QVC",             41105, 11, 5, 22, VIS_NEV, "QVC", "", 4);
    m_channels.emplace_back("11",    "WBAL",            10011, 11, 0, 0, VIS_YES, "WBAL", "", 1);
    m_channels.emplace_back("460",   "WBAL Cozi TV",    10460, 0, 0, 0, VIS_YES, "WBAL Cozi TV", "", 1);
    m_channels.emplace_back("511",   "WBAL-DTV",        10511, 0, 0, 0, VIS_YES, "WBAL-DTV", "", 1);
    m_channels.emplace_back("12",    "WMAR",            10012, 12, 0, 0, VIS_YES, "WMAR", "", 1);
    m_channels.emplace_back("459",   "WMARDT3",         10459, 459, 0, 0, VIS_YES, "WMARDT3 (WMAR-DT3)", "", 1);
    m_channels.emplace_back("463",   "WMAR Grit TV",    10463, 0, 0, 0, VIS_YES, "WMAR Grit TV", "", 1);
    m_channels.emplace_back("486",   "WMAR ION Myster", 10486, 0, 0, 0, VIS_YES, "WMAR ION Myster", "", 1);
    m_channels.emplace_back("512",   "WMAR-DTV",        10512, 0, 0, 0, VIS_YES, "WMAR-DTV", "", 1);
    m_channels.emplace_back("1",     "WMAR-HD",         30001, 0, 0, 0, VIS_YES, "WMAR-HD", "", 3);
    m_channels.emplace_back("2_1",   "WMAR-SD",         20201, 2, 1, 1, VIS_YES, "WMAR-SD", "", 2);
    m_channels.emplace_back("2_1",   "WMAR-HD",         40201, 2, 1, 27, VIS_YES, "WMAR-HD", "", 4);
    m_channels.emplace_back("2_1",   "WMAR-HD",         20200, 2, 1, 8, VIS_YES, "WMAR-HD", "", 2);
    m_channels.emplace_back("2_2",   "WMAR LA",         20202, 2, 2, 8, VIS_YES, "WMAR LA", "", 2);
    m_channels.emplace_back("2_2",   "GRIT",            40202, 2, 2, 27, VIS_YES, "GRIT", "", 4);
    m_channels.emplace_back("2_3",   "BOUNCE",          20203, 2, 3, 8, VIS_YES, "BOUNCE", "", 2);
    m_channels.emplace_back("2_3",   "BOUNCE",          40203, 2, 3, 27, VIS_YES, "BOUNCE", "", 4);
    m_channels.emplace_back("2_4",   "WMAR_Co",         20204, 2, 4, 8, VIS_YES, "WMAR_Co", "", 2);
    m_channels.emplace_back("2_4",   "MYSTERY",         40204, 2, 4, 27, VIS_YES, "MYSTERY", "", 4);
    m_channels.emplace_back("2_5",   "CourtTV",         40205, 2, 5, 27, VIS_YES, "CourtTV", "", 4);
    m_channels.emplace_back("2_6",   "Newsy",           40206, 2, 6, 27, VIS_YES, "Newsy", "", 4);
}

// Called once after all tests are finished.
void TestChannelUtil::cleanupTestCase(void)
{
}

// Called for every test case.
void TestChannelUtil::init(void)
{
}

// Called for every test case
void TestChannelUtil::cleanup(void)
{
}

void TestChannelUtil::test_getnextchannel_data(void)
{
    QTest::addColumn<quint32>("old_chanid");
    QTest::addColumn<quint32>("mplexid_restriction");
    QTest::addColumn<quint32>("chanid_restriction");
    QTest::addColumn<ChannelChangeDirection>("direction");
    QTest::addColumn<bool>("skip_non_visible");
    QTest::addColumn<bool>("skip_same_channum_and_callsign");
    QTest::addColumn<bool>("skip_other_sources");
    QTest::addColumn<quint32>("expected");

    // Start from non-existent channel
    QTest::newRow("badstart") << 12345U << 0U << 0U << UP << false << false << false << 41101U;

    // Find next/prev channel
    QTest::newRow("same") << 10011U << 0U << 0U << SAME << false << false << false << 10011U;
    QTest::newRow("up")   << 10011U << 0U << 0U << UP   << false << false << false << 10460U;
    QTest::newRow("down") << 10011U << 0U << 0U << DOWN << false << false << false << 41105U;

    // Find next/prev visible channel
    QTest::newRow("skip_nv_up")   << 41103U << 0U << 0U << UP   << true << false << false  << 10011U;
    QTest::newRow("skip_nv_down") << 10011U << 0U << 0U << DOWN << true << false << false  << 41103U;

    // Find next/prev channel not matching channum and callsign
    QTest::newRow("skip_cncs_up")   << 40201U << 0U << 0U << UP   << false << true << false  << 20202U;
    QTest::newRow("skip_cncs_down") << 40203U << 0U << 0U << DOWN << false << true << false  << 40202U;

    // Find next/prev channel on the current source
    QTest::newRow("skip_os_up")   << 10512U << 0U << 0U << UP   << false << false << true  << 10011U;
    QTest::newRow("skip_os_down") << 40202U << 0U << 0U << DOWN << false << false << true  << 40201U;

    // Find next/prev channel on specified multiplex
    QTest::newRow("mp_up")    << 41104U << 22U << 0U << UP   << false << false << false << 41105U;
    QTest::newRow("mp_up2")   << 41105U << 22U << 0U << UP   << false << false << false << 41101U;
    QTest::newRow("mp_down")  << 41101U << 22U << 0U << DOWN << false << false << false << 41105U;
    QTest::newRow("mp_down2") << 41101U << 22U << 0U << DOWN << true  << false << false << 41103U;

    // Find specific channel
    QTest::newRow("ch_up")    << 10011U << 0U << 10512U << UP << false << false << false << 10512U;
    // Find specific channel (not present)
    QTest::newRow("ch_up2")   << 10011U << 0U << 12345U << UP << false << false << false << 10011U;
}

void TestChannelUtil::test_getnextchannel(void)
{
    QFETCH(quint32, old_chanid);
    QFETCH(quint32, mplexid_restriction);
    QFETCH(quint32, chanid_restriction);
    QFETCH(ChannelChangeDirection, direction);
    QFETCH(bool, skip_non_visible);
    QFETCH(bool, skip_same_channum_and_callsign);
    QFETCH(bool, skip_other_sources);
    QFETCH(quint32, expected);

    uint channel =
        ChannelUtil::GetNextChannel(m_channels,
                                    old_chanid,
                                    mplexid_restriction,
                                    chanid_restriction,
                                    direction,
                                    skip_non_visible,
                                    skip_same_channum_and_callsign,
                                    skip_other_sources);
    QCOMPARE(channel, expected);
}

QTEST_APPLESS_MAIN(TestChannelUtil)
