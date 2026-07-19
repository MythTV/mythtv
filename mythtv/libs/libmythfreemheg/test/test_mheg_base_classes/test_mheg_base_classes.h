/*
 *  Class TestMhegBaseClasses
 *
 *  Copyright (c) David Hampton 2025
 *
 *  See the file LICENSE_FSF for licensing information.
 */

#include <QTest>
#include <iostream>

class TestMhegBaseClasses : public QObject
{
    Q_OBJECT

    std::array<int,10> m_original {8, 2, 5, 10, 4, 6, 1, 9, 7, 3};
  private slots:
    // Before/after all test cases
    static void initTestCase(void);
    static void cleanupTestCase(void);

    // Before/after each test cases
    static void init(void);
    static void cleanup(void);

    void test_sequence(void);
    void test_sequence2(void);
    void test_ownptr_sequence(void);
    void test_stack(void);
    static void test_octetstring_int8(void);
    static void test_octetstring_uint8(void);
};
