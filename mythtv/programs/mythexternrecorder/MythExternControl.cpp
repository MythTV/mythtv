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

#include <array>
#include <iostream>
#include <poll.h>
#include <unistd.h>

#include <QFile>
#include <QTextStream>

#include "libmythbase/mythlogging.h"

using namespace std::chrono_literals;

const QString VERSION = "1.0";

#define LOC Desc()

MythExternControl::MythExternControl(void)
    : m_buffer(this)
    , m_commands(this)
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
    std::lock_guard<std::mutex> lock(m_flowMutex);

    m_ready = true;
    m_flowCond.notify_all();
}

Q_SLOT void MythExternControl::Streaming(bool val)
{
    m_streaming = val;
    m_flowCond.notify_all();
}

void MythExternControl::Terminate(void)
{
    emit Close();
}

Q_SLOT void MythExternControl::Done(void)
{
    if (m_commandsRunning || m_bufferRunning)
    {
        m_run = false;
        m_flowCond.notify_all();
        m_runCond.notify_all();

        std::this_thread::sleep_for(50us);

        while (m_commandsRunning || m_bufferRunning)
        {
            std::unique_lock<std::mutex> lk(m_flowMutex);
            m_flowCond.wait_for(lk, 1s);
        }

        LOG(VB_RECORD, LOG_CRIT, LOC + "Terminated.");
    }
}

void MythExternControl::Error(const QString & msg)
{
    LOG(VB_RECORD, LOG_CRIT, LOC + msg);

    std::unique_lock<std::mutex> lk(m_msgMutex);
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
                                           const QString & serial,
                                           const QString & msg)
{
    std::unique_lock<std::mutex> lk(m_msgMutex);
    m_commands.SendStatus(cmd, serial, msg);
}

Q_SLOT void MythExternControl::ErrorMessage(const QString & msg)
{
    std::unique_lock<std::mutex> lk(m_msgMutex);
    if (m_errmsg.isEmpty())
        m_errmsg = msg;
    else
        m_errmsg += "; " + msg;
}

#undef LOC
#define LOC QString("%1").arg(m_parent->Desc())

void Commands::Close(void)
{
    std::lock_guard<std::mutex> lock(m_parent->m_flowMutex);

    emit m_parent->Close();
    m_parent->m_ready = false;
    m_parent->m_flowCond.notify_all();
}

void Commands::StartStreaming(const QString & serial)
{
    emit m_parent->StartStreaming(serial);
}

void Commands::StopStreaming(const QString & serial, bool silent)
{
    emit m_parent->StopStreaming(serial, silent);
}

void Commands::LockTimeout(const QString & serial) const
{
    emit m_parent->LockTimeout(serial);
}

void Commands::HasTuner(const QString & serial)   const
{
    emit m_parent->HasTuner(serial);
}

void Commands::HasPictureAttributes(const QString & serial) const
{
    emit m_parent->HasPictureAttributes(serial);
}

void Commands::SetBlockSize(const QString & serial, int blksz)
{
    emit m_parent->SetBlockSize(serial, blksz);
}

void Commands::TuneChannel(const QString & serial, const QString & channum)
{
    emit m_parent->TuneChannel(serial, channum);
}

void Commands::TuneStatus(const QString & serial)
{
    emit m_parent->TuneStatus(serial);
}

void Commands::LoadChannels(const QString & serial)
{
    emit m_parent->LoadChannels(serial);
}

void Commands::FirstChannel(const QString & serial)
{
    emit m_parent->FirstChannel(serial);
}

void Commands::NextChannel(const QString & serial)
{
    emit m_parent->NextChannel(serial);
}

void Commands::Cleanup(void)
{
    emit m_parent->Cleanup();
}

bool Commands::SendStatus(const QString & command, const QString & status)
{
    int len = write(2, status.toUtf8().constData(), status.size());
    len += write(2, "\n", 1);

    if (len != status.size() + 1)
    {
        LOG(VB_RECORD, LOG_ERR, LOC +
            QString("%1: Only wrote %2 of %3 bytes of message '%4'.")
            .arg(command).arg(len).arg(status.size()).arg(status));
        return false;
    }

    LOG(VB_RECORD, LOG_INFO, LOC + QString("Processing '%1' --> '%2'")
        .arg(command, status));

    m_parent->ClearError();
    return true;
}

