// -*- Mode: c++ -*-

// POSIX headers
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#if !defined( USING_MINGW ) && !defined( _MSC_VER )
#include <poll.h>
#include <sys/ioctl.h>
#endif
#ifdef ANDROID
#include <sys/wait.h>
#endif

// Qt headers
#include <QString>
#include <QFile>

// MythTV headers
#include "ExternalStreamHandler.h"
#include "ExternalChannel.h"
//#include "ThreadedFileWriter.h"
#include "dtvsignalmonitor.h"
#include "streamlisteners.h"
#include "mpegstreamdata.h"
#include "cardutil.h"
#include "exitcodes.h"

#define LOC QString("ExternalRec%1(%2): ").arg(_recorder_ids_string) \
                                          .arg(_device)

ExternIO::ExternIO(const QString & app,
                   const QStringList & args)
    : m_appin(-1), m_appout(-1), m_apperr(-1),
      m_pid(-1), m_bufsize(0), m_buffer(NULL),
      m_status(&m_status_buf, QIODevice::ReadWrite),
      m_errcnt(0)

{
    m_app  = (app);

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
    if (!m_args.contains("-q"))
        m_args << "-q";
    m_args.prepend(m_app.baseName());

    m_status.setString(&m_status_buf);
}

ExternIO::~ExternIO(void)
{
    close(m_appin);
    close(m_appout);
    close(m_apperr);

    // waitpid(m_pid, &status, 0);
    delete[] m_buffer;
}

bool ExternIO::Ready(int fd, int timeout, const QString & what)
{
#if !defined( USING_MINGW ) && !defined( _MSC_VER )
    struct pollfd m_poll[2];
    memset(m_poll, 0, sizeof(m_poll));

    m_poll[0].fd = fd;
    m_poll[0].events = POLLIN | POLLPRI;
    int ret = poll(m_poll, 1, timeout);

    if (m_poll[0].revents & POLLHUP)
    {
        m_error = what + " poll eof (POLLHUP)";
        return false;
    }
    else if (m_poll[0].revents & POLLNVAL)
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
#endif // !defined( USING_MINGW ) && !defined( _MSC_VER )
    return false;
}

int ExternIO::Read(QByteArray & buffer, int maxlen, int timeout)
{
    if (Error())
    {
        LOG(VB_RECORD, LOG_ERR,
            QString("ExternIO::Read: already in error state: '%1'")
            .arg(m_error));
        return 0;
    }

    if (!Ready(m_appout, timeout, "data"))
        return 0;

    if (m_bufsize < maxlen)
    {
        m_bufsize = maxlen;
        delete m_buffer;
        m_buffer = new char[m_bufsize];
    }

    int len = read(m_appout, m_buffer, maxlen);

    if (len < 0)
    {
        if (errno == EAGAIN)
        {
            if (++m_errcnt > kMaxErrorCnt)
            {
                m_error = "Failed to read from External Recorder: " + ENO;
                LOG(VB_RECORD, LOG_WARNING,
                    "External Recorder not ready. Giving up.");
            }
            else
                LOG(VB_RECORD, LOG_WARNING,
                    QString("External Recorder not ready. Will retry (%1/%2).")
                    .arg(m_errcnt).arg(kMaxErrorCnt));
        }
        else
        {
            m_error = "Failed to read from External Recorder: " + ENO;
            LOG(VB_RECORD, LOG_ERR, m_error);
        }
    }
    else
        m_errcnt = 0;

    if (len == 0)
        return 0;

    buffer.append(m_buffer, len);

    LOG(VB_RECORD, LOG_DEBUG,
        QString("ExternIO::Read '%1' bytes, buffer size %2")
        .arg(len).arg(buffer.size()));

    return len;
}

QString ExternIO::GetStatus(int timeout)
{
    if (Error())
    {
        LOG(VB_RECORD, LOG_ERR,
            QString("ExternIO::GetStatus: already in error state: '%1'")
            .arg(m_error));
        return QByteArray();
    }

    int waitfor = m_status.atEnd() ? timeout : 0;
    if (Ready(m_apperr, waitfor, "status"))
    {
        char buffer[2048];
        int len = read(m_apperr, buffer, 2048);
        m_status << QString::fromLatin1(buffer, len);
    }

    if (m_status.atEnd())
        return QByteArray();

    QString msg = m_status.readLine();

    LOG(VB_RECORD, LOG_DEBUG, QString("ExternIO::GetStatus '%1'")
        .arg(msg));

    return msg;
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
        .arg(QString(buffer)));

    int len = write(m_appin, buffer.constData(), buffer.size());
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

    return true;
}

