
// Own header
#include "lirc.h"

// C headers
#include <cstdio>
#include <cerrno>
#include <cstdlib>

// C++ headers
#include <algorithm>
#include <chrono> // for milliseconds
#include <thread> // for sleep_for

#include <vector>
using namespace std;

// Qt headers
#include <QCoreApplication>
#include <QEvent>
#include <QKeySequence>
#include <QStringList>

// MythTV headers
#include "mythdb.h"
#include "mythsystemlegacy.h"
#include "lircevent.h"
#include "lirc_client.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include "mythlogging.h"

#define LOC      QString("LIRC: ")

class LIRCPriv
{
  public:
    LIRCPriv() = default;
    ~LIRCPriv()
    {
        if (m_lircState)
        {
            lirc_deinit(m_lircState);
            m_lircState = nullptr;
        }
        if (m_lircConfig)
        {
            lirc_freeconfig(m_lircConfig);
            m_lircConfig = nullptr;
        }
    }

    struct lirc_state  *m_lircState  {nullptr};
    struct lirc_config *m_lircConfig {nullptr};
};

QMutex LIRC::lirclib_lock;

/** \class LIRC
 *  \brief Interface between mythtv and lircd
 *
 *   Create connection to the lircd daemon and translate remote keypresses
 *   into custom events which are posted to the mainwindow.
 */

LIRC::LIRC(QObject *main_window,
           QString lircd_device,
           QString our_program,
           QString config_file)
    : MThread("LIRC"),
      lock(QMutex::Recursive),
      m_mainWindow(main_window),
      lircdDevice(std::move(lircd_device)),
      program(std::move(our_program)),
      configFile(std::move(config_file)),
      doRun(false),
      buf_offset(0),
      eofCount(0),
      retryCount(0),
      d(new LIRCPriv())
{
    buf.resize(0);
}

LIRC::~LIRC()
{
    TeardownAll();
}

void LIRC::deleteLater(void)
{
    TeardownAll();
    QObject::deleteLater();
}

void LIRC::TeardownAll(void)
{
    QMutexLocker locker(&lock);
    if (doRun)
    {
        doRun = false;
        lock.unlock();
        wait();
        lock.lock();
    }

    if (d)
    {
        delete d;
        d = nullptr;
    }
}

static QByteArray get_ip(const QString &h)
{
    QByteArray hba = h.toLatin1();
    struct in_addr sin_addr {};
    if (inet_aton(hba.constData(), &sin_addr))
        return hba;

    struct addrinfo hints {};
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    struct addrinfo *result = nullptr;
    int err = getaddrinfo(hba.constData(), nullptr, &hints, &result);
    if (err)
    {
        LOG(VB_GENERAL, LOG_DEBUG,
            QString("get_ip: %1").arg(gai_strerror(err)));
        return QString("").toLatin1();
    }

    int addrlen = result->ai_addrlen;
    if (!addrlen)
    {
        freeaddrinfo(result);
        return QString("").toLatin1();
    }

    if (result->ai_addr->sa_family != AF_INET)
    {
        freeaddrinfo(result);
        return QString("").toLatin1();
    }

    sin_addr = ((struct sockaddr_in*)(result->ai_addr))->sin_addr;
    hba = QByteArray(inet_ntoa(sin_addr));
    freeaddrinfo(result);

    return hba;
}

