// -*- Mode: c++ -*-

// POSIX headers
#include <thread>
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <algorithm>
#if !defined( _WIN32 )
#include <poll.h>
#include <sys/ioctl.h>
#endif

#include <QtGlobal>
#ifdef Q_OS_ANDROID
#include <sys/wait.h>
#endif

// Qt headers
#include <QString>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

// MythTV headers
#include "config.h"
#include "libmythbase/exitcodes.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythrandom.h"

#include "ExternalChannel.h"
#include "ExternalStreamHandler.h"
//#include "ThreadedFileWriter.h"
#include "cardutil.h"
#include "dtvsignalmonitor.h"
#include "mpeg/mpegstreamdata.h"
#include "mpeg/streamlisteners.h"

#define LOC QString("ExternSH[%1](%2): ").arg(m_inputId).arg(m_loc)

ExternIO::ExternIO(const QString & app,
                   const QStringList & args)
    : m_app(QFileInfo(app)),
      m_status(&m_statusBuf, QIODevice::ReadWrite)
{
    if (!m_app.exists())
    {
        m_error = QString("ExternIO: '%1' does not exist.").arg(app);
        return;
    }
    if (!m_app.isReadable() || !m_app.isFile())
    {
        m_error = QString("ExternIO: '%1' is not readable.")
                  .arg(m_app.canonicalFilePath());
        return;
    }
    if (!m_app.isExecutable())
    {
        m_error = QString("ExternIO: '%1' is not executable.")
                  .arg(m_app.canonicalFilePath());
        return;
    }

    m_args = args;
    m_args.prepend(m_app.baseName());

    m_status.setString(&m_statusBuf);
}

ExternIO::~ExternIO(void)
{
    close(m_appIn);
    close(m_appOut);
    close(m_appErr);

    // waitpid(m_pid, &status, 0);
    delete[] m_buffer;
}

bool ExternIO::Ready([[maybe_unused]] int fd,
                     [[maybe_unused]] std::chrono::milliseconds timeout,
                     [[maybe_unused]] const QString & what)
{
#if !defined( _WIN32 )
    std::array<struct pollfd,2> m_poll {};

    m_poll[0].fd = fd;
    m_poll[0].events = POLLIN | POLLPRI;
    int ret = poll(m_poll.data(), 1, timeout.count());

    if (m_poll[0].revents & POLLHUP)
    {
        m_error = what + " poll eof (POLLHUP)";
        return false;
    }
    if (m_poll[0].revents & POLLNVAL)
    {
        LOG(VB_GENERAL, LOG_ERR, "poll error");
        return false;
    }
    if (m_poll[0].revents & POLLIN)
    {
        if (ret > 0)
            return true;

        if ((EOVERFLOW == errno))
            m_error = "poll overflow";
        return false;
    }
#endif // !defined( _WIN32 )
    return false;
}

int ExternIO::Read(QByteArray & buffer, int maxlen, std::chrono::milliseconds timeout)
{
    if (Error())
    {
        LOG(VB_RECORD, LOG_ERR,
            QString("ExternIO::Read: already in error state: '%1'")
            .arg(m_error));
        return 0;
    }

    if (!Ready(m_appOut, timeout, "data"))
        return 0;

    if (m_bufSize < maxlen)
    {
        m_bufSize = maxlen;
        delete [] m_buffer;
        m_buffer = new char[m_bufSize];
    }

    int len = read(m_appOut, m_buffer, maxlen);

    if (len < 0)
    {
        if (errno == EAGAIN)
        {
            if (++m_errCnt > kMaxErrorCnt)
            {
                m_error = "Failed to read from External Recorder: " + ENO;
                LOG(VB_RECORD, LOG_WARNING,
                    "External Recorder not ready. Giving up.");
            }
            else
            {
                LOG(VB_RECORD, LOG_WARNING,
                    QString("External Recorder not ready. Will retry (%1/%2).")
                    .arg(m_errCnt).arg(kMaxErrorCnt));
                std::this_thread::sleep_for(100ms);
            }
        }
        else
        {
            m_error = "Failed to read from External Recorder: " + ENO;
            LOG(VB_RECORD, LOG_ERR, m_error);
        }
    }
    else
    {
        m_errCnt = 0;
    }

    if (len == 0)
        return 0;

    buffer.append(m_buffer, len);

    LOG(VB_RECORD, LOG_DEBUG,
        QString("ExternIO::Read '%1' bytes, buffer size %2")
        .arg(len).arg(buffer.size()));

    return len;
}

QByteArray ExternIO::GetStatus(std::chrono::milliseconds timeout)
{
    if (Error())
    {
        LOG(VB_RECORD, LOG_ERR,
            QString("ExternIO::GetStatus: already in error state: '%1'")
            .arg(m_error));
        return {};
    }

    std::chrono::milliseconds waitfor = m_status.atEnd() ? timeout : 0ms;
    if (Ready(m_appErr, waitfor, "status"))
    {
        std::array<char,2048> buffer {};
        int len = read(m_appErr, buffer.data(), buffer.size());
        m_status << QString::fromLatin1(buffer.data(), len);
    }

    if (m_status.atEnd())
        return {};

    QString msg = m_status.readLine();

    LOG(VB_RECORD, LOG_DEBUG, QString("ExternIO::GetStatus '%1'")
        .arg(msg));

    return msg.toUtf8();
}

int ExternIO::Write(const QByteArray & buffer)
{
    if (Error())
    {
        LOG(VB_RECORD, LOG_ERR,
            QString("ExternIO::Write: already in error state: '%1'")
            .arg(m_error));
        return -1;
    }

    LOG(VB_RECORD, LOG_DEBUG, QString("ExternIO::Write('%1')")
        .arg(QString(buffer).simplified()));

    int len = write(m_appIn, buffer.constData(), buffer.size());
    if (len != buffer.size())
    {
        if (len > 0)
        {
            LOG(VB_RECORD, LOG_WARNING,
                QString("ExternIO::Write: only wrote %1 of %2 bytes '%3'")
                .arg(len).arg(buffer.size()).arg(QString(buffer)));
        }
        else
        {
            m_error = QString("ExternIO: Failed to write '%1' to app's stdin: ")
                      .arg(QString(buffer)) + ENO;
            return -1;
        }
    }

    return len;
}