/* Return true if the process is not, or is no longer running */
bool ExternIO::KillIfRunning(const QString & cmd)
{
#if CONFIG_DARWIN || (__FreeBSD__) || defined(__OpenBSD__)
    Q_UNUSED(cmd);
    return false;
#elif defined USING_MINGW
    Q_UNUSED(cmd);
    return false;
#elif defined( _MSC_VER )
    Q_UNUSED(cmd);
    return false;
#else
    QString grp = QString("pgrep -x -f \"%1\" 2>&1 > /dev/null").arg(cmd);
    QString kil = QString("pkill --signal 15 -x -f \"%1\" 2>&1 > /dev/null")
                  .arg(cmd);
    int res_grp, res_kil;

    res_grp = system(grp.toUtf8().constData());
    if (WEXITSTATUS(res_grp) == 1)
    {
        LOG(VB_RECORD, LOG_DEBUG, QString("'%1' not running.").arg(cmd));
        return true;
    }

    LOG(VB_RECORD, LOG_WARNING, QString("'%1' already running, killing...")
        .arg(cmd));
    res_kil = system(kil.toUtf8().constData());
    if (WEXITSTATUS(res_kil) == 1)
        LOG(VB_GENERAL, LOG_WARNING, QString("'%1' failed: %2")
            .arg(kil).arg(ENO));

    res_grp = system(grp.toUtf8().constData());
    if (WEXITSTATUS(res_grp) == 1)
    {
        LOG(WEXITSTATUS(res_kil) == 0 ? VB_RECORD : VB_GENERAL, LOG_WARNING,
            QString("'%1' terminated.").arg(cmd));
        return true;
    }

    usleep(50000);

    kil = QString("pkill --signal 9 -x -f \"%1\" 2>&1 > /dev/null").arg(cmd);
    res_kil = system(kil.toUtf8().constData());
    if (WEXITSTATUS(res_kil) > 0)
        LOG(VB_GENERAL, LOG_WARNING, QString("'%1' failed: %2")
            .arg(kil).arg(ENO));

    res_grp = system(grp.toUtf8().constData());
    LOG(WEXITSTATUS(res_kil) == 0 ? VB_RECORD : VB_GENERAL, LOG_WARNING,
        QString("'%1' %2.").arg(cmd)
        .arg(WEXITSTATUS(res_grp) == 0 ? "sill running" : "terminated"));

    return (WEXITSTATUS(res_grp) != 0);
#endif
}

