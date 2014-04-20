
#include <sys/stat.h>
#include <fcntl.h>

#include <termios.h>
#include <iostream>
#include <sys/poll.h>

using namespace std;

#include <QDir>
#include <QApplication>
#include <QTime>
#include <QThread>

#include "commandlineparser.h"
#include "mythfilerecorder.h"

#include "exitcodes.h"
#include "mythcontext.h"
#include "mythversion.h"
#include "mythlogging.h"

#define VERSION "1.0.0"
#define LOC QString("FileRecorder(%1): ").arg(m_filename)

Streamer::Streamer(void) :
    m_fd(0), m_pkt_size(0), m_bufsize(188 * 1000 * 10),
    m_remainder(0), m_paused(true), m_run(true), m_poll_mode(true)
{
    m_data = new uint8_t[m_bufsize + 1];
}

Streamer::~Streamer(void)
{
    Close();
}


void Streamer::Open(const QString & filename, bool loopinput,
                    bool poll_mode)
{
    m_filename = filename;
    m_loop = loopinput;
    m_poll_mode = poll_mode;

    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Opening '%1'")
        .arg(m_filename));

    m_fd = ::open(m_filename.toLocal8Bit().constData(), O_RDONLY);

    if (m_fd > 1)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("'%1' successfully opened")
            .arg(m_filename));
    }
    else
    {
        m_error = strerror(errno);
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Failed to open '%1': '%2'")
            .arg(m_filename).arg(m_error));
    }

#if 0
    // Make stdin 'raw'
    struct termios ttystate;
    tcgetattr(STDIN_FILENO, &ttystate);
    ttystate.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP
                            | INLCR | IGNCR | ICRNL | IXON);
    ttystate.c_oflag &= ~OPOST;
    ttystate.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    ttystate.c_cflag &= ~(CSIZE | PARENB);
    ttystate.c_cflag |= CS8;
    tcsetattr(STDIN_FILENO, TCSANOW, &ttystate);
#endif
}

bool Streamer::IsOpen(void) const
{
    if (m_fd > 0)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("'%1' is open")
            .arg(m_filename));
        return true;
    }
    else
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("'%1' not open")
            .arg(m_filename));
        return false;
    }
}

bool Streamer::Close(void)
{
    ::close(m_fd);
    m_fd = 0;
    m_paused = true;
    return true;
}

void Streamer::Stop(void)
{
    QMutexLocker locker(&streamlock);
    m_run = false;
}

void Streamer::WakeUp(void)
{
    streamlock.lock();
    streamWait.wakeAll();
    streamlock.unlock();
}

static uint send_cnt = 0;

void Streamer::Send(int sz)
{
    QMutexLocker locker(&streamlock);

    m_pkt_size = sz;
}

void Streamer::StreamData(void)
{
    int size, len;

    LOG(VB_GENERAL, LOG_INFO, LOC + "StreamData -- start");

    while (m_run)
    {
        streamlock.lock();
        LOG(VB_GENERAL, LOG_DEBUG, LOC + "waiting to stream");
        streamWait.wait(&streamlock, 50000);

        while (m_run && m_fd > 0 && !m_paused && m_pkt_size)
        {
            streamlock.unlock();
            LOG(VB_GENERAL, LOG_DEBUG, LOC +
                QString("Myth is ready for %1 bytes").arg(m_pkt_size));
            size = (m_bufsize - m_remainder < m_pkt_size) ?
                    m_bufsize - m_remainder : m_pkt_size;
            len = read(m_fd, &m_data[m_remainder], size);
            LOG(VB_GENERAL, LOG_DEBUG, LOC +
                QString("Read %1, sending %2 bytes").arg(size).arg(len));

            if (len == 0)
            {
                LOG(VB_GENERAL, LOG_INFO, LOC + "process_data read EOF: "
                    + ENO);
                if (m_loop)
                    lseek(m_fd, 0, SEEK_SET);
                else
                    m_run = false;
            }

            if (len < 0)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC + "process_data read failed: "
                    + ENO);

                return;
            }

            LOG(VB_GENERAL, LOG_DEBUG, LOC +
                QString("process_data read %1 bytes").arg(len));

            if (m_paused)
                m_remainder = 0;
            else
            {
                len += m_remainder;
                LOG(VB_GENERAL, LOG_DEBUG, LOC +
                    QString("process_data writing %1 bytes").arg(len));
                streamlock.lock();
                int wrote = write(1, m_data, len);

                if (m_poll_mode)
                    m_pkt_size -= wrote;
                else
                    usleep(1000);

                streamlock.unlock();

                LOG(VB_GENERAL, LOG_DEBUG, LOC +
                    QString("process_data wrote %1 bytes").arg(wrote));

                m_remainder = len - wrote;
                if (m_remainder)
                {
                    if (wrote >= m_remainder)
                        memcpy(m_data, m_data + wrote, m_remainder);
                    else
                        memmove(m_data, m_data + wrote, m_remainder);
                }
            }
            streamlock.lock();
            if (!m_pkt_size)
                break;
            streamWait.wait(&streamlock, 50);
        }
        streamlock.unlock();
    }
    LOG(VB_GENERAL, LOG_INFO, LOC + "StreamData -- end");
}


