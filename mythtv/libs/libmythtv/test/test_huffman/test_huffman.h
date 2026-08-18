#ifndef LIBMYTHTV_TEST_HUFFMAN_H
#define LIBMYTHTV_TEST_HUFFMAN_H

#include <QChar>     // Fix Qt6 GCC SFINAE warning
#include <QBitArray> // Fix Qt6 GCC SFINAE warning
#include <QObject>

class TestHuffman: public QObject
{
    Q_OBJECT;

  private slots:
    static void initTestCase(void);    // once
    void cleanupTestCase(void); // once
    void init(void);            // per test case
    void cleanup(void);         // per test case

    static void test_atsc1_data(void);
    static void test_atsc1(void);
    static void test_atsc2_data(void);
    static void test_atsc2(void);
    static void test_freesat_data(void);
    static void test_freesat(void);
};

#endif // LIBMYTHTV_TEST_HUFFMAN_H
