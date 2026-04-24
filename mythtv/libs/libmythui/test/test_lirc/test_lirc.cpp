#include "test_lirc.h"

#include <fcntl.h>
#include <iostream>

#include <QTest>

#include "lirc_client.cpp" // NOLINT(bugprone-suspicious-include)

static constexpr int SERVER_TIMEOUT { 50 }; //milliseconds

bool            TestLirc::s_shutting_down { false };

uint16_t        TestLirc::s_tcp_port      { 0 };
QThread        *TestLirc::s_tcp_thread    { nullptr };
QThread        *TestLirc::s_unix_thread   { nullptr };
QString         TestLirc::s_unix_sockname {};
std::vector<std::string> TestLirc::s_response {};


void TestLirc::fakeUnixHandler(QLocalSocket *sock)
{
    if (nullptr == sock)
        return;
    while (!s_shutting_down)
    {
        if (!sock->waitForReadyRead(SERVER_TIMEOUT))
            continue;
        QByteArray const data = sock->readAll(); // clazy:exclude=unused-non-trivial-variable
        // std::cerr << "fakeUnixHandler received: '" << data.data() << "'\n";
        for (auto string : s_response) {
            // std::cerr << "fakeUnixHandler sending: '" << string.data() << "'\n";
            sock->write(string.data());
        }
    }
    sock->close();
    delete sock;
}

void TestLirc::fakeUnixServer(void)
{
    auto *server = new QLocalServer;
    if (nullptr == server)
    {
        std::cerr << "QLocalServer allocation failed\n";
        return;
    }
    //std::cerr << "QLocalServer name: '" << qPrintable(s_unix_sockname) << "'\n";
    if (!server->listen(s_unix_sockname))
    {
        std::cerr << "QLocalServer listen() call failed: ("
                  << server->serverError() << ") "
                  << qPrintable(server->errorString()) << "\n";
        return;
    }
    while (!s_shutting_down)
    {
        server->waitForNewConnection(SERVER_TIMEOUT);
        fakeUnixHandler(server->nextPendingConnection());
    }
    server->close();
    delete server;
}

#ifdef INCLUDE_TCP_SERVER
void TestLirc::fakeTcpHandler(QTcpSocket *sock)
{
    QByteArray data;
    if (nullptr == sock)
        return;
    while (!s_shutting_down)
    {
        if (!sock->waitForReadyRead(SERVER_TIMEOUT))
            continue;
        data = sock->readAll();
        for (auto string : s_response)
            sock->write(string.data());
    }
    data = sock->readAll();
    sock->close();
    delete sock;
}

void TestLirc::fakeTcpServer(void)
{
    auto *server = new QTcpServer;
    if (nullptr == server)
    {
        std::cerr << "QTcpServer allocation failed\n";
        return;
    }
    if (!server->listen(QHostAddress::LocalHost))
    {
        std::cerr << "QTcpServer listen() call failed.\n";
        return;
    }
    s_tcp_port = server->serverPort();

    while (!s_shutting_down)
    {
        server->waitForNewConnection(SERVER_TIMEOUT);
        fakeTcpHandler(server->nextPendingConnection());
    }
    server->close();
    delete server;
}
#endif

// Called once. Computes the temporary socket name that will be used
// throughout.  With Qt, you have to actually create the file before
// the filename is available.
void TestLirc::initTestCase(void)
{
    QTemporaryFile tmpfile { "/tmp/myth_test_lirc_XXXXXXd" };
    if (!tmpfile.open())
        qFatal("Couldn't create tempory file name");
    s_unix_sockname = tmpfile.fileName();
    tmpfile.close();
    QFile::remove(s_unix_sockname);
}

// Called once.
void TestLirc::cleanupTestCase(void)
{
}

// Called for every test case.
void TestLirc::init(void)
{
    s_shutting_down = false;

    if (QFile::exists(s_unix_sockname))
        std::cerr << "Socket file " << qPrintable(s_unix_sockname) << " exists at start of test case.\n";
#ifdef INCLUDE_TCP_SERVER
    s_tcp_thread = QThread::create(fakeTcpServer);
    s_tcp_thread->start();
#endif
    s_unix_thread = QThread::create(fakeUnixServer);
    s_unix_thread->start();

    s_response.clear();
    m_state = nullptr;
    m_config = nullptr;

    // Allow the server to start
    QTest::qWait(1);
}

// Called for every test case
void TestLirc::cleanup(void)
{
    // Shutdown the server and wait for it to exit.
    s_shutting_down = true;
    QTest::qWait(SERVER_TIMEOUT * 1.1);
#ifdef INCLUDE_TCP_SERVER
    s_tcp_thread->quit();
#endif
    s_unix_thread->quit();
    QFile::remove(s_unix_sockname);

    // Cleanup
    if (m_config)
    {
        lirc_freeconfig(m_config);
        m_config = nullptr;
    }
    if (m_state)
    {
        lirc_deinit(m_state);
        m_state = nullptr;
    }
}