void ExternIO::Fork(void)
{
#if !defined( USING_MINGW ) && !defined( _MSC_VER )
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
        usleep(50000);
        if (!KillIfRunning(full_command))
        {
            m_error = QString("Unable to kill existing '%1'.")
                      .arg(full_command);
            LOG(VB_GENERAL, LOG_ERR, m_error);
            return;
        }
    }


    LOG(VB_RECORD, LOG_INFO, QString("ExternIO::Fork '%1'").arg(full_command));

    int in[2]  = {-1, -1};
    int out[2] = {-1, -1};
    int err[2] = {-1, -1};

    if (pipe(in) < 0)
    {
        m_error = "pipe(in) failed: " + ENO;
        return;
    }
    if (pipe(out) < 0)
    {
        m_error = "pipe(out) failed: " + ENO;
        close(in[0]);
        close(in[1]);
        return;
    }
    if (pipe(err) < 0)
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
        m_appin  = in[1];
        m_appout = out[0];
        m_apperr = err[0];

        bool error = false;
        error = (fcntl(m_appin,  F_SETFL, O_NONBLOCK) == -1);
        error |= (fcntl(m_appout, F_SETFL, O_NONBLOCK) == -1);
        error |= (fcntl(m_apperr, F_SETFL, O_NONBLOCK) == -1);

        if (error)
        {
            LOG(VB_GENERAL, LOG_WARNING,
                "ExternIO::Fork(): Failed to set O_NONBLOCK for FD: " + ENO);
            sleep(2);
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
    for (int i = sysconf(_SC_OPEN_MAX) - 1; i > 2; --i)
        close(i);

    /* Set the process group id to be the same as the pid of this
     * child process.  This ensures that any subprocesses launched by this
     * process can be killed along with the process itself. */
    if (setpgid(0,0) < 0)
    {
        std::cerr << "ExternIO: "
             << "setpgid() failed: "
             << strerror(errno) << endl;
    }

    /* run command */
    char *command = strdup(m_app.canonicalFilePath()
                                 .toUtf8().constData());
    char **arguments;
    int    len;

    // Copy QStringList to char**
    arguments = new char*[m_args.size() + 1];
    for (int i = 0; i < m_args.size(); ++i)
    {
        len = m_args[i].size() + 1;
        arguments[i] = new char[len];
        memcpy(arguments[i], m_args[i].toStdString().c_str(), len);
    }
    arguments[m_args.size()] = reinterpret_cast<char *>(0);

    if (execv(command, arguments) < 0)
    {
        // Can't use LOG due to locking fun.
        std::cerr << "ExternIO: "
             << "execv() failed: "
             << strerror(errno) << endl;
    }
    else
    {
        std::cerr << "ExternIO: "
                  << "execv() should not be here?: "
                  << strerror(errno) << endl;
    }

#endif // !defined( USING_MINGW ) && !defined( _MSC_VER )

    /* Failed to exec */
    _exit(GENERIC_EXIT_DAEMONIZING_ERROR); // this exit is ok
}


QMap<QString,ExternalStreamHandler*> ExternalStreamHandler::m_handlers;
QMap<QString,uint>              ExternalStreamHandler::m_handlers_refcnt;
QMutex                          ExternalStreamHandler::m_handlers_lock;

ExternalStreamHandler *ExternalStreamHandler::Get(const QString &devname,
                                                  int recorder_id)
{
    QMutexLocker locker(&m_handlers_lock);

    QString devkey = devname;

    QMap<QString,ExternalStreamHandler*>::iterator it = m_handlers.find(devkey);

    if (it == m_handlers.end())
    {
        ExternalStreamHandler *newhandler = new ExternalStreamHandler(devname);
        m_handlers[devkey] = newhandler;
        m_handlers_refcnt[devkey] = 1;

        LOG(VB_RECORD, LOG_INFO,
            QString("ExternSH: Creating new stream handler %1 for %2")
                .arg(devkey).arg(devname));
    }
    else
    {
        m_handlers_refcnt[devkey]++;
        uint rcount = m_handlers_refcnt[devkey];
        LOG(VB_RECORD, LOG_INFO,
            QString("ExternSH: Using existing stream handler for %1")
                .arg(devkey) + QString(" (%1 in use)").arg(rcount));
    }

    m_handlers[devkey]->AddRecorderId(recorder_id);
    return m_handlers[devkey];
}

void ExternalStreamHandler::Return(ExternalStreamHandler * & ref,
                                   int recorder_id)
{
    QMutexLocker locker(&m_handlers_lock);

    QString devname = ref->_device;

    QMap<QString,uint>::iterator rit = m_handlers_refcnt.find(devname);
    if (rit == m_handlers_refcnt.end())
        return;

    QMap<QString, ExternalStreamHandler*>::iterator it =
        m_handlers.find(devname);
    if (it != m_handlers.end())
        (*it)->DelRecorderId(recorder_id);

    LOG(VB_RECORD, LOG_INFO, QString("ExternSH: Return '%1' in use %2")
        .arg(devname).arg(*rit));

    if (*rit > 1)
    {
        ref = NULL;
        --(*rit);
        return;
    }

    if ((it != m_handlers.end()) && (*it == ref))
    {
        LOG(VB_RECORD, LOG_INFO, QString("ExternSH: Closing handler for %1")
                           .arg(devname));
        delete *it;
        m_handlers.erase(it);
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("ExternSH: Error: Couldn't find handler for %1")
                .arg(devname));
    }

    m_handlers_refcnt.erase(rit);
    ref = NULL;
}

/*
  ExternalStreamHandler
 */

