#include "test_huffman.h"

#include <fcntl.h>

#include <QTest>

#include "libmythtv/mpeg/atsc_huffman.h"
#include "libmythtv/mpeg/freesat_huffman.h"

// Called once. Computes the temporary socket name that will be used
// throughout.  With Qt, you have to actually create the file before
// the filename is available.
void TestHuffman::initTestCase(void)
{
}

// Called once.
void TestHuffman::cleanupTestCase(void)
{
}

// Called for every test case.
void TestHuffman::init(void)
{
}

// Called for every test case
void TestHuffman::cleanup(void)
{
}

template <typename T>
QByteArray SAtoQBA (T arr)
{
    return { (const char *)arr.data(), arr.size() };
}

// For the atsc functions, the table index is specified separately.
void TestHuffman::test_atsc1_data(void)
{
    QTest::addColumn<int>("table");
    QTest::addColumn<QByteArray>("input");
    QTest::addColumn<QString>("expected");

    const std::array<uint8_t,2> t1 =
        { // T:010  V:111100  end: 000
          0b01011110, 0b00000000  };
    const std::array<uint8_t,3> t2 =
        { // T:1101  V:1000  escape:111  .:00101110  end:1
          0b11011000, 0b11100101, 0b11010000 };
    const std::array<uint8_t,8> loremipsum1 =
        { // L:11101  o:00  r:101  e:101  m:1001101  space:001
          // i:010010  p:000101  s:10000  u:0010100  m:10010  end:0100
          0b11101001, 0b01101100, 0b11010010, 0b10010000,
          0b10110000, 0b00101001, 0b00100100 };
    const std::array<uint8_t,8> loremipsum2 =
        { // L:10011  o:10  r:011  e:101  m:100111  space:111
          // i:10101  p:000110  s:01100  u:110010  m:111111
          // .:110101  end:1
          0b10011100, 0b11101100, 0b11111110, 0b10100011,
          0b00110011, 0b00101111, 0b11110101, 0b10000000 };
    const std::array<uint8_t,9> loremescape1 =
        { // L:11101  o:00  r:101  e:101  escape:1010111100  m:01101101  space:001
          // i:010010  p:000101  s:10000  u:0010100  m:10010  end:0100
          0b11101001, 0b01101101, 0b01111000, 0b11011010,
          0b01010010, 0b00010110, 0b00000101, 0b00100100,
          0b10000000 };
    const std::array<uint8_t,9> loremescape2 =
        { // L:10011  o:10  r:011  e:101  escape:101110011 m:01101101  space:111
          // i:10101  p:000110  s:01100  u:110010  m:111111
          // .:110101  end:1
          0b10011100, 0b11101101, 0b11001101, 0b10110111,
          0b11010100, 0b01100110, 0b01100101, 0b11111110,
          0b10110000 };

    QTest::newRow("minimal 1")      << 1 << SAtoQBA(t1) << "TV";
    QTest::newRow("minimal 2")      << 2 << SAtoQBA(t2) << "TV.";
    QTest::newRow("Lorem ipsum 1")  << 1 << SAtoQBA(loremipsum1) << "Lorem ipsum";
    QTest::newRow("Lorem ipsum 2")  << 2 << SAtoQBA(loremipsum2) << "Lorem ipsum.";
    QTest::newRow("Lorem escape 1") << 1 << SAtoQBA(loremescape1) << "Lorem ipsum";
    QTest::newRow("Lorem escape 2") << 2 << SAtoQBA(loremescape2) << "Lorem ipsum.";
}

void TestHuffman::test_atsc1(void)
{
    QFETCH(int, table);
    QFETCH(const QByteArray, input);
    QFETCH(const QString, expected);

    QString actual = atsc_huffman1_to_string(reinterpret_cast<const uint8_t *>(input.data()), input.size(), table);
    QCOMPARE(actual, expected);
}

// For the atsc functions, the table index is specified separately.
void TestHuffman::test_atsc2_data(void)
{
    QTest::addColumn<int>("table");
    QTest::addColumn<QByteArray>("input");
    QTest::addColumn<QString>("expected");

    const std::array<uint8_t,2> t1 =
        { // T:1100010  V:11100101
          0b11000101, 0b11001010  };
    const std::array<uint8_t,3> t2 =
        { // T:11100100  V:11101011 .:110 0101
          0b11100100, 0b11101011, 0b11001010 };
    const std::array<uint8_t,7> loremipsum1 =
        { // L:1101001  o:0101  r:100000  e:0010  m:101000  space:000
          // i:100001  p:100111  s:0110  u:100101  m:101000  end:0100
          0b11010010, 0b10110000, 0b00010101, 0b00000010,
          0b00011001, 0b11011010, 0b01011010 };
    const std::array<uint8_t,8> loremipsum2 =
        { // L:11100000  o:10001  r:0101  e:0100  m:1100001  space:00
          // i:10011  p:1100000  s:10010  u:10101  m:1100001
          0b11100000, 0b10001010, 0b10100110, 0b00010010,
          0b01111000, 0b00100101, 0b01011100, 0b00100000 };

    QTest::newRow("minimal 1")     << 1 << SAtoQBA(t1) << "TV";
    QTest::newRow("minimal 2")     << 2 << SAtoQBA(t2) << "TV.";
    QTest::newRow("Lorem ipsum 1") << 1 << SAtoQBA(loremipsum1) << "Lorem ipsum";
    QTest::newRow("Lorem ipsum 2") << 2 << SAtoQBA(loremipsum2) << "Lorem ipsum ";
}

