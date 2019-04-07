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

#ifndef _MythExternControl_H_
#define _MythExternControl_H_

#include "MythExternRecApp.h"

#include <atomic>
#include <vector>
#include <queue>
#include <condition_variable>
#include <thread>

#include <QString>

class MythExternControl;

class Buffer : QObject
{
    Q_OBJECT

  public:
    enum constants {MAX_QUEUE = 500};

    Buffer(MythExternControl * parent);
    ~Buffer(void) = default;
    void Start(void) {
        m_thread = std::thread(&Buffer::Run, this);
    }
    void Join(void) {
        if (m_thread.joinable())
            m_thread.join();
    }
    bool Fill(const QByteArray & buffer);

    std::chrono::time_point<std::chrono::system_clock> HeartBeat(void) const
    { return m_heartbeat; }

  protected:
    void Run(void);

  private:
    using block_t = std::vector<uint8_t>;
    using stack_t = std::queue<block_t>;

    MythExternControl* m_parent;

    std::thread      m_thread;

    stack_t  m_data;

    std::chrono::time_point<std::chrono::system_clock> m_heartbeat;
};

class Commands : public QObject
{
    Q_OBJECT

  public:
    Commands(MythExternControl * parent)
        : m_parent(parent)
        , m_apiVersion(-1) {}
    ~Commands(void) = default;
    void Start(void) {
        m_thread = std::thread(&Commands::Run, this);
    }
    void Join(void) {
        if (m_thread.joinable())
            m_thread.join();
    }

    bool SendStatus(const QString & command, const QString & status);
    bool SendStatus(const QString & command, const QString & serial,
                    const QString & status);
    bool ProcessCommand(const QString & cmd);

  protected:
    void Run(void);
    bool Open(void);
    void Close(void);
    void StartStreaming(const QString & serial);
    void StopStreaming(const QString & serial, bool silent);
    void LockTimeout(const QString & serial) const;
    void HasTuner(const QString & serial) const;
    void HasPictureAttributes(const QString & serial) const;
    void SetBlockSize(const QString & serial, int blksz);
    void TuneChannel(const QString & serial, const QString & channum);
    void LoadChannels(const QString & serial);
    void FirstChannel(const QString & serial);
    void NextChannel(const QString & serial);

  private:
    std::thread m_thread;

    MythExternControl* m_parent;
    int m_apiVersion;
};

class MythExternControl : public QObject
{
    Q_OBJECT

    friend class Buffer;
    friend class Commands;

  public:
    MythExternControl(void);
    ~MythExternControl(void);

    QString Desc(void) const { return QString("%1: ").arg(m_desc); }

    void Terminate(void);

    void Error(const QString & msg);
    void Fatal(const QString & msg);

    QString ErrorString(void) const { return m_errmsg; }
    void ClearError(void) { m_errmsg.clear(); }

  signals:
    void Open(void);
    void Close(void);
    void StartStreaming(const QString & serial);
    void StopStreaming(const QString & serial, bool silent);
    void LockTimeout(const QString & serial) const;
    void HasTuner(const QString & serial) const;
    void HasPictureAttributes(const QString & serial) const;
    void SetBlockSize(const QString & serial, int blksz);
    void TuneChannel(const QString & serial, const QString & channum);
    void LoadChannels(const QString & serial);
    void FirstChannel(const QString & serial);
    void NextChannel(const QString & serial);

  public slots:
    void SetDescription(const QString & desc) { m_desc = desc; }
    void SendMessage(const QString & cmd, const QString & serial,
                     const QString & msg);
    void ErrorMessage(const QString & msg);
    void Opened(void);
    void Done(void);
    void Streaming(bool val);
    void Fill(const QByteArray & buffer) { m_buffer.Fill(buffer); }

  protected:
    Buffer       m_buffer;
    Commands     m_commands;
    QString      m_desc;

    std::atomic<bool> m_run;
    std::atomic<bool> m_commands_running;
    std::atomic<bool> m_buffer_running;
    std::mutex   m_run_mutex;
    std::condition_variable m_run_cond;
    std::mutex   m_msg_mutex;

    bool         m_fatal;
    QString      m_errmsg;

    std::mutex        m_flow_mutex;
    std::condition_variable m_flow_cond;
    std::atomic<bool> m_streaming;
    std::atomic<bool> m_xon;
    std::atomic<bool> m_ready;
};

#endif