void TestLirc::test_init(void)
{
    auto *state = lirc_init(nullptr, nullptr, nullptr, nullptr, 0);
    QCOMPARE(state, nullptr);

    state = lirc_init("/etc/lircrc", ".lircrc", "test_lirc", nullptr, 0);
    // Automatically clean up at function exit
    auto cleanup_fn = [](lirc_state **statep) { lirc_deinit(*statep); };
    std::unique_ptr<lirc_state*,decltype(cleanup_fn)> const cleanup { &state, cleanup_fn };
    QVERIFY(state != nullptr);
    QCOMPARE(state->lircrc_root_file, "/etc/lircrc");
    QCOMPARE(state->lircrc_user_file, ".lircrc");
    QCOMPARE(state->lirc_prog, "test_lirc");
    auto ret = lirc_deinit(state);
    state = nullptr;
    QCOMPARE(ret, LIRC_RET_SUCCESS);

    // This should fail because the socket connection should fail
    state = lirc_init("/etc/lircrc", ".lircrc", "test_lirc", "/dev/notasocket", 0);
    QCOMPARE(state, nullptr);
}

void TestLirc::test_trim_data(void)
{
    QTest::addColumn<QString>("qs_in");
    QTest::addColumn<QString>("qs_expected");

    QTest::newRow("end")   << "This is a string  \t" << "This is a string";
    QTest::newRow("begin") << " \t This is a string" << "This is a string";
    QTest::newRow("both")  << " \t This is a string \t " << "This is a string";
}

// Test the lirc_trim function.
void TestLirc::test_trim(void)
{
    QFETCH(const QString, qs_in);
    QFETCH(const QString, qs_expected);

    std::string in = qs_in.toStdString();
    std::string const expected = qs_expected.toStdString();

    std::string const out = lirc_trim(in);
    QCOMPARE(out, expected);
}

std::array<const char *,17>parse_string_strings
{
    /*  0 */ "Lorem ipsum dolor sit amet",

    /*  1 */ R"(Lorem \a\b\e dolor \f\n\r sit)",
    /*  2 */ "Lorem \a\b\e dolor \f\n\r sit",

    /*  3 */ "Lorem \\t\\v\\\n dolor",
    /*  4 */ "Lorem \t\v",

    /*  5 */ R"(Lorem\40ipsum\040 dolor \133sit\135)",
    /*  6 */ R"(Lorem\x20ipsum\x20 dolor \x5bsit\x5D)",
    /*  7 */ "Lorem ipsum  dolor [sit]",

    /*  8 */ R"(Lorem \A\B\V\Z dolor \[sit\])",
    /*  9 */ "Lorem \x01\x02\x16\x1A dolor [sit]",

    /* 10 */ "Lorem \\@ipsum dolor sit",
    /* 11 */ "Lorem ",

    /* 12 */ "Lorem\\420ipsum dolor",
    /* 13 */ "Lorem\\x110ipsum dolor",
    /* 14 */ "Lorem\x10ipsum dolor",
    /* 15 */ "Lorem \\xipsum dolor",
    /* 16 */ "Lorem \\x00ipsum dolor",
};

// This function has to use array indices to pass the
// original/expected strings to the test function because a)
// QTest::add_column it doesn't support char* columns, and b)
// converting these strings to QString eats the backslashes.
void TestLirc::test_parse_string_data(void)
{
    QTest::addColumn<int>("original_index");
    QTest::addColumn<int>("expected_index");

    QTest::newRow("plain")     <<  0 <<  0;
    QTest::newRow("single1")   <<  1 <<  2;
    QTest::newRow("single2")   <<  3 <<  4;
    QTest::newRow("octal")     <<  5 <<  7;
    QTest::newRow("hex")       <<  6 <<  7;
    QTest::newRow("default")   <<  8 <<  9;
    QTest::newRow("null")      << 10 << 11;
    QTest::newRow("octal-err") << 12 << 14;
    QTest::newRow("hex-err1")  << 13 << 14;
    QTest::newRow("hex-err2")  << 15 << 11;
    QTest::newRow("hex-null")  << 16 << 11;
}

// Test the lirc_parse_string and lirc_parse_escape functions.  There
// are a bunch of different cases to test in the latter.
void TestLirc::test_parse_string(void)
{
    QFETCH(const int, original_index);
    QFETCH(const int, expected_index);

    const char *name = "__FUNCTION__";
    std::string s = parse_string_strings[original_index];
    const std::string expected = parse_string_strings[expected_index];

    m_state = lirc_init("/etc/lircrc", ".lircrc", "test_lirc", nullptr, 0);
    QVERIFY(m_state != nullptr);

    lirc_parse_string(m_state, s, name, 0);
    QCOMPARE(s.data(), expected);
}

void TestLirc::test_parse_include_data(void)
{
    QTest::addColumn<QString>("qs_original");
    QTest::addColumn<QString>("qs_expected");

    // These two make changes
    QTest::newRow("dblquotes") << "\"abc.h\"" << "abc.h";
    QTest::newRow("anglebr") << "<abc.h>" << "abc.h";
    // These test failure conditions
    QTest::newRow("mixed1") << "\"abc.h>" << "\"abc.h>";
    QTest::newRow("mixed2") << "<abc.h\"" << "<abc.h\"";
    QTest::newRow("neither") << "whatever" << "whatever";
    QTest::newRow("short") << "a" << "a";
}

// Test the lirc_parse_include function.
void TestLirc::test_parse_include(void)
{
    QFETCH(const QString, qs_original);
    QFETCH(const QString, qs_expected);

    const std::string original = qs_original.toStdString();
    const std::string expected = qs_expected.toStdString();
    std::string actual = original;
    lirc_parse_include(actual, "", 0);
    QCOMPARE(actual, expected);
}

