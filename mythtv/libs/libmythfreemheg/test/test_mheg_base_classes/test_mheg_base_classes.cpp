/* *  Class TestMhegBaseClasses
 *
 *  Copyright (c) David Hampton 2025
 *
 *  See the file LICENSE_FSF for licensing information.
 */

#include "test_mheg_base_classes.h"

#include <memory>

#include "libmythfreemheg/ASN1Codes.h"
#include "libmythfreemheg/BaseClasses.h"

std::vector<uint8_t> data_asn1 = {
};

QByteArray data_text = "";



// Before all test cases
void TestMhegBaseClasses::initTestCase()
{
    MHSetLogging(stdout, MHLogError | MHLogWarning );
}

// After all test cases
void TestMhegBaseClasses::cleanupTestCase()
{
}

// Before each test case
void TestMhegBaseClasses::init()
{
}

// After each test case
void TestMhegBaseClasses::cleanup()
{
}

void TestMhegBaseClasses::test_sequence(void)
{
    MHSequence<int> seq;

    QCOMPARE(seq.Size(), 0);

    for (int i : original)
        seq.Append(i);

    // Test appends
    QCOMPARE(seq.Size(), 10);
    QCOMPARE(seq.GetAt(0), 8);
    QCOMPARE(seq.GetAt(4), 4);
    QCOMPARE(seq[4], 4);
    QCOMPARE(seq[9], 3);

    // Beginning
    QCOMPARE(seq[0], 8);
    seq.InsertAt(42, 0);
    QCOMPARE(seq.Size(), 11);
    QCOMPARE(seq[0], 42);
    QCOMPARE(seq[1], 8);
    seq.RemoveAt(0);
    QCOMPARE(seq.Size(), 10);
    QCOMPARE(seq[0], 8);

    // Middle
    QCOMPARE(seq[2], 5);
    seq.InsertAt(42, 2);
    QCOMPARE(seq.Size(), 11);
    QCOMPARE(seq[2], 42);
    QCOMPARE(seq[3], 5);
    seq.RemoveAt(2);
    QCOMPARE(seq.Size(), 10);
    QCOMPARE(seq[2], 5);

    // End
    QCOMPARE(seq[9], 3);
    seq.InsertAt(42, 10);
    QCOMPARE(seq.Size(), 11);
    QCOMPARE(seq[9], 3);
    QCOMPARE(seq[10], 42);
    seq.RemoveAt(10);
    QCOMPARE(seq.Size(), 10);
    QCOMPARE(seq[9], 3);
}

void TestMhegBaseClasses::test_sequence2(void)
{
    class Simple {
      public:
        int value;
    };
    MHSequence<Simple> seq;

    QCOMPARE(seq.Size(), 0);

    for (int i : original)
    {
        Simple a {};
        a.value = i;
        seq.Append(a);
    }
    Simple b {};
    b.value = 42;

    // Test appends
    QCOMPARE(seq.Size(), 10);
    QCOMPARE(seq.GetAt(0).value, 8);
    QCOMPARE(seq.GetAt(4).value, 4);
    QCOMPARE(seq[4].value, 4);
    QCOMPARE(seq[9].value, 3);

    // Beginning
    QCOMPARE(seq[0].value, 8);
    seq.InsertAt(b, 0);
    QCOMPARE(seq.Size(), 11);
    QCOMPARE(seq[0].value, 42);
    QCOMPARE(seq[1].value, 8);
    seq.RemoveAt(0);
    QCOMPARE(seq.Size(), 10);
    QCOMPARE(seq[0].value, 8);

    // Middle
    QCOMPARE(seq[2].value, 5);
    seq.InsertAt(b, 2);
    QCOMPARE(seq.Size(), 11);
    QCOMPARE(seq[2].value, 42);
    QCOMPARE(seq[3].value, 5);
    seq.RemoveAt(2);
    QCOMPARE(seq.Size(), 10);
    QCOMPARE(seq[2].value, 5);

    // End
    QCOMPARE(seq[9].value, 3);
    seq.InsertAt(b, 10);
    QCOMPARE(seq.Size(), 11);
    QCOMPARE(seq[9].value, 3);
    QCOMPARE(seq[10].value, 42);
    seq.RemoveAt(10);
    QCOMPARE(seq.Size(), 10);
    QCOMPARE(seq[9].value, 3);
}

void TestMhegBaseClasses::test_ownptr_sequence(void)
{
    class Simple {
      public:
        int value;
    };
    MHOwnPtrSequence<Simple> seq;

    QCOMPARE(seq.Size(), 0);

    for (int i : original)
    {
        auto* a = new Simple;
        a->value = i;
        seq.Append(a);
    }

    // Test appends
    QCOMPARE(seq.Size(), 10);
    QCOMPARE(seq.GetAt(0)->value, 8);
    QCOMPARE(seq.GetAt(4)->value, 4);
    QCOMPARE(seq[4]->value, 4);
    QCOMPARE(seq[9]->value, 3);

    // Beginning
    QCOMPARE(seq[0]->value, 8);
    auto* b = new Simple;
    b->value = 42;
    seq.InsertAt(b, 0);
    QCOMPARE(seq.Size(), 11);
    QCOMPARE(seq[0]->value, 42);
    QCOMPARE(seq[1]->value, 8);
    seq.RemoveAt(0);
    QCOMPARE(seq.Size(), 10);
    QCOMPARE(seq[0]->value, 8);

    // Middle
    QCOMPARE(seq[2]->value, 5);
    b = new Simple;
    b->value = 42;
    seq.InsertAt(b, 2);
    QCOMPARE(seq.Size(), 11);
    QCOMPARE(seq[2]->value, 42);
    QCOMPARE(seq[3]->value, 5);
    seq.RemoveAt(2);
    QCOMPARE(seq.Size(), 10);
    QCOMPARE(seq[2]->value, 5);

    // End
    QCOMPARE(seq[9]->value, 3);
    b = new Simple;
    b->value = 42;
    seq.InsertAt(b, 10);
    QCOMPARE(seq.Size(), 11);
    QCOMPARE(seq[9]->value, 3);
    QCOMPARE(seq[10]->value, 42);
    seq.RemoveAt(10);
    QCOMPARE(seq.Size(), 10);
    QCOMPARE(seq[9]->value, 3);
}

