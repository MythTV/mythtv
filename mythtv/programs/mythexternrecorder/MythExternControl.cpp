/*  -*- Mode: c++ -*-
 *
 * Copyright (C) John Poet 2018
 *
 * This file is part of MythTV
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "MythExternControl.h"
#include "mythlogging.h"

#include <QFile>
#include <QTextStream>

#include <unistd.h>
#include <poll.h>

#include <iostream>

using namespace std;

const QString VERSION = "0.1";

#define LOC Desc()

MythExternControl::MythExternControl(void)
    : m_buffer(this)
    , m_commands(this)
    , m_run(true)
    , m_commands_running(true)
    , m_buffer_running(true)
    , m_fatal(false)
    , m_streaming(false)
    , m_xon(false)
    , m_ready(false)
{
    setObjectName("Control");

    m_buffer.Start();
    m_commands.Start();
}

MythExternControl::~MythExternControl(void)
{
    Terminate();
    m_commands.Join();
    m_buffer.Join();
}

Q_SLOT void MythExternControl::Opened(void)
{
    std::lock_guard<std::mutex> lock(m_flow_mutex);

    m_ready = true;
    m_flow_cond.notify_all();
}

Q_SLOT void MythExternControl::Streaming(bool val)
{
    m_streaming = val;
    m_flow_cond.notify_all();
}

void MythExternControl::Terminate(void)
{
    emit Close();
}

Q_SLOT void MythExternControl::Done(void)
{
    if (m_commands_running || m_buffer_running)
    {
        m_run = false;
        m_flow_cond.notify_all();
        m_run_cond.notify_all();

        std::this_thread::sleep_for(std::chrono::microseconds(50));

        while (m_commands_running || m_buffer_running)
        {
            std::unique_lock<std::mutex> lk(m_flow_mutex);
            m_flow_cond.wait_for(lk, std::chrono::milliseconds(1000));
        }

        LOG(VB_RECORD, LOG_CRIT, LOC + "Terminated.");
    }
}

void MythExternControl::Error(const QString & msg)
{
    LOG(VB_RECORD, LOG_CRIT, LOC + msg);

    std::unique_lock<std::mutex> lk(m_msg_mutex);
    if (m_errmsg.isEmpty())
        m_errmsg = msg;
    else
        m_errmsg += "; " + msg;
}

void MythExternControl::Fatal(const QString & msg)
{
    Error(msg);
    m_fatal = true;
    Terminate();
}

Q_SLOT void MythExternControl::SendMessage(const QString & cmd,
                                           const QString & msg)
{
    std::unique_lock<std::mutex> lk(m_msg_mutex);
    m_commands.SendStatus(cmd, msg);
}

Q_SLOT void MythExternControl::ErrorMessage(const QString & msg)
{
    std::unique_lock<std::mutex> lk(m_msg_mutex);
    if (m_errmsg.isEmpty())
        m_errmsg = msg;
    else
        m_errmsg += "; " + msg;
}

#undef LOC
#define LOC QString("%1").arg(m_parent->Desc())

Commands::Commands(MythExternControl * parent)
    : m_thread()
    , m_parent(parent)
{
}

Commands::~Commands(void)
{
}

void Commands::Close(void)
{
    std::lock_guard<std::mutex> lock(m_parent->m_flow_mutex);

    emit m_parent->Close();
    m_parent->m_ready = false;
    m_parent->m_flow_cond.notify_all();
}

void Commands::StartStreaming(void)
{
    emit m_parent->StartStreaming();
}

void Commands::StopStreaming(bool silent)
{
    emit m_parent->StopStreaming(silent);
}

void Commands::LockTimeout(void) const
{
    emit m_parent->LockTimeout();
}

void Commands::HasTuner(void)   const
{
    emit m_parent->HasTuner();
}

void Commands::HasPictureAttributes(void) const
{
    emit m_parent->HasPictureAttributes();
}

void Commands::SetBlockSize(int blksz)
{
    emit m_parent->SetBlockSize(blksz);
}

void Commands::TuneChannel(const QString & channum)
{
    emit m_parent->TuneChannel(channum);
}

void Commands::LoadChannels(void)
{
    emit m_parent->LoadChannels();
}

void Commands::FirstChannel(void)
{
    emit m_parent->FirstChannel();
}

void Commands::NextChannel(void)
{
    emit m_parent->NextChannel();
}

bool Commands::SendStatus(const QString & command, const QString & status)
{
    int len = write(2, status.toUtf8().constData(), status.size());
    write(2, "\n", 1);

    if (len != static_cast<int>(status.size()))
    {
        LOG(VB_RECORD, LOG_ERR, LOC +
            QString("%1: Only wrote %2 of %3 bytes of status '%4'.")
            .arg(command).arg(len).arg(status.size()).arg(status));
        return false;
    }

    if (!command.isEmpty())
    {
        LOG(VB_RECORD, LOG_INFO, LOC + QString("Processing '%1' --> '%2'")
            .arg(command).arg(status));
    }
    else
        LOG(VB_RECORD, LOG_INFO, LOC + QString("%1").arg(status));

    m_parent->ClearError();
    return true;
}

bool Commands::ProcessCommand(const QString & cmd)
{
    LOG(VB_RECORD, LOG_DEBUG, LOC + QString("Processing '%1'").arg(cmd));

    std::unique_lock<std::mutex> lk(m_parent->m_msg_mutex);

    if (cmd.startsWith("Version?"))
    {
        if (m_parent->m_fatal)
            SendStatus(cmd, "ERR:" + m_parent->ErrorString());
        else
            SendStatus(cmd, "OK:" + VERSION);
    }
    else if (cmd.startsWith("HasLock?"))
    {
        if (m_parent->m_ready)
            SendStatus(cmd, "OK:Yes");
        else
            SendStatus(cmd, "OK:No");
    }
    else if (cmd.startsWith("SignalStrengthPercent"))
    {
        if (m_parent->m_ready)
            SendStatus(cmd, "OK:100");
        else
            SendStatus(cmd, "OK:20");
    }
    else if (cmd.startsWith("LockTimeout"))
    {
        LockTimeout();
    }
    else if (cmd.startsWith("HasTuner"))
    {
        HasTuner();
    }
    else if (cmd.startsWith("HasPictureAttributes"))
    {
        HasPictureAttributes();
    }
    else if (cmd.startsWith("SendBytes"))
    {
        // Used when FlowControl is Polling
        SendStatus(cmd, "ERR:Not supported");
    }
    else if (cmd.startsWith("XON"))
    {
        // Used when FlowControl is XON/XOFF
        SendStatus(cmd, "OK");
        m_parent->m_xon = true;
        m_parent->m_flow_cond.notify_all();
    }
    else if (cmd.startsWith("XOFF"))
    {
        SendStatus(cmd, "OK");
        // Used when FlowControl is XON/XOFF
        m_parent->m_xon = false;
        m_parent->m_flow_cond.notify_all();
    }
    else if (cmd.startsWith("TuneChannel"))
    {
        TuneChannel(cmd.mid(12));
    }
    else if (cmd.startsWith("LoadChannels"))
    {
        LoadChannels();
    }
    else if (cmd.startsWith("FirstChannel"))
    {
        FirstChannel();
    }
    else if (cmd.startsWith("NextChannel"))
    {
        NextChannel();
    }
    else if (cmd.startsWith("IsOpen?"))
    {
        std::unique_lock<std::mutex> lk(m_parent->m_run_mutex);
        if (m_parent->m_fatal)
            SendStatus(cmd, "ERR:" + m_parent->ErrorString());
        else if (m_parent->m_ready)
            SendStatus(cmd, "OK:Open");
        else
            SendStatus(cmd, "WARN:Not Open yet");
    }
    else if (cmd.startsWith("CloseRecorder"))
    {
        if (m_parent->m_streaming)
            StopStreaming(true);
        m_parent->Terminate();
        SendStatus(cmd, "OK:Terminating");
    }
    else if (cmd.startsWith("FlowControl?"))
    {
        SendStatus(cmd, "OK:XON/XOFF");
    }
    else if (cmd.startsWith("BlockSize"))
    {
        SetBlockSize(cmd.mid(10).toInt());
    }
    else if (cmd.startsWith("StartStreaming"))
    {
        StartStreaming();
    }
    else if (cmd.startsWith("StopStreaming"))
    {
        /* This does not close the stream!  When Myth is done with
         * this 'recording' ExternalChannel::EnterPowerSavingMode()
         * will be called, which invokes CloseRecorder() */
        StopStreaming(false);
    }
    else
        SendStatus(cmd, "ERR:Unrecognized command");

    return true;
}

