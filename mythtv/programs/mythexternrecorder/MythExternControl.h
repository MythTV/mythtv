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

#ifndef MYTHEXTERNCONTROL_H
#define MYTHEXTERNCONTROL_H

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
    static constexpr uint16_t kMaxQueue { 500 };

    explicit Buffer(MythExternControl * parent);
    ~Buffer(void) override = default;
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
    bool     m_dataSeen {false};

    std::chrono::time_point<std::chrono::system_clock> m_heartbeat;
};

class Commands : public QObject
{
    Q_OBJECT

  public:
    explicit Commands(MythExternControl * parent)
        : m_parent(parent) {}
    ~Commands(void) override = default;
    void Start(void) {
        m_thread = std::thread(&Commands::Run, this);
    }
    void Join(void) {
        if (m_thread.joinable())
            m_thread.join();
    }

    bool SendStatus(const QString & command,
                    const QString & status,
                    const QString & serial,
                    const QString & response = "");
    bool ProcessCommand(const QString & query);

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
    void TuneChannel(const QString & serial, const QVariantMap & args);
    void TuneStatus(const QString & serial);
    void LoadChannels(const QString & serial);
    void FirstChannel(const QString & serial);
    void NextChannel(const QString & serial);
    void Cleanup(void);

  private:
    std::thread m_thread;

    size_t       m_repCmdCnt  { 0 };
    QString      m_prevStatus;
    QString      m_prevMsgBuf;

    MythExternControl* m_parent { nullptr };
    int m_apiVersion { -1 };
};

class MythExternControl : public QObject
{
    Q_OBJECT

    friend class Buffer;
    friend class Commands;

  public:
    MythExternControl(void);
    ~MythExternControl(void) override;

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
    void LockTimeout(const QString & serial);
    void HasTuner(const QString & serial);
    void HasPictureAttributes(const QString & serial);
    void SetBlockSize(const QString & serial, int blksz);
    void TuneChannel(const QString & serial, const QVariantMap & args);
    void TuneStatus(const QString & serial);
    void LoadChannels(const QString & serial);
    void FirstChannel(const QString & serial);
    void NextChannel(const QString & serial);
    void Cleanup(void);
    void DataStarted(void);

  public slots:
    void SetDescription(const QString & desc) { m_desc = desc; }
    void SendMessage(const QString & command,
                     const QString & serial,
                     const QString & message,
                     const QString & status = "");

    void ErrorMessage(const QString & msg);
    void Opened(void);
    void Done(void);
    void Streaming(bool val);
    void Fill(const QByteArray & buffer) { m_buffer.Fill(buffer); }

  protected:
    Buffer       m_buffer;
    Commands     m_commands;
    QString      m_desc;

    std::atomic<bool> m_run              {true};
    std::atomic<bool> m_commandsRunning  {true};
    std::atomic<bool> m_bufferRunning    {true};
    std::mutex   m_runMutex;
    std::condition_variable m_runCond;
    std::mutex   m_msgMutex;

    bool         m_fatal                 {false};
    QString      m_errmsg;

    std::mutex        m_flowMutex;
    std::condition_variable m_flowCond;
    std::atomic<bool> m_streaming        {false};
    std::atomic<bool> m_xon              {false};
    std::atomic<bool> m_ready            {false};
};

#endif // MYTHEXTERNCONTROL_H
