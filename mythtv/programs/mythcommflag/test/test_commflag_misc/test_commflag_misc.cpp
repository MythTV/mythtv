/*
 *  Class TestCommFlagMisc
 *
 *  Copyright (c) David Hampton 2025
 *
 *  See the file LICENSE_FSF for licensing information.
 */

#include "test_commflag_misc.h"
#include "quickselect.h"

static constexpr char const * const TESTNAME = "test_commflag_misc";
static constexpr char const * const TESTVERSION = "test_commflag_misc_1.0";

//////////////////////////////////////////////////

// Before all test cases
void TestCommFlagMisc::initTestCase()
{
}

// After all test cases
void TestCommFlagMisc::cleanupTestCase()
{
}

// Before each test case
void TestCommFlagMisc::init()
{
}

// After each test case
void TestCommFlagMisc::cleanup()
{
}

void TestCommFlagMisc::test_quick_select_size_one(void)
{
    std::vector<uint8_t> data = { 8 };

    auto res = quick_select_median<uint8_t>(data.data(), data.size());
    QCOMPARE(res, 8);

    res = quick_select<uint8_t>(data.data(), data.size(), 0);
    QCOMPARE(res, 8);
}

void TestCommFlagMisc::test_quick_select_size_two(void)
{
    std::vector<uint8_t> data = { 42, 8 };

    auto res = quick_select_median<uint8_t>(data.data(), data.size());
    QCOMPARE(res, 8);

    res = quick_select<uint8_t>(data.data(), data.size(), 0);
    QCOMPARE(res, 8);
    res = quick_select<uint8_t>(data.data(), data.size(), 1);
    QCOMPARE(res, 42);
}

void TestCommFlagMisc::test_quick_select_size_even(void)
{
    std::vector<uint8_t> data
        { 8, 138, 108, 168, 118, 117, 115, 56 };

    auto res = quick_select_median<uint8_t>(data.data(), data.size());
    QCOMPARE(res, 115);

    res = quick_select<uint8_t>(data.data(), data.size(), 1);
    QCOMPARE(res, 56);
    res = quick_select<uint8_t>(data.data(), data.size(), 6);
    QCOMPARE(res, 138);
}

void TestCommFlagMisc::test_quick_select_size_odd(void)
{
    std::vector<uint8_t> data
        { 8, 138, 108, 168, 118, 117, 115 };

    auto res = quick_select_median<uint8_t>(data.data(), data.size());
    QCOMPARE(res, 117);

    res = quick_select<uint8_t>(data.data(), data.size(), 1);
    QCOMPARE(res, 108);
    res = quick_select<uint8_t>(data.data(), data.size(), 6);
    QCOMPARE(res, 168);
}


void TestCommFlagMisc::test_quick_select_dups(void)
{
    std::vector<uint8_t> data = {
          8, 138, 117, 168, 118, 8, 117,
    };

    auto res = quick_select_median<uint8_t>(data.data(), data.size());
    QCOMPARE(117, res);

    res = quick_select<uint8_t>(data.data(), data.size(), 1);
    QCOMPARE(8, res);
    res = quick_select<uint8_t>(data.data(), data.size(), 2);
    QCOMPARE(117, res);
    res = quick_select<uint8_t>(data.data(), data.size(), 3);
    QCOMPARE(117, res);
    res = quick_select<uint8_t>(data.data(), data.size(), 4);
    QCOMPARE(118, res);
}

void TestCommFlagMisc::test_quick_select8_data(void)
{
    QTest::addColumn<int>("select");
    QTest::addColumn<int>("expected");

    QTest::newRow("median")    << -1 << 118;
    QTest::newRow("start")     <<  0 <<   8;
    QTest::newRow("start+1")   <<  1 <<  39;
    QTest::newRow("start+2")   <<  2 <<  43;
    QTest::newRow("middle-2")  << 13 << 115;
    QTest::newRow("middle-1")  << 14 << 117;
    QTest::newRow("middle")    << 15 << 118;
    QTest::newRow("middle+1")  << 16 << 121;
    QTest::newRow("middle+2")  << 17 << 123;
    QTest::newRow("end-2")     << 29 << 227;
    QTest::newRow("end-1")     << 30 << 240;
    QTest::newRow("end")       << 31 << 252;
}