// Very simplistic test of of the lirc_mode function.  This function
// will get extensive testing by later tests.
void TestLirc::test_mode(void)
{
    std::string mode;
    std::string const token = "begin";
    std::string const token2;
    struct lirc_config_entry *new_config   {nullptr};
    struct lirc_config_entry *first_config {nullptr};
    struct lirc_config_entry *last_config  {nullptr};

    auto cleanup_fn2 = [](lirc_config_entry **ptr) { delete *ptr; };
    std::unique_ptr<lirc_config_entry*,decltype(cleanup_fn2)> const cleanup2 { &new_config, cleanup_fn2 };

    m_state = lirc_init("/etc/lircrc", ".lircrc", "test_lirc", nullptr, 0);
    QVERIFY(m_state != nullptr);

    int const res = lirc_mode(m_state, token, token2, mode,
                        &new_config, &first_config, &last_config,
                        [](std::string& /*s*/){return 0;}, __FUNCTION__,0);
    QCOMPARE(res, 0);
}

void TestLirc::test_parse_flags_data(void)
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<int>("expected");

    QTest::newRow("none") << "" << 0;
    QTest::newRow("trio1") << "once mode startup_mode" << 0x15;
    QTest::newRow("trio2") << "quit\tmode\ttoggle_reset" << 0x26;
    QTest::newRow("trio3") << "once|quit|startup_mode" << 0x13;
    QTest::newRow("unknown") << "xyzzy" << 0;
}

// Test the lirc_flags function.
void TestLirc::test_parse_flags(void)
{
    QFETCH(const QString, text);
    QFETCH(const int, expected);

    m_state = lirc_init("/etc/lircrc", ".lircrc", "test_lirc", nullptr, 0);
    QVERIFY(m_state != nullptr);

    uint const actual = lirc_flags(m_state, text.toUtf8().data());
    QCOMPARE(actual, (uint)expected);
}

void TestLirc::test_getfilename_data(void)
{
    QTest::addColumn<QString>("qs_home");
    QTest::addColumn<QString>("qs_user_file");
    QTest::addColumn<QString>("qs_filename");
    QTest::addColumn<QString>("qs_current");
    QTest::addColumn<QString>("qs_expected");

    QTest::newRow("nothing") << "" << "user_file" << "" << "" << "/user_file";
    QTest::newRow("home1") << "/home/test"  << "user_file" << "" << "" << "/home/test/user_file";
    QTest::newRow("home2") << "/home/test/" << "user_file" << "" << "" << "/home/test/user_file";
    QTest::newRow("tilde0") << "" << "x" << "~/lircrc" << "" << "//lircrc"; // sic
    QTest::newRow("tilde1") << "/home/test"  << "x" << "~/lircrc" << "" << "/home/test/lircrc";
    QTest::newRow("tilde2") << "/home/test/" << "x" << "~/lircrc" << "" << "/home/test//lircrc"; // sic
    QTest::newRow("absolute") << "" << "x" << "/tmp/lircrc" << "" << "/tmp/lircrc";
    QTest::newRow("relative1") << "" << "x" << "lircrc" << "" << "lircrc";
    QTest::newRow("relative2") << "" << "x" << "lircrc" << "/a/b/c" << "/a/b/lircrc";
}

// Test the lirc_getfilename function.
void TestLirc::test_getfilename(void)
{
    QFETCH(const QString, qs_home);
    QFETCH(const QString, qs_user_file);
    QFETCH(const QString, qs_filename);
    QFETCH(const QString, qs_current);
    QFETCH(const QString, qs_expected);

    if (qs_home.isEmpty())
        unsetenv("HOME");
    else
        setenv("HOME", qs_home.toUtf8().data(), 1);

    m_state = lirc_init("/etc/lircrc", qs_user_file.toUtf8().data(),
                        "test_lirc", nullptr, 0);
    QVERIFY(m_state != nullptr);

    std::string const filename = qs_filename.toStdString();
    std::string const current = qs_current.toStdString();
    std::string const expected = qs_expected.toStdString();
    std::string const actual = lirc_getfilename(m_state, filename, current);
    QCOMPARE(actual, expected);
}

void TestLirc::test_open_data(void)
{
    QTest::addColumn<QString>("qs_filename");
    QTest::addColumn<QString>("qs_root_file");
    QTest::addColumn<QString>("qs_user_file");
    QTest::addColumn<QString>("qs_expected");

    QString const filename1 = QStringLiteral(TEST_SOURCE_DIR) + "/data/lirc1.txt";
    QString const filename2 = "/" + filename1; // sic

    QTest::newRow("normal")        << filename1 << "/etc/lircrc" << ".lircrc" << filename1;
    QTest::newRow("fallback_user") << ""        << "/etc/lircrc" << filename1 << filename2;
    QTest::newRow("fallback_root") << ""        << filename1     << ".lircrc" << filename1;
}

// Test the lirc_open function.
void TestLirc::test_open(void)
{
    QFETCH(const QString, qs_filename);
    QFETCH(const QString, qs_root_file);
    QFETCH(const QString, qs_user_file);
    QFETCH(const QString, qs_expected);

    std::string file = qs_filename.toStdString();
    std::string root_file = qs_root_file.toStdString();
    std::string user_file = qs_user_file.toStdString();
    std::string expected =  qs_expected.toStdString();

    m_state = lirc_init(root_file.data(), user_file.data(), "test_lirc", nullptr, 0);
    QVERIFY(m_state != nullptr);

    // Fallback open
    std::string name_opened;
    std::string const current_file;
    auto *f = lirc_open(m_state, file, current_file, name_opened);

    // Automatically close file at function exit
    auto close_fh = [](FILE **fh) { if (*fh) fclose(*fh); };
    std::unique_ptr<FILE*,decltype(close_fh)> const cleanup { &f, close_fh };

    QVERIFY(f != nullptr);
    QVERIFY(!name_opened.empty());
    QCOMPARE(name_opened, expected);
}

