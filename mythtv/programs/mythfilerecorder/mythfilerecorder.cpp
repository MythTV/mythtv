
// C/C++
#include <algorithm>
#include <array>
#include <fcntl.h>
#include <iostream>
#include <sys/poll.h>
#include <sys/stat.h>
#include <termios.h>

// Qt
#include <QCoreApplication>
#include <QDir>
#include <QThread>
#include <QTime>

// MythTV
#include "libmyth/mythcontext.h"
#include "libmythbase/exitcodes.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythversion.h"

// MythFileRecorder
#include "mythfilerecorder.h"
#include "mythfilerecorder_commandlineparser.h"

static constexpr int API_VERSION { 1 };
static constexpr const char* VERSION { "1.0.0" };
#define LOC QString("File(%1): ").arg(m_fileName)

Streamer::Streamer(Commands *parent, QString fname,
                   int data_rate, bool loopinput) :
    m_parent(parent), m_fileName(std::move(fname)),
    m_loop(loopinput), m_dataRate(data_rate)
{
    setObjectName("Streamer");
    OpenFile();
    LOG(VB_RECORD, LOG_INFO, LOC + QString("Data Rate: %1").arg(m_dataRate));
}

Streamer::~Streamer(void)
{
    LOG(VB_RECORD, LOG_INFO, LOC + "Streamer::dtor -- begin");
    CloseFile();
    LOG(VB_RECORD, LOG_INFO, LOC + "Streamer::dtor -- end");
}

void Streamer::OpenFile(void)
{
    m_file = new QFile(m_fileName);
    if (!m_file || !m_file->open(QIODevice::ReadOnly))
    {
        LOG(VB_RECORD, LOG_ERR, LOC + QString("Failed to open '%1' - ")
            .arg(m_fileName) + ENO);
    }
}

void Streamer::CloseFile(void)
{
    LOG(VB_RECORD, LOG_INFO, LOC + "Streamer::Close -- begin");

    if (m_file)
    {
        delete m_file;
        m_file = nullptr;
    }

    LOG(VB_RECORD, LOG_INFO, LOC + "Streamer::Close -- end");
}

void Streamer::SendBytes(void)
{
    int pkt_size = 0;
    int buf_size = 0;

    LOG(VB_RECORD, LOG_DEBUG, LOC + "SendBytes -- start");

    if (!m_file)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "SendBytes -- file not open");
        return;
    }

    if (m_file->atEnd())
    {
        if (m_loop)
            m_file->reset();
        else if (m_buffer.isEmpty())
            m_parent->setEoF();
    }

    if (!m_file->atEnd())
    {
        int read_sz = m_blockSize.loadAcquire();
        if (!m_startTime.isValid())
            m_startTime = MythDate::current();
        qint64 delta = m_startTime.secsTo(MythDate::current()) + 1;
        int rate  = (delta * m_dataRate) - m_dataRead;

        read_sz = std::min(rate, read_sz);
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
        read_sz = std::min((m_bufferMax - m_buffer.size()), read_sz);
#else
        read_sz = std::min(static_cast<int>(m_bufferMax - m_buffer.size()), read_sz);
#endif

        if (read_sz > 0)
        {
            QByteArray buffer = m_file->read(read_sz);

            pkt_size = buffer.size();
            if (pkt_size > 0)
            {
                m_dataRead += pkt_size;
                if (m_buffer.size() + pkt_size > m_bufferMax)
                {
                    // This should never happen
                    LOG(VB_RECORD, LOG_WARNING, LOC +
                        QString("SendBytes -- Overflow: %1 > %2, "
                                "dropping first %3 bytes.")
                        .arg(m_buffer.size() + pkt_size)
                        .arg(m_bufferMax).arg(pkt_size));
                    m_buffer.remove(0, pkt_size);
                }
                m_buffer += buffer;
            }
        }
    }
    if (m_buffer.isEmpty())
    {
        LOG(VB_RECORD, LOG_DEBUG, LOC + "SendBytes -- Buffer is empty.");
        return;
    }

    buf_size = m_buffer.size();
    LOG(VB_RECORD, LOG_DEBUG, LOC +
        QString("SendBytes -- Read %1 from file.  %2 bytes buffered")
        .arg(pkt_size).arg(buf_size));

    int write_len = m_blockSize.loadAcquire();
    write_len = std::min(write_len, buf_size);
    LOG(VB_RECORD, LOG_DEBUG, LOC +
        QString("SendBytes -- writing %1 bytes").arg(write_len));

    int wrote = write(1, m_buffer.constData(), write_len);

    LOG(VB_RECORD, LOG_DEBUG, LOC +
        QString("SendBytes -- wrote %1 bytes").arg(wrote));

    if (wrote < buf_size)
    {
        m_buffer.remove(0, wrote);
        LOG(VB_RECORD, LOG_WARNING, LOC +
            QString("%1 bytes unwritten").arg(m_buffer.size()));
    }
    else
    {
        m_buffer.clear();
    }

    LOG(VB_RECORD, LOG_DEBUG, LOC + "SendBytes -- end");
}