void Commands::Run(void)
{
    setObjectName("Commands");

    QString cmd;
    int    timeout = 250;

    int ret;
    int poll_cnt = 1;
    struct pollfd polls[2];
    memset(polls, 0, sizeof(polls));

    polls[0].fd      = 0;
    polls[0].events  = POLLIN | POLLPRI;
    polls[0].revents = 0;

    QFile input;
    input.open(stdin, QIODevice::ReadOnly);
    QTextStream qtin(&input);

    LOG(VB_RECORD, LOG_INFO, LOC + "Command parser ready.");

    while (m_parent->m_run)
    {
        ret = poll(polls, poll_cnt, timeout);

        if (polls[0].revents & POLLHUP)
        {
            LOG(VB_RECORD, LOG_ERR, LOC + "poll eof (POLLHUP)");
            break;
        }
        else if (polls[0].revents & POLLNVAL)
        {
            m_parent->Fatal("poll error");
            return;
        }

        if (polls[0].revents & POLLIN)
        {
            if (ret > 0)
            {
                cmd = qtin.readLine();
                if (!ProcessCommand(cmd))
                    m_parent->Fatal("Invalid command");
            }
            else if (ret < 0)
            {
                if ((EOVERFLOW == errno))
                {
                    LOG(VB_RECORD, LOG_ERR, "command overflow");
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

    LOG(VB_RECORD, LOG_INFO, LOC + "Command parser: shutting down");
    m_parent->m_commands_running = false;
    m_parent->m_flow_cond.notify_all();
}

Buffer::Buffer(MythExternControl * parent)
    : m_parent(parent), m_thread()
{
    m_heartbeat = std::chrono::system_clock::now();
}

Buffer::~Buffer(void)
{
}

bool Buffer::Fill(const QByteArray & buffer)
{
    if (buffer.size() < 1)
        return false;

    static int dropped = 0;

    m_parent->m_flow_mutex.lock();
    if (m_data.size() < MAX_QUEUE)
    {
        block_t blk(reinterpret_cast<const uint8_t *>(buffer.constData()),
                    reinterpret_cast<const uint8_t *>(buffer.constData())
                    + buffer.size());

        m_data.push(blk);
        dropped = 0;

        LOG(VB_GENERAL, LOG_DEBUG, LOC +
            QString("Adding %1 bytes").arg(buffer.size()));
    }
    else
    {
        LOG(VB_RECORD, LOG_WARNING, LOC +
            QString("Packet queue overrun. Dropped %1 packets.")
            .arg(++dropped));
    }

    m_parent->m_flow_mutex.unlock();
    m_parent->m_flow_cond.notify_all();

    m_heartbeat = std::chrono::system_clock::now();

    return true;
}

void Buffer::Run(void)
{
    setObjectName("Buffer");

    bool       is_empty = false;
    bool       wait = false;
    time_t     send_time = time (NULL) + (60 * 5);
    uint64_t   write_total = 0;
    uint64_t   written = 0;
    uint64_t   write_cnt = 0;
    uint64_t   empty_cnt = 0;
    uint       sz;

    LOG(VB_RECORD, LOG_INFO, LOC + "Buffer: Ready for data.");

    while (m_parent->m_run)
    {
        {
            std::unique_lock<std::mutex> lk(m_parent->m_flow_mutex);
            m_parent->m_flow_cond.wait_for(lk,
                                           std::chrono::milliseconds
                                           (wait ? 5000 : 25));
            wait = false;
        }

        if (send_time < static_cast<double>(time (NULL)))
        {
            // Every 5 minutes, write out some statistics.
            send_time = time (NULL) + (60 * 5);
            write_total += written;
            if (m_parent->m_streaming)
                LOG(VB_RECORD, LOG_NOTICE, LOC +
                    QString("Count: %1, Empty cnt %2, Written %3, Total %4")
                    .arg(write_cnt).arg(empty_cnt)
                    .arg(written).arg(write_total));
            else
                LOG(VB_GENERAL, LOG_NOTICE, LOC + "Not streaming.");

            write_cnt = empty_cnt = written = 0;
        }

        if (m_parent->m_streaming)
        {
            if (m_parent->m_xon)
            {
                block_t pkt;
                m_parent->m_flow_mutex.lock();
                if (!m_data.empty())
                {
                    pkt = m_data.front();
                    m_data.pop();
                    is_empty = m_data.empty();
                }
                m_parent->m_flow_mutex.unlock();

                if (!pkt.empty())
                {
                    sz = write(1, pkt.data(), pkt.size());
                    written += sz;
                    ++write_cnt;

                    if (sz != pkt.size())
                    {
                        LOG(VB_GENERAL, LOG_WARNING, LOC +
                            QString("Only wrote %1 of %2 bytes to mythbackend")
                            .arg(sz).arg(pkt.size()));
                    }
                }

                if (is_empty)
                {
                    wait = true;
                    ++empty_cnt;
                }
            }
            else
                wait = true;
        }
        else
        {
            // Clear packet queue
            m_parent->m_flow_mutex.lock();
            if (!m_data.empty())
            {
                stack_t dummy;
                std::swap(m_data, dummy);
            }
            m_parent->m_flow_mutex.unlock();

            wait = true;
        }
    }

    LOG(VB_RECORD, LOG_INFO, LOC + "Buffer: shutting down");
    m_parent->m_buffer_running = false;
    m_parent->m_flow_cond.notify_all();
}