//
// Test sha_bang parsing and include/filestack code
//
void TestLirc::test_readconfig_internal1(void)
{
    QString const qs_filename = QStringLiteral(TEST_SOURCE_DIR) + "/data/lirc1.txt";
    const std::string filename = qs_filename.toStdString();
    std::string expected_sha_bang = "/bin/echo dummy shabang line";

    // Write a temporary file that contains a shabang and an include
    auto tmpfile = QTemporaryFile();
    QCOMPARE(tmpfile.open(), true);
    QString const output = QString("#!%1\ninclude %2\n")
        .arg(expected_sha_bang.data(), filename.data());
    tmpfile.write(output.toUtf8());
    tmpfile.close();
    const std::string tmpfilename = tmpfile.fileName().toStdString();

    // Now read the temporary file
    m_state = lirc_init("/etc/lircrc", ".lircrc", "test_lirc", nullptr, 0);
    QVERIFY(m_state != nullptr);

    std::string full_name;
    std::string sha_bang;

    auto result =
        lirc_readconfig_only_internal(m_state, tmpfilename,
                                      &m_config, [](std::string& /*s*/){return 0;},
                                      full_name,sha_bang);
    QCOMPARE(result, 0);
    QVERIFY(m_config != nullptr);
    QCOMPARE(full_name, tmpfilename);
    QCOMPARE(sha_bang, expected_sha_bang);
}

void TestLirc::test_readconfig_internal2(void)
{
    m_state = lirc_init("/etc/lircrc", ".lircrc", "test_lirc", nullptr, 0);
    QVERIFY(m_state != nullptr);

    QString const qs_filename = QStringLiteral(TEST_SOURCE_DIR) + "/data/lirc2.txt";
    const std::string filename = qs_filename.toStdString();
    std::string full_name;
    std::string sha_bang;

    // Read config file
    auto result =
        lirc_readconfig_only_internal(m_state, filename,
                                      &m_config, [](std::string& /*s*/){return 0;},
                                      full_name,sha_bang);
    QCOMPARE(result, 0);

    // Test returned config
    QVERIFY (m_config != nullptr);
    QCOMPARE(m_config->current_mode, "");
    QCOMPARE(m_config->next, m_config->first);
    struct lirc_config_entry *entry = m_config->next;
    QCOMPARE(entry->prog, "irexec");
    QCOMPARE(entry->rep_delay, 3U);
    QCOMPARE(entry->rep, 2U);
    QCOMPARE(entry->mode, "test2");
    struct lirc_code *code = entry->code;
    QVERIFY(code != nullptr);
    QVERIFY(code->remote == LIRC_ALL);
    QCOMPARE(code->button, "HOME");
    QCOMPARE(code->next, nullptr);
    struct lirc_list *list = entry->config;
    QCOMPARE(list->string, "echo \"Honey, I'm home.\"");
    list = list->next;
    QVERIFY(list != nullptr);
    QCOMPARE(list->string, "echo \"Honey! I'm home.\"");

    QVERIFY(entry->next != nullptr);
    entry = entry->next;
    QCOMPARE(entry->prog, "mythtv");
    QCOMPARE(entry->rep_delay, 0U);
    QCOMPARE(entry->rep, 0U);
    QCOMPARE(entry->mode, "test2");
    code = entry->code;
    QVERIFY(code != nullptr);
    QVERIFY(code->remote == LIRC_ALL);
    QCOMPARE(code->button, "Stop");
    QVERIFY(code->next != nullptr);
    code = code->next;
    QVERIFY(code->remote == LIRC_ALL);
    QCOMPARE(code->button, "Go");
    QCOMPARE(code->next, nullptr);
    list = entry->config;
    QCOMPARE(list->string, "Dizzy");
    QCOMPARE(list->next, nullptr);

    QCOMPARE(full_name, filename);
    QVERIFY(sha_bang.empty());
}

void TestLirc::test_readconfig_internal3(void)
{
    m_state = lirc_init("/etc/lircrc", ".lircrc", "test_lirc", nullptr, 0);
    QVERIFY(m_state != nullptr);

    QString const qs_filename = QStringLiteral(TEST_SOURCE_DIR) + "/data/lirc3.txt";
    const std::string filename = qs_filename.toStdString();

    // Read config file
    auto result =
        lirc_readconfig_only(m_state, filename,
                             &m_config, [](std::string& /*s*/){return 0;});
    QCOMPARE(result, 0);

    // Test returned config
    QVERIFY (m_config != nullptr);
    QCOMPARE(m_config->current_mode, "test3");
    QCOMPARE(m_config->next, m_config->first);
    struct lirc_config_entry *entry = m_config->next;
    QCOMPARE(entry->prog, "mythtv");
    QCOMPARE(entry->rep_delay, 0U);
    QCOMPARE(entry->rep, 0U);
    QCOMPARE(entry->mode, "test3");
    struct lirc_code *code = entry->code;
    QVERIFY(code != nullptr);
    QCOMPARE(code->remote, "silver");
    QCOMPARE(code->button, "Off");
    QCOMPARE(code->next, nullptr);
    struct lirc_list *list = entry->config;
    QCOMPARE(list->string, "Esc");

    std::string mode = lirc_getmode(m_state, m_config);
    QCOMPARE(mode, "test3");
    mode = lirc_setmode(m_state, m_config, "not test3");
    QCOMPARE(mode, "not test3");
    QCOMPARE(m_config->current_mode, "not test3");
}

