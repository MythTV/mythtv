#include "mythTest.h"
#include <QtTest/QTest>

void mythTest::initTestCase()
{}

void mythTest::init()
{}

void mythTest::cleanup()
{}

void mythTest::cleanupTestCase()
{}

void mythTest::someTest()
{
    QCOMPARE(1,2);
}


QTEST_MAIN(mythTest)
#include "mythTest.moc"