void TestHuffman::test_atsc2(void)
{
    QFETCH(int, table);
    QFETCH(const QByteArray, input);
    QFETCH(const QString, expected);

    QString actual = atsc_huffman2_to_string(reinterpret_cast<const uint8_t *>(input.data()), input.size(), table);
    QCOMPARE(actual, expected);
}

// For the freesat functions, the table index is embedded in the data.
void TestHuffman::test_freesat_data(void)
{
    QTest::addColumn<QByteArray>("input");
    QTest::addColumn<QString>("expected");

    const std::array<uint8_t,4> t1 =
        { // Type 1
          0, 1,
          // T:00  end:1101100101
          0b00110110, 0b01010000 };
    const std::array<uint8_t,4> t1a =
        { // Type 1
          0, 1,
          // T:00  V:111  end:0101110
          0b00111010, 0b11100000 };
    const std::array<uint8_t,6> t2 =
        { // Type 2 - no transition to end from 'V'
          0, 2,
          // T:111  escape:1110111101010010  end:00000000
          0b11111101, 0b11101010, 0b01000000, 0b00000000 };
    const std::array<uint8_t,6> t2a =
        { // Type 2 - no transition to end from 'V'
          0, 2,
          // T:111 V:1010  escape:10010100010  end:00000000
          0b11110101, 0b00101000, 0b10000000, 0b00000000 };
    const std::array<uint8_t,10> loremipsum1 =
        { // Type 1
          0, 1,
          // L:01100  o:10  r:00  e:101  m:001110  space:011
          // i:1110111  p:001000  s:0110  u:0101111  m:0101  end:0001
          0b01100100, 0b01010011, 0b10011111, 0b01110010,
          0b00011001, 0b01111010, 0b10001000 };
    const std::array<uint8_t,10> loremipsum2 =
        { // Type 2
          0, 2,
          // L:101101  o:10  r:100  e:111  m:011100  space:111
          // i:11101  p:001010  s:10010  u:110100  m:10011  end:10101010100
          0b10110110, 0b10011101, 0b11001111, 0b11010010,
          0b10100101, 0b10100100, 0b11101010, 0b10100000 };
    const std::array<uint8_t,11> loremescape1 =
        { // Type 1
          0, 1,
          // L:01100  o:10  r:00  e:101  escape:1101010110000000 m:01101101  space:011
          // i:1110111  p:001000  s:0110  u:0101111  m:0101  end:0001
          0b01100100, 0b01011101, 0b01011000, 0b00000110,
          0b11010111, 0b11011100, 0b10000110, 0b01011110,
          0b10100010 };
    const std::array<uint8_t,12> loremescape2 =
        { // Type 2
          0, 2,
          // L:101101  o:10  r:100  e:111  escape:0111011100101100  m:01101101  space:111
          // i:11101  p:001010  s:10010  u:110100  m:10011  end:10101010100
          0b10110110, 0b10011101, 0b11011100, 0b10110001,
          0b10110111, 0b11110100, 0b10101001, 0b01101001,
          0b00111010, 0b10101000 };

    QTest::newRow("minimal 1")      << SAtoQBA(t1)  << "T";
    QTest::newRow("minimal 1a")     << SAtoQBA(t1a) << "TV";
    QTest::newRow("minimal 2")      << SAtoQBA(t2)  << "T";
    QTest::newRow("minimal 2a")     << SAtoQBA(t2a) << "TV";
    QTest::newRow("Lorem ipsum 1")  << SAtoQBA(loremipsum1)  << "Lorem ipsum";
    QTest::newRow("Lorem ipsum 2")  << SAtoQBA(loremipsum2)  << "Lorem ipsum";
    QTest::newRow("Lorem escape 1") << SAtoQBA(loremescape1) << "Lorem ipsum";
    QTest::newRow("Lorem escape 2") << SAtoQBA(loremescape2) << "Lorem ipsum";
}

void TestHuffman::test_freesat(void)
{
    QFETCH(const QByteArray, input);
    QFETCH(const QString, expected);

    QString actual = freesat_huffman_to_string(reinterpret_cast<const uint8_t *>(input.data()), input.size());
    QCOMPARE(actual, expected);
}

QTEST_APPLESS_MAIN(TestHuffman)