void TestLirc::test_readconfig_internal4(void)
{
    m_state = lirc_init("/etc/lircrc", ".lircrc", "test_lirc", nullptr, 0);
    QVERIFY(m_state != nullptr);

    QString const qs_filename = QStringLiteral(TEST_SOURCE_DIR) + "/data/lirc4.txt";
    const std::string filename = qs_filename.toStdString();

    // Read config file
    auto result =
        lirc_readconfig_only(m_state, filename,
                             &m_config, [](std::string& /*s*/){return 0;});
    QCOMPARE(result, 0);

    // Test returned config
    QVERIFY (m_config != nullptr);
    QCOMPARE(m_config->current_mode, "");
    QCOMPARE(m_config->next, m_config->first);
    struct lirc_config_entry *entry = m_config->next;
    QCOMPARE(entry->prog, "irexec");
    QCOMPARE(entry->rep_delay, 3U);
    QCOMPARE(entry->rep, 2U);
    QCOMPARE(entry->mode, "test4");
    struct lirc_code *code = entry->code;
    QVERIFY(code != nullptr);
    QVERIFY(code->remote == LIRC_ALL);
    QCOMPARE(code->button, "HOME");
    QCOMPARE(code->next, nullptr);
    struct lirc_list *list = entry->config;
    QCOMPARE(list->string, "echo \"Honey, I'm home.\"");
}

// This test reaches a couple of cases in the lirc_startupmode function.
void TestLirc::test_readconfig_internal5(void)
{
    m_state = lirc_init("/etc/lircrc", ".lircrc", "test_lirc", nullptr, 0);
    QVERIFY(m_state != nullptr);

    QString const qs_filename = QStringLiteral(TEST_SOURCE_DIR) + "/data/lirc5.txt";
    const std::string filename = qs_filename.toStdString();

    // Read config file
    auto result =
        lirc_readconfig_only(m_state, filename,
                             &m_config, [](std::string& /*s*/){return 0;});
    QCOMPARE(result, 0);

    // Test returned config
    QVERIFY (m_config != nullptr);
    QCOMPARE(m_config->current_mode, "test_lirc");
    QCOMPARE(m_config->next, m_config->first);
    struct lirc_config_entry *entry = m_config->next;
    QCOMPARE(entry->prog, "mythtv");
    QCOMPARE(entry->mode, "test_lirc");
    QCOMPARE(entry->flags, 0U);
    struct lirc_code *code = entry->code;
    QVERIFY(code != nullptr);
    QCOMPARE(code->remote, "silver");
    QCOMPARE(code->button, "Stop");
    QVERIFY(code->next != nullptr);
    struct lirc_list *list = entry->config;
    QCOMPARE(list->string, "Dizzy");

    // Second item
    QVERIFY(entry->next != nullptr);
    entry = entry->next;
    QCOMPARE(entry->mode, "test_lirc");
    QCOMPARE(entry->flags, 0U);
    code = entry->code;
    QVERIFY(code != nullptr);
    QCOMPARE(code->remote, "silver");
    QCOMPARE(code->button, "Enter");

    // Third item
    QVERIFY(entry->next != nullptr);
    entry = entry->next;
    QCOMPARE(entry->mode, "");
    QCOMPARE(entry->flags, 9U);
    code = entry->code;
    QVERIFY(code != nullptr);
    QCOMPARE(code->remote, "silver");
    QCOMPARE(code->button, "Enter");

    lirc_clearmode(m_config);
    QCOMPARE(entry->flags, 1U);
}

// Tests the lirc_readconfig function first connection attempt.
//
// This also tests parts of lirc_readconfig_only_internal,
// lirc_getsocketname, lirc_identify, lirc_freeconfig.
void TestLirc::test_readconfig1(void)
{
    QString const qs_command { "/bin/echo make some noise" };

    // Write temporary config file.  The name must be the same as the
    // lircd socket name, minus the final 'd'.
    QString const qs_conf_name = s_unix_sockname.left(s_unix_sockname.size()-1);
    std::string ss_conf_name = qs_conf_name.toStdString();
    QFile conf_file = QFile(qs_conf_name);
    QVERIFY(conf_file.open(QIODevice::ReadWrite | QIODevice::Text));
    conf_file.write(QString("#!%1").arg(qs_command).toUtf8());
    conf_file.close();

    // Initialize
    m_state = lirc_init("/etc/lircrc", ".lircrc", "test_lirc", nullptr, 0);
    QVERIFY(m_state != nullptr);

    // Successful identification
    s_response.clear();
    s_response.emplace_back("BEGIN\n");
    s_response.emplace_back("IDENT test_lirc\n");
    s_response.emplace_back("SUCCESS\n");
    s_response.emplace_back("END\n");

    // Read config file
    auto result = lirc_readconfig(m_state, ss_conf_name,
                                  &m_config, [](std::string& /*s*/){return 0;});
    QCOMPARE(result, 0);
    if (result != 0)
        return;
    QVERIFY(m_config != nullptr);
    if (m_config == nullptr)
        return;
    QVERIFY(m_config->sockfd != -1);

    s_response.clear();
    s_response.emplace_back("BEGIN\n");
    s_response.emplace_back("GETMODE\n");
    s_response.emplace_back("SUCCESS\n");
    s_response.emplace_back("DATA\n");
    s_response.emplace_back("1\n");
    s_response.emplace_back("REALLYGOODMODE\n");
    s_response.emplace_back("END\n");
    std::string mode = lirc_getmode(m_state, m_config);
    QCOMPARE(mode, "REALLYGOODMODE");

    s_response.clear();
    s_response.emplace_back("BEGIN\n");
    s_response.emplace_back("SETMODE\n");
    s_response.emplace_back("SUCCESS\n");
    s_response.emplace_back("END\n");
    mode = lirc_setmode(m_state, m_config, "");
    QCOMPARE(mode, "");
    QCOMPARE(m_config->current_mode, "");

    s_response.clear();
    s_response.emplace_back("BEGIN\n");
    s_response.emplace_back("SETMODE not test3\n");
    s_response.emplace_back("SUCCESS\n");
    s_response.emplace_back("DATA\n");
    s_response.emplace_back("1\n");
    s_response.emplace_back("not test3\n");
    s_response.emplace_back("END\n");
    mode = lirc_setmode(m_state, m_config, "not test3");
    QCOMPARE(mode, "not test3");               // remote said
    QCOMPARE(m_config->current_mode, "");      // local unchanged
}

