/*
 *  Class TestMhegParser
 *
 *  Copyright (c) David Hampton 2025
 *
 *  See the file LICENSE_FSF for licensing information.
 */

#include <QTest>
#include <iostream>
//#include "recordingextender.h"

class TestMhegParser : public QObject
{
    Q_OBJECT

  private slots:
    // Before/after all test cases
    static void initTestCase(void);
    static void cleanupTestCase(void);

    // Before/after each test cases
    static void init(void);
    static void cleanup(void);

    static void test_parser_asn1(void);
    static void test_parser_text(void);
};
