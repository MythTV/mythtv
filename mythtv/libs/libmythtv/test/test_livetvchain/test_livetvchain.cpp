#include "test_livetvchain.h"

#include <QTest>

#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythlogging.h"
#include "libmythtv/programinfo.h"

// Create an entry in the m_chain list.
void TestLiveTvChain::Append(uint chanid, const QString& channum, bool discont,
                             const QString& inputname, const QString& inputtype,
                             const QString& starttime, const QString& endtime)
{
    LiveTVChainEntry newent;

    newent.chanid = chanid;
    newent.starttime = QDateTime::fromString(starttime);
    newent.endtime = QDateTime::fromString(endtime);
    newent.discontinuity = discont;
    newent.hostprefix = "myth://localhost/";
    newent.inputtype = inputtype;
    newent.channum = channum;
    newent.inputname = inputname;
    m_chain.append(newent);
}

// Override the original EntryToProgram function which requires a
// database to work.  The DoGetNextProgram function logic only uses
// this value to get the file size.  Its mainly there to return to the
// caller.
ProgramInfo *TestLiveTvChain::EntryToProgram(const LiveTVChainEntry &entry) const
{
    // Need to simulate failures to get to the second pass over the
    // list of entries.
    if (entry.chanid == 0)
        return nullptr;

    auto *pginfo = new ProgramInfo();
    if (nullptr == pginfo)
        return nullptr;

    QString pathname = QString("%1%2/%3/%4/%5/%6")
        .arg(entry.hostprefix, entry.inputtype, entry.inputname, entry.channum)
        .arg(entry.chanid).arg(entry.starttime.toString(Qt::TextDate));
    pginfo->SetChanID(entry.chanid);
    pginfo->SetScheduledStartTime(entry.starttime);
    pginfo->SetScheduledEndTime(entry.endtime);
    pginfo->SetPathname(pathname);
    pginfo->SetFilesize(entry.starttime.secsTo(entry.endtime));
    return pginfo;
}

// Called once.
//
// Create the list of items in a "live tv chain".  Looking at actual
// data, This list seems to always be added to in pairs, and is only
// cleared when exiting live tv.
void TestLiveTvChain::initTestCase(void)
{
    // Ignore any database requests.
    gCoreContext = new MythCoreContext("test_livechain_1.0", nullptr);
    auto *db = gCoreContext->GetDB();
    db->IgnoreDatabase(true);

    // Build the test "live tv chain".

    // 0 - no pgminfo
    Append(0, "508", false, "MPEG2TS", "DUMMY",
           "Thu Apr 2 21:21:46 2026 GMT", "Thu Apr 2 21:21:47 2026 GMT");
    Append(10508, "508", false, "MPEG2TS", "HDHOMERUN",
           "Thu Apr 2 21:21:47 2026 GMT", "Thu Apr 2 21:21:57 2026 GMT");

    // 2
    Append(10511, "511", true, "MPEG2TS", "DUMMY",
           "Thu Apr 2 21:21:57 2026 GMT", "Thu Apr 2 21:21:57 2026 GMT");
    Append(10511, "511", true, "MPEG2TS", "MPEG",
           "Thu Apr 2 21:21:58 2026 GMT", "Thu Apr 2 21:22:04 2026 GMT");

    // 4
    Append(10513, "513", true, "MPEG2TS", "DUMMY",
           "Thu Apr 2 21:22:04 2026 GMT", "Thu Apr 2 21:22:05 2026 GMT");
    Append(10513, "513", true, "MPEG2TS", "MPEG",
           "Thu Apr 2 21:22:05 2026 GMT", "Thu Apr 2 21:22:11 2026 GMT");

    // 6
    Append(10512, "512", true, "MPEG2TS", "DUMMY",
           "Thu Apr 2 21:22:11 2026 GMT", "Thu Apr 2 21:22:24 2026 GMT");
    // 7 - file size zero
    Append(10512, "512", true, "MPEG2TS", "MPEG",
           "Thu Apr 2 21:22:24 2026 GMT", "Thu Apr 2 21:22:24 2026 GMT");

    // 8
    Append(30001, "1", true, "MPEG2TS", "DUMMY",
           "Thu Apr 2 21:22:37 2026 GMT", "Thu Apr 2 21:22:41 2026 GMT");
    // 9 - no pgminfo
    Append(0, "1", true, "MPEG2TS", "FREEBOX",
           "Thu Apr 2 21:22:41 2026 GMT", "Thu Apr 2 21:32:41 2026 GMT");
}