bool ExternIO::Run(void)
{
    LOG(VB_RECORD, LOG_INFO, QString("ExternIO::Run()"));

    Fork();
    GetStatus(10ms);

    return true;
}

/* Return true if the process is not, or is no longer running */
bool ExternIO::KillIfRunning([[maybe_unused]] const QString & cmd)
{
#if defined(Q_OS_DARWIN) || defined(__FreeBSD__) || defined(__OpenBSD__)
    return false;
#elif defined( _WIN32 )
    return false;
#else
    QString grp = QString("pgrep -x -f -- \"%1\" 2>&1 > /dev/null").arg(cmd);
    QString kil = QString("pkill --signal 15 -x -f -- \"%1\" 2>&1 > /dev/null")
                  .arg(cmd);

    int res_grp = system(grp.toUtf8().constData());
    if (WEXITSTATUS(res_grp) == 1)
    {
        LOG(VB_RECORD, LOG_DEBUG, QString("'%1' not running.").arg(cmd));
        return true;
    }

    LOG(VB_RECORD, LOG_WARNING, QString("'%1' already running, killing...")
        .arg(cmd));
    int res_kil = system(kil.toUtf8().constData());
    if (WEXITSTATUS(res_kil) == 1)
        LOG(VB_GENERAL, LOG_WARNING, QString("'%1' failed: %2")
            .arg(kil, ENO));

    res_grp = system(grp.toUtf8().constData());
    if (WEXITSTATUS(res_grp) == 1)
    {
        LOG(WEXITSTATUS(res_kil) == 0 ? VB_RECORD : VB_GENERAL, LOG_WARNING,
            QString("'%1' terminated.").arg(cmd));
        return true;
    }

    std::this_thread::sleep_for(50ms);

    kil = QString("pkill --signal 9 -x -f \"%1\" 2>&1 > /dev/null").arg(cmd);
    res_kil = system(kil.toUtf8().constData());
    if (WEXITSTATUS(res_kil) > 0)
        LOG(VB_GENERAL, LOG_WARNING, QString("'%1' failed: %2")
            .arg(kil, ENO));

    res_grp = system(grp.toUtf8().constData());
    LOG(WEXITSTATUS(res_kil) == 0 ? VB_RECORD : VB_GENERAL, LOG_WARNING,
        QString("'%1' %2.")
        .arg(cmd, WEXITSTATUS(res_grp) == 0 ? "sill running" : "terminated"));

    return (WEXITSTATUS(res_grp) != 0);
#endif
}

void ExternIO::Fork(void)
{
#if !defined( _WIN32 )
    if (Error())
    {
        LOG(VB_RECORD, LOG_INFO, QString("ExternIO in bad state: '%1'")
            .arg(m_error));
        return;
    }

    QString full_command = QString("%1").arg(m_args.join(" "));

    if (!KillIfRunning(full_command))
    {
        // Give it one more chance.
        std::this_thread::sleep_for(50ms);
        if (!KillIfRunning(full_command))
        {
            m_error = QString("Unable to kill existing '%1'.")
                      .arg(full_command);
            LOG(VB_GENERAL, LOG_ERR, m_error);
            return;
        }
    }


    LOG(VB_RECORD, LOG_INFO, QString("ExternIO::Fork '%1'").arg(full_command));

    std::array<int,2> in  = {-1, -1};
    std::array<int,2> out = {-1, -1};
    std::array<int,2> err = {-1, -1};

    if (pipe(in.data()) < 0)
    {
        m_error = "pipe(in) failed: " + ENO;
        return;
    }
    if (pipe(out.data()) < 0)
    {
        m_error = "pipe(out) failed: " + ENO;
        close(in[0]);
        close(in[1]);
        return;
    }
    if (pipe(err.data()) < 0)
    {
        m_error = "pipe(err) failed: " + ENO;
        close(in[0]);
        close(in[1]);
        close(out[0]);
        close(out[1]);
        return;
    }

    m_pid = fork();
    if (m_pid < 0)
    {
        // Failed
        m_error = "fork() failed: " + ENO;
        return;
    }
    if (m_pid > 0)
    {
        // Parent
        close(in[0]);
        close(out[1]);
        close(err[1]);
        m_appIn  = in[1];
        m_appOut = out[0];
        m_appErr = err[0];

        bool error = false;
        error  = (fcntl(m_appIn,  F_SETFL, O_NONBLOCK) == -1);
        error |= (fcntl(m_appOut, F_SETFL, O_NONBLOCK) == -1);
        error |= (fcntl(m_appErr, F_SETFL, O_NONBLOCK) == -1);

        if (error)
        {
            LOG(VB_GENERAL, LOG_WARNING,
                "ExternIO::Fork(): Failed to set O_NONBLOCK for FD: " + ENO);
            std::this_thread::sleep_for(2s);
            _exit(GENERIC_EXIT_PIPE_FAILURE);
        }

        LOG(VB_RECORD, LOG_INFO, "Spawned");
        return;
    }

    // Child
    close(in[1]);
    close(out[0]);
    close(err[0]);
    if (dup2( in[0], 0) < 0)
    {
        std::cerr << "dup2(stdin) failed: " << strerror(errno);
        _exit(GENERIC_EXIT_PIPE_FAILURE);
    }
    else if (dup2(out[1], 1) < 0)
    {
        std::cerr << "dup2(stdout) failed: " << strerror(errno);
        _exit(GENERIC_EXIT_PIPE_FAILURE);
    }
    else if (dup2(err[1], 2) < 0)
    {
        std::cerr << "dup2(stderr) failed: " << strerror(errno);
        _exit(GENERIC_EXIT_PIPE_FAILURE);
    }

    /* Close all open file descriptors except stdin/stdout/stderr */
#if HAVE_CLOSE_RANGE
    close_range(3, sysconf(_SC_OPEN_MAX) - 1, 0);
#else
    for (int i = sysconf(_SC_OPEN_MAX) - 1; i > 2; --i)
        close(i);
#endif

    /* Set the process group id to be the same as the pid of this
     * child process.  This ensures that any subprocesses launched by this
     * process can be killed along with the process itself. */
    if (setpgid(0,0) < 0)
    {
        std::cerr << "ExternIO: "
             << "setpgid() failed: "
             << strerror(errno) << std::endl;
    }

    /* run command */
    char *command = strdup(m_app.canonicalFilePath()
                                 .toUtf8().constData());
    // Copy QStringList to char**
    char **arguments = new char*[m_args.size() + 1];
    for (int i = 0; i < m_args.size(); ++i)
    {
        int len = m_args[i].size() + 1;
        arguments[i] = new char[len];
        memcpy(arguments[i], m_args[i].toStdString().c_str(), len);
    }
    arguments[m_args.size()] = nullptr;

    if (execv(command, arguments) < 0)
    {
        // Can't use LOG due to locking fun.
        std::cerr << "ExternIO: "
             << "execv() failed: "
             << strerror(errno) << std::endl;
    }
    else
    {
        std::cerr << "ExternIO: "
                  << "execv() should not be here?: "
                  << strerror(errno) << std::endl;
    }

#endif // !defined( _WIN32 )

    /* Failed to exec */
    _exit(GENERIC_EXIT_DAEMONIZING_ERROR); // this exit is ok
}


