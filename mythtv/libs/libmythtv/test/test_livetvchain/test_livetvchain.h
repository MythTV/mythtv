#ifndef LIBMYTHTV_TEST_LIVETVCHAIN_H
#define LIBMYTHTV_TEST_LIVETVCHAIN_H

#include <QChar>     // Fix Qt6 GCC SFINAE warning
#include <QBitArray> // Fix Qt6 GCC SFINAE warning
#include <QObject>
#include <QString>

#include "libmythtv/livetvchain.h"

class TestLiveTvChain: public QObject, LiveTVChain
{
    Q_OBJECT;

    // Helper function
    void Append(uint chanid, const QString& channum, bool discont,
                const QString& inputname, const QString& inputtype,
                const QString& starttime, const QString& endtime);

    // Override LiveTVChain function because we have no database.
    ProgramInfo *EntryToProgram(const LiveTVChainEntry &entry) const override;

    // Test functions
  private slots:
           void initTestCase(void);    // once
           void cleanupTestCase(void); // once
    static void init(void);            // per test case
    static void cleanup(void);         // per test case

           void test_totalsize(void);
           void test_getlengthatpos(void);
    static void test_getnextprogram_data(void);
           void test_getnextprogram(void);
};

#endif // LIBMYTHTV_TEST_LIVETVCHAIN_H