ExternalStreamHandler::ExternalStreamHandler(const QString & path) :
    StreamHandler(path), m_IO(0), m_tsopen(false), m_io_errcnt(0),
    m_poll_mode(false), m_notify(false), m_replay(true)
{
    setObjectName("ExternSH");

    m_args = path.split(' ',QString::SkipEmptyParts) +
             logPropagateArgs.split(' ', QString::SkipEmptyParts);
    m_app = m_args.first();
    m_args.removeFirst();
    LOG(VB_RECORD, LOG_INFO, LOC + QString("args \"%1\"")
        .arg(m_args.join(" ")));

    if (!OpenApp())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Failed to start %1").arg(_device));
    }
}

int ExternalStreamHandler::StreamingCount(void) const
{
    return m_streaming_cnt.loadAcquire();
}

void ExternalStreamHandler::run(void)
{
    QString    cmd;
    QString    result;
    QString    ready_cmd;
    bool       xon = false;
    QByteArray buffer;
    uint       len, read_len;
    uint       empty_cnt = 0;
    MythTimer timer;

    timer.start();

    RunProlog();

    LOG(VB_RECORD, LOG_INFO, LOC + "run(): begin");

    SetRunning(true, true, false);
    m_notify = false;

    if (m_poll_mode)
        ready_cmd = "SendBytes";
    else
        ready_cmd = "XON";

    uint remainder = 0;
    while (_running_desired && !_error)
    {
        if (!IsTSOpen())
        {
            LOG(VB_RECORD, LOG_WARNING, LOC + "TS not open yet.");
            usleep(1000);
            continue;
        }

        if (StreamingCount() == 0)
        {
            usleep(1000);
            continue;
        }

        UpdateFiltersFromStreamData();

        if (!xon || m_poll_mode)
        {
            ProcessCommand(ready_cmd, 1000, result);
            if (result.startsWith("ERR"))
            {
                LOG(VB_GENERAL, LOG_ERR, LOC + QString("Aborting: %1 -> %2")
                    .arg(ready_cmd).arg(result));
                _error = true;
            }
            else
                xon = true;
        }

        if (buffer.isEmpty())
        {
            if (++empty_cnt % 1000 == 0)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC + "No data from external app");
                m_notify = true;
            }
        }
        else
        {
            empty_cnt = 0;
            m_notify = false;
        }

        while (read_len = 0, buffer.size() == PACKET_SIZE ||
               (read_len = m_IO->Read(buffer, PACKET_SIZE - remainder, 100)) > 0)
        {
            if (m_IO->Error())
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    QString("Failed to read from External Recorder: %1")
                    .arg(m_IO->ErrorString()));
                _error = true;
                break;
            }

            if (!_running_desired || _error)
                break;

            if (read_len > 0)
                empty_cnt = 0;

            if (xon)
            {
                if (timer.elapsed() >= 2000)
                {
                    // Since we may never need to send the XOFF
                    // command, occationally check to see if the
                    // External recorder needs to report an issue.
                    if (CheckForError())
                    {
                        buffer.clear();
                        RestartStream();
                        xon = false;
                        break;
                    }
                    timer.restart();
                }

                if (buffer.size() > TOO_FAST_SIZE)
                {
                    if (!m_poll_mode)
                    {
                        // Data is comming a little too fast, so XOFF
                        // to give us time to process it.
                        ProcessCommand(QString("XOFF"), 50, result);
                        if (result.startsWith("ERR"))
                        {
                            LOG(VB_GENERAL, LOG_ERR, LOC +
                                QString("Aborting: XOFF -> %2")
                                .arg(result));
                            _error = true;
                        }
                    }
                    xon = false;
                }
            }

            len = remainder = buffer.size();

            if (!m_stream_lock.tryLock())
                continue;

            if (!_listener_lock.tryLock())
                continue;

            StreamDataList::const_iterator sit = _stream_data_list.begin();
            for (; sit != _stream_data_list.end(); ++sit)
                remainder = sit.key()->ProcessData
                            (reinterpret_cast<const uint8_t *>
                             (buffer.constData()), buffer.size());

            _listener_lock.unlock();

            if (m_replay)
            {
                m_replay_buffer += buffer.left(len - remainder);
                if (m_replay_buffer.size() > (50 * PACKET_SIZE))
                {
                    m_replay_buffer.remove(0, len - remainder);
                    LOG(VB_RECORD, LOG_WARNING, LOC +
                        QString("Replay size truncated to %1 bytes")
                        .arg(m_replay_buffer.size()));
                }
            }

            m_stream_lock.unlock();

            if (remainder == 0)
                buffer.clear();
            else if (len > remainder) // leftover bytes
                buffer.remove(0, len - remainder);
        }

        if (m_IO->Error())
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("Error from External Recorder: %1")
                .arg(m_IO->ErrorString()));
            CloseApp();
            _error = true;
            break;
        }
    }
    LOG(VB_RECORD, LOG_INFO, LOC + "run(): " +
        QString("%1 shutdown").arg(_error ? "Error" : "Normal"));

    RemoveAllPIDFilters();
    SetRunning(false, true, false);

    LOG(VB_RECORD, LOG_INFO, LOC + "run(): " + "end");

    RunEpilog();
}