// Tests the lirc_readconfig function starting the lircd daemon.
//
// This also tests parts of lirc_readconfig_only_internal,
// lirc_getsocketname, lirc_identify, lirc_freeconfig.
//
// This doesn't test the second attempt to connect to the lircd
// daemon, which is identical to the first.
void TestLirc::test_readconfig2(void)
{
    QString const qs_target_file = s_unix_sockname + ".rc7";

    // Write temporary config file.  This needs to be an invalid name
    // to force lirc_readconfig to try and start the lircd daemon.
    QString const qs_conf_name = s_unix_sockname.left(s_unix_sockname.size()-2);
    std::string ss_conf_name = qs_conf_name.toStdString();
    QFile conf_file = QFile(qs_conf_name);
    QVERIFY(conf_file.open(QIODevice::ReadWrite | QIODevice::Text));
    conf_file.write(QString("#!touch %1").arg(qs_target_file).toUtf8());
    conf_file.close();

    // Initialize
    m_state = lirc_init("/etc/lircrc", ".lircrc", "test_lirc", nullptr, 0);
    QVERIFY(m_state != nullptr);

    // Read config file
    auto result = lirc_readconfig(m_state, ss_conf_name,
                                  &m_config, [](std::string& /*s*/){return 0;});
    QCOMPARE(result, -1);

    // Test if target file created
    bool const exists = QFile::exists(qs_target_file);
    QCOMPARE(exists, true);
    if (exists)
        QFile::remove(qs_target_file);

    // Readconfig failed so 'config' is invalid. Clear it to prevent a
    // crash in the cleanup function.
    m_config = nullptr;
}

void TestLirc::test_code2char_internal(void)
{
    m_state = lirc_init("/etc/lircrc", ".lircrc", "mythtv", nullptr, 0);
    QVERIFY(m_state != nullptr);

    QString const qs_filename = QStringLiteral(TEST_SOURCE_DIR) + "/data/lirc_full.txt";
    const std::string filename = qs_filename.toStdString();

    // Read config file
    auto result =
        lirc_readconfig_only(m_state, filename,
                             &m_config, [](std::string& /*s*/){return 0;});
    QCOMPARE(result, 0);

    // Test returned config
    QVERIFY(m_config != nullptr);

    struct lirc_config_entry *entry = m_config->next;
    std::string remote = LIRC_ALL;
    std::string button = "purple";
    int const level = lirc_iscode(entry, remote, button, 0);
    QCOMPARE(level, 0);

    // Random key 1
    std::string string {};
    std::string prog {};
    int x = lirc_code2char_internal(m_state, m_config, "0 0 Off *",
                                    string, prog);
    QCOMPARE(x,0);
    QCOMPARE(string, "Esc");
    QCOMPARE(prog, "mythtv");

    // Random key 2
    string.clear();
    prog.clear();
    x = lirc_code2char_internal(m_state, m_config, "0 0 Green *",
                                string, prog);
    QCOMPARE(x,0);
    QCOMPARE(string, "I");
    QCOMPARE(prog, "mythtv");

    // Key doesn't exist
    string.clear();
    prog.clear();
    x = lirc_code2char_internal(m_state, m_config, "0 0 Mauve *",
                                string, prog);
    QCOMPARE(x,0);
    QCOMPARE(string, "");
    QCOMPARE(prog, "");

    // Key with repeat
    string.clear();
    prog.clear();
    x = lirc_code2char_internal(m_state, m_config, "0 0 Yellow *",
                                string, prog);
    QCOMPARE(x,0);
    QCOMPARE(string, "F10");
    QCOMPARE(prog, "mythtv");
    string.clear();
    prog.clear();
    x = lirc_code2char_internal(m_state, m_config, "0 0 Yellow *",
                                string, prog);
    QCOMPARE(x,0);
    QCOMPARE(string, "");
    QCOMPARE(prog, "");
    x = lirc_code2char_internal(m_state, m_config, "0 0 Yellow *",
                                string, prog);
    QCOMPARE(x,0);
    QCOMPARE(string, "F10");
    QCOMPARE(prog, "mythtv");
    string.clear();
    prog.clear();
    x = lirc_code2char_internal(m_state, m_config, "0 0 Yellow *",
                                string, prog);
    QCOMPARE(x,0);
    QCOMPARE(string, "");
    QCOMPARE(prog, "");
}