QMap<int, ExternalStreamHandler*> ExternalStreamHandler::s_handlers;
QMap<int, uint>                   ExternalStreamHandler::s_handlersRefCnt;
QMutex                            ExternalStreamHandler::s_handlersLock;

ExternalStreamHandler *ExternalStreamHandler::Get(const QString &devname,
                                                  int inputid, int majorid)
{
    QMutexLocker locker(&s_handlersLock);

    QMap<int, ExternalStreamHandler*>::iterator it = s_handlers.find(majorid);

    if (it == s_handlers.end())
    {
        auto *newhandler = new ExternalStreamHandler(devname, inputid, majorid);
        s_handlers[majorid] = newhandler;
        s_handlersRefCnt[majorid] = 1;

        LOG(VB_RECORD, LOG_INFO,
            QString("ExternSH[%1:%2]: Creating new stream handler for %3 "
                    "(1 in use)")
            .arg(inputid).arg(majorid).arg(devname));
    }
    else
    {
        ++s_handlersRefCnt[majorid];
        uint rcount = s_handlersRefCnt[majorid];
        LOG(VB_RECORD, LOG_INFO,
            QString("ExternSH[%1:%2]: Using existing stream handler for %3")
            .arg(inputid).arg(majorid).arg(devname) +
            QString(" (%1 in use)").arg(rcount));
    }

    return s_handlers[majorid];
}

void ExternalStreamHandler::Return(ExternalStreamHandler * & ref,
                                   int inputid)
{
    QMutexLocker locker(&s_handlersLock);

    int majorid = ref->m_majorId;

    QMap<int, uint>::iterator rit = s_handlersRefCnt.find(majorid);
    if (rit == s_handlersRefCnt.end())
        return;

    QMap<int, ExternalStreamHandler*>::iterator it =
        s_handlers.find(majorid);

    if (*rit > 1)
    {
        ref = nullptr;
        --(*rit);

        LOG(VB_RECORD, LOG_INFO,
            QString("ExternSH[%1:%2]: Return handler (%3 still in use)")
            .arg(inputid).arg(majorid).arg(*rit));

        return;
    }

    if ((it != s_handlers.end()) && (*it == ref))
    {
        LOG(VB_RECORD, LOG_INFO,
            QString("ExternSH[%1:%2]: Closing handler (0 in use)")
            .arg(inputid).arg(majorid));
        delete *it;
        s_handlers.erase(it);
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("ExternSH[%1:%2]: Error: No handler to return!")
            .arg(inputid).arg(majorid));
    }

    s_handlersRefCnt.erase(rit);
    ref = nullptr;
}

/*
  ExternalStreamHandler
 */

ExternalStreamHandler::ExternalStreamHandler(const QString & path,
                                             int inputid,
                                             int majorid)
    : StreamHandler(path, inputid)
    , m_loc(m_device)
    , m_majorId(majorid)
{
    setObjectName("ExternSH");

    m_args = path.split(' ',Qt::SkipEmptyParts) +
             logPropagateArgs.split(' ', Qt::SkipEmptyParts);
    //NOLINTNEXTLINE(cppcoreguidelines-prefer-member-initializer)
    m_app = m_args.first();
    m_args.removeFirst();

    // Pass one (and only one) 'quiet'
    if (!m_args.contains("--quiet") && !m_args.contains("-q"))
        m_args << "--quiet";

    m_args << "--inputid" << QString::number(majorid);
    LOG(VB_RECORD, LOG_INFO, LOC + QString("args \"%1\"")
        .arg(m_args.join(" ")));

    if (!OpenApp())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Failed to start %1").arg(m_device));
    }
}

int ExternalStreamHandler::StreamingCount(void) const
{
    return m_streamingCnt.loadAcquire();
}