bool ExternalStreamHandler::OpenApp(void)
{
    {
        QMutexLocker locker(&m_IO_lock);

        if (m_IO)
        {
            LOG(VB_RECORD, LOG_WARNING, LOC + "OpenApp: already open!");
            return true;
        }

        m_IO = new ExternIO(m_app, m_args);

        if (m_IO == NULL)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "ExternIO failed: " + ENO);
            _error = true;
        }
        else
        {
            LOG(VB_RECORD, LOG_INFO, LOC + QString("Spawn '%1'").arg(_device));
            m_IO->Run();
            if (m_IO->Error())
            {
                LOG(VB_GENERAL, LOG_ERR,
                    "Failed to start External Recorder: " + m_IO->ErrorString());
                delete m_IO;
                m_IO = NULL;
                _error = true;
                return false;
            }
        }
    }

    QString result;

    if (!IsAppOpen())
    {
        LOG(VB_RECORD, LOG_ERR, LOC + "Application is not responding.");
        _error = true;
        return false;
    }

    // Gather capabilities
    if (!ProcessCommand("HasTuner?", 2500, result))
    {
        LOG(VB_RECORD, LOG_ERR, LOC +
            QString("Bad response to 'HasTuner?' - '%1'").arg(result));
        _error = true;
        return false;
    }
    m_hasTuner = result.startsWith("OK:Yes");
    if (!ProcessCommand("HasPictureAttributes?", 2500, result))
    {
        LOG(VB_RECORD, LOG_ERR, LOC +
            QString("Bad response to 'HasPictureAttributes?' - '%1'")
            .arg(result));
        _error = true;
        return false;
    }
    m_hasPictureAttributes = result.startsWith("OK:Yes");

    /* Operate in "poll" or "xon/xoff" mode */
    m_poll_mode = ProcessCommand("FlowControl?", 2500, result) &&
                  result.startsWith("OK:Poll");

    LOG(VB_RECORD, LOG_INFO, LOC + "App opened successfully");
    LOG(VB_RECORD, LOG_INFO, LOC +
        QString("Capabilities: tuner(%1) "
                "Picture attributes(%2) "
                "Flow control(%3)")
        .arg(m_hasTuner ? "yes" : "no")
        .arg(m_hasPictureAttributes ? "yes" : "no")
        .arg(m_poll_mode ? "Polling" : "XON/XOFF")
        );

    /* Let the external app know how many bytes will read without blocking */
    ProcessCommand(QString("BlockSize:%1").arg(PACKET_SIZE), 2500, result);

    return true;
}

bool ExternalStreamHandler::IsAppOpen(void)
{
    if (m_IO == NULL)
    {
        LOG(VB_RECORD, LOG_WARNING, LOC +
            "WARNING: Unable to communicate with external app.");
        return false;
    }

    QString result;
    return ProcessCommand("Version?", 5000, result);
}

bool ExternalStreamHandler::IsTSOpen(void)
{
    if (m_tsopen)
        return true;

    QString result;

    if (!ProcessCommand("IsOpen?", 2500, result))
        return false;

    if (result.startsWith("OK:Open"))
        m_tsopen = true;
    return m_tsopen;
}