// Test the read_string function used when receiving LIRC data from a
// remote lircd process.  This is normally connected to a socket file
// descriptor, but since this function is only testing the read
// capability its just connected to a plain text file.
void TestLirc::test_readstring(void)
{
    m_state = lirc_init("/etc/lircrc", ".lircrc", "test_lirc", nullptr, 0);
    QVERIFY(m_state != nullptr);

    // Test normal file reads
    const char *path { TEST_SOURCE_DIR "/data/lirc6.txt" };
    int fd = open(path, O_RDONLY);
    QVERIFY(fd != -1);
    const char *data = lirc_read_string(m_state, fd);
    QCOMPARE(data, "Lorem ipsum dolor sit amet,");
    data = lirc_read_string(m_state, fd);
    QCOMPARE(data, "consectetur adipiscing elit,");
    data = lirc_read_string(m_state, fd);
    QCOMPARE(data, "sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.");
    data = lirc_read_string(m_state, fd);
    QVERIFY(data == nullptr);
    close(fd);

    // Test too long of a line
    path = TEST_SOURCE_DIR "/data/lirc7.txt";
    fd = open(path, O_RDONLY);
    QVERIFY(fd != -1);
    data = lirc_read_string(m_state, fd);
    QVERIFY(data == nullptr);
    close(fd);
}

void TestLirc::test_getsocketname(void)
{
    std::string input  { "/run/lirc" };
    std::string output { "/run/lircd" };
    struct sockaddr_un addr {};
    addr.sun_family=AF_UNIX;

    size_t const len = lirc_getsocketname(input, addr.sun_path, sizeof(addr.sun_path));
    // lirc_getsocketname counts the NUL terminator?
    QCOMPARE(len, output.size()+1);
    QCOMPARE(addr.sun_path, output.data());
}

#ifdef INCLUDE_TCP_SERVER
void TestLirc::test_tcp_sending(void)
{
    while (!s_tcp_port)
        QTest::qWait(50); // milliseconds

    auto *sock = new QTcpSocket();
    QVERIFY(sock != nullptr);

    sock->connectToHost(QHostAddress::LocalHost, s_tcp_port);
    QVERIFY(sock->waitForConnected());

    s_response.clear();
    s_response.emplace_back("Go away");
    sock->write("Hello Server");
    sock->waitForReadyRead(100);
    QByteArray data = sock->readAll(); // clazy:exclude=unused-non-trivial-variable
    sock->close();
}
#endif

void TestLirc::test_udp_sending(void)
{
    auto *sock = new QLocalSocket();
    auto del_fn = [](QLocalSocket **sock2) { delete *sock2; };
    std::unique_ptr<QLocalSocket*,decltype(del_fn)> const cleanup { &sock, del_fn };
    QVERIFY(sock != nullptr);

    sock->connectToServer(s_unix_sockname);
    QVERIFY(sock->waitForConnected());

    s_response.clear();
    s_response.emplace_back("Go away");
    sock->write("Hello Server");
    sock->waitForReadyRead(100);
    QByteArray const data = sock->readAll();
    QCOMPARE(data,"Go away");
    sock->close();
}

void TestLirc::test_identify(void)
{
    m_state = lirc_init("/etc/lircrc", ".lircrc", "test_lirc", nullptr, 0);
    QVERIFY(m_state != nullptr);

    auto *sock = new QLocalSocket();
    auto del_fn = [](QLocalSocket **sock2) { delete *sock2; };
    std::unique_ptr<QLocalSocket*,decltype(del_fn)> const cleanup { &sock, del_fn };
    QVERIFY(sock != nullptr);

    sock->connectToServer(s_unix_sockname);
    QVERIFY(sock->waitForConnected());
    int const sockfd = sock->socketDescriptor();
    QVERIFY(sockfd != -1);

    constexpr char const * const command = "IDENT test_lirc\n";

    // Successful identification
    s_response.clear();
    s_response.emplace_back("BEGIN\n");
    s_response.emplace_back(command);
    s_response.emplace_back("SUCCESS\n");
    s_response.emplace_back("END\n");
    int success = lirc_identify(m_state, sockfd);
    QCOMPARE(success, LIRC_RET_SUCCESS);

    // Successful identification w/finding begin
    s_response.clear();
    s_response.emplace_back("GARBAGE1\n");
    s_response.emplace_back("GARBAGE2\n");
    s_response.emplace_back("BEGIN\n");
    s_response.emplace_back(command);
    s_response.emplace_back("SUCCESS\n");
    s_response.emplace_back("END\n");
    success = lirc_identify(m_state, sockfd);
    QCOMPARE(success, LIRC_RET_SUCCESS);

    // Successful identification w/restart
    s_response.clear();
    s_response.emplace_back("BEGIN\n");
    s_response.emplace_back("OOPS\n");
    s_response.emplace_back("BEGIN\n");
    s_response.emplace_back(command);
    s_response.emplace_back("SUCCESS\n");
    s_response.emplace_back("END\n");
    success = lirc_identify(m_state, sockfd);
    QCOMPARE(success, LIRC_RET_SUCCESS);

    // Successful identification w/o SUCCESS statement
    s_response.clear();
    s_response.emplace_back("BEGIN\n");
    s_response.emplace_back(command);
    s_response.emplace_back("END\n");
    success = lirc_identify(m_state, sockfd);
    QCOMPARE(success, LIRC_RET_SUCCESS);

    // Failed identification with ERROR response
    s_response.clear();
    s_response.emplace_back("BEGIN\n");
    s_response.emplace_back(command);
    s_response.emplace_back("ERROR\n");
    s_response.emplace_back("END\n");
    success = lirc_identify(m_state, sockfd);
    QCOMPARE(success, LIRC_RET_ERROR);

    // Failed identification with garbage response
    s_response.clear();
    s_response.emplace_back("BEGIN\n");
    s_response.emplace_back(command);
    s_response.emplace_back("RORRE\n");
    success = lirc_identify(m_state, sockfd);
    QCOMPARE(success, LIRC_RET_ERROR);

    QByteArray const data = sock->readAll(); // clazy:exclude=unused-non-trivial-variable
    sock->close();
}