Commands::Commands(void)
{
    setObjectName("Command");
}

bool Commands::send_status(const QString & status) const
{
    QByteArray buf = status.toLatin1() + '\n';
    int len = write(2, buf.constData(), buf.size());
    if (len != buf.size())
    {
        LOG(VB_RECORD, LOG_ERR, LOC +
            QString("Status -- Wrote %1 of %2 bytes of status '%3'")
            .arg(len).arg(status.size()).arg(status));
        return false;
    }
    LOG(VB_RECORD, LOG_DEBUG, "Status: " + status);
    return true;
}

bool Commands::process_command(QString & cmd)
{
    LOG(VB_RECORD, LOG_DEBUG, LOC + cmd);

    if (cmd.startsWith("Version?"))
    {
        send_status(QString("OK:%1").arg(VERSION));
        return true;
    }
    if (cmd.startsWith("APIVersion?"))
    {
        send_status(QString("OK:%1").arg(API_VERSION));
        return true;
    }
    if (cmd.startsWith("APIVersion:1"))
    {
        QString reply = (API_VERSION == 1) ? "OK:Yes": "OK:No";
        send_status(reply);
        return true;
    }
    if (cmd.startsWith("HasLock?"))
    {
        send_status(m_streamer->IsOpen() ? "OK:Yes" : "OK:No");
        return true;
    }
    if (cmd.startsWith("SignalStrengthPercent"))
    {
        send_status(m_streamer->IsOpen() ? "OK:100" : "OK:0");
        return true;
    }
    if (cmd.startsWith("LockTimeout"))
    {
        send_status("OK:1000");
        return true;
    }
    if (cmd.startsWith("HasTuner?"))
    {
        send_status("OK:No");
        return true;
    }
    if (cmd.startsWith("HasPictureAttributes?"))
    {
        send_status("OK:No");
        return true;
    }

    if (!m_streamer)
    {
        send_status("ERR:Not Initialized");
        LOG(VB_RECORD, LOG_ERR, LOC + QString("%1 failed - not initialized!")
            .arg(cmd));
        return false;
    }

    if (cmd.startsWith("SendBytes"))
    {
        if (!m_streamer->IsOpen())
        {
            send_status("ERR:file not open");
            LOG(VB_RECORD, LOG_ERR, LOC + "SendBytes - file not open.");
        }
        else
        {
            if (m_eof.loadAcquire() != 0)
                send_status("ERR:End of file");
            else
            {
                send_status("OK");
                emit SendBytes();
            }
        }
    }
#if 0
    else if (cmd.startsWith("XON"))
    {
        // Used when FlowControl is XON/XOFF
    }
    else if (cmd.startsWith("XOFF"))
    {
        // Used when FlowControl is XON/XOFF
    }
    else if (cmd.startsWith("TuneChannel"))
    {
        // Used if we announce that we have a 'tuner'

        /**
         * TODO:  extend to allow to 'tune' to different files.
         */
    }
#endif
    else if (cmd.startsWith("IsOpen?"))
    {
        if (m_streamer->IsOpen())
            send_status("OK:Open");
        else
            send_status(QString("ERR:Not Open: '%2'")
                        .arg(m_streamer->ErrorString()));
    }
    else if (cmd.startsWith("CloseRecorder"))
    {
        send_status("OK:Terminating");
        emit CloseFile();
        return false;
    }
    else if (cmd.startsWith("FlowControl?"))
    {
        send_status("OK:Polling");
    }
    else if (cmd.startsWith("BlockSize"))
    {
        m_streamer->BlockSize(cmd.mid(10).toInt());
        send_status("OK");
    }
    else if (cmd.startsWith("StartStreaming"))
    {
        send_status("OK:Started");
    }
    else if (cmd.startsWith("StopStreaming"))
    {
        /* This does not close the stream!  When Myth is done with
         * this 'recording' ExternalChannel::EnterPowerSavingMode()
         * will be called, which invokes CloseRecorder() */
        send_status("OK:Stopped");
    }
    else
    {
        send_status(QString("ERR:Unknown command '%1'").arg(cmd));
        LOG(VB_RECORD, LOG_ERR, LOC + QString("Unknown command '%1'")
            .arg(cmd));
    }

    return true;
}