// Called once.
void TestLiveTvChain::cleanupTestCase(void)
{
    m_chain.clear();
}

// Called for every test case.
void TestLiveTvChain::init(void)
{
}

// Called for every test case
void TestLiveTvChain::cleanup(void)
{
}

void TestLiveTvChain::test_totalsize(void)
{
    int actual = TotalSize();
    QCOMPARE(actual, 10);
}

void TestLiveTvChain::test_getlengthatpos(void)
{
    std::chrono::seconds actual;

    actual = GetLengthAtPos(0);
    QCOMPARE(actual, 1s);
    actual = GetLengthAtPos(1);
    QCOMPARE(actual, 10s);
    actual = GetLengthAtPos(3);
    QCOMPARE(actual, 6s);
    actual = GetLengthAtPos(6);
    QCOMPARE(actual, 13s);
}

void TestLiveTvChain::test_getnextprogram_data(void)
{
    QTest::addColumn<bool>("up");
    QTest::addColumn<int>("curpos");
    QTest::addColumn<int>("newid");
    QTest::addColumn<bool>("e_discont");
    QTest::addColumn<bool>("e_newtype");
    QTest::addColumn<QString>("e_path");

    // Get info for 1 (new is -1)
    QTest::newRow("minusone") << true << 1 << -1 << true << true <<
        "myth://localhost/HDHOMERUN/MPEG2TS/508/10508/Thu Apr 2 21:21:47 2026 GMT";

    QTest::newRow("identity") << true << 1 << 1 << true << true <<
        "myth://localhost/HDHOMERUN/MPEG2TS/508/10508/Thu Apr 2 21:21:47 2026 GMT";

    // Get info for 0 -> 1
    QTest::newRow("newtype") << true << 0 << 1 << false << true <<
        "myth://localhost/HDHOMERUN/MPEG2TS/508/10508/Thu Apr 2 21:21:47 2026 GMT";

    // Get info for 3 -> 5
    QTest::newRow("sametype") << true << 3 << 5 << true << false <<
        "myth://localhost/MPEG/MPEG2TS/513/10513/Thu Apr 2 21:22:05 2026 GMT";

    // Skip entry: 6 is dummy, 7 is zero length, 8 is dummy, 9 no pginfo
    QTest::newRow("skip,up") << true << 1 << 6 << true << true <<
        "myth://localhost/MPEG/MPEG2TS/513/10513/Thu Apr 2 21:22:05 2026 GMT";
    QTest::newRow("skip,down") << false << 1 << 8 << true << true <<
        "myth://localhost/MPEG/MPEG2TS/513/10513/Thu Apr 2 21:22:05 2026 GMT";

    // Bounce off end.  Can't bounce off top.
    QTest::newRow("bounce,up") << true << 1 << 9 << true << true <<
        "myth://localhost/MPEG/MPEG2TS/513/10513/Thu Apr 2 21:22:05 2026 GMT";
    QTest::newRow("bounce,down") << false << 1 << 0 << true << true <<
        "myth://localhost/HDHOMERUN/MPEG2TS/508/10508/Thu Apr 2 21:21:47 2026 GMT";
}

void TestLiveTvChain::test_getnextprogram(void)
{
    QFETCH(bool, up);
    QFETCH(int, curpos);
    QFETCH(int, newid);
    QFETCH(bool, e_discont);
    QFETCH(bool, e_newtype);
    QFETCH(QString, e_path);

    bool a_discont { false };
    bool a_newtype { false };

    ProgramInfo *program = DoGetNextProgram(up, curpos, newid, a_discont, a_newtype);
    QVERIFY(program != nullptr);
    if (nullptr == program)
        return;
    QString a_path = program->GetPathname();
    QCOMPARE(a_discont, e_discont);
    QCOMPARE(a_newtype, e_newtype);
    QCOMPARE(a_path, e_path);
}

QTEST_GUILESS_MAIN(TestLiveTvChain)