void ExternalStreamHandler::CloseApp(void)
{
    m_IO_lock.lock();
    if (m_IO)
    {
        QString result;

        LOG(VB_RECORD, LOG_INFO, LOC + "CloseRecorder");
        m_IO_lock.unlock();
        ProcessCommand("CloseRecorder", 30000, result);
        m_IO_lock.lock();

        if (!result.startsWith("OK:Terminating"))
        {
            LOG(VB_RECORD, LOG_INFO, LOC +
                "CloseRecorder failed, sending kill.");

            QString full_command = QString("%1").arg(m_args.join(" "));

            if (!m_IO->KillIfRunning(full_command))
            {
                // Give it one more chance.
                usleep(50000);
                if (!m_IO->KillIfRunning(full_command))
                {
                    LOG(VB_GENERAL, LOG_ERR,
                        QString("Unable to kill existing '%1'.")
                        .arg(full_command));
                    return;
                }
            }
        }
        delete m_IO;
        m_IO = 0;
    }
    m_IO_lock.unlock();
}

bool ExternalStreamHandler::RestartStream(void)
{
    bool streaming = (StreamingCount() > 0);

    LOG(VB_RECORD, LOG_INFO, LOC + "Restarting stream.");

    if (streaming)
    {
        if (!StopStreaming())
            return false;
    }

    usleep(1000000);  // 1 second

    if (streaming)
    {
        return StartStreaming();
    }

    return true;
}

void ExternalStreamHandler::ReplayStream(void)
{
    if (m_replay)
    {
        QString    result;

        // Let the external app know that we could be busy for a little while
        if (!m_poll_mode)
            ProcessCommand(QString("XOFF"), 50, result);

        /* If the input is not a 'broadcast' it may only have one
         * copy of the SPS right at the beginning of the stream,
         * so make sure we don't miss it!
         */
        QMutexLocker listen_lock(&_listener_lock);

        if (!_stream_data_list.empty())
        {
            StreamDataList::const_iterator sit = _stream_data_list.begin();
            for (; sit != _stream_data_list.end(); ++sit)
                sit.key()->ProcessData(reinterpret_cast<const uint8_t *>
                                       (m_replay_buffer.constData()),
                                       m_replay_buffer.size());
        }
        LOG(VB_RECORD, LOG_INFO, LOC + QString("Replayed %1 bytes")
            .arg(m_replay_buffer.size()));
        m_replay_buffer.clear();
        m_replay = false;

        // Let the external app know that we are ready
        if (!m_poll_mode)
            ProcessCommand(QString("XON"), 50, result);
    }
}

bool ExternalStreamHandler::StartStreaming(void)
{
    QString result;

    QMutexLocker locker(&m_stream_lock);

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
        if (!ProcessCommand("StartStreaming", 15000, result))
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + QString("StartStreaming failed: '%1'")
                .arg(result));
            if (!result.startsWith("Warn"))
            {
                LOG(VB_RECORD, LOG_WARNING, LOC +
                    QString("StartStreaming failed: '%1'").arg(result));
                _error = true;
            }
            return false;
        }

        if (result != "OK:Started")
        {
            LOG(VB_GENERAL, LOG_WARNING, LOC +
                QString("StartStreaming failed: '%1'").arg(result));
            return false;
        }

        LOG(VB_RECORD, LOG_INFO, LOC + "Streaming started");
    }
    else
        LOG(VB_RECORD, LOG_INFO, LOC + "Already streaming");

    m_streaming_cnt.ref();

    LOG(VB_RECORD, LOG_INFO, LOC +
        QString("StartStreaming %1 listeners")
        .arg(StreamingCount()));

    return true;
}

bool ExternalStreamHandler::StopStreaming(void)
{
    QString result;

    QMutexLocker locker(&m_stream_lock);

    LOG(VB_RECORD, LOG_INFO, LOC +
        QString("StopStreaming %1 listeners")
        .arg(StreamingCount()));

    if (StreamingCount() == 0)
    {
        LOG(VB_RECORD, LOG_INFO, LOC +
            "StopStreaming requested, but we are not streaming!");
        return true;
    }

    if (m_streaming_cnt.deref())
    {
        LOG(VB_RECORD, LOG_INFO, LOC +
            QString("StopStreaming delayed, still have %1 listeners")
            .arg(StreamingCount()));
        return true;
    }

    LOG(VB_RECORD, LOG_INFO, LOC + "StopStreaming");

    if (!IsAppOpen())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "External Recorder not started.");
        return false;
    }

    if (!ProcessCommand("StopStreaming", 15000, result))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("StopStreaming: '%1'")
            .arg(result));
        if (!result.startsWith("Warn"))
        {
            LOG(VB_RECORD, LOG_ERR, LOC +
                QString("StopStreaming failed: '%1'").arg(result));
            _error = true;
        }
        return false;
    }

    if (!result.startsWith("OK:Stopped"))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            QString("Unexpected response to StopStreaming: '%1'")
            .arg(result));
        return false;
    }

    PurgeBuffer();
    LOG(VB_RECORD, LOG_INFO, LOC + "Streaming stopped");

    return true;
}