void ExternalStreamHandler::run(void)
{
    QString    result;
    QString    ready_cmd;
    QByteArray buffer;
    int        sz = 0;
    uint       len = 0;
    uint       read_len = 0;
    uint       restart_cnt = 0;
    MythTimer  status_timer;
    MythTimer  nodata_timer;

    bool       good_data = false;
    uint       data_proc_err = 0;
    uint       data_short_err = 0;

    if (!m_io)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("%1 is not running.").arg(m_device));
    }

    status_timer.start();

    RunProlog();

    LOG(VB_RECORD, LOG_INFO, LOC + "run(): begin");

    SetRunning(true, true, false);

    if (m_pollMode)
        ready_cmd = "SendBytes";
    else
        ready_cmd = "XON";

    uint remainder = 0;
    while (m_runningDesired && !m_bError)
    {
        if (!IsTSOpen())
        {
            LOG(VB_RECORD, LOG_WARNING, LOC + "TS not open yet.");
            std::this_thread::sleep_for(10ms);
            continue;
        }

        if (StreamingCount() == 0)
        {
            std::this_thread::sleep_for(10ms);
            continue;
        }

        UpdateFiltersFromStreamData();

        if (!m_xon || m_pollMode)
        {
            if (buffer.size() > TOO_FAST_SIZE)
            {
                LOG(VB_RECORD, LOG_WARNING, LOC +
                    "Internal buffer too full to accept more data from "
                    "external application.");
            }
            else
            {
                if (!ProcessCommand(ready_cmd, result))
                {
                    if (result.startsWith("ERR"))
                    {
                        LOG(VB_GENERAL, LOG_ERR, LOC +
                            QString("Aborting: %1 -> %2")
                            .arg(ready_cmd, result));
                        m_bError = true;
                        continue;
                    }

                    if (restart_cnt++)
                        std::this_thread::sleep_for(20s);
                    if (!RestartStream())
                    {
                        LOG(VB_RECORD, LOG_ERR, LOC +
                            "Failed to restart stream.");
                        m_bError = true;
                    }
                    continue;
                }
                m_xon = true;
            }
        }

        if (m_xon)
        {
            if (status_timer.elapsed() >= 2s)
            {
                // Since we may never need to send the XOFF
                // command, occationally check to see if the
                // External recorder needs to report an issue.
                if (CheckForError())
                {
                    if (restart_cnt++)
                        std::this_thread::sleep_for(20s);
                    if (!RestartStream())
                    {
                        LOG(VB_RECORD, LOG_ERR, LOC +
                            "Failed to restart stream.");
                        m_bError = true;
                    }
                    continue;
                }

                status_timer.restart();
            }

            if (buffer.size() > TOO_FAST_SIZE)
            {
                if (!m_pollMode)
                {
                    // Data is comming a little too fast, so XOFF
                    // to give us time to process it.
                    if (!ProcessCommand(QString("XOFF"), result))
                    {
                        if (result.startsWith("ERR"))
                        {
                            LOG(VB_GENERAL, LOG_ERR, LOC +
                                QString("Aborting: XOFF -> %2")
                                .arg(result));
                            m_bError = true;
                        }
                    }
                    m_xon = false;
                }
            }

            read_len = 0;
            if (m_io != nullptr)
            {
                sz = PACKET_SIZE - remainder;
                if (sz > 0)
                    read_len = m_io->Read(buffer, sz, 100ms);
            }
        }
        else
        {
            read_len = 0;
        }

        if (read_len == 0)
        {
            if (!nodata_timer.isRunning())
                nodata_timer.start();
            else
            {
                if (nodata_timer.elapsed() >= 50s)
                {
                    LOG(VB_GENERAL, LOG_WARNING, LOC +
                        "No data for 50 seconds, Restarting stream.");
                    if (!RestartStream())
                    {
                        LOG(VB_RECORD, LOG_ERR, LOC +
                            "Failed to restart stream.");
                        m_bError = true;
                    }
                    nodata_timer.stop();
                    continue;
                }
            }

            std::this_thread::sleep_for(50ms);

            // HLS type streams may only produce data every ~10 seconds
            if (nodata_timer.elapsed() < 12s && buffer.size() < TS_PACKET_SIZE)
                continue;
        }
        else
        {
            nodata_timer.stop();
            restart_cnt = 0;
        }

        if (m_io == nullptr)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "I/O thread has disappeared!");
            m_bError = true;
            break;
        }
        if (m_io->Error())
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("Fatal Error from External Recorder: %1")
                .arg(m_io->ErrorString()));
            CloseApp();
            m_bError = true;
            break;
        }

        len = remainder = buffer.size();

        if (len == 0)
            continue;

        if (len < TS_PACKET_SIZE)
        {
            if (m_xon && data_short_err++ == 0)
                LOG(VB_RECORD, LOG_INFO, LOC + "Waiting for a full TS packet.");
            std::this_thread::sleep_for(50us);
            continue;
        }
        if (data_short_err)
        {
            if (data_short_err > 1)
            {
                LOG(VB_RECORD, LOG_INFO, LOC +
                    QString("Waited for a full TS packet %1 times.")
                    .arg(data_short_err));
            }
            data_short_err = 0;
        }

        if (!m_streamLock.tryLock())
            continue;

        if (!m_listenerLock.tryLock())
            continue;

        for (auto sit = m_streamDataList.cbegin();
             sit != m_streamDataList.cend(); ++sit)
        {
            remainder = sit.key()->ProcessData
                        (reinterpret_cast<const uint8_t *>
                         (buffer.constData()), buffer.size());
        }

        m_listenerLock.unlock();

        if (m_replay)
        {
            m_replayBuffer += buffer.left(len - remainder);
            if (m_replayBuffer.size() > (50 * PACKET_SIZE))
            {
                m_replayBuffer.remove(0, len - remainder);
                LOG(VB_RECORD, LOG_WARNING, LOC +
                    QString("Replay size truncated to %1 bytes")
                    .arg(m_replayBuffer.size()));
            }
        }

        m_streamLock.unlock();

        if (remainder == 0)
        {
            buffer.clear();
            good_data = (len != 0U);
        }
        else if (len > remainder) // leftover bytes
        {
            buffer.remove(0, len - remainder);
            good_data = (len != 0U);
        }
        else if (len == remainder)
        {
            good_data = false;
        }

        if (good_data)
        {
            if (data_proc_err)
            {
                if (data_proc_err > 1)
                {
                    LOG(VB_RECORD, LOG_WARNING, LOC +
                        QString("Failed to process the data received %1 times.")
                        .arg(data_proc_err));
                }
                data_proc_err = 0;
            }
        }
        else
        {
            if (data_proc_err++ == 0)
            {
                LOG(VB_RECORD, LOG_WARNING, LOC +
                    "Failed to process the data received");
            }
        }
    }

    LOG(VB_RECORD, LOG_INFO, LOC + "run(): " +
        QString("%1 shutdown").arg(m_bError ? "Error" : "Normal"));

    RemoveAllPIDFilters();
    SetRunning(false, true, false);

    LOG(VB_RECORD, LOG_INFO, LOC + "run(): " + "end");

    RunEpilog();
}

bool ExternalStreamHandler::SetAPIVersion(void)
{
    QString result;

    if (ProcessCommand("APIVersion?", result, 10s))
    {
        QStringList tokens = result.split(':', Qt::SkipEmptyParts);
        if (tokens.size() > 1)
            m_apiVersion = tokens[1].toUInt();
        m_apiVersion = std::min(m_apiVersion, static_cast<int>(MAX_API_VERSION));
        if (m_apiVersion < 1)
        {
            LOG(VB_RECORD, LOG_ERR, LOC +
                QString("Bad response to 'APIVersion?' - '%1'. "
                        "Expecting 1, 2 or 3").arg(result));
            m_apiVersion = 1;
        }

        ProcessCommand(QString("APIVersion:%1").arg(m_apiVersion), result);
        return true;
    }

    return false;
}