bool LIRC::Init(void)
{
    QMutexLocker locker(&lock);
    if (d->m_lircState)
        return true;

    uint64_t vtype = (0 == retryCount) ? VB_GENERAL : VB_FILE;

    int lircd_socket = -1;
    if (lircdDevice.startsWith('/'))
    {
        // Connect the unix socket
        QByteArray dev = lircdDevice.toLocal8Bit();
        if (dev.size() > 107)
        {
            LOG(vtype, LOG_ERR, LOC +
                QString("lircdDevice '%1'").arg(lircdDevice) +
                " is too long for the 'unix' socket API");

            return false;
        }

        lircd_socket = socket(AF_UNIX, SOCK_STREAM, 0);
        if (lircd_socket < 0)
        {
            LOG(vtype, LOG_ERR, LOC + QString("Failed to open Unix socket '%1'")
                    .arg(lircdDevice) + ENO);

            return false;
        }

        struct sockaddr_un addr {};
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, dev.constData(),107);

        int ret = ::connect(lircd_socket, (struct sockaddr*) &addr,
                            sizeof(addr));

        if (ret < 0)
        {
            LOG(vtype, LOG_ERR, LOC +
                QString("Failed to connect to Unix socket '%1'")
                    .arg(lircdDevice) + ENO);

            close(lircd_socket);
            return false;
        }
    }
    else
    {
        lircd_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (lircd_socket < 0)
        {
            LOG(vtype, LOG_ERR, LOC + QString("Failed to open TCP socket '%1'")
                    .arg(lircdDevice) + ENO);

            return false;
        }

        QString dev  = lircdDevice;
        uint    port = 8765;
        QStringList tmp = lircdDevice.split(':');
        if (2 == tmp.size())
        {
            dev  = tmp[0];
            port = (tmp[1].toUInt()) ? tmp[1].toUInt() : port;
        }
        QByteArray device = get_ip(dev);
        struct sockaddr_in addr {};
        addr.sin_family = AF_INET;
        addr.sin_port   = htons(port);

        if (!inet_aton(device.constData(), &addr.sin_addr))
        {
            LOG(vtype, LOG_ERR, LOC + QString("Failed to parse IP address '%1'")
                    .arg(dev));

            close(lircd_socket);
            return false;
        }

        int ret = ::connect(lircd_socket, (struct sockaddr*) &addr,
                            sizeof(addr));
        if (ret < 0)
        {
            LOG(vtype, LOG_ERR, LOC +
                QString("Failed to connect TCP socket '%1'")
                    .arg(lircdDevice) + ENO);

            close(lircd_socket);
            return false;
        }

        // On Linux, select() can indicate data when there isn't
        // any due to TCP checksum in-particular; to avoid getting
        // stuck on a read() call add the O_NONBLOCK flag.
        int flags = fcntl(lircd_socket, F_GETFD);
        if (flags >= 0)
        {
            ret = fcntl(lircd_socket, F_SETFD, flags | O_NONBLOCK);
            if (ret < 0)
            {
                LOG(VB_GENERAL, LOG_WARNING, LOC +
                    QString("Failed set flags for socket '%1'")
                        .arg(lircdDevice) + ENO);
            }
        }

        // Attempt to inline out-of-band messages and keep the connection open..
        int i = 1;
        ret = setsockopt(lircd_socket, SOL_SOCKET, SO_OOBINLINE, &i, sizeof(i));
        if (ret < 0)
        {
            LOG(VB_GENERAL, LOG_WARNING, LOC +
                QString("Failed setting OOBINLINE option for socket '%1'")
                    .arg(lircdDevice) + ENO);
        }
        i = 1;
        ret = setsockopt(lircd_socket, SOL_SOCKET, SO_KEEPALIVE, &i, sizeof(i));
        if (ret < 0)
        {
            LOG(VB_GENERAL, LOG_WARNING, LOC +
                QString("Failed setting KEEPALIVE option for socket '%1'")
                    .arg(lircdDevice) + ENO);
        }
    }

    d->m_lircState = lirc_init("/etc/lircrc", ".lircrc", "mythtv", nullptr, 0);
    if (!d->m_lircState)
    {
        close(lircd_socket);
        return false;
    }
    d->m_lircState->lirc_lircd = lircd_socket;

    // parse the config file
    if (!d->m_lircConfig)
    {
        QMutexLocker static_lock(&lirclib_lock);
        QByteArray cfg = configFile.toLocal8Bit();
        if (lirc_readconfig(d->m_lircState, cfg.constData(), &d->m_lircConfig, nullptr))
        {
            LOG(vtype, LOG_ERR, LOC +
                QString("Failed to read config file '%1'").arg(configFile));

            lirc_deinit(d->m_lircState);
            d->m_lircState = nullptr;
            return false;
        }
    }

    LOG(VB_GENERAL, LOG_INFO, LOC +
        QString("Successfully initialized '%1' using '%2' config")
            .arg(lircdDevice).arg(configFile));

    return true;
}

void LIRC::start(void)
{
    QMutexLocker locker(&lock);

    if (!d->m_lircState)
    {
        LOG(VB_GENERAL, LOG_ERR, "start() called without lircd socket");
        return;
    }

    doRun = true;
    MThread::start();
}

bool LIRC::IsDoRunSet(void) const
{
    QMutexLocker locker(&lock);
    return doRun;
}