void TestLirc::test_send_command(void)
{
    m_state = lirc_init("/etc/lircrc", ".lircrc", "test_lirc", nullptr, 0);
    QVERIFY(m_state != nullptr);

    auto *sock = new QLocalSocket();
    auto del_fn = [](QLocalSocket **sock2) { delete *sock2; };
    std::unique_ptr<QLocalSocket*,decltype(del_fn)> const cleanup { &sock, del_fn };
    QVERIFY(sock != nullptr);

    static constexpr size_t BUFLEN = 256;
    std::array<char,BUFLEN> buf {};
    size_t buf_len {0};
    int ret_status {-2};

    sock->connectToServer(s_unix_sockname);
    QVERIFY(sock->waitForConnected());
    int const sockfd = sock->socketDescriptor();
    QVERIFY(sockfd != -1);

    constexpr char const * const command = "XYZZY\n";

    // Bad DATA response
    s_response.clear();
    s_response.emplace_back("BEGIN\n");
    s_response.emplace_back(command);
    s_response.emplace_back("SUCCESS\n");
    s_response.emplace_back("DATA\n");
    s_response.emplace_back("q\n"); // <= should be a number
    int n_lines = lirc_send_command(m_state, sockfd, command,
                                    buf.data(), &buf_len, &ret_status);
    QCOMPARE(n_lines, -1);
    QCOMPARE(buf_len, 0U);
    QCOMPARE(ret_status, -2);  // not updated

    // Bad zero lines of data
    s_response.clear();
    s_response.emplace_back("BEGIN\n");
    s_response.emplace_back(command);
    s_response.emplace_back("SUCCESS\n");
    s_response.emplace_back("DATA\n");
    s_response.emplace_back("0\n");
    s_response.emplace_back("OOPS\n");
    ret_status = -2;
    buf_len = BUFLEN;
    n_lines = lirc_send_command(m_state, sockfd, command,
                                buf.data(), &buf_len, &ret_status);
    QCOMPARE(n_lines, -1);
    QCOMPARE(buf_len, BUFLEN); // not updated
    QCOMPARE(ret_status, -2);  // not updated

    // Zero lines of data
    s_response.clear();
    s_response.emplace_back("BEGIN\n");
    s_response.emplace_back(command);
    s_response.emplace_back("SUCCESS\n");
    s_response.emplace_back("DATA\n");
    s_response.emplace_back("0\n");
    s_response.emplace_back("END\n");
    ret_status = -2;
    buf_len = BUFLEN;
    n_lines = lirc_send_command(m_state, sockfd, command,
                                buf.data(), &buf_len, &ret_status);
    QCOMPARE(n_lines, 0);
    QCOMPARE(buf_len, 0U);
    QCOMPARE(ret_status, LIRC_RET_SUCCESS);

    // Actual DATA response
    s_response.clear();
    s_response.emplace_back("BEGIN\n");
    s_response.emplace_back(command);
    s_response.emplace_back("SUCCESS\n");
    s_response.emplace_back("DATA\n");
    s_response.emplace_back("1\n");
    s_response.emplace_back("ONE LINE OF DATA\n");
    s_response.emplace_back("END\n");
    ret_status = -2;
    buf_len = BUFLEN;
    n_lines = lirc_send_command(m_state, sockfd, command,
                                buf.data(), &buf_len, &ret_status);
    QCOMPARE(n_lines, 1);
    QCOMPARE(buf_len, 17U);
    std::array<char,BUFLEN> expected1 { "ONE LINE OF DATA\0" };
    QVERIFY(memcmp(buf.data(), expected1.data(),buf_len) == 0);

    QCOMPARE(ret_status, LIRC_RET_SUCCESS);

    // Actual DATA response
    s_response.clear();
    s_response.emplace_back("BEGIN\n");
    s_response.emplace_back(command);
    s_response.emplace_back("SUCCESS\n");
    s_response.emplace_back("DATA\n");
    s_response.emplace_back("6\n");
    for (int i = 1; i <= 6; i++)
        s_response.emplace_back(qPrintable(QString("LINE %1\n").arg(i)));
    s_response.emplace_back("END\n");
    ret_status = -2;
    buf_len = BUFLEN;
    n_lines = lirc_send_command(m_state, sockfd, command,
                                buf.data(), &buf_len, &ret_status);
    QCOMPARE(n_lines, 6);
    QCOMPARE(buf_len, 42U);
    std::array<char,BUFLEN> expected2 { "LINE 1\0LINE 2\0LINE 3\0LINE 4\0LINE 5\0LINE 6\0" };
    QVERIFY(memcmp(buf.data(), expected2.data(),buf_len) == 0);
    QCOMPARE(ret_status, LIRC_RET_SUCCESS);

    QByteArray const data = sock->readAll(); // clazy:exclude=unused-non-trivial-variable
    sock->close();
}

QTEST_APPLESS_MAIN(TestLirc)