QString ExternalStreamHandler::UpdateDescription(void)
{
    if (m_apiVersion > 1)
    {
        QString result;

        if (ProcessCommand("Description?", result))
            m_loc = result.mid(3);
        else
            m_loc = m_device;
    }

    return m_loc;
}

bool ExternalStreamHandler::OpenApp(void)
{
    {
        QMutexLocker locker(&m_ioLock);

        if (m_io)
        {
            LOG(VB_RECORD, LOG_WARNING, LOC + "OpenApp: already open!");
            return true;
        }

        m_io = new ExternIO(m_app, m_args);

        if (m_io == nullptr)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "ExternIO failed: " + ENO);
            m_bError = true;
        }
        else
        {
            LOG(VB_RECORD, LOG_INFO, LOC + QString("Spawn '%1'").arg(m_device));
            m_io->Run();
            if (m_io->Error())
            {
                LOG(VB_GENERAL, LOG_ERR,
                    "Failed to start External Recorder: " + m_io->ErrorString());
                delete m_io;
                m_io = nullptr;
                m_bError = true;
                return false;
            }
        }
    }

    QString result;

    if (!SetAPIVersion())
    {
        // Try again using API version 2
        m_apiVersion = 2;
        if (!SetAPIVersion())
            m_apiVersion = 1;
    }

    if (!IsAppOpen())
    {
        LOG(VB_RECORD, LOG_ERR, LOC + "Application is not responding.");
        m_bError = true;
        return false;
    }

    UpdateDescription();

    // Gather capabilities
    if (!ProcessCommand("HasTuner?", result))
    {
        LOG(VB_RECORD, LOG_ERR, LOC +
            QString("Bad response to 'HasTuner?' - '%1'").arg(result));
        m_bError = true;
        return false;
    }
    m_hasTuner = result.startsWith("OK:Yes");

    if (!ProcessCommand("HasPictureAttributes?", result))
    {
        LOG(VB_RECORD, LOG_ERR, LOC +
            QString("Bad response to 'HasPictureAttributes?' - '%1'")
            .arg(result));
        m_bError = true;
        return false;
    }
    m_hasPictureAttributes = result.startsWith("OK:Yes");

    /* Operate in "poll" or "xon/xoff" mode */
    m_pollMode = ProcessCommand("FlowControl?", result) &&
                  result.startsWith("OK:Poll");

    LOG(VB_RECORD, LOG_INFO, LOC + "App opened successfully");
    LOG(VB_RECORD, LOG_INFO, LOC +
        QString("Capabilities: tuner(%1) "
                "Picture attributes(%2) "
                "Flow control(%3)")
        .arg(m_hasTuner ? "yes" : "no",
             m_hasPictureAttributes ? "yes" : "no",
             m_pollMode ? "Polling" : "XON/XOFF")
        );

    /* Let the external app know how many bytes will read without blocking */
    ProcessCommand(QString("BlockSize:%1").arg(PACKET_SIZE), result);

    return true;
}

bool ExternalStreamHandler::IsAppOpen(void)
{
    if (m_io == nullptr)
    {
        LOG(VB_RECORD, LOG_WARNING, LOC +
            "WARNING: Unable to communicate with external app.");
        return false;
    }

    QString result;
    return ProcessCommand("Version?", result, 10s);
}

bool ExternalStreamHandler::IsTSOpen(void)
{
    if (m_tsOpen)
        return true;

    QString result;

    if (!ProcessCommand("IsOpen?", result))
        return false;

    m_tsOpen = true;
    return m_tsOpen;
}

void ExternalStreamHandler::CloseApp(void)
{
    m_ioLock.lock();
    if (m_io)
    {
        QString result;

        LOG(VB_RECORD, LOG_INFO, LOC + "CloseRecorder");
        m_ioLock.unlock();
        ProcessCommand("CloseRecorder", result, 10s);
        m_ioLock.lock();

        if (!result.startsWith("OK"))
        {
            LOG(VB_RECORD, LOG_INFO, LOC +
                "CloseRecorder failed, sending kill.");

            QString full_command = QString("%1").arg(m_args.join(" "));

            if (!ExternIO::KillIfRunning(full_command))
            {
                // Give it one more chance.
                std::this_thread::sleep_for(50ms);
                if (!ExternIO::KillIfRunning(full_command))
                {
                    LOG(VB_GENERAL, LOG_ERR,
                        QString("Unable to kill existing '%1'.")
                        .arg(full_command));
                    return;
                }
            }
        }
        delete m_io;
        m_io = nullptr;
    }
    m_ioLock.unlock();
}

bool ExternalStreamHandler::RestartStream(void)
{
    bool streaming = (StreamingCount() > 0);

    LOG(VB_RECORD, LOG_INFO, LOC + "Restarting stream.");
    m_damaged = true;

    if (streaming)
        StopStreaming();

    std::this_thread::sleep_for(1s);

    if (streaming)
        return StartStreaming();

    return true;
}

void ExternalStreamHandler::ReplayStream(void)
{
    if (m_replay)
    {
        QString    result;

        // Let the external app know that we could be busy for a little while
        if (!m_pollMode)
        {
            ProcessCommand(QString("XOFF"), result);
            m_xon = false;
        }

        /* If the input is not a 'broadcast' it may only have one
         * copy of the SPS right at the beginning of the stream,
         * so make sure we don't miss it!
         */
        QMutexLocker listen_lock(&m_listenerLock);

        if (!m_streamDataList.empty())
        {
            for (auto sit = m_streamDataList.cbegin();
                 sit != m_streamDataList.cend(); ++sit)
            {
                sit.key()->ProcessData(reinterpret_cast<const uint8_t *>
                                       (m_replayBuffer.constData()),
                                       m_replayBuffer.size());
            }
        }
        LOG(VB_RECORD, LOG_INFO, LOC + QString("Replayed %1 bytes")
            .arg(m_replayBuffer.size()));
        m_replayBuffer.clear();
        m_replay = false;

        // Let the external app know that we are ready
        if (!m_pollMode)
        {
            if (ProcessCommand(QString("XON"), result))
                m_xon = true;
        }
    }
}

