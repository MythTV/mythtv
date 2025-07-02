/*
 *  Class TestCommFlagMisc
 *
 *  Copyright (c) David Hampton 2025
 *
 *  See the file LICENSE_FSF for licensing information.
 */

#include <QTest>
#include <iostream>

class TestCommFlagMisc : public QObject
{
    Q_OBJECT

  private slots:
    // Before/after all test cases
    static void initTestCase(void);
    static void cleanupTestCase(void);

    // Before/after each test cases
    static void init(void);
    static void cleanup(void);

    // Degenerate cases
    static void test_quick_select_size_one(void);
    static void test_quick_select_size_two(void);

    // Even/odd lengths
    static void test_quick_select_size_even(void);
    static void test_quick_select_size_odd(void);

    // Duplicates
    static void test_quick_select_dups(void);

    // uchar
    static void test_quick_select8_data(void);
    static void test_quick_select8(void);

    // ushort
    static void test_quick_select16_data(void);
    static void test_quick_select16(void);

    // float
    static void test_quick_selectf_data(void);
    static void test_quick_selectf(void);
};