Commands::Commands(void) : m_run(true), m_poll_mode(true)
{
    setObjectName("Command");
}

Commands::~Commands(void)
{
}

bool Commands::send_status(const QString & status) const
{
    QByteArray buf = status.toLatin1() + '\n';
    int len = write(2, buf.constData(), buf.size());
    if (len != buf.size())
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Wrote %1 of %2 bytes of status '%3'")
            .arg(len).arg(status.size()).arg(status));
        return false;
    }
    return true;
}

bool Commands::process_command(QString & cmd)
{
    if (cmd.startsWith("Version?"))
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + "Version");
        send_status(VERSION);
        return true;
    }
    if (cmd.startsWith("HasLock?"))
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + "HasLock");
        send_status("OK:Yes");
        return true;
    }
    if (cmd.startsWith("SignalStrengthPercent"))
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + "SignalStrengthPercent");
        send_status("OK:100");
        return true;
    }
    if (cmd.startsWith("LockTimeout"))
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + "LockTimeout");
        send_status("OK:5000");
        return true;
    }
    if (cmd.startsWith("HasTuner?"))
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + "HasTuner - No");
        send_status("OK:No");
        return true;
    }
    if (cmd.startsWith("HasPictureAttributes?"))
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + "HasPictureAttributes - No");
        send_status("OK:No");
        return true;
    }
    if (cmd.startsWith("FlowControl?"))
    {
        if (m_poll_mode)
        {
            LOG(VB_GENERAL, LOG_INFO, LOC + "FlowControl - POLL");
            send_status("OK:Poll");
        }
        else
        {
            LOG(VB_GENERAL, LOG_INFO, LOC + "FlowControl - XON/XOFF");
            send_status("OK:XonXoff");
        }
        return true;
    }

    if (!m_streamer)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("%1 failed - not initialized!")
            .arg(cmd));
        send_status("ERR:Not Initialized");
        return false;
    }

    if (cmd.startsWith("SendBytes:"))
    {
        LOG(VB_GENERAL, LOG_DEBUG, LOC + cmd);
        m_streamer->Send(cmd.mid(10).toInt());
        send_status("OK");
        m_streamer->WakeUp();
    }
    else if (cmd.startsWith("XON"))
    {
        LOG(VB_GENERAL, LOG_DEBUG, LOC + cmd);
        m_streamer->Pause(false);
        m_streamer->Send(188 * 1024);
        m_streamer->WakeUp();
        send_status("OK:XON");
    }
    else if (cmd.startsWith("XOFF"))
    {
        LOG(VB_GENERAL, LOG_DEBUG, LOC + cmd);
        m_streamer->Pause(true);
        m_streamer->WakeUp();
        send_status("OK:XOFF");
    }
    else if (cmd.startsWith("IsOpen?"))
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + cmd);
        if (m_streamer->IsOpen())
            send_status("OK:Open");
        else
            send_status(QString("ERR:Not Open: '%2'")
                        .arg(m_streamer->ErrorString()));
    }
    else if (cmd.startsWith("CloseRecorder"))
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + cmd);
        if (m_streamer->Close())
        {
            m_run = false;
            m_streamer->WakeUp();
            send_status("OK:Terminating");
        }
        else
            send_status("ERR:Close failed");
    }
    else if (cmd.startsWith("StartStreaming"))
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + "Streaming starting");
        m_streamer->Pause(false);
        m_streamer->WakeUp();
        send_status("OK:Started");
    }
    else if (cmd.startsWith("StopStreaming"))
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + "Streaming stopping");
        m_streamer->Pause(true);
        m_streamer->WakeUp();
        send_status("OK:Stopped");
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Unknown command '%1'")
            .arg(cmd));
        send_status(QString("ERR:Unknown command '%1'").arg(cmd));
    }

    return true;
}