bool ExternalStreamHandler::StartStreaming(void)
{
    QString result;

    QMutexLocker locker(&m_streamLock);

    UpdateDescription();

    LOG(VB_RECORD, LOG_INFO, LOC +
        QString("StartStreaming with %1 current listeners")
        .arg(StreamingCount()));

    if (!IsAppOpen())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "External Recorder not started.");
        return false;
    }

    if (StreamingCount() == 0)
    {
        if (!ProcessCommand("StartStreaming", result, 15s))
        {
            LogLevel_t level = LOG_ERR;
            if (result.startsWith("warn", Qt::CaseInsensitive))
                level = LOG_WARNING;
            else
                m_bError = true;

            LOG(VB_GENERAL, level, LOC + QString("StartStreaming failed: '%1'")
                .arg(result));

            return false;
        }

        LOG(VB_RECORD, LOG_INFO, LOC + "Streaming started");
    }
    else
    {
        LOG(VB_RECORD, LOG_INFO, LOC + "Already streaming");
    }

    m_streamingCnt.ref();

    LOG(VB_RECORD, LOG_INFO, LOC +
        QString("StartStreaming %1 listeners")
        .arg(StreamingCount()));

    return true;
}

bool ExternalStreamHandler::StopStreaming(void)
{
    QMutexLocker locker(&m_streamLock);

    LOG(VB_RECORD, LOG_INFO, LOC +
        QString("StopStreaming %1 listeners")
        .arg(StreamingCount()));

    if (StreamingCount() == 0)
    {
        LOG(VB_RECORD, LOG_INFO, LOC +
            "StopStreaming requested, but we are not streaming!");
        return true;
    }

    if (m_streamingCnt.deref())
    {
        LOG(VB_RECORD, LOG_INFO, LOC +
            QString("StopStreaming delayed, still have %1 listeners")
            .arg(StreamingCount()));
        return true;
    }

    LOG(VB_RECORD, LOG_INFO, LOC + "StopStreaming");

    if (!m_pollMode && m_xon)
    {
        QString result;
        ProcessCommand(QString("XOFF"), result);
        m_xon = false;
    }

    if (!IsAppOpen())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "External Recorder not started.");
        return false;
    }

    QString result;
    if (!ProcessCommand("StopStreaming", result, 10s))
    {
        LogLevel_t level = LOG_ERR;
        if (result.startsWith("warn", Qt::CaseInsensitive))
            level = LOG_WARNING;
        else
            m_bError = true;

        LOG(VB_GENERAL, level, LOC + QString("StopStreaming: '%1'")
            .arg(result));

        return false;
    }

    PurgeBuffer();
    LOG(VB_RECORD, LOG_INFO, LOC + "Streaming stopped");

    return true;
}

bool ExternalStreamHandler::ProcessCommand(const QString & cmd,
                                           QString & result,
                                           std::chrono::milliseconds timeout,
                                           uint retry_cnt)
{
    QMutexLocker locker(&m_processLock);

    if (m_apiVersion == 3)
    {
        QVariantMap vcmd;
        QVariantMap vresult;
        QByteArray  response;
        QStringList tokens = cmd.split(':');
        vcmd["command"] = tokens[0];
        if (tokens.size() > 1)
            vcmd["value"] = tokens[1];

        LOG(VB_RECORD, LOG_DEBUG, LOC +
            QString("Arguments: %1").arg(tokens.join("\n")));

        bool r = ProcessJson(vcmd, vresult, response, timeout, retry_cnt);
        result = QString("%1:%2").arg(vresult["status"].toString(),
                                      vresult["message"].toString());
        return r;
    }
    if (m_apiVersion == 2)
        return ProcessVer2(cmd, result, timeout, retry_cnt);
    if (m_apiVersion == 1)
        return ProcessVer1(cmd, result, timeout, retry_cnt);

    LOG(VB_RECORD, LOG_ERR, LOC +
        QString("Invalid API version %1.  Expected 1 or 2").arg(m_apiVersion));
    return false;
}

bool ExternalStreamHandler::ProcessVer1(const QString & cmd,
                                        QString & result,
                                        std::chrono::milliseconds timeout,
                                        uint retry_cnt)
{
    LOG(VB_RECORD, LOG_DEBUG, LOC + QString("ProcessVer1('%1')")
        .arg(cmd));

    for (uint cnt = 0; cnt < retry_cnt; ++cnt)
    {
        QMutexLocker locker(&m_ioLock);

        if (!m_io)
        {
            LOG(VB_RECORD, LOG_ERR, LOC + "External I/O not ready!");
            return false;
        }

        QByteArray buf(cmd.toUtf8(), cmd.size());
        buf += '\n';

        if (m_io->Error())
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "External Recorder in bad state: " +
                m_io->ErrorString());
            return false;
        }

        /* Try to keep in sync, if External app was too slow in responding
         * to previous query, consume the response before sending new query */
        m_io->GetStatus(0ms);

        /* Send new query */
        m_io->Write(buf);

        MythTimer timer(MythTimer::kStartRunning);
        while (timer.elapsed() < timeout)
        {
            result = m_io->GetStatus(timeout);
            if (m_io->Error())
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    "Failed to read from External Recorder: " +
                    m_io->ErrorString());
                    m_bError = true;
                return false;
            }

            // Out-of-band error message
            if (result.startsWith("STATUS:ERR") ||
                result.startsWith("0:STATUS:ERR"))
            {
                LOG(VB_RECORD, LOG_ERR, LOC + result);
                result.remove(0, result.indexOf(":ERR") + 1);
                return false;
            }
            // STATUS message are "out of band".
            // Ignore them while waiting for a responds to a command
            if (!result.startsWith("STATUS") && !result.startsWith("0:STATUS"))
                break;
            LOG(VB_RECORD, LOG_INFO, LOC +
                QString("Ignoring response '%1'").arg(result));
        }

        if (result.size() < 1)
        {
            LOG(VB_GENERAL, LOG_WARNING, LOC +
                QString("External Recorder did not respond to '%1'").arg(cmd));
        }
        else
        {
            bool okay = result.startsWith("OK");
            if (okay || result.startsWith("WARN") || result.startsWith("ERR"))
            {
                LogLevel_t level = LOG_INFO;

                m_ioErrCnt = 0;
                if (!okay)
                    level = LOG_WARNING;
                else if (cmd.startsWith("SendBytes"))
                    level = LOG_DEBUG;

                LOG(VB_RECORD, level,
                    LOC + QString("ProcessCommand('%1') = '%2' took %3ms %4")
                    .arg(cmd, result,
                         QString::number(timer.elapsed().count()),
                         okay ? "" : "<-- NOTE"));

                return okay;
            }
            LOG(VB_GENERAL, LOG_WARNING, LOC +
                QString("External Recorder invalid response to '%1': '%2'")
                .arg(cmd, result));
        }

        if (++m_ioErrCnt > 10)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Too many I/O errors.");
            m_bError = true;
            break;
        }
    }

    return false;
}