bool Commands::SendStatus(const QString & command, const QString & serial,
                          const QString & status)
{
    QString msg = QString("%1:%2").arg(serial, status);

    int len = write(2, msg.toUtf8().constData(), msg.size());
    len += write(2, "\n", 1);

    if (len != msg.size() + 1)
    {
        LOG(VB_RECORD, LOG_ERR, LOC +
            QString("%1: Only wrote %2 of %3 bytes of message '%4'.")
            .arg(command).arg(len).arg(msg.size()).arg(msg));
        return false;
    }

    if (!command.isEmpty())
    {
        LOG(VB_RECORD, LOG_INFO, LOC + QString("Processing '%1' --> '%2'")
            .arg(command, msg));
    }
#if 0
    else
        LOG(VB_RECORD, LOG_INFO, LOC + QString("%1").arg(msg));
#endif

    m_parent->ClearError();
    return true;
}

bool Commands::ProcessCommand(const QString & cmd)
{
    LOG(VB_RECORD, LOG_DEBUG, LOC + QString("Processing '%1'").arg(cmd));

    std::unique_lock<std::mutex> lk1(m_parent->m_msgMutex);

    if (cmd.startsWith("APIVersion?"))
    {
        if (m_parent->m_fatal)
            SendStatus(cmd, "ERR:" + m_parent->ErrorString());
        else
            SendStatus(cmd, "OK:2");
        return true;
    }

    QStringList tokens = cmd.split(':', Qt::SkipEmptyParts);
    if (tokens.size() < 2)
    {
        SendStatus(cmd, "0",
                   QString("0:ERR:Version 2 API expects serial_no:msg format. "
                           "Saw '%1' instead").arg(cmd));
        return true;
    }

    if (tokens[1].startsWith("APIVersion?"))
    {
        if (m_parent->m_fatal)
            SendStatus(cmd, tokens[0], "ERR:" + m_parent->ErrorString());
        else
            SendStatus(cmd, tokens[0], "OK:2");
    }
    else if (tokens[1].startsWith("APIVersion"))
    {
        if (tokens.size() > 1)
        {
            m_apiVersion = tokens[2].toInt();
            SendStatus(cmd, tokens[0], QString("OK:%1").arg(m_apiVersion));
        }
        else
            SendStatus(cmd, tokens[0], "ERR:Missing API Version number");
    }
    else if (tokens[1].startsWith("Version?"))
    {
        if (m_parent->m_fatal)
            SendStatus(cmd, tokens[0], "ERR:" + m_parent->ErrorString());
        else
            SendStatus(cmd, tokens[0], "OK:" + VERSION);
    }
    else if (tokens[1].startsWith("Description?"))
    {
        if (m_parent->m_fatal)
            SendStatus(cmd, tokens[0], "ERR:" + m_parent->ErrorString());
        else if (m_parent->m_desc.trimmed().isEmpty())
            SendStatus(cmd, tokens[0], "WARN:Not set");
        else
            SendStatus(cmd, tokens[0], "OK:" + m_parent->m_desc.trimmed());
    }
    else if (tokens[1].startsWith("HasLock?"))
    {
        if (m_parent->m_ready)
            SendStatus(cmd, tokens[0], "OK:Yes");
        else
            SendStatus(cmd, tokens[0], "OK:No");
    }
    else if (tokens[1].startsWith("SignalStrengthPercent"))
    {
        if (m_parent->m_ready)
            SendStatus(cmd, tokens[0], "OK:100");
        else
            SendStatus(cmd, tokens[0], "OK:20");
    }
    else if (tokens[1].startsWith("LockTimeout?"))
    {
        LockTimeout(tokens[0]);
    }
    else if (tokens[1].startsWith("HasTuner"))
    {
        HasTuner(tokens[0]);
    }
    else if (tokens[1].startsWith("HasPictureAttributes"))
    {
        HasPictureAttributes(tokens[0]);
    }
    else if (tokens[1].startsWith("SendBytes"))
    {
        // Used when FlowControl is Polling
        SendStatus(cmd, tokens[0], "ERR:Not supported");
    }
    else if (tokens[1].startsWith("XON"))
    {
        // Used when FlowControl is XON/XOFF
        if (m_parent->m_streaming)
        {
            SendStatus(cmd, tokens[0], "OK");
            m_parent->m_xon = true;
            m_parent->m_flowCond.notify_all();
        }
        else
            SendStatus(cmd, tokens[0], "WARN:Not streaming");
    }
    else if (tokens[1].startsWith("XOFF"))
    {
        if (m_parent->m_streaming)
        {
            SendStatus(cmd, tokens[0], "OK");
            // Used when FlowControl is XON/XOFF
            m_parent->m_xon = false;
            m_parent->m_flowCond.notify_all();
        }
        else
            SendStatus(cmd, tokens[0], "WARN:Not streaming");
    }
    else if (tokens[1].startsWith("TuneChannel"))
    {
        if (tokens.size() > 2)
            TuneChannel(tokens[0], tokens[2]);
        else
            SendStatus(cmd, tokens[0], "ERR:Missing channum");
    }
    else if (tokens[1].startsWith("TuneStatus?"))
    {
        TuneStatus(tokens[0]);
    }
    else if (tokens[1].startsWith("LoadChannels"))
    {
        LoadChannels(tokens[0]);
    }
    else if (tokens[1].startsWith("FirstChannel"))
    {
        FirstChannel(tokens[0]);
    }
    else if (tokens[1].startsWith("NextChannel"))
    {
        NextChannel(tokens[0]);
    }
    else if (tokens[1].startsWith("IsOpen?"))
    {
        std::unique_lock<std::mutex> lk2(m_parent->m_runMutex);
        if (m_parent->m_fatal)
            SendStatus(cmd, tokens[0], "ERR:" + m_parent->ErrorString());
        else if (m_parent->m_ready)
            SendStatus(cmd, tokens[0], "OK:Open");
        else
            SendStatus(cmd, tokens[0], "WARN:Not Open yet");
    }
    else if (tokens[1].startsWith("CloseRecorder"))
    {
        if (m_parent->m_streaming)
            StopStreaming(tokens[0], true);
        m_parent->Terminate();
        SendStatus(cmd, tokens[0], "OK:Terminating");
        Cleanup();
    }
    else if (tokens[1].startsWith("FlowControl?"))
    {
        SendStatus(cmd, tokens[0], "OK:XON/XOFF");
    }
    else if (tokens[1].startsWith("BlockSize"))
    {
        if (tokens.size() > 1)
            SetBlockSize(tokens[0], tokens[2].toUInt());
        else
            SendStatus(cmd, tokens[0], "ERR:Missing block size");
    }
    else if (tokens[1].startsWith("StartStreaming"))
    {
        StartStreaming(tokens[0]);
    }
    else if (tokens[1].startsWith("StopStreaming"))
    {
        /* This does not close the stream!  When Myth is done with
         * this 'recording' ExternalChannel::EnterPowerSavingMode()
         * will be called, which invokes CloseRecorder() */
        StopStreaming(tokens[0], false);
    }
    else
        SendStatus(cmd, tokens[0],
                   QString("ERR:Unrecognized command '%1'").arg(tokens[1]));

    return true;
}