bool Commands::Run(const QString & filename, bool loopinput)
{
    QString cmd;
    char    buf[128];

    int ret, len;
    int poll_cnt = 1;
    struct pollfd polls[2];
    memset(polls, 0, sizeof(polls));

    polls[0].fd      = 0;
    polls[0].events  = POLLIN | POLLPRI;
    polls[0].revents = 0;
    m_timeout = 10;

    m_filename = filename;

    QThread *streamThread = new QThread(this);
    streamThread->setObjectName("Streamer");
    m_streamer = new Streamer;

    connect(streamThread, SIGNAL(started()), m_streamer, SLOT(StreamData()));
    connect(streamThread, SIGNAL(finished()), m_streamer, SLOT(deleteLater()));
    m_streamer->moveToThread(streamThread);

    m_streamer->Open(filename, loopinput, m_poll_mode);

    // Starts an event loop, and emits streamThread->started()
    streamThread->start();

    LOG(VB_GENERAL, LOG_INFO, LOC + "Listening for commands");

    while (m_run)
    {
        ret = poll(polls, poll_cnt, m_timeout);

        if (polls[0].revents & POLLHUP)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "poll eof (POLLHUP)");
            break;
        }
        else if (polls[0].revents & POLLNVAL)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "poll error");
            return false;
        }

        if (polls[0].revents & POLLIN)
        {
            if (ret > 0)
            {
                len = read(0, &buf, 128 - 1);
                buf[len] = 0;
                if (len > 0 && buf[len - 1] == '\n')
                    buf[len - 1] = 0;
                cmd = QString(buf);
                process_command(cmd);
            }
            else if (ret < 0)
            {
                if ((EOVERFLOW == errno))
                {
                    LOG(VB_GENERAL, LOG_ERR, LOC + "command overflow.");
                    break; // we have an error to handle
                }

                if ((EAGAIN == errno) || (EINTR  == errno))
                {
                    LOG(VB_GENERAL, LOG_ERR, LOC + "retry command read.");
                    continue; // errors that tell you to try again
                }

                LOG(VB_GENERAL, LOG_ERR, LOC + "unknown error reading command.");
            }
        }
    }

    return true;
}


#include <fstream>

int main(int argc, char *argv[])
{
    {
        std::ofstream dbg("/tmp/mfr.out", ios::app);
        dbg << argv[0] << " " << argv[1] << "\n";
    }

    MythFileRecorderCommandLineParser cmdline;

    if (!cmdline.Parse(argc, argv))
    {
        cmdline.PrintHelp();
        return GENERIC_EXIT_INVALID_CMDLINE;
    }

    if (cmdline.toBool("showhelp"))
    {
        cmdline.PrintHelp();
        return GENERIC_EXIT_OK;
    }

    if (cmdline.toBool("showversion"))
    {
        cmdline.PrintVersion();
        return GENERIC_EXIT_OK;
    }

    bool loopinput = !cmdline.toBool("noloop");

    QApplication a(argc, argv);
    QCoreApplication::setApplicationName("mythfilerecorder");

    int retval;
    if ((retval = cmdline.ConfigureLogging()) != GENERIC_EXIT_OK)
        return retval;

    QString filename = "";
    if (!cmdline.toString("infile").isEmpty())
        filename = cmdline.toString("infile");
    else if (cmdline.GetArgs().size() >= 1)
        filename = cmdline.GetArgs()[0];

    Commands recorder;
    recorder.Run(filename, loopinput);

    return GENERIC_EXIT_OK;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