bool ExternalStreamHandler::ProcessVer2(const QString & command,
                                        QString & result,
                                        std::chrono::milliseconds timeout,
                                        uint retry_cnt)
{
    QString status;
    QString raw;

    for (uint cnt = 0; cnt < retry_cnt; ++cnt)
    {
        QString cmd = QString("%1:%2").arg(++m_serialNo).arg(command);

        LOG(VB_RECORD, LOG_DEBUG, LOC + QString("ProcessVer2('%1') serial(%2)")
            .arg(cmd).arg(m_serialNo));

        QMutexLocker locker(&m_ioLock);

        if (!m_io)
        {
            LOG(VB_RECORD, LOG_ERR, LOC + "External I/O not ready!");
            return false;
        }

        QByteArray buf(cmd.toUtf8(), cmd.size());
        buf += '\n';

        if (m_io->Error())
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "External Recorder in bad state: " +
                m_io->ErrorString());
            return false;
        }

        /* Send query */
        m_io->Write(buf);

        QStringList tokens;

        MythTimer timer(MythTimer::kStartRunning);
        while (timer.elapsed() < timeout)
        {
            result = m_io->GetStatus(timeout);
            if (m_io->Error())
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    "Failed to read from External Recorder: " +
                    m_io->ErrorString());
                    m_bError = true;
                return false;
            }

            if (!result.isEmpty())
            {
                raw = result;
                tokens = result.split(':', Qt::SkipEmptyParts);

                // Look for result with the serial number of this query
                if (tokens.size() > 1 && tokens[0].toUInt() >= m_serialNo)
                    break;

                /* Other messages are "out of band" */

                // Check for error message missing serial#
                if (tokens[0].startsWith("ERR"))
                    break;

                // Remove serial#
                tokens.removeFirst();
                result = tokens.join(':');
                bool err = (tokens.size() > 1 && tokens[1].startsWith("ERR"));
                LOG(VB_RECORD, (err ? LOG_WARNING : LOG_INFO), LOC + raw);
                if (err)
                {
                    // Remove "STATUS"
                    tokens.removeFirst();
                    result = tokens.join(':');
                    return false;
                }
            }
        }

        if (timer.elapsed() >= timeout)
        {
            LOG(VB_RECORD, LOG_ERR, LOC +
                QString("ProcessVer2: Giving up waiting for response for "
                        "command '%2'").arg(cmd));
        }
        else if (tokens.size() < 2)
        {
            LOG(VB_RECORD, LOG_ERR, LOC +
                QString("Did not receive a valid response "
                        "for command '%1', received '%2'").arg(cmd, result));
        }
        else if (tokens[0].toUInt() > m_serialNo)
        {
            LOG(VB_RECORD, LOG_ERR, LOC +
                QString("ProcessVer2: Looking for serial no %1, "
                        "but received %2 for command '%2'")
                .arg(QString::number(m_serialNo), tokens[0], cmd));
        }
        else
        {
            tokens.removeFirst();
            status = tokens[0].trimmed();
            result = tokens.join(':');

            bool okay = (status == "OK");
            if (okay || status.startsWith("WARN") || status.startsWith("ERR"))
            {
                LogLevel_t level = LOG_INFO;

                m_ioErrCnt = 0;
                if (!okay)
                    level = LOG_WARNING;
                else if (command.startsWith("SendBytes") ||
                         (command.startsWith("TuneStatus") &&
                          result == "OK:InProgress"))
                    level = LOG_DEBUG;

                LOG(VB_RECORD, level,
                    LOC + QString("ProcessV2('%1') = '%2' took %3ms %4")
                    .arg(cmd, result, QString::number(timer.elapsed().count()),
                         okay ? "" : "<-- NOTE"));

                return okay;
            }
            LOG(VB_GENERAL, LOG_WARNING, LOC +
                QString("External Recorder invalid response to '%1': '%2'")
                .arg(cmd, result));
        }

        if (++m_ioErrCnt > 10)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Too many I/O errors.");
            m_bError = true;
            break;
        }
    }

    return false;
}