void LIRC::Process(const QByteArray &data)
{
    QMutexLocker static_lock(&lirclib_lock);

    // lirc_code2char will make code point to a static datafer..
    char *code = nullptr;
    int ret = lirc_code2char(
        d->m_lircState, d->m_lircConfig, const_cast<char*>(data.constData()), &code);

    while ((0 == ret) && code)
    {
        QString lirctext(code);
        QString qtcode = code;
        qtcode.replace("ctrl-",  "ctrl+",  Qt::CaseInsensitive);
        qtcode.replace("alt-",   "alt+",   Qt::CaseInsensitive);
        qtcode.replace("shift-", "shift+", Qt::CaseInsensitive);
        qtcode.replace("meta-",  "meta+",  Qt::CaseInsensitive);
        QKeySequence a(qtcode);

        // Send a dummy keycode if we couldn't convert the key sequence.
        // This is done so the main code can output a warning for bad
        // mappings.
        if (a.isEmpty())
        {
            QCoreApplication::postEvent(
                m_mainWindow, new LircKeycodeEvent(
                    QEvent::KeyPress, 0,
                    (Qt::KeyboardModifiers)
                    LircKeycodeEvent::kLIRCInvalidKeyCombo,
                    QString(), lirctext));
        }

        vector<LircKeycodeEvent*> keyReleases;

        for (int i = 0; i < a.count(); i++)
        {
            int keycode = a[i];
            Qt::KeyboardModifiers mod = Qt::NoModifier;
            mod |= (Qt::SHIFT & keycode) ? Qt::ShiftModifier : Qt::NoModifier;
            mod |= (Qt::META  & keycode) ? Qt::MetaModifier  : Qt::NoModifier;
            mod |= (Qt::CTRL  & keycode) ? Qt::ControlModifier: Qt::NoModifier;
            mod |= (Qt::ALT   & keycode) ? Qt::AltModifier   : Qt::NoModifier;

            keycode &= ~Qt::MODIFIER_MASK;

            QString text = "";
            if (!mod)
                text = QString(QChar(keycode));

            QCoreApplication::postEvent(
                m_mainWindow, new LircKeycodeEvent(
                    QEvent::KeyPress, keycode, mod, text, lirctext));

            keyReleases.push_back(
                new LircKeycodeEvent(
                    QEvent::KeyRelease, keycode, mod, text, lirctext));
        }

        for (int i = (int)keyReleases.size() - 1; i>=0; i--)
            QCoreApplication::postEvent(m_mainWindow, keyReleases[i]);

        ret = lirc_code2char(
            d->m_lircState, d->m_lircConfig, const_cast<char*>(data.constData()), &code);
    }
}

void LIRC::run(void)
{
    RunProlog();
#if 0
    LOG(VB_GENERAL, LOG_DEBUG, LOC + "run -- start");
#endif
    /* Process all events read */
    while (IsDoRunSet())
    {
        if (eofCount && retryCount)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

        if ((eofCount >= 10) || (!d->m_lircState))
        {
            QMutexLocker locker(&lock);
            eofCount = 0;
            if (++retryCount > 1000)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    "Failed to reconnect, exiting LIRC thread.");
                doRun = false;
                continue;
            }
            LOG(VB_FILE, LOG_WARNING, LOC + "EOF -- reconnecting");

            lirc_deinit(d->m_lircState);
            d->m_lircState = nullptr;

            if (Init())
                retryCount = 0;
            else
                // wait a while before we retry..
                std::this_thread::sleep_for(std::chrono::seconds(2));

            continue;
        }

        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(d->m_lircState->lirc_lircd, &readfds);

        // the maximum time select() should wait
        struct timeval timeout {1, 100 * 1000}; // 1 second, 100 ms

        int ret = select(d->m_lircState->lirc_lircd + 1, &readfds, nullptr, nullptr,
                         &timeout);

        if (ret < 0 && errno != EINTR)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "select() failed" + ENO);
            continue;
        }

        //  0: Timer expired with no data, repeat select
        // -1: Iinterrupted while waiting, repeat select
        if (ret <= 0)
            continue;

        QList<QByteArray> codes = GetCodes();
        for (uint i = 0; i < (uint) codes.size(); i++)
            Process(codes[i]);
    }
#if 0
    LOG(VB_GENERAL, LOG_DEBUG, LOC + "run -- end");
#endif
    RunEpilog();
}

QList<QByteArray> LIRC::GetCodes(void)
{
    QList<QByteArray> ret;
    ssize_t len = -1;

    uint buf_size = buf.size() + 128;
    buf.resize(buf_size);

    while (true)
    {
        len = read(d->m_lircState->lirc_lircd, buf.data() + buf_offset, 128);
        if (len >= 0)
            break;

        switch (errno)
        {
            case EINTR:
                continue;

            case EAGAIN:
                return ret;

            case ENOTCONN:   // 107 (according to asm-generic/errno.h)
                if (!eofCount)
                    LOG(VB_GENERAL, LOG_NOTICE, LOC + "GetCodes -- EOF?");
                eofCount++;
                return ret;

            default:
                LOG(VB_GENERAL, LOG_ERR, LOC + "Could not read socket" + ENO);
                return ret;
        }
    }

    if (!len)
    {
        if (!eofCount)
            LOG(VB_GENERAL, LOG_NOTICE, LOC + "GetCodes -- eof?");
        eofCount++;
        return ret;
    }

    eofCount   = 0;
    retryCount = 0;

    buf_offset += len;
    buf.truncate(buf_offset);
    ret = buf.split('\n');
    if (buf.endsWith('\n'))
    {
        buf_offset = 0;
        return ret;
    }

    buf = ret.takeLast();
    buf_offset = buf.size();
    return ret;
}