bool Commands::Run(const QString & filename, int data_rate, bool loopinput)
{
    QString cmd;

    int poll_cnt = 1;
    std::array<struct pollfd,2> polls {};

    polls[0].fd      = 0;
    polls[0].events  = POLLIN | POLLPRI;
    polls[0].revents = 0;

    m_fileName = filename;

    m_streamer = new Streamer(this, m_fileName, data_rate, loopinput);
    auto *streamThread = new QThread(this);

    m_streamer->moveToThread(streamThread);
    connect(streamThread, &QThread::finished,
            m_streamer, &QObject::deleteLater);

    connect(this, &Commands::SendBytes,
            m_streamer, &Streamer::SendBytes);

    streamThread->start();

    QFile input;
    input.open(stdin, QIODevice::ReadOnly);
    QTextStream qtin(&input);

    LOG(VB_RECORD, LOG_INFO, LOC + "Listening for commands");

    while (m_run)
    {
        int ret = poll(polls.data(), poll_cnt, m_timeout);

        if (polls[0].revents & POLLHUP)
        {
            LOG(VB_RECORD, LOG_ERR, LOC + "poll eof (POLLHUP)");
            break;
        }
        if (polls[0].revents & POLLNVAL)
        {
            LOG(VB_RECORD, LOG_ERR, LOC + "poll error");
            return false;
        }

        if (polls[0].revents & POLLIN)
        {
            if (ret > 0)
            {
                cmd = qtin.readLine();
                if (!process_command(cmd))
                {
                    streamThread->quit();
                    streamThread->wait();
                    delete streamThread;
                    streamThread = nullptr;
                    m_streamer = nullptr;
                    m_run = false;
                }
            }
            else if (ret < 0)
            {
                if ((EOVERFLOW == errno))
                {
                    LOG(VB_RECORD, LOG_ERR, LOC + "command overflow.");
                    break; // we have an error to handle
                }

                if ((EAGAIN == errno) || (EINTR  == errno))
                {
                    LOG(VB_RECORD, LOG_ERR, LOC + "retry command read.");
                    continue; // errors that tell you to try again
                }

                LOG(VB_RECORD, LOG_ERR, LOC + "unknown error reading command.");
            }
        }
    }

    return true;
}


int main(int argc, char *argv[])
{
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
        MythFileRecorderCommandLineParser::PrintVersion();
        return GENERIC_EXIT_OK;
    }

    bool loopinput = !cmdline.toBool("noloop");
    int  data_rate = cmdline.toInt("data_rate");

    QCoreApplication a(argc, argv);
    QCoreApplication::setApplicationName("mythfilerecorder");

    int retval = cmdline.ConfigureLogging();
    if (retval != GENERIC_EXIT_OK)
        return retval;

    QString filename = "";
    if (!cmdline.toString("infile").isEmpty())
        filename = cmdline.toString("infile");
    else if (!cmdline.GetArgs().empty())
        filename = cmdline.GetArgs().at(0);

    Commands recorder;
    recorder.Run(filename, data_rate, loopinput);

    return GENERIC_EXIT_OK;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