bool ExternalStreamHandler::ProcessJson(const QVariantMap & vmsg,
                                        QVariantMap & elements,
                                        QByteArray & response,
                                        std::chrono::milliseconds timeout,
                                        uint retry_cnt)
{
    for (uint cnt = 0; cnt < retry_cnt; ++cnt)
    {
        QVariantMap query(vmsg);

        uint serial = ++m_serialNo;
        query["serial"] = serial;
        QString cmd = query["command"].toString();

        QJsonDocument qdoc;
        qdoc = QJsonDocument::fromVariant(query);
        QByteArray cmdbuf = qdoc.toJson(QJsonDocument::Compact);

        LOG(VB_RECORD, LOG_DEBUG, LOC +
            QString("ProcessJson: %1").arg(QString(cmdbuf)));

        if (m_io->Error())
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "External Recorder in bad state: " +
                m_io->ErrorString());
            return false;
        }

        /* Send query */
        m_io->Write(cmdbuf);
        m_io->Write("\n");

        MythTimer timer(MythTimer::kStartRunning);
        while (timer.elapsed() < timeout)
        {
            response = m_io->GetStatus(timeout);
            if (m_io->Error())
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    "Failed to read from External Recorder: " +
                    m_io->ErrorString());
                    m_bError = true;
                return false;
            }

            if (!response.isEmpty())
            {
                QJsonParseError parseError {};
                QJsonDocument doc;

                doc = QJsonDocument::fromJson(response, &parseError);

                if (parseError.error != QJsonParseError::NoError)
                {
                    LOG(VB_GENERAL, LOG_ERR, LOC +
                        QString("ExternalRecorder returned invalid JSON message: %1: %2\n%3\nfor\n%4")
                        .arg(parseError.offset)
                        .arg(parseError.errorString(),
                             QString(response),
                             QString(cmdbuf)));
                }
                else
                {
                    elements = doc.toVariant().toMap();
                    if (!elements.contains("serial"))
                        continue;

                    serial = elements["serial"].toInt();
                    if (serial >= m_serialNo)
                        break;

                    if (elements.contains("status") &&
                        elements["status"] != "OK")
                    {
                        LOG(VB_RECORD, LOG_WARNING, LOC + QString("%1: %2")
                            .arg(elements["status"].toString(),
                                 elements["message"].toString()));
                    }
                }
            }
        }

        if (timer.elapsed() >= timeout)
        {
            LOG(VB_RECORD, LOG_ERR, LOC +
                QString("ProcessJson: Giving up waiting for response for "
                        "command '%2'").arg(QString(cmdbuf)));

        }

        if (serial > m_serialNo)
        {
            LOG(VB_RECORD, LOG_ERR, LOC +
                QString("ProcessJson: Looking for serial no %1, "
                        "but received %2 for command '%2'")
                .arg(QString::number(m_serialNo))
                .arg(serial)
                .arg(QString(cmdbuf)));
        }
        else if (!elements.contains("status"))
        {
            LOG(VB_RECORD, LOG_ERR, LOC +
                QString("ProcessJson: ExternalRecorder 'status' not found in %1")
                .arg(QString(response)));
        }
        else
        {
            QString status = elements["status"].toString();
            bool okay = (status == "OK");
            if (okay || status == "WARN" || status == "ERR")
            {
                LogLevel_t level = LOG_INFO;

                m_ioErrCnt = 0;
                if (!okay)
                    level = LOG_WARNING;
                else if (cmd == "SendBytes" ||
                         (cmd == "TuneStatus?" &&
                          elements["message"] == "InProgress"))
                    level = LOG_DEBUG;

                LOG(VB_RECORD, level,
                    LOC + QString("ProcessJson('%1') = %2:%3:%4 took %5ms %6")
                    .arg(QString(cmdbuf))
                    .arg(elements["serial"].toInt())
                    .arg(elements["status"].toString(),
                         elements["message"].toString(),
                         QString::number(timer.elapsed().count()),
                         okay ? "" : "<-- NOTE")
                    );

                return okay;
            }
            LOG(VB_GENERAL, LOG_WARNING, LOC +
                QString("External Recorder invalid response to '%1': '%2'")
                .arg(QString(cmdbuf),
                     QString(response)));
        }

        if (++m_ioErrCnt > 10)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Too many I/O errors.");
            m_bError = true;
            break;
        }
    }

    return false;
}

bool ExternalStreamHandler::CheckForError(void)
{
    QByteArray response;
    bool       err = false;

    QMutexLocker locker(&m_ioLock);

    if (!m_io)
    {
        LOG(VB_RECORD, LOG_ERR, LOC + "External I/O not ready!");
        return true;
    }

    if (m_io->Error())
    {
        LOG(VB_GENERAL, LOG_ERR, "External Recorder in bad state: " +
            m_io->ErrorString());
        return true;
    }

    response = m_io->GetStatus(0ms);
    while (!response.isEmpty())
    {
        if (m_apiVersion > 2)
        {
            QJsonParseError parseError {};
            QJsonDocument doc;
            QVariantMap   elements;

            doc = QJsonDocument::fromJson(response, &parseError);

            if (parseError.error != QJsonParseError::NoError)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    QString("ExternalRecorder returned invalid JSON message: %1: %2\n%3\n")
                    .arg(parseError.offset)
                    .arg(parseError.errorString(), QString(response)));
            }
            else
            {
                elements = doc.toVariant().toMap();
                if (elements.contains("command") &&
                    elements["command"] == "STATUS")
                {
                    LogLevel_t level { LOG_INFO };
                    QString status = elements["status"].toString();
                    if (status.startsWith("err", Qt::CaseInsensitive))
                    {
                        level = LOG_ERR;
                        err |= true;
                    }
                    else if (status.startsWith("warn",
                                               Qt::CaseInsensitive))
                    {
                        level = LOG_WARNING;
                    }
                    else if (status.startsWith("damage",
                                               Qt::CaseInsensitive))
                    {
                        level = LOG_WARNING;
                        m_damaged |= true;
                    }
                    LOG(VB_RECORD, level,
                        LOC + elements["message"].toString());
                }
            }
        }
        else
        {
            QString res = QString(response);
            if (m_apiVersion == 2)
            {
                QStringList tokens = res.split(':', Qt::SkipEmptyParts);
                tokens.removeFirst();
                res = tokens.join(':');
                for (int idx = 1; idx < tokens.size(); ++idx)
                {
                    err |= tokens[idx].startsWith("ERR",
                                                  Qt::CaseInsensitive);
                    m_damaged |= tokens[idx].startsWith("damage",
                                                       Qt::CaseInsensitive);
                }
            }
            else
            {
                err |= res.startsWith("STATUS:ERR",
                                      Qt::CaseInsensitive);
                m_damaged |= res.startsWith("STATUS:DAMAGE",
                                           Qt::CaseInsensitive);
            }

            LOG(VB_RECORD, (err ? LOG_WARNING : LOG_INFO), LOC + res);
        }

        response = m_io->GetStatus(0ms);
    }

    return err;
}

void ExternalStreamHandler::PurgeBuffer(void)
{
    if (m_io)
    {
        QByteArray buffer;
        m_io->Read(buffer, PACKET_SIZE, 1ms);
        m_io->GetStatus(1ms);
    }
}

void ExternalStreamHandler::PriorityEvent(int /*fd*/)
{
    // TODO report on buffer overruns, etc.
}