void Commands::Run(void)
{
    setObjectName("Commands");

    QString cmd;

    std::array<struct pollfd,2> polls {};

    polls[0].fd      = 0;
    polls[0].events  = POLLIN | POLLPRI;
    polls[0].revents = 0;

    QFile input;
    input.open(stdin, QIODevice::ReadOnly);
    QTextStream qtin(&input);

    LOG(VB_RECORD, LOG_INFO, LOC + "Command parser ready.");

    while (m_parent->m_run)
    {
        int timeout = 250;
        int poll_cnt = 1;
        int ret = poll(polls.data(), poll_cnt, timeout);

        if (polls[0].revents & POLLHUP)
        {
            LOG(VB_RECORD, LOG_ERR, LOC + "poll eof (POLLHUP)");
            break;
        }
        if (polls[0].revents & POLLNVAL)
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
    m_parent->m_commandsRunning = false;
    m_parent->m_flowCond.notify_all();
}

Buffer::Buffer(MythExternControl * parent)
    : m_parent(parent)
{
    m_heartbeat = std::chrono::system_clock::now();
}

bool Buffer::Fill(const QByteArray & buffer)
{
    if (buffer.size() < 1)
        return false;

    static int s_dropped = 0;
    static int s_droppedBytes = 0;

    m_parent->m_flowMutex.lock();

    if (!m_dataSeen)
    {
        m_dataSeen = true;
        emit m_parent->DataStarted();
    }

    if (m_data.size() < MAX_QUEUE)
    {
        block_t blk(reinterpret_cast<const uint8_t *>(buffer.constData()),
                    reinterpret_cast<const uint8_t *>(buffer.constData())
                    + buffer.size());

        m_data.push(blk);
        s_dropped = 0;

        LOG(VB_GENERAL, LOG_DEBUG, LOC +
            QString("Adding %1 bytes").arg(buffer.size()));
    }
    else
    {
        s_droppedBytes += buffer.size();
        LOG(VB_RECORD, LOG_WARNING, LOC +
            QString("Packet queue overrun. Dropped %1 packets, %2 bytes.")
            .arg(++s_dropped).arg(s_droppedBytes));

        std::this_thread::sleep_for(250us);
    }

    m_parent->m_flowMutex.unlock();
    m_parent->m_flowCond.notify_all();

    m_heartbeat = std::chrono::system_clock::now();

    return true;
}

void Buffer::Run(void)
{
    setObjectName("Buffer");

    bool       is_empty = false;
    bool       wait = false;
    auto       send_time = std::chrono::system_clock::now() + 5min;
    uint64_t   write_total = 0;
    uint64_t   written = 0;
    uint64_t   write_cnt = 0;
    uint64_t   empty_cnt = 0;

    LOG(VB_RECORD, LOG_INFO, LOC + "Buffer: Ready for data.");

    while (m_parent->m_run)
    {
        {
            std::unique_lock<std::mutex> lk(m_parent->m_flowMutex);
            m_parent->m_flowCond.wait_for(lk, wait ? 5s : 25ms);
            wait = false;
        }

        if (send_time < std::chrono::system_clock::now())
        {
            // Every 5 minutes, write out some statistics.
            send_time = std::chrono::system_clock::now() + 5min;
            write_total += written;
            if (m_parent->m_streaming)
            {
                LOG(VB_RECORD, LOG_NOTICE, LOC +
                    QString("Count: %1, Empty cnt %2, Written %3, Total %4")
                    .arg(write_cnt).arg(empty_cnt)
                    .arg(written).arg(write_total));
            }
            else
            {
                LOG(VB_GENERAL, LOG_NOTICE, LOC + "Not streaming.");
            }

            write_cnt = empty_cnt = written = 0;
        }

        if (m_parent->m_streaming)
        {
            if (m_parent->m_xon)
            {
                block_t pkt;
                m_parent->m_flowMutex.lock();
                if (!m_data.empty())
                {
                    pkt = m_data.front();
                    m_data.pop();
                    is_empty = m_data.empty();
                }
                m_parent->m_flowMutex.unlock();

                if (!pkt.empty())
                {
                    uint sz = write(1, pkt.data(), pkt.size());
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
            m_parent->m_flowMutex.lock();
            if (!m_data.empty())
            {
                stack_t dummy;
                std::swap(m_data, dummy);
            }
            m_parent->m_flowMutex.unlock();

            wait = true;
        }
    }

    LOG(VB_RECORD, LOG_INFO, LOC + "Buffer: shutting down");
    m_parent->m_bufferRunning = false;
    m_parent->m_flowCond.notify_all();
}