bool ExternalStreamHandler::ProcessCommand(const QString & cmd, uint timeout,
                                           QString & result)
{
    bool okay;

    LOG(VB_RECORD, LOG_DEBUG, LOC + QString("ProcessCommand('%1')")
        .arg(cmd));

    for (int idx = 0; idx < 10; ++idx)
    {
        QMutexLocker locker(&m_IO_lock);

        if (!m_IO)
        {
            LOG(VB_RECORD, LOG_ERR, LOC + "External I/O not ready!");
            return false;
        }

        QByteArray buf(cmd.toUtf8(), cmd.size());
        buf += '\n';

        if (m_IO->Error())
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "External Recorder in bad state: " +
                m_IO->ErrorString());
            return false;
        }

        /* Try to keep in sync, if External app was too slow in responding
         * to previous query, consume the response before sending new query */
        m_IO->GetStatus(0);

        /* Send new query */
        m_IO->Write(buf);

        for (int cnt = 0; cnt < 5; ++cnt)
        {
            result = m_IO->GetStatus(timeout);
            if (m_IO->Error())
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    "Failed to read from External Recorder: " +
                    m_IO->ErrorString());
                    _error = true;
                return false;
            }
            // STATUS message are "out of band".
            // Ignore them while waiting for a responds to a command
            if (!result.startsWith("STATUS"))
                break;
        }

        if (result.size() < 1)
        {
            LOG(VB_GENERAL, LOG_WARNING, LOC +
                QString("External Recorder did not respond to '%1'").arg(cmd));
        }
        else
        {
            okay = result.startsWith("OK");
            if (okay || result.startsWith("WARN") || result.startsWith("ERR"))
            {
                m_io_errcnt = 0;

                m_notify |= (!okay || !cmd.startsWith("SendBytes"));
                LOG(VB_RECORD, m_notify ? LOG_INFO : LOG_DEBUG,
                    LOC + QString("ProcessCommand('%1') = '%2'")
                    .arg(cmd).arg(result));
                m_notify = false;

                return okay;
            }
            else
                LOG(VB_GENERAL, LOG_WARNING, LOC +
                    QString("External Recorder invalid response to '%1': '%2'")
                    .arg(cmd).arg(result));
        }

        if (++m_io_errcnt > 10)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Too many I/O errors.");
            _error = true;
            break;
        }
        usleep(timeout);
    }

    return false;
}

bool ExternalStreamHandler::CheckForError(void)
{
    QString result;
    bool    err = false;

    QMutexLocker locker(&m_IO_lock);

    if (!m_IO)
    {
        LOG(VB_RECORD, LOG_ERR, LOC + "External I/O not ready!");
        return true;
    }

    if (m_IO->Error())
    {
        LOG(VB_GENERAL, LOG_ERR, "External Recorder in bad state: " +
            m_IO->ErrorString());
        return true;
    }

    do
    {
        result = m_IO->GetStatus(0);
        if (!result.isEmpty())
        {
            err |= result.startsWith("STATUS:ERR");
            LOG(VB_RECORD, (err ? LOG_WARNING : LOG_INFO), LOC + result);
        }
    }
    while (!result.isEmpty());

    return err;
}

void ExternalStreamHandler::PurgeBuffer(void)
{
    if (m_IO)
    {
        QByteArray buffer;
        m_IO->Read(buffer, PACKET_SIZE, 1);
        m_IO->GetStatus(1);
    }
}

void ExternalStreamHandler::PriorityEvent(int /*fd*/)
{
    // TODO report on buffer overruns, etc.
}
