#ifndef LIBMYTHUI_TEST_LIRC_H
#define LIBMYTHUI_TEST_LIRC_H

#include <QLocalServer>
#include <QLocalSocket>
#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryFile>
#include <QThread>

#include "lirc_client.h"

#include <QtGlobal>
#if QT_VERSION >= QT_VERSION_CHECK(6,5,0)
#include <QtProcessorDetection>
#endif

class TestLirc: public QObject
{
    Q_OBJECT;

    static bool            s_shutting_down;

    static uint16_t      s_tcp_port;
    static QThread      *s_tcp_thread;
    static QThread      *s_unix_thread;
    static QString       s_unix_sockname;
    static std::vector<std::string> s_response;

    struct lirc_state *m_state {nullptr};
    struct lirc_config *m_config { nullptr };

    static void fakeUnixHandler(QLocalSocket *sock);
    static void fakeUnixServer(void);
    static void fakeTcpHandler(QTcpSocket *sock);
    static void fakeTcpServer(void);

  private slots:
    static void initTestCase(void);    // once
    void cleanupTestCase(void); // once
    void init(void);            // per test case
    void cleanup(void);         // per test case

    static void test_init(void);
    static void test_trim_data(void);
    static void test_trim(void);
    static void test_parse_string_data(void);
           void test_parse_string(void);
    static void test_parse_include_data(void);
    static void test_parse_include(void);
           void test_mode(void);
    static void test_parse_flags_data(void);
           void test_parse_flags(void);
    static void test_getfilename_data(void);
           void test_getfilename(void);
    static void test_open_data(void);
           void test_open(void);
           void test_readconfig_internal1(void);
           void test_readconfig_internal2(void);
           void test_readconfig_internal3(void);
           void test_readconfig_internal4(void);
           void test_readconfig_internal5(void);
           void test_readconfig1(void);
           void test_readconfig2(void);
           void test_code2char_internal(void);
           void test_readstring(void);
    static void test_getsocketname(void);
#ifndef Q_PROCESSOR_S390
#ifdef INCLUDE_TCP_SERVER
           void test_tcp_sending(void);
#endif
           static void test_udp_sending(void);
           void test_identify(void);
           void test_send_command(void);
#endif
};

#endif // LIBMYTHUI_TEST_LIRC_H
