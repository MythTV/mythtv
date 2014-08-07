
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
#define LOC QString("File(%1): ").arg(m_fileName)

Streamer::Streamer(Commands *parent, const QString &fname, bool loopinput) :
    m_parent(parent), m_fileName(fname), m_file(NULL), m_loop(loopinput),
    m_bufferMax(188 * 100000), m_blockSize(m_bufferMax / 4)
{
    setObjectName("Streamer");
    OpenFile();
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
        m_file = NULL;
    }

    LOG(VB_RECORD, LOG_INFO, LOC + "Streamer::Close -- end");
}

void Streamer::SendBytes(void)
{
    int read_sz, pkt_size, buf_size, write_len, wrote;

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
        read_sz = m_blockSize;
        if (read_sz > m_bufferMax - m_buffer.size())
            read_sz = m_bufferMax - m_buffer.size();
        QByteArray buffer = m_file->read(read_sz);

        pkt_size = buffer.size();
        if (pkt_size > 0)
        {
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

    if (m_buffer.isEmpty())
    {
        LOG(VB_RECORD, LOG_DEBUG, LOC + "SendBytes -- Buffer is empty.");
        return;
    }

    buf_size = m_buffer.size();
    LOG(VB_RECORD, LOG_DEBUG, LOC +
        QString("SendBytes -- Read %1 from file.  %2 bytes buffered")
        .arg(pkt_size).arg(buf_size));

    write_len = m_blockSize;
    if (write_len > buf_size)
        write_len = buf_size;
    LOG(VB_RECORD, LOG_DEBUG, LOC +
        QString("SendBytes -- writing %1 bytes").arg(write_len));

    wrote = write(1, m_buffer.constData(), write_len);

    LOG(VB_RECORD, LOG_DEBUG, LOC +
        QString("SendBytes -- wrote %1 bytes").arg(wrote));

    if (wrote < buf_size)
    {
        m_buffer.remove(0, wrote);
        LOG(VB_RECORD, LOG_WARNING, LOC +
            QString("%1 bytes unwritten").arg(m_buffer.size()));
    }
    else
        m_buffer.clear();

    LOG(VB_RECORD, LOG_DEBUG, LOC + "SendBytes -- end");
}


Commands::Commands(void) : m_run(true), m_eof(false)
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
        LOG(VB_RECORD, LOG_ERR, LOC +
            QString("Status -- Wrote %1 of %2 bytes of status '%3'")
            .arg(len).arg(status.size()).arg(status));
        return false;
    }
    else
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
            if (m_eof)
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

bool Commands::Run(const QString & filename, bool loopinput)
{
    QString cmd;

    int ret;
    int poll_cnt = 1;
    struct pollfd polls[2];
    memset(polls, 0, sizeof(polls));

    polls[0].fd      = 0;
    polls[0].events  = POLLIN | POLLPRI;
    polls[0].revents = 0;
    m_timeout = 10;

    m_fileName = filename;

    m_streamer = new Streamer(this, m_fileName, loopinput);
    QThread *streamThread = new QThread(this);

    m_streamer->moveToThread(streamThread);
    connect(streamThread, SIGNAL(finished(void)),
            m_streamer, SLOT(deleteLater(void)));

    connect(this, SIGNAL(SendBytes(void)),
            m_streamer, SLOT(SendBytes(void)));

    streamThread->start();

    QFile input;
    input.open(stdin, QIODevice::ReadOnly);
    QTextStream qtin(&input);

    LOG(VB_RECORD, LOG_INFO, LOC + "Listening for commands");

    while (m_run)
    {
        ret = poll(polls, poll_cnt, m_timeout);

        if (polls[0].revents & POLLHUP)
        {
            LOG(VB_RECORD, LOG_ERR, LOC + "poll eof (POLLHUP)");
            break;
        }
        else if (polls[0].revents & POLLNVAL)
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
                    streamThread = NULL;
                    m_streamer = NULL;
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
