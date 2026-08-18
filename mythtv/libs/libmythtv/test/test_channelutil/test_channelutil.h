#ifndef LIBMYTHTV_TEST_CHANNELUTIL_H
#define LIBMYTHTV_TEST_CHANNELUTIL_H

#include <QChar>     // Fix Qt6 GCC SFINAE warning
#include <QBitArray> // Fix Qt6 GCC SFINAE warning
#include <QObject>

#include "libmythtv/channelinfo.h"
#include "libmythtv/channelutil.h"
#include "libmythtv/tv.h"

class TestChannelUtil: public QObject
{
    Q_OBJECT;

    ChannelInfoList m_channels;

  private slots:
    void initTestCase(void);    // once
    void cleanupTestCase(void); // once
    void init(void);            // per test case
    void cleanup(void);         // per test case

    static void test_getnextchannel_data(void);
           void test_getnextchannel(void);
};

#endif // LIBMYTHTV_TEST_CHANNELUTIL_H