void TestMhegBaseClasses::test_stack(void)
{
    MHStack<int> stack;

    QCOMPARE(stack.Size(), 0);
    QCOMPARE(stack.Empty(), true);

    for (int i : original)
        stack.Push(i);

    QCOMPARE(stack.Size(), 10);
    QCOMPARE(stack.Empty(), false);

    QCOMPARE(stack.Top(),  3);
    QCOMPARE(stack.Pop(),  3);
    QCOMPARE(stack.Top(),  7);
    QCOMPARE(stack.Size(), 9);
}

/* Test using the char* constructor */
void TestMhegBaseClasses::test_octetstring_int8(void)
{
    const char *str1 = "Lorem ipsum dolor sit amet, consectetur porttitor.";
    const char *str2 = "Lorem ipsum dolor sit amet, consectetur tincidunt.";
    const char *str3 = "Lorem ipsum dolor sit amet!";

    // Constructor 2, Size
    MHOctetString r {str1};
    QCOMPARE(r.Size(), 50L);

    // Constructor 2, Size, Compare, Equal, GetAt, Bytes
    MHOctetString s {str1, 50};
    QCOMPARE(s.Size(), 50L);
    const auto* bytes = reinterpret_cast<const char*>(s.Bytes());
    int len = strlen(bytes);
    QCOMPARE(len, 50);
    QVERIFY(strcmp(bytes, str1) == 0);
    QVERIFY(s.Compare(str1) == 0);
    QVERIFY(s.Compare(str2)  > 0);
    QVERIFY(s.Compare(str3) <  0);
    QCOMPARE(s.Equal(str1), true);
    QCOMPARE(s.Equal(str2), false);
    QCOMPARE(s.Equal(str3), false);
    QCOMPARE(s.GetAt( 0), 'L');
    QCOMPARE(s.GetAt( 1), 'o');
    QCOMPARE(s.GetAt(49), '.');

    // Constructor 4
    MHOctetString a(s,10,5);
    QCOMPARE(a.Size(), 5);
    QCOMPARE(a.GetAt(0), 'm');
    QCOMPARE(a.GetAt(4), 'l');
    QCOMPARE(a.Printable(), "m dol");

    // Constructor 5 - Ambiguous, won't compile.
    // MHOctetString b(s);
    // QCOMPARE(b.Size(), 50);
    // QCOMPARE(b.Equal(str1), true);

    // Constructor 1, Copy
    MHOctetString c;
    c.Copy(s);
    QCOMPARE(c.Size(), 50);
    QCOMPARE(c.Equal(str1), true);

    // Constructor 1, operator=
    MHOctetString d;
    d = s;
    QCOMPARE(d.Size(), 50);
    QCOMPARE(d.Equal(str1), true);

    // Append
    c.Append(d);
    QCOMPARE(c.Size(), 100);
}

/* Test using the uchar* constructor */
void TestMhegBaseClasses::test_octetstring_uint8(void)
{
    const std::array<uint8_t,16> ustr1 { // "Lorem ipsum.\0Id."
        0x4C, 0x6F, 0x72, 0x6E, 0x6D, 0x20, 0x69, 0x70,
        0x73, 0x75, 0x6D, 0x2E, 0x00, 0x4A, 0x64, 0x2E };
    const std::array<uint8_t,16> ustr2 { // "Lorem ipsum.\0Ut."
        0x4C, 0x6F, 0x72, 0x6E, 0x6D, 0x20, 0x69, 0x70,
        0x73, 0x75, 0x6D, 0x2E, 0x00, 0x55, 0x74, 0x2E };
    const std::array<uint8_t,12> ustr3 { // "Lorem ipsum!"
        0x4C, 0x6F, 0x72, 0x6E, 0x6D, 0x20, 0x69, 0x70,
        0x73, 0x75, 0x6D, 0x21 };

    MHOctetString s1 {ustr1.data(), ustr1.size()};
    MHOctetString s2 {ustr2.data(), ustr2.size()};
    MHOctetString s3 {ustr3.data(), ustr3.size()};

    QCOMPARE(s1.Size(), 16L);
    const auto* bytes = reinterpret_cast<const char*>(s1.Bytes());
    int len = strlen(bytes);
    QCOMPARE(len, 12);  // Don't use strlen. There can be embedded NUL characters.
    QVERIFY(memcmp(bytes, ustr1.data(), ustr1.size()) == 0);
    QVERIFY(s1.Compare(s1) == 0);
    QVERIFY(s1.Compare(s2)  > 0);
    QVERIFY(s1.Compare(s3) <  0);
    QCOMPARE(s1.Equal(s1), true);
    QCOMPARE(s1.Equal(s2), false);
    QCOMPARE(s1.Equal(s3), false);
    QCOMPARE(s1.GetAt( 0), 'L');
    QCOMPARE(s1.GetAt( 1), 'o');
    QCOMPARE(s1.GetAt(15), '.');
}


QTEST_GUILESS_MAIN(TestMhegBaseClasses)