void TestCommFlagMisc::test_quick_select8(void)
{
    std::vector<uint8_t> data = {
        8, 138, 108, 168, 118, 117, 115,  56,
        126,  81, 193,  87,  43, 227, 106,  50,
        121,  47, 240, 214, 252, 220, 210,  39,
        190, 106, 169, 226, 158, 123, 108,  88
    };

    QFETCH(int, select);
    QFETCH(int, expected);

    if (select == -1)
    {
        auto res = quick_select_median<uint8_t>(data.data(), data.size());
        QCOMPARE(res, expected);
        return;
    }

    auto res = quick_select<uint8_t>(data.data(), data.size(), select);
    QCOMPARE(res, expected);
}

void TestCommFlagMisc::test_quick_select16_data(void)
{
    QTest::addColumn<int>("select");
    QTest::addColumn<int>("expected");
    
    QTest::newRow("median")    << -1 << 30390;
    QTest::newRow("start")     <<  0 <<  5434;
    QTest::newRow("start+1")   <<  1 <<  6298;
    QTest::newRow("start+2")   <<  2 <<  8367;
    QTest::newRow("middle-2")  << 13 << 23736;
    QTest::newRow("middle-1")  << 14 << 24641;
    QTest::newRow("middle")    << 15 << 30390;
    QTest::newRow("middle+1")  << 16 << 32079;
    QTest::newRow("middle+2")  << 17 << 32997;
    QTest::newRow("end-2")     << 29 << 61564;
    QTest::newRow("end-1")     << 30 << 64234;
    QTest::newRow("end")       << 31 << 64791;
}

void TestCommFlagMisc::test_quick_select16(void)
{
    std::vector<uint16_t> data = {
        23132, 56221, 57681,  9907, 13602, 30390, 64791, 24641,
        14608, 32997, 12830, 17196, 61564, 61452, 14972,  8367,
        19259, 64234,  6298, 23736, 36542, 45571, 46701, 59332,
        48687, 32079, 17199, 36902,  5434, 33665, 12342, 35164 };

    QFETCH(int, select);
    QFETCH(int, expected);

    if (select == -1)
    {
        auto res = quick_select_median<uint16_t>(data.data(), data.size());
        QCOMPARE(res, expected);
        return;
    }

    auto res = quick_select<uint16_t>(data.data(), data.size(), select);
    QCOMPARE(res, expected);
}

void TestCommFlagMisc::test_quick_selectf_data(void)
{
    QTest::addColumn<int>("select");
    QTest::addColumn<qreal>("expected");

    QTest::newRow("median")    << -1 << 43.71;
    QTest::newRow("start")     <<  0 <<  1.95;
    QTest::newRow("start+1")   <<  1 <<  4.67;
    QTest::newRow("start+2")   <<  2 <<  7.46;
    QTest::newRow("middle-2")  << 13 << 42.24;
    QTest::newRow("middle-1")  << 14 << 43.66;
    QTest::newRow("middle")    << 15 << 43.71;
    QTest::newRow("middle+1")  << 16 << 51.18;
    QTest::newRow("middle+2")  << 17 << 51.29;
    QTest::newRow("end-2")     << 29 << 90.78;
    QTest::newRow("end-1")     << 30 << 95.88;
    QTest::newRow("end")       << 31 << 97.48;
}

void TestCommFlagMisc::test_quick_selectf(void)
{
    std::vector<float> data = {
        90.62, 31.92, 89.50, 97.48,  1.95, 59.99,  4.67, 51.18,
        43.71, 84.70, 43.66, 20.05, 34.40, 90.78, 51.29, 32.07,
        71.23, 24.45, 61.72, 26.97, 17.47, 66.35, 80.53, 24.52,
         7.46, 87.38, 95.88, 42.24, 72.16, 64.72, 29.13, 13.80
    };

    QFETCH(int, select);
    QFETCH(qreal, expected);
    auto expectedf = static_cast<float>(expected);

    if (select == -1)
    {
        auto res = quick_select_median<float>(data.data(), data.size());
        QCOMPARE(expectedf, res);
        return;
    }

    auto res = quick_select<float>(data.data(), data.size(), select);
    QCOMPARE(expectedf, res);
}

QTEST_GUILESS_MAIN(TestCommFlagMisc)
